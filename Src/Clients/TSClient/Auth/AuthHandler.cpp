#include "AuthHandler.h"

#include <QUrlQuery>
#include <QDebug>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QRandomGenerator>

Q_LOGGING_CATEGORY(TSAuthHandlerLog, "TSClient.authhandler")

AuthHandler::AuthHandler(QObject *parent) : QObject(parent)
{
    qCDebug(TSAuthHandlerLog) << "Initializing TradeStation Auth Handler";
    
    // Generate random state for CSRF protection
    m_expectedState = generateRandomState();
    qCDebug(TSAuthHandlerLog) << "Generated expected state:" << m_expectedState;
    
    // Initialize network manager
    m_networkManager = new QNetworkAccessManager(this);
}

AuthHandler::~AuthHandler() = default;

void AuthHandler::startAuthentication()
{
    qCDebug(TSAuthHandlerLog) << "Starting authentication process...";
    
    // Load credentials from storage
    m_clientToken = ClientToken::loadFromSettings();

    if (!m_clientToken.isValid()) {
        bool success = promptForCredentials();

        if (!success) {
            qCDebug(TSAuthHandlerLog) << "Failed to load or obtain valid credentials";
            emit authFinished(false, AuthToken(), "Unable to obtain valid TradeStation API credentials");
            return;
        }
    }

    // Start HTTP server and display authorization URL
    startHttpServer();
    
    // Construct the TradeStation authorization URL
    QString authUrl = QString("https://signin.tradestation.com/authorize?"
                              "response_type=code&client_id=%1&redirect_uri=%2&"
                              "audience=https://api.tradestation.com&state=%3&"
                              "scope=openid%20offline_access%20profile%20MarketData%20Matrix%20ReadAccount%20Trade")
                          .arg(m_clientToken.getClientId(), m_redirectUri, m_expectedState);

    qCDebug(TSAuthHandlerLog) << "Authorization URL:" << authUrl;
    
    // Let subclass display the URL to user (opens browser or prints to console)
    displayAuthorizationUrl(authUrl);
}

void AuthHandler::startHttpServer()
{
    qCDebug(TSAuthHandlerLog) << "Starting HTTP server...";
    
    // Create server if not exists
    if (!m_httpServer) {
        m_httpServer = new QTcpServer(this);
    }

    // Check if server is already listening
    if (m_httpServer->isListening()) {
        qCDebug(TSAuthHandlerLog) << "HTTP server is already running on port" << m_httpServer->serverPort();
        return;
    }

    // Try to bind to default port first
    m_currentPort = DEFAULT_PORT;
    bool serverStarted = false;

    for (quint16 portAttempt = 0; portAttempt < MAX_PORT_ATTEMPTS && !serverStarted; ++portAttempt) {
        quint16 portToTry = m_currentPort + portAttempt;
        qCDebug(TSAuthHandlerLog) << "Attempting to start server on port" << portToTry;
        
        if (tryBindPort(portToTry)) {
            m_currentPort = portToTry;
            serverStarted = true;
            updateRedirectUri(m_currentPort);
            qCDebug(TSAuthHandlerLog) << "Successfully bound to port" << m_currentPort;
            break;
        }
        
        qCDebug(TSAuthHandlerLog) << "Failed to bind to port" << portToTry << ", trying next port";
    }

    if (!serverStarted) {
        QString errorMsg = QString("Failed to find available port after %1 attempts").arg(MAX_PORT_ATTEMPTS);
        qCDebug(TSAuthHandlerLog) << errorMsg;
        showError("Server Error", errorMsg);
        emit authFinished(false, AuthToken(), errorMsg);
        return;
    }

    connect(m_httpServer, &QTcpServer::newConnection, this, &AuthHandler::handleNewConnection);
    qCDebug(TSAuthHandlerLog) << "HTTP server successfully started on" 
                    << m_httpServer->serverAddress().toString() 
                    << "port" << m_httpServer->serverPort();
}

bool AuthHandler::tryBindPort(quint16 port)
{
    if (m_httpServer->listen(QHostAddress::LocalHost, port)) {
        return true;
    }

    QString errorMsg;
    switch (m_httpServer->serverError()) {
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
                        .arg(m_httpServer->errorString());
    }
    
    qCDebug(TSAuthHandlerLog) << "Port" << port << "binding failed:" << errorMsg;
    return false;
}

void AuthHandler::updateRedirectUri(quint16 port)
{
    m_redirectUri = QString("http://localhost:%1/callback").arg(port);
    qCDebug(TSAuthHandlerLog) << "Updated redirect URI:" << m_redirectUri;
}

QString AuthHandler::generateRandomState()
{
    qCDebug(TSAuthHandlerLog) << "Generating random state for CSRF protection...";
    const QString chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    QString state;
    state.reserve(32);
    
    for (int i = 0; i < 32; ++i) {
        int randomIndex = QRandomGenerator::global()->bounded(chars.length());
        state.append(chars[randomIndex]);
    }
    
    qCDebug(TSAuthHandlerLog) << "Generated random state:" << state;
    return state;
}

void AuthHandler::handleNewConnection()
{
    qCDebug(TSAuthHandlerLog) << "New connection received on HTTP server";
    QTcpSocket *socket = m_httpServer->nextPendingConnection();
    
    if (!socket) {
        qCDebug(TSAuthHandlerLog) << "Error: null socket received from nextPendingConnection";
        return;
    }
    
    qCDebug(TSAuthHandlerLog) << "Connection accepted:";
    qCDebug(TSAuthHandlerLog) << "  Peer Address:" << socket->peerAddress().toString();
    qCDebug(TSAuthHandlerLog) << "  Peer Port:" << socket->peerPort();

    connect(socket, &QTcpSocket::readyRead, this, &AuthHandler::handleSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, [socket]() {
        qCDebug(TSAuthHandlerLog) << "Connection closed";
        socket->deleteLater();
    });
    connect(socket, &QTcpSocket::errorOccurred, this, &AuthHandler::handleSocketError);
    connect(socket, &QTcpSocket::stateChanged, this, &AuthHandler::handleSocketStateChanged);
}

void AuthHandler::handleSocketReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        qCDebug(TSAuthHandlerLog) << "Error: Invalid socket in handleSocketReadyRead";
        return;
    }

    QByteArray request = socket->readAll();
    QString requestStr(request);
    qCDebug(TSAuthHandlerLog) << "Received HTTP request:" << requestStr;

    // Parse the HTTP request
    QStringList requestLines = requestStr.split("\r\n");
    if (requestLines.isEmpty()) {
        qCDebug(TSAuthHandlerLog) << "Error: Empty HTTP request";
        socket->write("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid request");
        socket->disconnectFromHost();
        return;
    }

    // Parse the request line
    QStringList requestParts = requestLines[0].split(" ");
    if (requestParts.size() < 3) {
        qCDebug(TSAuthHandlerLog) << "Error: Invalid HTTP request line";
        socket->write("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid request format");
        socket->disconnectFromHost();
        return;
    }

    QString method = requestParts[0];
    QString path = requestParts[1];
    
    qCDebug(TSAuthHandlerLog) << "HTTP Method:" << method;
    qCDebug(TSAuthHandlerLog) << "Request Path:" << path;

    // Only handle GET requests to /callback
    if (method != "GET" || !path.startsWith("/callback")) {
        qCDebug(TSAuthHandlerLog) << "Error: Invalid method or path";
        socket->write("HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nNot Found");
        socket->disconnectFromHost();
        return;
    }

    // Parse query parameters
    const QString baseUrl = "http://localhost";
    QUrl url(baseUrl + path);
    QUrlQuery query(url.query());

    QString code = query.queryItemValue("code");
    QString state = query.queryItemValue("state");

    qCDebug(TSAuthHandlerLog) << "Parsed parameters:";
    qCDebug(TSAuthHandlerLog) << "  Path:" << url.path();
    qCDebug(TSAuthHandlerLog) << "  Code: [REDACTED]";
    qCDebug(TSAuthHandlerLog) << "  State:" << state;
    qCDebug(TSAuthHandlerLog) << "  Expected State:" << m_expectedState;

    if (code.isEmpty() || state.isEmpty()) {
        qCDebug(TSAuthHandlerLog) << "Error: Missing code or state parameter";
        socket->write("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nMissing required parameters");
        socket->disconnectFromHost();
        return;
    }

    if (state == m_expectedState) {
        qCDebug(TSAuthHandlerLog) << "State validation successful";
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
        qCDebug(TSAuthHandlerLog) << "State validation failed - possible security issue";
        socket->write("HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\n\r\nState mismatch - possible security issue");
    }

    socket->disconnectFromHost();
}

void AuthHandler::handleCodeReceived(const QString& code)
{
    qCDebug(TSAuthHandlerLog) << "Authorization code received, initiating token exchange";
    exchangeCodeForTokens(code);
}

void AuthHandler::exchangeCodeForTokens(const QString& code)
{
    qCDebug(TSAuthHandlerLog) << "Exchanging authorization code for tokens...";
    
    // Construct the token URL
    QUrl tokenUrl("https://signin.tradestation.com/oauth/token");
    
    // Create the request
    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    
    // Create the form data
    QUrlQuery query;
    query.addQueryItem("grant_type", "authorization_code");
    query.addQueryItem("client_id", m_clientToken.getClientId());
    query.addQueryItem("client_secret", m_clientToken.getClientSecret());
    query.addQueryItem("code", code);
    query.addQueryItem("redirect_uri", m_redirectUri);

    QString requestData = query.toString(QUrl::FullyEncoded);
    qCDebug(TSAuthHandlerLog) << "Initiating token exchange request";

    QNetworkReply *reply = m_networkManager->post(request, requestData.toUtf8());
    
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            qCDebug(TSAuthHandlerLog) << "Token exchange successful";
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            handleTokenResponse(doc.object());
        } else {
            qCDebug(TSAuthHandlerLog) << "Token exchange failed:" << reply->errorString();
            handleTokenError(reply->errorString());
        }
        reply->deleteLater();
    });
}

void AuthHandler::handleTokenResponse(const QJsonObject& response)
{
    if (!parseTokenResponse(response)) {
        handleTokenError("Invalid token response");
        return;
    }
    
    qCDebug(TSAuthHandlerLog) << "Authentication completed successfully";
    emit authFinished(true, m_authToken, "Authentication successful");
}

void AuthHandler::handleTokenError(const QString& error)
{
    qCWarning(TSAuthHandlerLog) << "Token error:" << error;
    showError("Authentication Error", "Authentication failed: " + error);
    emit authFinished(false, AuthToken(), "Authentication failed: " + error);
}

bool AuthHandler::parseTokenResponse(const QJsonObject& response)
{
    qCDebug(TSAuthHandlerLog) << "Parsing token response...";
    
    // Create new AuthToken from response
    m_authToken = AuthToken::receiveAuthToken(response);
    
    if (!m_authToken.isValid()) {
        qCWarning(TSAuthHandlerLog) << "Invalid token response";
        return false;
    }
    
    qCDebug(TSAuthHandlerLog) << "Token received and validated successfully";

    return true;
}

void AuthHandler::handleSocketError(QAbstractSocket::SocketError socketError)
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    qCDebug(TSAuthHandlerLog) << "Socket error occurred:";
    qCDebug(TSAuthHandlerLog) << "  Error:" << socketError;
    qCDebug(TSAuthHandlerLog) << "  Error String:" << socket->errorString();
    qCDebug(TSAuthHandlerLog) << "  Peer Address:" << socket->peerAddress().toString();
    qCDebug(TSAuthHandlerLog) << "  Peer Port:" << socket->peerPort();
}

void AuthHandler::handleSocketStateChanged(QAbstractSocket::SocketState socketState)
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    qCDebug(TSAuthHandlerLog) << "Socket state changed:";
    qCDebug(TSAuthHandlerLog) << "  New State:" << socketState;
    qCDebug(TSAuthHandlerLog) << "  Peer Address:" << socket->peerAddress().toString();
    qCDebug(TSAuthHandlerLog) << "  Peer Port:" << socket->peerPort();
}
