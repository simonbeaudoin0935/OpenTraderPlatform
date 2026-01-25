#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QString>

#include "AuthToken.h"
#include "ClientToken.h"
#include "CONSTANTS.h"

Q_DECLARE_LOGGING_CATEGORY(TSAuthHandlerLog)

// Base class for OAuth authentication handling
// Provides core OAuth logic (HTTP server, token exchange) without GUI dependencies
class AuthHandler : public QObject
{
    Q_OBJECT

  public:
    explicit AuthHandler(QObject* parent = nullptr);
    ~AuthHandler() override;

    // Start the authentication process
    // Returns true if initialization succeeded, false otherwise
    [[nodiscard]] bool startAuthentication();

    // Get the resulting auth token (valid only after successful authentication)
    [[nodiscard]] AuthToken getAuthToken() const
    {
        return m_authToken;
    }

  signals:
    void authFinished(bool success, AuthToken token, QString reason);

  protected:
    // Virtual methods to be implemented by subclasses for different UI modes
    virtual bool promptForCredentials(QString& clientId, QString& clientSecret) = 0;
    virtual void showAuthUrl(const QString& authUrl) = 0;
    virtual void showError(const QString& title, const QString& message) = 0;
    virtual void showServerError(const QString& errorMsg);

    // Protected data members accessible to subclasses
    AuthToken m_authToken;
    ClientToken m_clientToken;
    QString m_redirectUri;
    QString m_expectedState;

  private slots:
    void handleNewConnection();
    void handleSocketReadyRead();
    void handleCodeReceived(const QString& code);
    void handleTokenResponse(const QJsonObject& response);
    void handleTokenError(const QString& error);
    void handleSocketError(QAbstractSocket::SocketError socketError);
    void handleSocketStateChanged(QAbstractSocket::SocketState socketState);

  private:
    QTcpServer* m_httpServer = nullptr;
    QNetworkAccessManager* m_networkManager = nullptr;

    // Server configuration
    quint16 m_currentPort = AuthConstants::DEFAULT_AUTH_PORT;

    // Core OAuth logic methods
    bool loadOrPromptCredentials();
    void startHttpServer();
    bool tryBindPort(quint16 port);
    void updateRedirectUri(quint16 port);
    void startAuthorization();
    void exchangeCodeForTokens(const QString& code);
    bool parseTokenResponse(const QJsonObject& response);
    QString generateRandomState();
};
