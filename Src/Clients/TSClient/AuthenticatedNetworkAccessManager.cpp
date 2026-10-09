#include "AuthenticatedNetworkAccessManager.h"

#include <QLoggingCategory>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <memory>
#include <utility>

#include "Assume.h"
#include "CONSTANTS.h"

namespace
{
    Q_LOGGING_CATEGORY(authenticationGateLog, "TSClient.authenticationGate")
    Q_LOGGING_CATEGORY(networkWatchdogLog, "TSClient.networkWatchdog")

    class BlockedReply final : public QNetworkReply
    {
      public:
        BlockedReply(const QNetworkRequest& p_request, QNetworkAccessManager::Operation p_operation, QObject* p_parent)
            : QNetworkReply(p_parent)
        {
            setRequest(p_request);
            setUrl(p_request.url());
            setOperation(p_operation);
            open(QIODevice::ReadOnly | QIODevice::Unbuffered);
            setError(QNetworkReply::AuthenticationRequiredError,
                     "TradeStation request blocked locally: authenticate in Credentials first.");
            QTimer::singleShot(0,
                               this,
                               [this]()
                               {
                                   setFinished(true);
                                   emit errorOccurred(error());
                                   emit finished();
                               });
        }

        void abort() override {}

      protected:
        qint64 readData(char*, qint64) override
        {
            return -1;
        }
    };
} // namespace

AuthenticatedNetworkAccessManager::AuthenticatedNetworkAccessManager(AuthorizationCheck p_authorized, QObject* p_parent)
    : QNetworkAccessManager(p_parent)
    , m_authorized(std::move(p_authorized))
    , m_streamFirstResponseTimeoutMs(TSClientNetworkConstants::STREAM_FIRST_RESPONSE_TIMEOUT_MS)
    , m_restTransferTimeoutMs(TSClientNetworkConstants::REST_TRANSFER_TIMEOUT_MS)
{
    ASSUME_TRUE(static_cast<bool>(m_authorized));
}

void AuthenticatedNetworkAccessManager::setWatchdogTimeouts(int p_streamFirstResponseMs, int p_restTransferMs)
{
    ASSUME_GT(p_streamFirstResponseMs, 0);
    ASSUME_GT(p_restTransferMs, 0);
    m_streamFirstResponseTimeoutMs = p_streamFirstResponseMs;
    m_restTransferTimeoutMs = p_restTransferMs;
}

QNetworkReply* AuthenticatedNetworkAccessManager::createRequest(Operation p_operation,
                                                                const QNetworkRequest& p_request,
                                                                QIODevice* p_outgoingData)
{
    if (!m_authorized(p_request))
    {
        if (!m_reportedBlocked)
        {
            qCWarning(authenticationGateLog) << "TradeStation API requests paused until credentials are usable";
            m_reportedBlocked = true;
        }
        auto* reply = new BlockedReply(p_request, p_operation, this);
        Q_CHECK_PTR(reply);
        return reply;
    }
    m_reportedBlocked = false;

    QNetworkReply* reply = QNetworkAccessManager::createRequest(p_operation, p_request, p_outgoingData);
    Q_CHECK_PTR(reply);
    watchReply(reply, p_request.attribute(StreamingRequestAttribute).toBool());
    return reply;
}

void AuthenticatedNetworkAccessManager::retire()
{
    ASSUME_FALSE(m_retired);
    m_retired = true;
    if (m_liveReplies == 0)
    {
        deleteLater();
    }
}

void AuthenticatedNetworkAccessManager::watchReply(QNetworkReply* p_reply, bool p_isStreaming)
{
    ++m_liveReplies;
    auto c = connect(p_reply, &QObject::destroyed, this, &AuthenticatedNetworkAccessManager::onReplyDestroyed);
    ASSUME_TRUE(c);

    const int timeoutMs = p_isStreaming ? m_streamFirstResponseTimeoutMs : m_restTransferTimeoutMs;
    const QString description =
        QStringLiteral("%1 %2").arg(p_isStreaming ? QStringLiteral("stream") : QStringLiteral("request"),
                                    p_reply->url().path());

    // Owned by the reply so it dies with it
    auto* watchdog = new QTimer(p_reply);
    Q_CHECK_PTR(watchdog);
    watchdog->setSingleShot(true);
    auto responded = std::make_shared<bool>(false);

    // Streams only need a first sign of life; REST requests are watched for progress until finished
    const auto onProgress = [watchdog, responded, p_isStreaming]()
    {
        *responded = true;
        if (p_isStreaming)
        {
            watchdog->stop();
        }
        else
        {
            watchdog->start();
        }
    };
    c = connect(p_reply, &QNetworkReply::metaDataChanged, watchdog, onProgress);
    ASSUME_TRUE(c);
    c = connect(p_reply, &QNetworkReply::readyRead, watchdog, onProgress);
    ASSUME_TRUE(c);
    c = connect(p_reply, &QNetworkReply::finished, watchdog, &QTimer::stop);
    ASSUME_TRUE(c);

    c = connect(watchdog,
                &QTimer::timeout,
                this,
                [this, reply = QPointer<QNetworkReply>(p_reply), responded, description, timeoutMs, p_isStreaming]()
                {
                    if (!*responded)
                    {
                        qCWarning(networkWatchdogLog) << "No response from TradeStation for" << description << "within"
                                                      << timeoutMs << "ms; HTTP/2 connection looks stalled";
                        emit connectionStalled(description);
                    }

                    if (!p_isStreaming && !reply.isNull() && reply->isRunning())
                    {
                        qCWarning(networkWatchdogLog)
                            << "Aborting" << description << "after" << timeoutMs << "ms without progress";
                        reply->abort();
                    }
                });
    ASSUME_TRUE(c);

    watchdog->start(timeoutMs);
}

void AuthenticatedNetworkAccessManager::onReplyDestroyed()
{
    --m_liveReplies;
    ASSUME_GTE(m_liveReplies, 0);
    if (m_retired && m_liveReplies == 0)
    {
        deleteLater();
    }
}
