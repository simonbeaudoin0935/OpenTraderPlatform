#pragma once

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <functional>

/**
 * QNetworkAccessManager used for all TradeStation traffic.
 *
 * Besides gating requests on authentication, it watches every real reply for a wedged HTTP/2
 * connection: a request that never gets a response is reported through connectionStalled(), and
 * REST requests without progress are aborted. Once retire()d, the manager deletes itself after the
 * last reply it created has been destroyed, so established streams keep working on the old connection
 * while new requests go through a fresh manager.
 */
class AuthenticatedNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT

  public:
    using AuthorizationCheck = std::function<bool(const QNetworkRequest&)>;

    // Request attribute marking long-lived streaming requests (no REST transfer timeout).
    static constexpr QNetworkRequest::Attribute StreamingRequestAttribute =
        static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 1);

    explicit AuthenticatedNetworkAccessManager(AuthorizationCheck p_authorized, QObject* p_parent = nullptr);

    /**
     * Stop being the active manager. The object deletes itself once all of its replies are destroyed.
     */
    void retire();

    [[nodiscard]] bool isRetired() const
    {
        return m_retired;
    }

    // Overrides the TSClientNetworkConstants watchdog delays for requests created afterwards (tests)
    void setWatchdogTimeouts(int p_streamFirstResponseMs, int p_restTransferMs);

  signals:
    /**
     * @brief A request issued through this manager received no response at all within its watchdog delay
     * Thread context: Emitted from TSClient thread
     * @param p_description Request kind and URL path of the stalled request
     */
    void connectionStalled(const QString& p_description);

  protected:
    QNetworkReply* createRequest(Operation p_operation,
                                 const QNetworkRequest& p_request,
                                 QIODevice* p_outgoingData = nullptr) override;

  private:
    void watchReply(QNetworkReply* p_reply, bool p_isStreaming);
    void onReplyDestroyed();

    AuthorizationCheck m_authorized;
    bool m_reportedBlocked = false;
    bool m_retired = false;
    int m_liveReplies = 0;
    int m_streamFirstResponseTimeoutMs;
    int m_restTransferTimeoutMs;
};
