#include <QLocalServer>
#include <QLocalSocket>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QThread>
#include <QTest>
#include <atomic>
#include <memory>

#include "OpenTraderPlatform/StrategySDK/MessageFraming.h"
#include "OpenTraderPlatform/StrategySDK/UnixSocketConnection.h"
#include "OpenTraderPlatform/StrategySDK/ExternalStrategyRuntime.h"
#include "opentraderplatform_strategy_v1.pb.h"

namespace SDK = OpenTraderPlatform::StrategySDK;
namespace Protocol = opentraderplatform::strategy::v1;

class StrategyTransportTests : public QObject
{
    Q_OBJECT

  private slots:
    void receivesFrames_data()
    {
        QTest::addColumn<int>("chunkSize");
        QTest::addColumn<int>("frameCount");
        QTest::newRow("bytewise-prefix-and-payload") << 1 << 1;
        QTest::newRow("fragmented-payload") << 7 << 1;
        QTest::newRow("coalesced-frames") << 4096 << 3;
    }

    void receivesFrames()
    {
        QFETCH(int, chunkSize);
        QFETCH(int, frameCount);
        exercise(chunkSize, frameCount, -1, false);
    }

    void disconnectDuringFrame_data()
    {
        QTest::addColumn<int>("bytesBeforeDisconnect");
        QTest::newRow("partial-prefix") << 2;
        QTest::newRow("partial-payload") << 6;
    }

    void disconnectDuringFrame()
    {
        QFETCH(int, bytesBeforeDisconnect);
        exercise(4096, 1, bytesBeforeDisconnect, false);
    }

    void rejectsOversizedPrefix()
    {
        exercise(4096, 1, -1, true);
    }

    void rejectsOversizedSerialization()
    {
        Protocol::HostToStrategyEnvelope envelope;
        envelope.mutable_shutdown()->set_reason(std::string(SDK::kMaxFramePayloadSize, 'x'));
        QVERIFY(SDK::serializeFramedMessage(envelope).empty());
    }

    void runtimeRejectsMalformedOrTruncatedFrame_data()
    {
        QTest::addColumn<bool>("truncatePayload");
        QTest::newRow("malformed-protobuf") << false;
        QTest::newRow("disconnect-mid-payload") << true;
    }

    void runtimeRejectsMalformedOrTruncatedFrame()
    {
        QFETCH(bool, truncatePayload);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QByteArray oldID = qgetenv("OPENTRADERPLATFORM_STRATEGY_ID");
        const QByteArray oldSocket = qgetenv("OPENTRADERPLATFORM_STRATEGY_SOCKET");
        const bool hadID = qEnvironmentVariableIsSet("OPENTRADERPLATFORM_STRATEGY_ID");
        const bool hadSocket = qEnvironmentVariableIsSet("OPENTRADERPLATFORM_STRATEGY_SOCKET");
        const auto restore = qScopeGuard(
            [&]()
            {
                if (hadID)
                    qputenv("OPENTRADERPLATFORM_STRATEGY_ID", oldID);
                else
                    qunsetenv("OPENTRADERPLATFORM_STRATEGY_ID");
                if (hadSocket)
                    qputenv("OPENTRADERPLATFORM_STRATEGY_SOCKET", oldSocket);
                else
                    qunsetenv("OPENTRADERPLATFORM_STRATEGY_SOCKET");
            });
        const QString path = directory.path() + "/runtime.sock";
        qputenv("OPENTRADERPLATFORM_STRATEGY_ID", "transport-runtime-test");
        qputenv("OPENTRADERPLATFORM_STRATEGY_SOCKET", path.toUtf8());
        QLocalServer server;
        QVERIFY(server.listen(path));
        struct Handler : SDK::ExternalStrategyHandler
        {
            void onStart(const Protocol::StrategyConfiguration&) override
            {
                started = true;
            }
            bool started = false;
        } handler;
        SDK::ExternalStrategyRuntime runtime({"TransportRuntimeTest", "1.0.0", {}}, handler);
        int exitCode = -1;
        std::unique_ptr<QThread> worker(QThread::create([&]() { exitCode = runtime.run(); }));
        QLocalSocket* peer = nullptr;
        const auto cleanup = qScopeGuard(
            [&]()
            {
                if (peer)
                    peer->abort();
                if (!worker->wait(5000))
                    qFatal("Malformed-frame runtime failed to stop");
            });
        worker->start();
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 5000);
        peer = server.nextPendingConnection();
        QVERIFY(peer);
        const auto prefix = SDK::encodeFrameSize(truncatePayload ? 8 : 1);
        QByteArray wire(reinterpret_cast<const char*>(prefix.data()), prefix.size());
        wire.append(char(0xff));
        QCOMPARE(peer->write(wire), wire.size());
        peer->flush();
        QTRY_COMPARE_WITH_TIMEOUT(peer->bytesToWrite(), qint64(0), 5000);
        if (truncatePayload)
            peer->disconnectFromServer();
        QTRY_VERIFY_WITH_TIMEOUT(worker->isFinished(), 5000);
        QVERIFY(worker->wait(5000));
        QCOMPARE(exitCode, 1);
        QVERIFY(!handler.started);
    }

  private:
    void exercise(int p_chunkSize, int p_frameCount, int p_disconnectAfter, bool p_oversized)
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QLocalServer server;
        const QString path = directory.path() + "/transport.sock";
        QVERIFY(server.listen(path));
        SDK::UnixSocketConnection connection;
        QVERIFY(connection.connectTo(path.toStdString()));
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 5000);
        std::unique_ptr<QLocalSocket> peer(server.nextPendingConnection());
        QVERIFY(peer);

        Protocol::HostToStrategyEnvelope envelope;
        envelope.mutable_shutdown()->set_reason("Transport fixture payload");
        const auto frame = SDK::serializeFramedMessage(envelope);
        QByteArray wire;
        if (p_oversized)
        {
            const auto prefix = SDK::encodeFrameSize(SDK::kMaxFramePayloadSize + 1);
            wire.append(reinterpret_cast<const char*>(prefix.data()), prefix.size());
        }
        else
        {
            for (int i = 0; i < p_frameCount; ++i)
                wire.append(reinterpret_cast<const char*>(frame.data()), frame.size());
        }
        if (p_disconnectAfter >= 0)
            wire.truncate(p_disconnectAfter);

        bool success = true;
        std::vector<std::vector<std::uint8_t>> payloads;
        std::atomic<bool> finished{false};
        std::unique_ptr<QThread> reader(QThread::create(
            [&]()
            {
                for (int i = 0; i < p_frameCount; ++i)
                {
                    std::vector<std::uint8_t> payload;
                    if (!connection.readFrame(&payload))
                    {
                        success = false;
                        break;
                    }
                    payloads.push_back(std::move(payload));
                }
                finished.store(true);
            }));
        const auto cleanup = qScopeGuard(
            [&]()
            {
                peer->abort();
                if (!reader->wait(5000))
                    qFatal("Transport fixture reader failed to stop");
            });
        reader->start();
        for (qsizetype offset = 0; offset < wire.size(); offset += p_chunkSize)
        {
            const auto chunk = wire.mid(offset, p_chunkSize);
            QCOMPARE(peer->write(chunk), chunk.size());
            peer->flush();
            QTRY_COMPARE_WITH_TIMEOUT(peer->bytesToWrite(), qint64(0), 5000);
        }
        if (p_disconnectAfter >= 0)
            peer->disconnectFromServer();
        QTRY_VERIFY_WITH_TIMEOUT(finished.load(), 5000);
        QVERIFY(reader->wait(5000));
        if (p_oversized || p_disconnectAfter >= 0)
        {
            QVERIFY(!success);
            QVERIFY(payloads.empty());
            return;
        }
        QVERIFY(success);
        QCOMPARE(payloads.size(), static_cast<std::size_t>(p_frameCount));
        for (const auto& payload: payloads)
        {
            Protocol::HostToStrategyEnvelope parsed;
            QVERIFY(SDK::parseMessagePayload(payload, &parsed));
            QCOMPARE(parsed.shutdown().reason(), envelope.shutdown().reason());
        }
    }
};

QTEST_GUILESS_MAIN(StrategyTransportTests)
#include "StrategyTransportTests.moc"
