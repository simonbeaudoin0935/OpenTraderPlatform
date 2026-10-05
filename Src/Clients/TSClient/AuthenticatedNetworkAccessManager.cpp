#include "AuthenticatedNetworkAccessManager.h"

#include <QLoggingCategory>
#include <QNetworkReply>
#include <QTimer>
#include <utility>

#include "Assume.h"

namespace
{
    Q_LOGGING_CATEGORY(authenticationGateLog, "TSClient.authenticationGate")

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
    : QNetworkAccessManager(p_parent), m_authorized(std::move(p_authorized))
{
    ASSUME_TRUE(static_cast<bool>(m_authorized));
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
    return QNetworkAccessManager::createRequest(p_operation, p_request, p_outgoingData);
}
