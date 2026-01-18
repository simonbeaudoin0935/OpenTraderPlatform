#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QTimer>

#include "AuthToken.h"
#include "ClientToken.h"

Q_DECLARE_LOGGING_CATEGORY(TSAuthHandlerLog)

/**
 * @brief Base class for TradeStation OAuth authentication handlers
 * 
 * This class provides the core OAuth 2.0 authentication logic including:
 * - Local HTTP server for redirect URI handling
 * - CSRF state generation and validation
 * - Token exchange with TradeStation
 * - Credential management
 * 
 * Subclasses implement user interaction (GUI dialogs vs command-line prompts)
 */
class AuthHandler : public QObject
{
    Q_OBJECT

public:
    explicit AuthHandler(QObject *parent = nullptr);
    ~AuthHandler() override;

    /**
     * @brief Start the authentication process
     * 
     * This will:
     * 1. Load or prompt for client credentials
     * 2. Start HTTP server
     * 3. Display authorization URL to user
     * 4. Wait for callback with authorization code
     * 5. Exchange code for tokens
     */
    virtual void startAuthentication();

signals:
    /**
     * @brief Emitted when authentication completes (success or failure)
     * @param success True if authentication succeeded
     * @param token The received auth token (invalid if success=false)
     * @param reason Human-readable status message
     */
    void authFinished(bool success, AuthToken token, QString reason);

protected:
    // Core OAuth data
    AuthToken m_authToken;
    ClientToken m_clientToken;
    QString m_redirectUri;
    QString m_expectedState;

    // Network components
    QTcpServer *m_httpServer = nullptr;
    QNetworkAccessManager *m_networkManager = nullptr;

    // Server configuration
    static const quint16 DEFAULT_PORT = 8080;
    static const quint16 MAX_PORT_ATTEMPTS = 10;
    quint16 m_currentPort = DEFAULT_PORT;

    // Pure virtual methods - subclasses must implement user interaction
    
    /**
     * @brief Prompt user for client credentials
     * @return True if valid credentials were obtained
     */
    virtual bool promptForCredentials() = 0;
    
    /**
     * @brief Display the authorization URL to the user
     * @param authUrl The URL the user needs to visit
     * 
     * GUI implementation: Opens in system browser
     * TUI implementation: Prints URL for manual copy/paste
     */
    virtual void displayAuthorizationUrl(const QString& authUrl) = 0;
    
    /**
     * @brief Show error message to user
     * @param title Error title/category
     * @param message Detailed error message
     */
    virtual void showError(const QString& title, const QString& message) = 0;

    // Common OAuth methods
    void startHttpServer();
    bool tryBindPort(quint16 port);
    void updateRedirectUri(quint16 port);
    QString generateRandomState();
    void exchangeCodeForTokens(const QString& code);
    bool parseTokenResponse(const QJsonObject& response);

private slots:
    void handleNewConnection();
    void handleSocketReadyRead();
    void handleCodeReceived(const QString& code);
    void handleTokenResponse(const QJsonObject& response);
    void handleTokenError(const QString& error);
    void handleSocketError(QAbstractSocket::SocketError socketError);
    void handleSocketStateChanged(QAbstractSocket::SocketState socketState);
};
