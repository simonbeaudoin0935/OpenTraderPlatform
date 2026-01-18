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
#include <QLabel>
#include <QDesktopServices>

#include "AuthWindow.h"

Q_LOGGING_CATEGORY(TSAuthWindowLog, "TSClient.authwindow")

AuthWindow::AuthWindow(QWidget* parent) : QDialog(parent)
{
    qCDebug(TSAuthWindowLog) << "Initializing TradeStation Auth Window";

    // Set dialog properties
    setWindowTitle("TradeStation Authentication");
    setModal(true);
    setMinimumSize(500, 200);
    setAttribute(Qt::WA_DeleteOnClose); // Ensure dialog is deleted when closed

    // Connect dialog finished signal first
    connect(this, &QDialog::finished, this, &AuthWindow::handleDialogFinished);


    // Generate random state for CSRF protection
    expectedState = generateRandomState();
    qCDebug(TSAuthWindowLog) << "Generated expected state:" << expectedState;

    // Load credentials first
    clientToken = ClientToken::loadFromSettings();

    if (!clientToken.isValid())
    {

        bool success = promptForCredentials();

        if (!success)
        {
            qCDebug(TSAuthWindowLog) << "Failed to load or obtain valid credentials";
            QMessageBox::critical(this,
                                  "Error",
                                  "Unable to obtain valid TradeStation API credentials. "
                                  "The authentication will be cancelled.");
            QTimer::singleShot(0, this, &QDialog::reject);
            return;
        }
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
    if (result == QDialog::Accepted)
    {
        emit authFinished(true, authToken, "Authentication successful");
    }
    else
    {
        emit authFinished(false, authToken, "Authentication cancelled or failed");
    }
}

void AuthWindow::setupUi()
{
    auto* layout = new QVBoxLayout(this);

    // Create status label with instructions
    auto* statusLabel = new QLabel(this);
    statusLabel->setWordWrap(true);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setText("<h2>TradeStation Authentication</h2>"
                         "<p>Your default web browser should open automatically with the TradeStation login page.</p>"
                         "<p><b>Please complete the authentication in your browser.</b></p>"
                         "<p>This window will close automatically once authentication is complete.</p>"
                         "<p><i>Note: Keep this window open while you complete the authentication.</i></p>");
    statusLabel->setMargin(20);
    layout->addWidget(statusLabel);

    // Add dialog buttons
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    setLayout(layout);
}

void AuthWindow::startHttpServer()
{
    qCDebug(TSAuthWindowLog) << "Starting HTTP server...";

    // Create server if not exists
    if (!httpServer)
    {
        httpServer = new QTcpServer(this);
    }

    // Check if server is already listening
    if (httpServer->isListening())
    {
        qCDebug(TSAuthWindowLog) << "HTTP server is already running on port" << httpServer->serverPort();
        return;
    }

    // Try to bind to default port first
    currentPort = DEFAULT_PORT;
    bool serverStarted = false;

    for (quint16 portAttempt = 0; portAttempt < MAX_PORT_ATTEMPTS && !serverStarted; ++portAttempt)
    {
        quint16 portToTry = currentPort + portAttempt;
        qCDebug(TSAuthWindowLog) << "Attempting to start server on port" << portToTry;

        if (tryBindPort(portToTry))
        {
            currentPort = portToTry;
            serverStarted = true;
            updateRedirectUri(currentPort);
            qCDebug(TSAuthWindowLog) << "Successfully bound to port" << currentPort;
            break;
        }

        qCDebug(TSAuthWindowLog) << "Failed to bind to port" << portToTry << ", trying next port";
    }

    if (!serverStarted)
    {
        QString errorMsg = QString("Failed to find available port after %1 attempts").arg(MAX_PORT_ATTEMPTS);
        qCDebug(TSAuthWindowLog) << errorMsg;
        QMessageBox::warning(this, "Server Error", errorMsg);
        return;
    }

    connect(httpServer, &QTcpServer::newConnection, this, &AuthWindow::handleNewConnection);
    qCDebug(TSAuthWindowLog) << "HTTP server successfully started on" << httpServer->serverAddress().toString()
                             << "port" << httpServer->serverPort();
}

bool AuthWindow::tryBindPort(quint16 port)
{
    if (httpServer->listen(QHostAddress::LocalHost, port))
    {
        return true;
    }

    QString errorMsg;
    switch (httpServer->serverError())
    {
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
        errorMsg = QString("Failed to start HTTP server on port %1: %2").arg(port).arg(httpServer->errorString());
    }

    qCDebug(TSAuthWindowLog) << "Port" << port << "binding failed:" << errorMsg;
    return false;
}

void AuthWindow::updateRedirectUri(quint16 port)
{
    // Update the redirect URI with the new port
    redirectUri = QString("http://localhost:%1/callback").arg(port);
    qCDebug(TSAuthWindowLog) << "Updated redirect URI:" << redirectUri;
}

void AuthWindow::startAuthorization()
{
    qCDebug(TSAuthWindowLog) << "Starting authorization process...";
    // Construct the TradeStation authorization URL
    QString authUrl = QString("https://signin.tradestation.com/authorize?"
                              "response_type=code&client_id=%1&redirect_uri=%2&"
                              "audience=https://api.tradestation.com&state=%3&"
                              "scope=openid%20offline_access%20profile%20MarketData%20Matrix%20ReadAccount%20Trade")
                          .arg(clientToken.getClientId(), redirectUri, expectedState);

    qCDebug(TSAuthWindowLog) << "Authorization URL:" << authUrl;
    qCDebug(TSAuthWindowLog) << "Opening URL in system browser...";

    // Open the URL in the system's default web browser
    if (!QDesktopServices::openUrl(QUrl(authUrl)))
    {
        qCWarning(TSAuthWindowLog) << "Failed to open system browser";
        QMessageBox::warning(this,
                             "Browser Error",
                             "Failed to open your default web browser. Please copy the URL manually:\n\n" + authUrl);
    }
    else
    {
        qCDebug(TSAuthWindowLog) << "Successfully opened URL in system browser";
    }
}

void AuthWindow::handleNewConnection()
{
    qCDebug(TSAuthWindowLog) << "New connection received on HTTP server";
    QTcpSocket* socket = httpServer->nextPendingConnection();

    if (!socket)
    {
        qCDebug(TSAuthWindowLog) << "Error: null socket received from nextPendingConnection";
        return;
    }

    qCDebug(TSAuthWindowLog) << "Connection accepted:";
    qCDebug(TSAuthWindowLog) << "  Peer Address:" << socket->peerAddress().toString();
    qCDebug(TSAuthWindowLog) << "  Peer Port:" << socket->peerPort();

    connect(socket, &QTcpSocket::readyRead, this, &AuthWindow::handleSocketReadyRead);
    connect(socket,
            &QTcpSocket::disconnected,
            this,
            [socket]()
            {
                qCDebug(TSAuthWindowLog) << "Connection closed";
                socket->deleteLater();
            });
    connect(socket, &QTcpSocket::errorOccurred, this, &AuthWindow::handleSocketError);
    connect(socket, &QTcpSocket::stateChanged, this, &AuthWindow::handleSocketStateChanged);
}

void AuthWindow::handleSocketReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
    {
        qCDebug(TSAuthWindowLog) << "Error: Invalid socket in handleSocketReadyRead";
        return;
    }

    QByteArray request = socket->readAll();
    QString requestStr(request);
    qCDebug(TSAuthWindowLog) << "Received HTTP request:" << requestStr;

    // Parse the HTTP request
    QStringList requestLines = requestStr.split("\r\n");
    if (requestLines.isEmpty())
    {
        qCDebug(TSAuthWindowLog) << "Error: Empty HTTP request";
        socket->write("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid request");
        socket->disconnectFromHost();
        return;
    }

    // Parse the request line (e.g., "GET /callback?code=xyz&state=abc HTTP/1.1")
    QStringList requestParts = requestLines[0].split(" ");
    if (requestParts.size() < 3)
    {
        qCDebug(TSAuthWindowLog) << "Error: Invalid HTTP request line";
        socket->write("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid request format");
        socket->disconnectFromHost();
        return;
    }

    QString method = requestParts[0];
    QString path = requestParts[1];

    qCDebug(TSAuthWindowLog) << "HTTP Method:" << method;
    qCDebug(TSAuthWindowLog) << "Request Path:" << path;

    // Only handle GET requests to /callback
    if (method != "GET" || !path.startsWith("/callback"))
    {
        qCDebug(TSAuthWindowLog) << "Error: Invalid method or path";
        socket->write("HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nNot Found");
        socket->disconnectFromHost();
        return;
    }

    // Parse query parameters
    QUrl url("http://localhost" + path); // TODO put this in a variable in the header file
    QUrlQuery query(url.query());

    QString code = query.queryItemValue("code");
    QString state = query.queryItemValue("state");

    qCDebug(TSAuthWindowLog) << "Parsed parameters:";
    qCDebug(TSAuthWindowLog) << "  Path:" << url.path();
    qCDebug(TSAuthWindowLog) << "  Code: [REDACTED]";
    qCDebug(TSAuthWindowLog) << "  State:" << state;
    qCDebug(TSAuthWindowLog) << "  Expected State:" << expectedState;

    if (code.isEmpty() || state.isEmpty())
    {
        qCDebug(TSAuthWindowLog) << "Error: Missing code or state parameter";
        socket->write("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nMissing required parameters");
        socket->disconnectFromHost();
        return;
    }

    if (state == expectedState)
    {
        qCDebug(TSAuthWindowLog) << "State validation successful";
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
    }
    else
    {
        qCDebug(TSAuthWindowLog) << "State validation failed - possible security issue";
        socket->write(
            "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\n\r\nState mismatch - possible security issue");
    }

    socket->disconnectFromHost();
}

void AuthWindow::handleCodeReceived(const QString& code)
{
    qCDebug(TSAuthWindowLog) << "Authorization code received, initiating token exchange";
    exchangeCodeForTokens(code);
}

void AuthWindow::exchangeCodeForTokens(const QString& code)
{
    qCDebug(TSAuthWindowLog) << "Exchanging authorization code for tokens...";

    // Construct the token URL
    QUrl tokenUrl("https://signin.tradestation.com/oauth/token");

    // Create the request
    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    // Create the form data
    QUrlQuery query;
    query.addQueryItem("grant_type", "authorization_code");
    query.addQueryItem("client_id", clientToken.getClientId());
    query.addQueryItem("client_secret", clientToken.getClientSecret());
    query.addQueryItem("code", code);
    query.addQueryItem("redirect_uri", redirectUri);

    QString requestData = query.toString(QUrl::FullyEncoded);
    qCDebug(TSAuthWindowLog) << "Initiating token exchange request";

    QNetworkReply* reply = networkManager->post(request, requestData.toUtf8());

    connect(reply,
            &QNetworkReply::finished,
            [this, reply]()
            {
                if (reply->error() == QNetworkReply::NoError)
                {
                    QByteArray responseData = reply->readAll();
                    qCDebug(TSAuthWindowLog) << "Token exchange successful";
                    QJsonDocument doc = QJsonDocument::fromJson(responseData);
                    handleTokenResponse(doc.object());
                }
                else
                {
                    qCDebug(TSAuthWindowLog) << "Token exchange failed:" << reply->errorString();
                    handleTokenError(reply->errorString());
                }
                reply->deleteLater();
            });
}

void AuthWindow::handleTokenResponse(const QJsonObject& response)
{
    if (!parseTokenResponse(response))
    {
        handleTokenError("Invalid token response");
        return;
    }
    accept(); // Close dialog on success
}

void AuthWindow::handleTokenError(const QString& error)
{
    qCWarning(TSAuthWindowLog) << "Token error:" << error;
    QMessageBox::warning(this, "Authentication Error", "Authentication failed: " + error);
    reject(); // Close dialog on error
}

bool AuthWindow::parseTokenResponse(const QJsonObject& response)
{
    qCDebug(TSAuthWindowLog) << "Parsing token response...";

    // Create new AuthToken from response
    authToken = AuthToken::receiveAuthToken(response);

    if (!authToken.isValid())
    {
        qCWarning(TSAuthWindowLog) << "Invalid token response";
        return false;
    }

    qCDebug(TSAuthWindowLog) << "Token received and validated successfully";

    return true;
}


QString AuthWindow::generateRandomState()
{
    qCDebug(TSAuthWindowLog) << "Generating random state for CSRF protection...";
    // Generate a random 32-character string using alphanumeric characters
    const QString chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    QString state;
    state.reserve(32); // Reserve space for 32 characters

    for (int i = 0; i < 32; ++i)
    {
        int randomIndex = QRandomGenerator::global()->bounded(chars.length());
        state.append(chars[randomIndex]);
    }

    qCDebug(TSAuthWindowLog) << "Generated random state:" << state;
    return state;
}

void AuthWindow::handleSocketError(QAbstractSocket::SocketError socketError)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    qCDebug(TSAuthWindowLog) << "Socket error occurred:";
    qCDebug(TSAuthWindowLog) << "  Error:" << socketError;
    qCDebug(TSAuthWindowLog) << "  Error String:" << socket->errorString();
    qCDebug(TSAuthWindowLog) << "  Peer Address:" << socket->peerAddress().toString();
    qCDebug(TSAuthWindowLog) << "  Peer Port:" << socket->peerPort();
}

void AuthWindow::handleSocketStateChanged(QAbstractSocket::SocketState socketState)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    qCDebug(TSAuthWindowLog) << "Socket state changed:";
    qCDebug(TSAuthWindowLog) << "  New State:" << socketState;
    qCDebug(TSAuthWindowLog) << "  Peer Address:" << socket->peerAddress().toString();
    qCDebug(TSAuthWindowLog) << "  Peer Port:" << socket->peerPort();
}

bool AuthWindow::promptForCredentials()
{
    bool ok;
    QInputDialog dialog(this);
    dialog.setWindowTitle("TradeStation API Setup");
    dialog.setLabelText("Enter your Client ID:");
    dialog.setTextEchoMode(QLineEdit::Normal);
    dialog.setMinimumWidth(400); // Set minimum width to 400 pixels
    ok = dialog.exec();
    QString newClientId = dialog.textValue();

    if (!ok || newClientId.isEmpty())
    {
        qCDebug(TSAuthWindowLog) << "User cancelled Client ID input";
        return false;
    }

    dialog.setLabelText("Enter your Client Secret:");
    dialog.setTextEchoMode(QLineEdit::Password);
    dialog.setTextValue(""); // Clear previous input
    ok = dialog.exec();
    QString newClientSecret = dialog.textValue();

    if (!ok || newClientSecret.isEmpty())
    {
        qCDebug(TSAuthWindowLog) << "User cancelled Client Secret input";
        return false;
    }

    // Create a new ClientToken with the provided credentials
    clientToken = ClientToken(newClientId, newClientSecret);

    if (clientToken.isValid())
    {
        // Ask user if they want to save credentials
        QMessageBox::StandardButton saveChoice = QMessageBox::question(
            this,
            "Save Credentials",
            "Would you like to save these credentials for future use?\n\n"
            "If you choose not to save them, you'll need to enter them each time you start the application.",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No // Default to No for security
        );

        if (saveChoice == QMessageBox::Yes)
        {
            if (ClientToken::storeToSettings(clientToken))
            {
                qCDebug(TSAuthWindowLog) << "Credentials saved successfully";
                return true;
            }
        }
        else
        {
            qCDebug(TSAuthWindowLog) << "User chose not to save credentials";
            ClientToken::clearSettings(); // Clear any existing credentials
            return true;
        }
    }
    else
    {
        QMessageBox::warning(this,
                             "Invalid Credentials",
                             "The provided credentials appear to be invalid. "
                             "Please check your TradeStation API credentials and try again.");
    }

    return false;
}
