#pragma once

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

#include "AuthToken.h"
#include "ClientToken.h"

Q_DECLARE_LOGGING_CATEGORY(TSAuthWindowLog)

class AuthWindow : public QDialog
{
    Q_OBJECT

public:
    explicit AuthWindow(QWidget *parent = nullptr);
    ~AuthWindow() override;

signals:
    void authFinished(bool success, AuthToken token, QString reason);

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

    AuthToken authToken;
    ClientToken clientToken;

    QWebEngineView *webView = nullptr;
    QTcpServer *httpServer = nullptr;
    QNetworkAccessManager *networkManager = nullptr;
    
    QString redirectUri;
    QString expectedState;

    // Server configuration
    static const quint16 DEFAULT_PORT = 8080;
    static const quint16 MAX_PORT_ATTEMPTS = 10;
    quint16 currentPort = DEFAULT_PORT;

    // Instance-specific UI and server functions
    void setupUi();
    void startAuthorization();
    void startHttpServer();
    bool tryBindPort(quint16 port);
    void updateRedirectUri(quint16 port);
    void exchangeCodeForTokens(const QString& code);
    bool parseTokenResponse(const QJsonObject& response);
    QString generateRandomState();

    // Credential management
    bool promptForCredentials();
};
