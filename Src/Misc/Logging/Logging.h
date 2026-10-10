#pragma once

#include <QtGlobal>
#include <QTextStream>
#include <QMap>
#include <QSettings>
#include <QObject>
#include <QMetaEnum>
#include <QLoggingCategory>
#include <QStringList>
#include <QVariant>

#include "Assume.h"

#define DEBUG qCDebug(LOGGING_CATEGORY) << this->objectName()
#define sDEBUG qCDebug(LOGGING_CATEGORY) << __FUNCTION__

#define INFO qCInfo(LOGGING_CATEGORY) << this->objectName()
#define sINFO qCInfo(LOGGING_CATEGORY) << __FUNCTION__

#define WARNING qCWarning(LOGGING_CATEGORY) << this->objectName()
#define sWARNING qCWarning(LOGGING_CATEGORY) << __FUNCTION__

#define CRITICAL qCCritical(LOGGING_CATEGORY) << this->objectName()
#define sCRITICAL qCCritical(LOGGING_CATEGORY) << __FUNCTION__

#define FATAL qFatal()

Q_DECLARE_LOGGING_CATEGORY(InputLog)

namespace QtEnum
{
    // Converts an enum value to its string representation
    // @note Works only for enums registered with Q_ENUM
    // @note Assumes the enum value is valid; asserts if not
    template<typename Enum> QString toString(Enum value)
    {
        const QMetaEnum meta = QMetaEnum::fromType<Enum>();

        const char* keyPtr = meta.valueToKey(static_cast<int>(value));
        ASSUME_TRUE(keyPtr != nullptr);

        return QString(keyPtr);
    }

    // Converts a string to the corresponding enum value
    // @note Assumes the string is valid; asserts if not
    template<typename Enum> Enum fromString(QStringView str)
    {
        bool ok = false;
        const QMetaEnum meta = QMetaEnum::fromType<Enum>();

        int value = meta.keyToValue(str.toLatin1().constData(), &ok);
        if (!ok)
        {
            qCritical() << "QtEnum::fromString: Invalid enum string:" << str << "for enum type" << meta.name();
            Q_UNREACHABLE();
        }


        return static_cast<Enum>(value);
    }
} // namespace QtEnum

void initLogging();
void reinstallColoredMessageHandler();
void shutdownLogging();
QString getLogsFolderPath();

// Replay time injection — called by ReplayEngine when the simulated market time advances.
// When set, every log line gains a dual timestamp: [real_time]-[replay_time].
// Call clearReplayTime() when replay stops so the suffix is removed.
void setCurrentReplayTime(const QDateTime& dt);
void clearReplayTime();

QString inputDetail(QStringView key, QStringView value);
QString inputDetail(QStringView key, const QString& value);
QString formatInputLogMessage(QStringView source, QStringView action, const QStringList& details = {});
void logInputEvent(QStringView source, QStringView action, const QStringList& details = {});

template<typename T> QString inputDetail(QStringView key, const T& value)
{
    return inputDetail(key, QVariant::fromValue(value).toString());
}

// Singleton to broadcast log messages to GUI
class LogBroadcaster : public QObject
{
    Q_OBJECT
  public:
    static LogBroadcaster& instance();

    void broadcastLogMessage(const QString& message);

    // Controls whether log messages are forwarded to the GUI widget.
    // When false, both the HTML formatting and the cross-thread signal are
    // skipped entirely — file/stdout logging is unaffected.
    // Thread-safe (backed by std::atomic<bool>).
    static void setGuiLoggingEnabled(bool enabled);
    static bool isGuiLoggingEnabled();

    // Number of CRIT lines written to the platform log since startup.
    // Counted regardless of category/level filters. Thread-safe.
    static int criticalLogCount();

    // Most recent CRIT lines (plain log format), oldest first, at most
    // AsyncLogger::RECENT_CRITICAL_LOG_CAPACITY entries. Thread-safe.
    static QStringList recentCriticalLogs();

    // Absolute path of this invocation's platform log file (empty before initLogging()).
    static QString currentLogFilePath();

  signals:
    void logMessageReceived(const QString& message);

    /**
     * @brief A CRIT line was just written to the platform log
     * Thread context: Emitted from whichever thread logged the message; connect with
     * Qt::QueuedConnection to receive it on the GUI thread
     * @param p_count Total number of CRIT lines since startup
     */
    void criticalLogCountChanged(int p_count);

  private:
    LogBroadcaster() : QObject(nullptr) {}
    ~LogBroadcaster() = default;

    Q_DISABLE_COPY(LogBroadcaster) // Delete copy constructor and assignment operator
};

class LoggingConfig
{
  public:
    static LoggingConfig& instance();

    bool isCategoryEnabled(const QString& category) const;
    void setCategoryEnabled(const QString& category, bool enabled);
    QStringList getCategories() const;
    void writeConfigToDisk();

    bool isDebugDisabled() const;
    void setDebugDisabled(bool disabled);
    bool isInfoDisabled() const;
    void setInfoDisabled(bool disabled);

  private:
    LoggingConfig();
    ~LoggingConfig() = default;

    Q_DISABLE_COPY(LoggingConfig) // Delete copy constructor and assignment operator

    QMap<QString, bool> m_categoryEnabled;
    QStringList m_categories;
    QSettings m_settings;
};
