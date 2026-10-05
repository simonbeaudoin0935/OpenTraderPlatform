#include "PlatformControlClient.h"

#include "Core/PlatformControlProtocol.h"

#include <QLocalSocket>

PlatformControlClient::PlatformControlClient() : PlatformControlClient(Options{}) {}

PlatformControlClient::PlatformControlClient(Options p_options) : m_options(std::move(p_options))
{
    if (m_options.socketPath.isEmpty())
    {
        m_options.socketPath = PlatformControlProtocol::socketPath();
    }

    if (m_options.connectTimeoutMs <= 0)
    {
        m_options.connectTimeoutMs = static_cast<int>(PlatformControlProtocol::kDefaultTimeoutMs);
    }

    if (m_options.responseTimeoutMs <= 0)
    {
        m_options.responseTimeoutMs = static_cast<int>(PlatformControlProtocol::kDefaultTimeoutMs);
    }
}

std::expected<QJsonObject, QString> PlatformControlClient::sendCommand(const QJsonObject& p_request) const
{
    QLocalSocket socket;
    socket.connectToServer(m_options.socketPath);
    if (!socket.waitForConnected(m_options.connectTimeoutMs))
    {
        return std::unexpected(QString("Failed to connect to platform control socket %1: %2")
                                   .arg(m_options.socketPath, socket.errorString()));
    }

    const QByteArray encodedRequest = PlatformControlProtocol::serializeMessage(p_request);
    if (socket.write(encodedRequest) != encodedRequest.size())
    {
        return std::unexpected(QString("Failed to write full control request: %1").arg(socket.errorString()));
    }

    if (!socket.waitForBytesWritten(m_options.responseTimeoutMs))
    {
        return std::unexpected(QString("Timed out while sending control request: %1").arg(socket.errorString()));
    }

    QByteArray responseBuffer;
    while (responseBuffer.indexOf('\n') < 0)
    {
        if (!socket.waitForReadyRead(m_options.responseTimeoutMs))
        {
            return std::unexpected(QString("Timed out waiting for control response: %1").arg(socket.errorString()));
        }

        responseBuffer.append(socket.readAll());
        if (responseBuffer.size() > PlatformControlProtocol::kMaxMessageBytes)
        {
            return std::unexpected("Control response exceeded maximum size");
        }
    }

    const qsizetype newlineIndex = responseBuffer.indexOf('\n');
    const QByteArray responseLine = responseBuffer.left(newlineIndex).trimmed();
    if (responseLine.isEmpty())
    {
        return std::unexpected("Control server returned an empty response");
    }

    return PlatformControlProtocol::parseMessage(responseLine);
}
