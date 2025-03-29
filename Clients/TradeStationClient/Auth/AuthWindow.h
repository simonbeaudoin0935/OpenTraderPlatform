#ifndef AUTHWINDOW_H
#define AUTHWINDOW_H

#include <QDialog>
#include <QWebEngineView>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QTimer>
#include <QSettings>
#include <QInputDialog>
#include <QMessageBox>

Q_DECLARE_LOGGING_CATEGORY(tsAuth)

class AuthWindow : public QDialog
{
    Q_OBJECT

public:
    explicit AuthWindow(QWidget *parent = nullptr);
    ~AuthWindow() override;

    // Static getter methods for authentication results
    static QString getAccessToken() { return accessToken; }
    static QString getRefreshToken() { return refreshToken; }
    static QString getIdToken() { return idToken; }
    static bool isAlreadyAuthenticated() { return loadTokens(); }

signals:
    void authenticationCompleted(bool success);
    void authenticationFailed(const QString error);

private slots:
    void handleNewConnection();
    void handleSocketReadyRead();
    void handleCodeReceived(const QString& code);
    void handleTokenResponse(const QJsonObject& response);
    void handleTokenError(const QString& error);
    void handleSocketError(QAbstractSocket::SocketError socketError);
    void handleSocketStateChanged(QAbstractSocket::SocketState socketState);
    void handleDialogFinished(int result);

private:
    QWebEngineView *webView = nullptr;
    QTcpServer *httpServer = nullptr;
    QNetworkAccessManager *networkManager = nullptr;
    QSettings *credentialsStore = nullptr;
    
    QString clientId;
    QString clientSecret;
    QString redirectUri;
    QString expectedState;
    static QString accessToken;
    static QString refreshToken;
    static QString idToken;
    QDateTime tokenReceivedTime;
    int tokenTimeoutSeconds; // Token timeout in seconds

    // Server configuration
    static const quint16 DEFAULT_PORT = 8080;
    static const quint16 MAX_PORT_ATTEMPTS = 10;
    quint16 currentPort = DEFAULT_PORT;

    // Static token management functions
    static bool loadTokens();
    static bool isTokenExpired(const QDateTime& tokenReceivedTime, int tokenTimeoutSeconds);
    static void clearTokens();

    // Instance-specific UI and server functions
    void setupUi();
    void startAuthorization();
    void startHttpServer();
    bool tryBindPort(quint16 port);
    void updateRedirectUri(quint16 port);
    void exchangeCodeForTokens(const QString& code);
    bool parseTokenResponse(const QJsonObject& response);
    void saveTokens();
    QString generateRandomState();

    // Token validation
    bool areTokensValid() const;
    void setTokenTimeout(int seconds);

    // Credential management
    bool loadCredentials();
    bool saveCredentials(const QString& clientId, const QString& clientSecret);
    bool promptForCredentials();
    void initializeCredentialStore();
    void clearCredentials();
    bool validateCredentials(const QString& clientId, const QString& clientSecret);
};

#endif // AUTHWINDOW_H
