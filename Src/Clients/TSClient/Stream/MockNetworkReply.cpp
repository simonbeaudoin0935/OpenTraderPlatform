#include "MockNetworkReply.h"
#include "Logging.h"

#include <QJsonDocument>
#include <QJsonObject>

#define LOGGING_CATEGORY MockNetworkReplyLog

Q_LOGGING_CATEGORY(MockNetworkReplyLog, "MockNetworkReply")

MockNetworkReply::MockNetworkReply(QObject* p_parent) : QNetworkReply(p_parent)
{
    // Open the internal buffer for reading and writing
    m_buffer.open(QIODevice::ReadWrite);

    // Set the QIODevice to open state for reading
    open(QIODevice::ReadOnly);

    DEBUG << "MockNetworkReply created";
}

MockNetworkReply::~MockNetworkReply()
{
    m_heartbeatTimer.stop();
    m_buffer.close();
    DEBUG << "MockNetworkReply destroyed";
}

void MockNetworkReply::startHeartbeat(int p_intervalMs)
{
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &MockNetworkReply::sendHeartbeat);
    m_heartbeatTimer.start(p_intervalMs);
    DEBUG << "Started heartbeat timer at" << p_intervalMs << "ms interval";
}

void MockNetworkReply::stopHeartbeat()
{
    m_heartbeatTimer.stop();
    DEBUG << "Stopped heartbeat timer";
}

void MockNetworkReply::sendHeartbeat()
{
    if (m_aborted)
    {
        return;
    }

    QJsonObject heartbeat;
    heartbeat["Heartbeat"] = true;
    heartbeat["Timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    QByteArray heartbeatData = QJsonDocument(heartbeat).toJson(QJsonDocument::Compact) + "\n";

    injectData(heartbeatData);
}

void MockNetworkReply::injectData(const QByteArray& p_data)
{
    if (m_aborted)
    {
        WARNING << "Cannot inject data - reply has been aborted";
        return;
    }

    if (p_data.isEmpty())
    {
        return;
    }

    // Debug: Log what data is being injected
    QString dataPreview = QString::fromUtf8(p_data).left(200);  // First 200 chars
    DEBUG << "MockNetworkReply::injectData called with data:" << dataPreview;

    // The Stream class expects newline-delimited JSON objects.
    // The recorded data should already include newlines, but we add one
    // as a safeguard if missing to ensure Stream can parse the data.
    QByteArray dataToWrite = p_data;
    if (!p_data.endsWith('\n'))
    {
        dataToWrite.append('\n');
        DEBUG << "Added missing newline delimiter to injected data";
    }

    // Save current read position
    qint64 currentPos = m_buffer.pos();

    // Seek to end to append data
    m_buffer.seek(m_buffer.size());
    m_buffer.write(dataToWrite);

    // Restore read position
    m_buffer.seek(currentPos);

    // Emit readyRead to notify Stream that data is available
    emit readyRead();
}

qint64 MockNetworkReply::bytesAvailable() const
{
    // Return bytes available in our buffer plus any from base class
    return m_buffer.bytesAvailable() + QNetworkReply::bytesAvailable();
}

bool MockNetworkReply::isSequential() const
{
    // Streaming data is sequential (cannot seek backwards)
    return true;
}

void MockNetworkReply::abort()
{
    if (m_aborted)
    {
        return;
    }

    DEBUG << "MockNetworkReply aborted";
    m_aborted = true;
    m_buffer.close();

    // Set error and emit finished
    setError(QNetworkReply::OperationCanceledError, "Aborted");
    emit errorOccurred(QNetworkReply::OperationCanceledError);
    emit finished();
}

qint64 MockNetworkReply::readData(char* p_data, qint64 p_maxlen)
{
    if (m_aborted)
    {
        return -1;
    }

    // Read from internal buffer
    return m_buffer.read(p_data, p_maxlen);
}

qint64 MockNetworkReply::writeData(const char* p_data, qint64 p_len)
{
    // Writing not supported - we only inject data via injectData()
    Q_UNUSED(p_data)
    Q_UNUSED(p_len)
    return -1;
}
