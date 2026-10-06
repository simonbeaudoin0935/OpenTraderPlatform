#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <cstring>
#include <memory>

#include "MarketData/StreamMarketData.h"
#include "MainAlgo.h"
#include "Misc/SecureStorage.h"
#include "Misc/Settings.h"
#include "Misc/YubiKeyStorage.h"
#include "TSClient.h"

class ErrorReply : public QNetworkReply
{
  public:
    explicit ErrorReply(const QByteArray& p_body, int p_status) : m_body(p_body)
    {
        open(QIODevice::ReadOnly);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, p_status);
        if (p_status != 200)
        {
            setError(QNetworkReply::UnknownContentError, "Invalid symbol");
        }
    }

    void abort() override {}
    qint64 bytesAvailable() const override
    {
        return m_body.size() + QNetworkReply::bytesAvailable();
    }
    bool isSequential() const override
    {
        return true;
    }
    void complete()
    {
        setFinished(true);
        emit finished();
    }

  protected:
    qint64 readData(char* p_data, qint64 p_maxSize) override
    {
        const qint64 count = std::min(p_maxSize, static_cast<qint64>(m_body.size()));
        if (count == 0)
        {
            return -1;
        }
        std::memcpy(p_data, m_body.constData(), static_cast<size_t>(count));
        m_body.remove(0, count);
        return count;
    }

  private:
    QByteArray m_body;
};

class TestMarketStream : public StreamMarketData
{
  public:
    TestMarketStream(QNetworkReply* p_reply, QObject* p_parent) : StreamMarketData(p_reply, p_parent) {}

  protected:
    void processJsonObject(const QJsonObject&) override {}
};

class StreamRecoveryTests : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase()
    {
        QVERIFY(m_directory.isValid());
        qputenv("XDG_CONFIG_HOME", m_directory.path().toUtf8());
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_directory.path());
        QVERIFY(SecureStorage::configureBackend(SecureStorage::Backend::YubiKey));
        QString error;
        // Lock only the isolated test vault so client startup cannot touch hardware or a real keyring.
        QVERIFY2(YubiKeyStorage::reset(error), qPrintable(error));
        m_settings = std::make_unique<QSettings>(m_directory.path() + "/state.ini", QSettings::IniFormat);
        appStateSettings = m_settings.get();
        TSClient::getInstance()->start();
        auto startup = std::make_shared<QPromise<void>>();
        startup->start();
        const auto ready = startup->future();
        QVERIFY(QMetaObject::invokeMethod(
            TSClient::getInstance(),
            [startup]()
            {
                TSClient::getInstance()->setMode(TSClient::Mode::Replay);
                startup->finish();
            },
            Qt::QueuedConnection));
        QTRY_VERIFY_WITH_TIMEOUT(ready.isFinished(), 5000);
    }

    void finalErrorBodyIsParsedOnce_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<int>("status");
        QTest::newRow("no-newline") << QByteArray("{\"Error\":\"BadRequest\",\"Message\":\"invalid symbol\"}") << 400;
        QTest::newRow("newline") << QByteArray("{\"Error\":\"BadRequest\",\"Message\":\"invalid symbol\"}\n") << 400;
        QTest::newRow("bars-invalid-symbol")
            << QByteArray("{\"Error\":\"InvalidSymbol\",\"Message\":\"Symbol cannot be found\"}") << 200;
        QTest::newRow("quotes-invalid-symbol") << QByteArray("{\"Error\":\"FAILED, INVALID SYMBOL\"}") << 200;
        QTest::newRow("http-only") << QByteArray() << 400;
    }

    void retriesAreTerminalOrBackedOff()
    {
        LiveStreamRetryState rejected;
        QVERIFY(!rejected.schedule(Stream::StreamError::BadRequest).has_value());
        QVERIFY(rejected.terminal);
        QVERIFY(!rejected.schedule(Stream::StreamError::Failed).has_value());
        LiveStreamRetryState forbidden;
        QVERIFY(!forbidden.schedule(Stream::StreamError::Forbidden).has_value());
        QVERIFY(forbidden.terminal);

        LiveStreamRetryState retry;
        QCOMPARE(retry.schedule(Stream::StreamError::Timeout).value(), StreamConstants::LIVE_RETRY_INITIAL_DELAY_MS);
        QVERIFY(!retry.schedule(Stream::StreamError::Failed).has_value());
        retry.pending = false;
        QCOMPARE(retry.schedule(Stream::StreamError::Failed).value(), StreamConstants::LIVE_RETRY_INITIAL_DELAY_MS * 2);
        for (int attempt = 0; attempt < 10; ++attempt)
        {
            retry.pending = false;
            QVERIFY(retry.schedule(Stream::StreamError::Failed).value() <= StreamConstants::LIVE_RETRY_MAX_DELAY_MS);
        }
        QCOMPARE(retry.delayMs, StreamConstants::LIVE_RETRY_MAX_DELAY_MS);
    }

    void finalErrorBodyIsParsedOnce()
    {
        QFETCH(QByteArray, body);
        QFETCH(int, status);
        auto* reply = new ErrorReply(body, status);
        auto* stream = new TestMarketStream(reply, this);
        QSignalSpy closed(stream, &Stream::streamClosed);
        reply->complete();
        reply->complete();
        QCOMPARE(closed.size(), 1);
        QCOMPARE(qvariant_cast<Stream::StreamError>(closed.first().first()), Stream::StreamError::BadRequest);
        if (!body.isEmpty())
        {
            QVERIFY(closed.first().at(1).toString().contains(
                QJsonDocument::fromJson(body).object().value("Error").toString()));
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void factoriesWorkOnClientThread()
    {
        bool created = false;
        auto* client = TSClient::getInstance();
        QVERIFY(QMetaObject::invokeMethod(
            client,
            [&]()
            {
                const auto bars = client->openStreamBars("TEST", 1, TSClient::BarUnit::Minute);
                const auto quote = client->openStreamQuote({"TEST"});
                const auto positions = client->openStreamPositions("SIM123456");
                const auto orders = client->openStreamOrders("SIM123456");
                created = !bars.isNull() && !quote.isNull() && !positions.isNull() && !orders.isNull();
                for (Stream* stream: {static_cast<Stream*>(bars.data()),
                                      static_cast<Stream*>(quote.data()),
                                      static_cast<Stream*>(positions.data()),
                                      static_cast<Stream*>(orders.data())})
                {
                    if (stream)
                    {
                        client->closeStream(stream);
                    }
                }
            },
            Qt::BlockingQueuedConnection));
        QVERIFY(created);
    }

    void queuedDepthCompletesWithoutSelfDeadlock()
    {
        auto* client = TSClient::getInstance();
        QList<QPointer<StreamMarketDepthAggregate>> streams;
        for (int index = 0; index < MarketDepthConstants::MAX_CONCURRENT_STREAMS; ++index)
        {
            const auto result = client->openStreamMarketDepthAggregate(QString("TEST%1").arg(index), 10);
            QVERIFY(result.has_value());
            streams.append(result.value());
        }
        auto queued = client->openStreamMarketDepthAggregate("QUEUED", 10);
        QVERIFY(!queued.has_value());
        auto future = queued.error();
        client->closeStream(streams.takeFirst());
        QTRY_VERIFY_WITH_TIMEOUT(future.isFinished(), 5000);
        const auto stream = future.result();
        QVERIFY(!stream.isNull());
        streams.append(stream);
        for (const auto& remaining: streams)
        {
            client->closeStream(remaining);
        }
        QTRY_COMPARE_WITH_TIMEOUT(StreamMarketDepthAggregate::getNumberOfMarketDepthAggregateStreams(), 0, 5000);
    }

    void cleanupTestCase()
    {
        Stream::setShuttingDown(true);
        TSClient::destroyInstance();
        appStateSettings = nullptr;
    }

  private:
    QTemporaryDir m_directory;
    std::unique_ptr<QSettings> m_settings;
};

QTEST_GUILESS_MAIN(StreamRecoveryTests)
#include "StreamRecoveryTests.moc"
