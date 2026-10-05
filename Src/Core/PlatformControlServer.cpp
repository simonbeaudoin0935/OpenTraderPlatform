#include "PlatformControlServer.h"

#include "MainApp.h"
#include "PlatformControlProtocol.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QLocalSocket>
#include <QPointer>

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
        const QPointer<QLocalSocket> guardedSocket(socket);

        QObject::connect(socket,
                         &QLocalSocket::readyRead,
                         this,
                         [this, guardedSocket]()
                         {
                            if (guardedSocket.isNull())
                            {
                                return;
                            }

                            handleSocketReadyRead(guardedSocket.get());
                         });
        QObject::connect(socket,
                         &QLocalSocket::disconnected,
                         this,
                         [this, guardedSocket]()
                         {
                            if (guardedSocket.isNull())
                            {
                                return;
                            }

                            handleSocketDisconnected(guardedSocket.get());
                         });
        QObject::connect(socket,
                         &QLocalSocket::errorOccurred,
                         this,
                         [guardedSocket](const QLocalSocket::LocalSocketError p_error) {
                            if (guardedSocket.isNull())
                            {
                                return;
                            }

                            qWarning() << "Platform control client socket error" << p_error << ":"
                                        << guardedSocket->errorString();
                         });
    }
}

void PlatformControlServer::handleSocketReadyRead(QLocalSocket* p_socket)
{
    if (p_socket == nullptr || !m_socketBuffers.contains(p_socket))
    {
        return;
    }

    const QPointer<QLocalSocket> guardedSocket(p_socket);
    QByteArray& buffer = m_socketBuffers[p_socket];
    buffer.append(p_socket->readAll());
    if (buffer.size() > PlatformControlProtocol::kMaxMessageBytes)
    {
        sendResponse(guardedSocket.get(), makeProtocolErrorResponse("Platform control request exceeded maximum size"));
        if (!guardedSocket.isNull())
        {
            guardedSocket->disconnectFromServer();
        }
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
        sendResponse(guardedSocket.get(), makeProtocolErrorResponse("Platform control request was empty"));
        if (!guardedSocket.isNull())
        {
            guardedSocket->disconnectFromServer();
        }
        return;
    }

    const auto parsedRequest = PlatformControlProtocol::parseMessage(requestLine);
    if (!parsedRequest.has_value())
    {
        sendResponse(guardedSocket.get(), makeProtocolErrorResponse(parsedRequest.error()));
        if (!guardedSocket.isNull())
        {
            guardedSocket->disconnectFromServer();
        }
        return;
    }

    const QJsonObject response = m_mainApp->handleControlRequest(parsedRequest.value());
    if (guardedSocket.isNull() || !m_socketBuffers.contains(guardedSocket.get()))
    {
        return;
    }

    sendResponse(guardedSocket.get(), response);
    guardedSocket->disconnectFromServer();
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

    const QByteArray payload = PlatformControlProtocol::serializeMessage(p_response);
    const qint64 written = p_socket->write(payload);
    if (written != payload.size())
    {
        qWarning() << "Platform control response write was partial:" << written << "of" << payload.size();
    }

    p_socket->flush();
    if (p_socket->bytesToWrite() > 0 && !p_socket->waitForBytesWritten(30000))
    {
        qWarning() << "Platform control response did not flush before disconnect:" << p_socket->errorString();
    }
}
