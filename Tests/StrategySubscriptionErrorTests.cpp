#include <QLocalServer>
#include <QLocalSocket>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>
#include <array>
#include <cstring>
#include <memory>
#include <vector>

#include "OpenTraderPlatform/StrategySDK/ExternalStrategyRuntime.h"
#include "OpenTraderPlatform/StrategySDK/MessageFraming.h"

namespace SDK = OpenTraderPlatform::StrategySDK;
namespace Protocol = opentraderplatform::strategy::v1;

class ErrorHandler : public SDK::ExternalStrategyHandler
{
  public:
    void onStart(const Protocol::StrategyConfiguration&) override
    {
        if (claimDuringStart)
        {
            const auto claim = runtime->claimSymbols({"AMDO"});
            claimRejected = claim.grantedSymbols.empty();
            readyAfterFailure = !runtime->hasFailed();
        }
    }
    void onHostError(const Protocol::ErrorMessage& p_error) override
    {
        errors.push_back(p_error);
        if (failRequired && p_error.has_subscription_error() && p_error.subscription_error().terminal() &&
            p_error.subscription_error().symbol() == "AMDO")
        {
            runtime->fail("Required symbol AMDO rejected by TradeStation: invalid symbol");
            runtime->fail("This duplicate failure must not overwrite the original");
            blockedLog = !runtime->log("Must not continue after failure");
        }
    }
    void onStop(std::string_view p_reason) override
    {
        stopReason = p_reason;
    }
    std::vector<Protocol::ErrorMessage> errors;
    std::string stopReason;
    SDK::ExternalStrategyRuntime* runtime = nullptr;
    bool failRequired = false;
    bool claimDuringStart = false;
    bool claimRejected = false;
    bool readyAfterFailure = false;
    bool blockedLog = false;
};

class StrategySubscriptionErrorTests : public QObject
{
    Q_OBJECT

  private slots:
    void deliversSubscriptionFailuresViaExistingCallback_data()
    {
        QTest::addColumn<bool>("failRequired");
        QTest::addColumn<bool>("claimDuringStart");
        QTest::newRow("continue-on-errors") << false << false;
        QTest::newRow("required-symbol-fails") << true << false;
        QTest::newRow("required-symbol-fails-during-startup-claim") << true << true;
    }

    void deliversSubscriptionFailuresViaExistingCallback()
    {
        QFETCH(bool, failRequired);
        QFETCH(bool, claimDuringStart);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QByteArray oldID = qgetenv("OPENTRADERPLATFORM_STRATEGY_ID");
        const QByteArray oldSocket = qgetenv("OPENTRADERPLATFORM_STRATEGY_SOCKET");
        const auto restoreEnvironment = qScopeGuard(
            [&]()
            {
                qputenv("OPENTRADERPLATFORM_STRATEGY_ID", oldID);
                qputenv("OPENTRADERPLATFORM_STRATEGY_SOCKET", oldSocket);
            });
        const QString socketPath = directory.path() + "/host.sock";
        qputenv("OPENTRADERPLATFORM_STRATEGY_ID", "test-subscription-errors");
        qputenv("OPENTRADERPLATFORM_STRATEGY_SOCKET", socketPath.toUtf8());
        QLocalServer server;
        QVERIFY(server.listen(socketPath));
        ErrorHandler handler;
        SDK::StrategyDescription description{"SubscriptionErrorTest", "1.0.0", {}};
        SDK::ExternalStrategyRuntime runtime(description, handler);
        handler.runtime = &runtime;
        handler.failRequired = failRequired;
        handler.claimDuringStart = claimDuringStart;
        int exitCode = -1;
        std::unique_ptr<QThread> thread(QThread::create([&]() { exitCode = runtime.run(); }));
        QLocalSocket* socket = nullptr;
        const auto cleanup = qScopeGuard(
            [&]()
            {
                if (socket)
                {
                    socket->abort();
                }
                if (!thread->wait(5000))
                {
                    qFatal("Strategy subscription test runtime failed to stop");
                }
            });
        thread->start();
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 5000);
        socket = server.nextPendingConnection();
        QVERIFY(socket);

        if (claimDuringStart)
        {
            Protocol::HostToStrategyEnvelope start;
            start.mutable_start()->mutable_configuration()->set_name("SubscriptionErrorTest");
            const auto frame = SDK::serializeFramedMessage(start);
            QCOMPARE(socket->write(reinterpret_cast<const char*>(frame.data()), frame.size()),
                     static_cast<qint64>(frame.size()));
        }
        for (const bool terminal: {false, true})
        {
            Protocol::HostToStrategyEnvelope envelope;
            auto* error = envelope.mutable_error();
            error->set_code(terminal ? "market_data_subscription_rejected" : "market_data_subscription_retrying");
            error->set_message("AMDO depth subscription failed");
            auto* details = error->mutable_subscription_error();
            details->set_symbol("AMDO");
            details->set_feed("depth");
            details->set_reason(terminal ? "BadRequest" : "Timeout");
            details->set_terminal(terminal);
            details->set_retry_delay_ms(terminal ? 0 : 1000);
            const auto frame = SDK::serializeFramedMessage(envelope);
            QCOMPARE(socket->write(reinterpret_cast<const char*>(frame.data()), frame.size()),
                     static_cast<qint64>(frame.size()));
        }
        Protocol::HostToStrategyEnvelope shutdown;
        shutdown.mutable_shutdown()->set_reason("Test completed");
        const auto frame = SDK::serializeFramedMessage(shutdown);
        QCOMPARE(socket->write(reinterpret_cast<const char*>(frame.data()), frame.size()),
                 static_cast<qint64>(frame.size()));
        socket->flush();
        QTRY_VERIFY_WITH_TIMEOUT(thread->isFinished(), 5000);
        QVERIFY(thread->wait(5000));
        QCOMPARE(exitCode, failRequired ? 1 : 0);
        QCOMPARE(handler.errors.size(), size_t(2));
        QVERIFY(handler.errors[0].has_subscription_error());
        const auto& transient = handler.errors[0].subscription_error();
        QCOMPARE(QString::fromStdString(transient.symbol()), QString("AMDO"));
        QCOMPARE(QString::fromStdString(transient.feed()), QString("depth"));
        QVERIFY(!transient.terminal());
        QCOMPARE(transient.retry_delay_ms(), 1000u);
        const auto& terminal = handler.errors[1].subscription_error();
        QVERIFY(terminal.terminal());
        QCOMPARE(terminal.retry_delay_ms(), 0u);
        QCOMPARE(QString::fromStdString(terminal.reason()), QString("BadRequest"));
        if (failRequired)
        {
            QVERIFY(handler.blockedLog);
            QCOMPARE(QString::fromStdString(handler.stopReason),
                     QString("Required symbol AMDO rejected by TradeStation: invalid symbol"));
            if (claimDuringStart)
            {
                QVERIFY(handler.claimRejected);
                QVERIFY(!handler.readyAfterFailure);
            }
            QTRY_VERIFY_WITH_TIMEOUT(socket->bytesAvailable() > 0, 5000);
            QTest::qWait(50);
            const QByteArray outbound = socket->readAll();
            int offset = 0;
            int failures = 0;
            while (outbound.size() - offset >= static_cast<int>(SDK::kFramePrefixSize))
            {
                std::array<std::uint8_t, SDK::kFramePrefixSize> prefix;
                std::memcpy(prefix.data(), outbound.constData() + offset, prefix.size());
                offset += static_cast<int>(prefix.size());
                const int length = static_cast<int>(SDK::decodeFrameSize(prefix));
                QVERIFY(length <= outbound.size() - offset);
                Protocol::StrategyToHostEnvelope envelope;
                QVERIFY(envelope.ParseFromArray(outbound.constData() + offset, length));
                offset += length;
                QVERIFY(!envelope.has_strategy_ready());
                if (envelope.has_error() && envelope.error().strategy_failure())
                {
                    ++failures;
                    QCOMPARE(QString::fromStdString(envelope.error().message()),
                             QString("Required symbol AMDO rejected by TradeStation: invalid symbol"));
                }
            }
            QCOMPARE(failures, 1);
        }
    }
};

QTEST_GUILESS_MAIN(StrategySubscriptionErrorTests)
#include "StrategySubscriptionErrorTests.moc"
