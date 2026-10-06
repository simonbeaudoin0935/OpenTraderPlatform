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
