#include <QLocalServer>
#include <QLocalSocket>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>
#include <memory>
#include <vector>

#include "OpenTraderPlatform/StrategySDK/ExternalStrategyRuntime.h"
#include "OpenTraderPlatform/StrategySDK/MessageFraming.h"

namespace SDK = OpenTraderPlatform::StrategySDK;
namespace Protocol = opentraderplatform::strategy::v1;

class ErrorHandler : public SDK::ExternalStrategyHandler
{
  public:
    void onStart(const Protocol::StrategyConfiguration&) override {}
    void onHostError(const Protocol::ErrorMessage& p_error) override
    {
        errors.push_back(p_error);
    }
    std::vector<Protocol::ErrorMessage> errors;
};

class StrategySubscriptionErrorTests : public QObject
{
    Q_OBJECT

  private slots:
    void deliversSubscriptionFailuresViaExistingCallback()
    {
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
        QCOMPARE(exitCode, 0);
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
    }
};

QTEST_GUILESS_MAIN(StrategySubscriptionErrorTests)
#include "StrategySubscriptionErrorTests.moc"
