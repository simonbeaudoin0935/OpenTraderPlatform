#include "AuthWindow.h"
#include <QVBoxLayout>
#include <QUrlQuery>
#include <QDebug>
#include <QRegularExpression>
#include <QStatusBar>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QSettings>
#include <QFile>
#include <QRandomGenerator>
#include <QDateTime>
#include <QTimer>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialogButtonBox>

Q_LOGGING_CATEGORY(tsAuth, "tradestation.auth")

// Define static members
QString AuthWindow::accessToken;
QString AuthWindow::refreshToken;
QString AuthWindow::idToken;

AuthWindow::AuthWindow(QWidget *parent) : QDialog(parent)
{
    qCDebug(tsAuth) << "Initializing TradeStation Auth Window";
    
    // Set dialog properties
    setWindowTitle("TradeStation Authentication");
    setModal(true);
    setFixedSize(800, 800);
    setAttribute(Qt::WA_DeleteOnClose);  // Ensure dialog is deleted when closed
    
    // Connect dialog finished signal first
    connect(this, &QDialog::finished, this, &AuthWindow::handleDialogFinished);
    
    // Load tokens first and check if they're valid
    if (loadTokens()) {
        qCDebug(tsAuth) << "Valid tokens found, completing authentication immediately";
        QTimer::singleShot(0, this, &QDialog::accept);
        return;
    }
    
    // Generate random state for CSRF protection
    expectedState = generateRandomState();
    qCDebug(tsAuth) << "Generated expected state:" << expectedState;
    
    // Load credentials first
    if (!loadCredentials()) {
        qCDebug(tsAuth) << "Failed to load or obtain valid credentials";
        QMessageBox::critical(this, "Error",
                            "Unable to obtain valid TradeStation API credentials. "
                            "The authentication will be cancelled.");
        QTimer::singleShot(0, this, &QDialog::reject);
        return;
    }

    // Initialize other components
    networkManager = new QNetworkAccessManager(this);
    setupUi();
    startHttpServer();
    startAuthorization();
}

AuthWindow::~AuthWindow() = default;

void AuthWindow::handleDialogFinished(int result)
{
    if (result == QDialog::Accepted) {
        emit authenticationCompleted(true);
    } else {
        emit authenticationFailed("Authentication cancelled or failed");
    }
}

void AuthWindow::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    
    // Create web view
    webView = new QWebEngineView(this);
    layout->addWidget(webView);
    
    // Add dialog buttons
    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Cancel,
        Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    setLayout(layout);
}

void AuthWindow::startHttpServer()
{
    qCDebug(tsAuth) << "Starting HTTP server...";
    
    // Create server if not exists
    if (!httpServer) {
        httpServer = new QTcpServer(this);
    }

    // Check if server is already listening
    if (httpServer->isListening()) {
        qCDebug(tsAuth) << "HTTP server is already running on port" << httpServer->serverPort();
        return;
    }

    // Try to bind to default port first
    currentPort = DEFAULT_PORT;
    bool serverStarted = false;

    for (quint16 portAttempt = 0; portAttempt < MAX_PORT_ATTEMPTS && !serverStarted; ++portAttempt) {
        quint16 portToTry = currentPort + portAttempt;
        qCDebug(tsAuth) << "Attempting to start server on port" << portToTry;
        
        if (tryBindPort(portToTry)) {
            currentPort = portToTry;
            serverStarted = true;
            updateRedirectUri(currentPort);
            qCDebug(tsAuth) << "Successfully bound to port" << currentPort;
            break;
        }
        
        qCDebug(tsAuth) << "Failed to bind to port" << portToTry << ", trying next port";
    }

    if (!serverStarted) {
        QString errorMsg = QString("Failed to find available port after %1 attempts").arg(MAX_PORT_ATTEMPTS);
        qCDebug(tsAuth) << errorMsg;
        QMessageBox::warning(this, "Server Error", errorMsg);
        return;
    }

    connect(httpServer, &QTcpServer::newConnection, this, &AuthWindow::handleNewConnection);
    qCDebug(tsAuth) << "HTTP server successfully started on" 
                    << httpServer->serverAddress().toString() 
                    << "port" << httpServer->serverPort();
}

bool AuthWindow::tryBindPort(quint16 port)
{
    if (httpServer->listen(QHostAddress::LocalHost, port)) {
        return true;
    }

    QString errorMsg;
    switch (httpServer->serverError()) {
        case QAbstractSocket::AddressInUseError:
            errorMsg = QString("Port %1 is already in use by another application.").arg(port);
            break;
        case QAbstractSocket::SocketAccessError:
            errorMsg = QString("Permission denied to use port %1.").arg(port);
            break;
        case QAbstractSocket::SocketResourceError:
            errorMsg = "System resource limit reached.";
            break;
        case QAbstractSocket::UnsupportedSocketOperationError:
            errorMsg = "The local HTTP server operation is not supported on this system.";
            break;
        default:
            errorMsg = QString("Failed to start HTTP server on port %1: %2")
                        .arg(port)
                        .arg(httpServer->errorString());
    }
    
    qCDebug(tsAuth) << "Port" << port << "binding failed:" << errorMsg;
    return false;
}

void AuthWindow::updateRedirectUri(quint16 port)
{
    // Update the redirect URI with the new port
    redirectUri = QString("http://localhost:%1/callback").arg(port);
    qCDebug(tsAuth) << "Updated redirect URI:" << redirectUri;
}

void AuthWindow::startAuthorization()
{
    qCDebug(tsAuth) << "Starting authorization process...";
    // Construct the TradeStation authorization URL
    QString authUrl = QString("https://signin.tradestation.com/authorize?"
                              "response_type=code&client_id=%1&redirect_uri=%2&"
                              "audience=https://api.tradestation.com&state=%3&"
                              "scope=openid%20offline_access%20profile%20MarketData%20ReadAccount%20Trade")
                          .arg(clientId, redirectUri, expectedState);

    qCDebug(tsAuth) << "Authorization URL:" << authUrl;
    qCDebug(tsAuth) << "Loading URL in web view...";

    // Load the URL in the web view
    webView->load(QUrl(authUrl));
}

void AuthWindow::handleNewConnection()
{
    qCDebug(tsAuth) << "New connection received on HTTP server";
    QTcpSocket *socket = httpServer->nextPendingConnection();
    
    if (!socket) {
        qCDebug(tsAuth) << "Error: null socket received from nextPendingConnection";
        return;
    }
    
    qCDebug(tsAuth) << "Connection accepted:";
    qCDebug(tsAuth) << "  Peer Address:" << socket->peerAddress().toString();
    qCDebug(tsAuth) << "  Peer Port:" << socket->peerPort();

    connect(socket, &QTcpSocket::readyRead, this, &AuthWindow::handleSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, [socket]() {
        qCDebug(tsAuth) << "Connection closed";
        socket->deleteLater();
    });
    connect(socket, &QTcpSocket::errorOccurred, this, &AuthWindow::handleSocketError);
    connect(socket, &QTcpSocket::stateChanged, this, &AuthWindow::handleSocketStateChanged);
}

void AuthWindow::handleSocketReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        qCDebug(tsAuth) << "Error: Invalid socket in handleSocketReadyRead";
        return;
    }

    QByteArray request = socket->readAll();
    QString requestStr(request);
    qCDebug(tsAuth) << "Received HTTP request:" << requestStr;

    // Parse the HTTP request
    QStringList requestLines = requestStr.split("\r\n");
    if (requestLines.isEmpty()) {
        qCDebug(tsAuth) << "Error: Empty HTTP request";
        socket->write("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid request");
        socket->disconnectFromHost();
        return;
    }

    // Parse the request line (e.g., "GET /callback?code=xyz&state=abc HTTP/1.1")
    QStringList requestParts = requestLines[0].split(" ");
    if (requestParts.size() < 3) {
        qCDebug(tsAuth) << "Error: Invalid HTTP request line";
        socket->write("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid request format");
        socket->disconnectFromHost();
        return;
    }

    QString method = requestParts[0];
    QString path = requestParts[1];
    
    qCDebug(tsAuth) << "HTTP Method:" << method;
    qCDebug(tsAuth) << "Request Path:" << path;

    // Only handle GET requests to /callback
    if (method != "GET" || !path.startsWith("/callback")) {
        qCDebug(tsAuth) << "Error: Invalid method or path";
        socket->write("HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nNot Found");
        socket->disconnectFromHost();
        return;
    }

    // Parse query parameters
    QUrl url("http://localhost" + path);
    QUrlQuery query(url.query());

    QString code = query.queryItemValue("code");
    QString state = query.queryItemValue("state");

    qCDebug(tsAuth) << "Parsed parameters:";
    qCDebug(tsAuth) << "  Path:" << url.path();
    qCDebug(tsAuth) << "  Code:" << code;
    qCDebug(tsAuth) << "  State:" << state;
    qCDebug(tsAuth) << "  Expected State:" << expectedState;

    if (code.isEmpty() || state.isEmpty()) {
        qCDebug(tsAuth) << "Error: Missing code or state parameter";
        socket->write("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nMissing required parameters");
        socket->disconnectFromHost();
        return;
    }

    if (state == expectedState) {
        qCDebug(tsAuth) << "State validation successful";
        handleCodeReceived(code);
        // Send a nice HTML response
        QString htmlResponse = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "\r\n"
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head>\n"
            "    <title>Authentication Successful</title>\n"
            "    <style>\n"
            "        body { font-family: Arial, sans-serif; text-align: center; padding-top: 50px; }\n"
            "        h1 { color: #4CAF50; }\n"
            "    </style>\n"
            "</head>\n"
            "<body>\n"
            "    <h1>Authentication Successful!</h1>\n"
            "    <p>You can now close this window and return to the application.</p>\n"
            "</body>\n"
            "</html>";
        socket->write(htmlResponse.toUtf8());
    } else {
        qCDebug(tsAuth) << "State validation failed - possible security issue";
        socket->write("HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\n\r\nState mismatch - possible security issue");
    }

    socket->disconnectFromHost();
}

void AuthWindow::handleCodeReceived(const QString& code)
{
    qCDebug(tsAuth) << "Authorization code received, initiating token exchange";
    exchangeCodeForTokens(code);
    webView->hide();
}

void AuthWindow::exchangeCodeForTokens(const QString& code)
{
    qCDebug(tsAuth) << "Starting token exchange process...";
    QUrl tokenUrl("https://signin.tradestation.com/oauth/token");
    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery query;
    query.addQueryItem("grant_type", "authorization_code");
    query.addQueryItem("client_id", clientId);
    query.addQueryItem("client_secret", clientSecret);
    query.addQueryItem("code", code);
    query.addQueryItem("redirect_uri", redirectUri);

    QString requestData = query.toString(QUrl::FullyEncoded);
    qCDebug(tsAuth) << "Token exchange request data:" << requestData;

    QNetworkReply *reply = networkManager->post(request, requestData.toUtf8());
    
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            qCDebug(tsAuth) << "Token exchange response:" << responseData;
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            handleTokenResponse(doc.object());
        } else {
            qCDebug(tsAuth) << "Token exchange failed:" << reply->errorString();
            handleTokenError(reply->errorString());
        }
        reply->deleteLater();
    });
}

void AuthWindow::handleTokenResponse(const QJsonObject& response)
{
    if (!parseTokenResponse(response)) {
        handleTokenError("Invalid token response");
        return;
    }
    saveTokens();
    accept(); // Close dialog on success
}

void AuthWindow::handleTokenError(const QString& error)
{
    qCWarning(tsAuth) << "Token error:" << error;
    QMessageBox::warning(this, "Authentication Error", "Authentication failed: " + error);
    reject(); // Close dialog on error
}

bool AuthWindow::parseTokenResponse(const QJsonObject& response)
{
    qCDebug(tsAuth) << "Parsing token response...";
    
    // Check for access_token
    if (!response.contains("access_token")) {
        qCWarning(tsAuth) << "No access_token in response";
        return false;
    }
    accessToken = response["access_token"].toString();
    
    if (accessToken.isEmpty()) {
        qCWarning(tsAuth) << "Empty access_token in response";
        return false;
    }
    
    // Check for refresh_token
    if (!response.contains("refresh_token")) {
        qCWarning(tsAuth) << "No refresh_token in response";
        return false;
    }
    refreshToken = response["refresh_token"].toString();
    if (refreshToken.isEmpty()) {
        qCWarning(tsAuth) << "Empty refresh_token in response";
        return false;
    }
    
    // Check for id_token
    if (!response.contains("id_token")) {
        qCWarning(tsAuth) << "No id_token in response";
        return false;
    }
    idToken = response["id_token"].toString();
    if (idToken.isEmpty()) {
        qCWarning(tsAuth) << "Empty id_token in response";
        return false;
    }
    

    
    // Get the expires_in value from the response
    if (!response.contains("expires_in")) {
        qCWarning(tsAuth) << "No expires_in value in response";
        return false;
    }
    
    tokenTimeoutSeconds = response["expires_in"].toInt();
    if (tokenTimeoutSeconds <= 0) {
        qCWarning(tsAuth) << "Invalid expires_in value:" << tokenTimeoutSeconds;
        return false;
    }
    
        // Set the token received time
    tokenReceivedTime = QDateTime::currentDateTime();
    
    qCDebug(tsAuth) << "Token details:";
    qCDebug(tsAuth) << "  Access Token length:" << accessToken.length();
    qCDebug(tsAuth) << "  Refresh Token length:" << refreshToken.length();
    qCDebug(tsAuth) << "  ID Token length:" << idToken.length();
    qCDebug(tsAuth) << "  Received at:" << tokenReceivedTime.toString(Qt::ISODate);
    qCDebug(tsAuth) << "  Timeout:" << tokenTimeoutSeconds << "seconds";
    
    return true;
}

void AuthWindow::saveTokens()
{
    qCDebug(tsAuth) << "Saving tokens to persistent storage...";
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "TradeStationAuth", "Tokens");
    settings.setValue("access_token", accessToken);
    settings.setValue("refresh_token", refreshToken);
    settings.setValue("id_token", idToken);
    settings.setValue("token_received_time", tokenReceivedTime.toString(Qt::ISODate));
    settings.setValue("token_timeout_seconds", tokenTimeoutSeconds);
    settings.sync();
    qCDebug(tsAuth) << "Tokens saved successfully";
}

bool AuthWindow::loadTokens()
{
    qCDebug(tsAuth) << "Loading tokens from persistent storage...";
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "TradeStationAuth", "Tokens");
    
    // Early return if no access token exists
    accessToken = settings.value("access_token").toString();
    if (accessToken.isEmpty()) {
        qCDebug(tsAuth) << "No existing tokens found";
        return false;
    }
    
    // Load other tokens
    refreshToken = settings.value("refresh_token").toString();
    idToken = settings.value("id_token").toString();
    
    // Early return if any token is missing
    if (refreshToken.isEmpty() || idToken.isEmpty()) {
        qCDebug(tsAuth) << "Tokens are invalid: one or more tokens are empty";
        clearTokens();
        return false;
    }
    
    // Load and validate token received time
    QString timeStr = settings.value("token_received_time").toString();
    if (timeStr.isEmpty()) {
        qCDebug(tsAuth) << "Tokens are invalid: token received time is missing";
        clearTokens();
        return false;
    }
    
    QDateTime tokenReceivedTime = QDateTime::fromString(timeStr, Qt::ISODate);
    if (!tokenReceivedTime.isValid()) {
        qCDebug(tsAuth) << "Tokens are invalid: token received time is invalid";
        clearTokens();
        return false;
    }
    
    // Load timeout value
    int tokenTimeoutSeconds = settings.value("token_timeout_seconds", 3600).toInt();
    
    // Check if tokens are expired
    if (isTokenExpired(tokenReceivedTime, tokenTimeoutSeconds)) {
        qCDebug(tsAuth) << "Tokens are invalid: token has expired";
        clearTokens();
        return false;
    }
    
    qCDebug(tsAuth) << "Existing tokens found and loaded";
    qCDebug(tsAuth) << "  Token received at:" << tokenReceivedTime.toString(Qt::ISODate);
    qCDebug(tsAuth) << "  Token timeout:" << tokenTimeoutSeconds << "seconds";
    qCDebug(tsAuth) << "  Access Token length:" << accessToken.length();
    qCDebug(tsAuth) << "  Refresh Token length:" << refreshToken.length();
    qCDebug(tsAuth) << "  ID Token length:" << idToken.length();

    return true;
}

bool AuthWindow::areTokensValid() const
{
    if (accessToken.isEmpty() || refreshToken.isEmpty() || idToken.isEmpty()) {
        qCDebug(tsAuth) << "Tokens are invalid: one or more tokens are empty";
        return false;
    }
    
    if (!tokenReceivedTime.isValid()) {
        qCDebug(tsAuth) << "Tokens are invalid: token received time is invalid";
        return false;
    }
    
    if (isTokenExpired(tokenReceivedTime, tokenTimeoutSeconds)) {
        qCDebug(tsAuth) << "Tokens are invalid: token has expired";
        return false;
    }
    
    return true;
}

bool AuthWindow::isTokenExpired(const QDateTime& tokenReceivedTime, int tokenTimeoutSeconds)
{
    if (!tokenReceivedTime.isValid()) {
        return true;
    }
    
    QDateTime currentTime = QDateTime::currentDateTime();
    int secondsSinceReceived = tokenReceivedTime.secsTo(currentTime);
    
    // Add 5-second buffer to prevent edge cases
    const int BUFFER_SECONDS = 5;
    bool isExpired = secondsSinceReceived >= (tokenTimeoutSeconds - BUFFER_SECONDS);
    
    qCDebug(tsAuth) << "Token expiration check:";
    qCDebug(tsAuth) << "  Current time:" << currentTime.toString(Qt::ISODate);
    qCDebug(tsAuth) << "  Token received:" << tokenReceivedTime.toString(Qt::ISODate);
    qCDebug(tsAuth) << "  Seconds since received:" << secondsSinceReceived;
    qCDebug(tsAuth) << "  Timeout:" << tokenTimeoutSeconds;
    qCDebug(tsAuth) << "  Buffer:" << BUFFER_SECONDS << "seconds";
    qCDebug(tsAuth) << "  Is expired:" << isExpired;
    
    return isExpired;
}

void AuthWindow::setTokenTimeout(int seconds)
{
    tokenTimeoutSeconds = seconds;
    qCDebug(tsAuth) << "Token timeout set to" << seconds << "seconds";
}

void AuthWindow::clearTokens()
{
    // TODO check if this is needed
    //accessToken.clear();
    //refreshToken.clear();
    //idToken.clear();
    
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "TradeStationAuth", "Tokens");
    settings.remove("access_token");
    settings.remove("refresh_token");
    settings.remove("id_token");
    settings.remove("token_received_time");
    settings.remove("token_timeout_seconds");
    settings.sync();
    
    qCDebug(tsAuth) << "Tokens cleared from memory and persistent storage";
}

QString AuthWindow::generateRandomState()
{
    qCDebug(tsAuth) << "Generating random state for CSRF protection...";
    // Generate a random 32-character string using alphanumeric characters
    const QString chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    QString state;
    state.reserve(32);  // Reserve space for 32 characters
    
    for (int i = 0; i < 32; ++i) {
        int randomIndex = QRandomGenerator::global()->bounded(chars.length());
        state.append(chars[randomIndex]);
    }
    
    qCDebug(tsAuth) << "Generated random state:" << state;
    return state;
}

void AuthWindow::handleSocketError(QAbstractSocket::SocketError socketError)
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    qCDebug(tsAuth) << "Socket error occurred:";
    qCDebug(tsAuth) << "  Error:" << socketError;
    qCDebug(tsAuth) << "  Error String:" << socket->errorString();
    qCDebug(tsAuth) << "  Peer Address:" << socket->peerAddress().toString();
    qCDebug(tsAuth) << "  Peer Port:" << socket->peerPort();
}

void AuthWindow::handleSocketStateChanged(QAbstractSocket::SocketState socketState)
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    qCDebug(tsAuth) << "Socket state changed:";
    qCDebug(tsAuth) << "  New State:" << socketState;
    qCDebug(tsAuth) << "  Peer Address:" << socket->peerAddress().toString();
    qCDebug(tsAuth) << "  Peer Port:" << socket->peerPort();
}

void AuthWindow::initializeCredentialStore()
{
    // Initialize settings with organization and application name
    credentialsStore = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                                     "TradeStationAuth", "Credentials", this);
    credentialsStore->setFallbacksEnabled(false);  // Don't fall back to global settings

    qCDebug(tsAuth) << "Credentials storage location:" << credentialsStore->fileName();
}

bool AuthWindow::loadCredentials()
{
    if (!credentialsStore) {
        initializeCredentialStore();
    }

    // Check if credentials exist
    if (credentialsStore->contains("credentials/client_id") &&
        credentialsStore->contains("credentials/client_secret")) {
        
        clientId = credentialsStore->value("credentials/client_id").toString();
        clientSecret = credentialsStore->value("credentials/client_secret").toString();

        if (validateCredentials(clientId, clientSecret)) {
            qCDebug(tsAuth) << "Successfully loaded credentials from secure storage";
            return true;
        } else {
            qCDebug(tsAuth) << "Loaded credentials failed validation";
            clearCredentials();  // Clear invalid credentials
            return promptForCredentials();
        }
    }

    qCDebug(tsAuth) << "No existing credentials found";
    return promptForCredentials();
}

bool AuthWindow::saveCredentials(const QString& newClientId, const QString& newClientSecret)
{
    if (!credentialsStore) {
        initializeCredentialStore();
    }

    if (!validateCredentials(newClientId, newClientSecret)) {
        qCDebug(tsAuth) << "Invalid credentials format - not saving";
        return false;
    }

    credentialsStore->setValue("credentials/client_id", newClientId);
    credentialsStore->setValue("credentials/client_secret", newClientSecret);
    
    // Force an immediate write to disk and check for errors
    credentialsStore->sync();
    
    if (credentialsStore->status() != QSettings::NoError) {
        qCDebug(tsAuth) << "Error saving credentials:" << credentialsStore->status();
        QMessageBox::critical(this, "Error", 
                            "Failed to save credentials securely. Please check file permissions.");
        return false;
    }

    qCDebug(tsAuth) << "Credentials saved successfully to:" << credentialsStore->fileName();
    return true;
}

bool AuthWindow::promptForCredentials()
{
    bool ok;
    QInputDialog dialog(this);
    dialog.setWindowTitle("TradeStation API Setup");
    dialog.setLabelText("Enter your Client ID:");
    dialog.setTextEchoMode(QLineEdit::Normal);
    dialog.setMinimumWidth(400);  // Set minimum width to 400 pixels
    ok = dialog.exec();
    QString newClientId = dialog.textValue();

    if (!ok || newClientId.isEmpty()) {
        qCDebug(tsAuth) << "User cancelled Client ID input";
        return false;
    }

    dialog.setLabelText("Enter your Client Secret:");
    dialog.setTextEchoMode(QLineEdit::Password);
    dialog.setTextValue("");  // Clear previous input
    ok = dialog.exec();
    QString newClientSecret = dialog.textValue();

    if (!ok || newClientSecret.isEmpty()) {
        qCDebug(tsAuth) << "User cancelled Client Secret input";
        return false;
    }

    if (validateCredentials(newClientId, newClientSecret)) {
        // Ask user if they want to save credentials
        QMessageBox::StandardButton saveChoice = QMessageBox::question(
            this,
            "Save Credentials",
            "Would you like to save these credentials for future use?\n\n"
            "If you choose not to save them, you'll need to enter them each time you start the application.",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No  // Default to No for security
        );

        if (saveChoice == QMessageBox::Yes) {
            if (saveCredentials(newClientId, newClientSecret)) {
                clientId = newClientId;
                clientSecret = newClientSecret;
                qCDebug(tsAuth) << "Credentials saved successfully";
                return true;
            }
        } else {
            qCDebug(tsAuth) << "User chose not to save credentials";
            clearCredentials();  // Clear any existing credentials
            clientId = newClientId;
            clientSecret = newClientSecret;
            return true;
        }
    } else {
        QMessageBox::warning(this, "Invalid Credentials",
                           "The provided credentials appear to be invalid. "
                           "Please check your TradeStation API credentials and try again.");
    }

    return false;
}

void AuthWindow::clearCredentials()
{
    if (!credentialsStore) {
        initializeCredentialStore();
    }

    credentialsStore->remove("credentials/client_id");
    credentialsStore->remove("credentials/client_secret");
    credentialsStore->sync();

    clientId.clear();
    clientSecret.clear();

    qCDebug(tsAuth) << "Credentials cleared from secure storage";
}

bool AuthWindow::validateCredentials(const QString& clientId, const QString& clientSecret)
{
    qCDebug(tsAuth) << "Validating credentials:";
    qCDebug(tsAuth) << "  Client ID length:" << clientId.length();
    qCDebug(tsAuth) << "  Client Secret length:" << clientSecret.length();

    // Basic validation of credential format
    // TradeStation client IDs are typically 32 characters
    // and client secrets are typically longer
    if (clientId.length() < 20) {
        qCDebug(tsAuth) << "Validation failed: Client ID too short (minimum 20 characters)";
        return false;
    }
    
    if (clientSecret.length() < 20) {
        qCDebug(tsAuth) << "Validation failed: Client Secret too short (minimum 20 characters)";
        return false;
    }

    // Check for valid characters (alphanumeric only for client ID)
    QRegularExpression validClientId("^[a-zA-Z0-9]+$");
    
    if (!validClientId.match(clientId).hasMatch()) {
        qCDebug(tsAuth) << "Validation failed: Client ID contains invalid characters (must be alphanumeric)";
        return false;
    }
    
    // Check for valid characters (alphanumeric, hyphen, and underscore for client secret)
    QRegularExpression validClientSecret("^[a-zA-Z0-9_-]+$");
    
    if (!validClientSecret.match(clientSecret).hasMatch()) {
        qCDebug(tsAuth) << "Validation failed: Client Secret contains invalid characters (must be alphanumeric, hyphen, or underscore)";
        return false;
    }

    qCDebug(tsAuth) << "Credential validation successful";
    return true;
}
