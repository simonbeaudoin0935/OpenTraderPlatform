#include "StrategyLogger.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>

StrategyLogger::StrategyLogger(const QString& p_strategyName) : m_strategyName(p_strategyName) {}

void StrategyLogger::log(QtMsgType p_level, const QString& p_message)
{
    QMutexLocker locker(&m_mutex);

    StrategyLogMessage msg;
    msg.timestamp = QDateTime::currentDateTime();
    msg.level = p_level;
    msg.message = p_message;

    m_messages.append(msg);

    // Maintain circular buffer - remove oldest if we exceed max
    if (m_messages.size() > MAX_MESSAGES)
    {
        m_messages.removeFirst();
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
        {
            result.append(msg);
        }
    }
    return result;
}

QVector<StrategyLogMessage> StrategyLogger::getRecentMessages(int p_count) const
{
    QMutexLocker locker(&m_mutex);

    QVector<StrategyLogMessage> result;
    int start = std::max(0LL, m_messages.size() - p_count);
    for (int i = start; i < m_messages.size(); ++i)
    {
        result.append(m_messages[i]);
    }
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
    QString xdgStateHome = qEnvironmentVariable("XDG_STATE_HOME");
    if (xdgStateHome.isEmpty())
        xdgStateHome = QDir::homePath() + "/.local/state";
    QString logDir = xdgStateHome + "/L2Trader/StrategiesLogs";

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    QString fileName = QString("strategy_%1_%2.log").arg(m_strategyName, timestamp);

    return logDir + "/" + fileName;
}

QString StrategyLogger::saveToFile()
{
    QString filePath = getLogFilePath();
    QFileInfo fileInfo(filePath);
    QDir logDir(fileInfo.dir());

    // Create directory if it doesn't exist
    if (!logDir.exists())
    {
        if (!logDir.mkpath("."))
        {
            qWarning() << "Failed to create log directory:" << logDir.path();
            return QString();
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "Failed to open log file for writing:" << filePath;
        return QString();
    }

    QTextStream stream(&file);

    {
        QMutexLocker locker(&m_mutex);
        for (const auto& msg: m_messages)
        {
            QString levelStr;
            switch (msg.level)
            {
            case QtDebugMsg:
                levelStr = "DEBUG";
                break;
            case QtInfoMsg:
                levelStr = "INFO";
                break;
            case QtWarningMsg:
                levelStr = "WARNING";
                break;
            case QtCriticalMsg:
                levelStr = "CRITICAL";
                break;
            case QtFatalMsg:
                levelStr = "FATAL";
                break;
            default:
                levelStr = "UNKNOWN";
            }

            stream << QString("[%1] %2 - %3\n")
                          .arg(msg.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"), levelStr, msg.message);
        }
    }

    file.close();
    qInfo() << "Saved strategy logs to:" << filePath;
    return filePath;
}
