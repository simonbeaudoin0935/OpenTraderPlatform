#pragma once

#include <QNetworkAccessManager>
#include <functional>

class AuthenticatedNetworkAccessManager : public QNetworkAccessManager
{
  public:
    using AuthorizationCheck = std::function<bool(const QNetworkRequest&)>;
    explicit AuthenticatedNetworkAccessManager(AuthorizationCheck p_authorized, QObject* p_parent = nullptr);

  protected:
    QNetworkReply* createRequest(Operation p_operation,
                                 const QNetworkRequest& p_request,
                                 QIODevice* p_outgoingData = nullptr) override;

  private:
    AuthorizationCheck m_authorized;
    bool m_reportedBlocked = false;
};
