#include "PlatformControlServer.h"

#include "MainApp.h"
#include "PlatformControlProtocol.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QLocalSocket>

namespace
{
    [[nodiscard]] QJsonObject makeProtocolErrorResponse(const QString& p_error)
    {
        QJsonObject response;
        response["protocolVersion"] = PlatformControlProtocol::kProtocolVersion;
        response["ok"] = false;
        response["error"] = p_error;
        response["message"] = "Platform control request rejected.";
        return response;
    }
} // namespace

PlatformControlServer::PlatformControlServer(MainApp* p_mainApp, QObject* p_parent)
    : QObject(p_parent), m_mainApp(p_mainApp)
{
    Q_CHECK_PTR(m_mainApp);

    QObject::connect(&m_server, &QLocalServer::newConnection, this, [this]() { handleNewConnection(); });
}

PlatformControlServer::~PlatformControlServer()
{
    stopListening();
}

bool PlatformControlServer::startListening()
{
    const QString socketPath = PlatformControlProtocol::socketPath();
    const QFileInfo socketInfo(socketPath);
    QDir().mkpath(socketInfo.absolutePath());

    stopListening();

    if (!QLocalServer::removeServer(socketPath))
    {
        qInfo() << "No stale platform control socket to remove at" << socketPath;
    }

    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(socketPath))
    {
        qCritical() << "Failed to start platform control socket at" << socketPath << ":" << m_server.errorString();
        return false;
    }

    qInfo() << "Platform control socket listening at" << socketPath;
    return true;
}

void PlatformControlServer::stopListening()
{
    for (auto it = m_socketBuffers.begin(); it != m_socketBuffers.end(); ++it)
    {
        if (it.key() != nullptr)
        {
            it.key()->disconnectFromServer();
            it.key()->deleteLater();
        }
    }
    m_socketBuffers.clear();

    if (m_server.isListening())
    {
        const QString socketPath = m_server.fullServerName();
        m_server.close();
        QLocalServer::removeServer(socketPath);
        qInfo() << "Platform control socket closed";
    }
}

void PlatformControlServer::handleNewConnection()
{
    while (m_server.hasPendingConnections())
    {
        QLocalSocket* socket = m_server.nextPendingConnection();
        Q_CHECK_PTR(socket);
        m_socketBuffers.insert(socket, {});

        QObject::connect(socket, &QLocalSocket::readyRead, this, [this, socket]() { handleSocketReadyRead(socket); });
        QObject::connect(socket,
                         &QLocalSocket::disconnected,
                         this,
                         [this, socket]() { handleSocketDisconnected(socket); });
        QObject::connect(socket,
                         &QLocalSocket::errorOccurred,
                         this,
                         [socket](const QLocalSocket::LocalSocketError p_error) {
                             qWarning() << "Platform control client socket error" << p_error << ":"
                                        << socket->errorString();
                         });
    }
}

void PlatformControlServer::handleSocketReadyRead(QLocalSocket* p_socket)
{
    if (p_socket == nullptr || !m_socketBuffers.contains(p_socket))
    {
        return;
    }

    QByteArray& buffer = m_socketBuffers[p_socket];
    buffer.append(p_socket->readAll());
    if (buffer.size() > PlatformControlProtocol::kMaxMessageBytes)
    {
        sendResponse(p_socket, makeProtocolErrorResponse("Platform control request exceeded maximum size"));
        p_socket->disconnectFromServer();
        return;
    }

    const qsizetype newlineIndex = buffer.indexOf('\n');
    if (newlineIndex < 0)
    {
        return;
    }

    const QByteArray requestLine = buffer.left(newlineIndex).trimmed();
    buffer.remove(0, newlineIndex + 1);

    if (requestLine.isEmpty())
    {
        sendResponse(p_socket, makeProtocolErrorResponse("Platform control request was empty"));
        p_socket->disconnectFromServer();
        return;
    }

    const auto parsedRequest = PlatformControlProtocol::parseMessage(requestLine);
    if (!parsedRequest.has_value())
    {
        sendResponse(p_socket, makeProtocolErrorResponse(parsedRequest.error()));
        p_socket->disconnectFromServer();
        return;
    }

    sendResponse(p_socket, m_mainApp->handleControlRequest(parsedRequest.value()));
    p_socket->disconnectFromServer();
}

void PlatformControlServer::handleSocketDisconnected(QLocalSocket* p_socket)
{
    m_socketBuffers.remove(p_socket);
    if (p_socket != nullptr)
    {
        p_socket->deleteLater();
    }
}

void PlatformControlServer::sendResponse(QLocalSocket* p_socket, const QJsonObject& p_response)
{
    if (p_socket == nullptr)
    {
        return;
    }

    p_socket->write(PlatformControlProtocol::serializeMessage(p_response));
    p_socket->flush();
}
