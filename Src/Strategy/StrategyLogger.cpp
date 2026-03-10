#include "StrategyLogger.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>

StrategyLogger::StrategyLogger(const QString& p_strategyName) : m_strategyName(p_strategyName)
{
    QString xdgStateHome = qEnvironmentVariable("XDG_STATE_HOME");
    if (xdgStateHome.isEmpty())
        xdgStateHome = QDir::homePath() + "/.local/state";
    QString logDirPath = xdgStateHome + "/L2Trader/StrategiesLogs";

    QDir logDir(logDirPath);
    if (!logDir.exists())
        logDir.mkpath(".");

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    m_filePath = logDirPath + "/" + QString("strategy_%1_%2.log").arg(m_strategyName, timestamp);

    m_file = std::make_unique<QFile>(m_filePath);
    if (m_file->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        m_stream = std::make_unique<QTextStream>(m_file.get());
        qInfo() << "StrategyLogger: logging to" << m_filePath;
    }
    else
    {
        qWarning() << "StrategyLogger: failed to open log file:" << m_filePath;
        m_file.reset();
    }
}

static QString levelToString(QtMsgType p_level)
{
    switch (p_level)
    {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARNING";
    case QtCriticalMsg:
        return "CRITICAL";
    case QtFatalMsg:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}

void StrategyLogger::log(QtMsgType p_level, const QString& p_message)
{
    QMutexLocker locker(&m_mutex);

    StrategyLogMessage msg;
    msg.timestamp = QDateTime::currentDateTime();
    msg.level = p_level;
    msg.message = p_message;

    // Append to in-memory circular buffer for live UI display
    m_messages.append(msg);
    if (m_messages.size() > MAX_MESSAGES)
        m_messages.removeFirst();

    // Write to file immediately
    if (m_stream)
    {
        *m_stream << QString("[%1] %2 - %3\n")
                         .arg(msg.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"), levelToString(p_level), p_message);
        m_stream->flush();
    }
}

QVector<StrategyLogMessage> StrategyLogger::getMessages() const
{
    QMutexLocker locker(&m_mutex);
    return m_messages;
}

QVector<StrategyLogMessage> StrategyLogger::getMessagesSince(const QDateTime& p_since) const
{
    QMutexLocker locker(&m_mutex);

    QVector<StrategyLogMessage> result;
    for (const auto& msg: m_messages)
    {
        if (msg.timestamp >= p_since)
            result.append(msg);
    }
    return result;
}

QVector<StrategyLogMessage> StrategyLogger::getRecentMessages(int p_count) const
{
    QMutexLocker locker(&m_mutex);

    QVector<StrategyLogMessage> result;
    int start = std::max(0LL, m_messages.size() - p_count);
    for (int i = start; i < m_messages.size(); ++i)
        result.append(m_messages[i]);
    return result;
}

void StrategyLogger::clear()
{
    QMutexLocker locker(&m_mutex);
    m_messages.clear();
}

int StrategyLogger::messageCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_messages.size();
}

QString StrategyLogger::getLogFilePath() const
{
    return m_filePath;
}

QString StrategyLogger::saveToFile() const
{
    // File is written incrementally — nothing to flush; just return the path.
    return m_filePath;
}
