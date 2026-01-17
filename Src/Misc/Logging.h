#pragma once

#include <QtGlobal>
#include <QTextStream>
#include <QMap>
#include <QSettings>
#include <QObject>
#include <QMetaEnum>

namespace QtEnum
{
    template <typename Enum>
    QString toString(Enum value)
    {
        return QMetaEnum::fromType<Enum>().valueToKey(static_cast<int>(value));
    }

    template <typename Enum>
    std::optional<Enum> fromString(QStringView str)
    {
        bool ok = false;
        const QMetaEnum meta = QMetaEnum::fromType<Enum>();
        int value = meta.keyToValue(str.toLatin1().constData(), &ok);
        if (!ok)
            return std::nullopt;
        return static_cast<Enum>(value);
    }

}

#define DEBUG    qCDebug(LOGGING_CATEGORY)    << this->objectName()
#define INFO     qCInfo(LOGGING_CATEGORY)     << this->objectName()
#define WARNING  qCWarning(LOGGING_CATEGORY)  << this->objectName()
#define CRITICAL qCCritical(LOGGING_CATEGORY) << this->objectName()
#define FATAL    qFatal


void initLogging();
void reinstallColoredMessageHandler();

// Singleton to broadcast log messages to GUI
class LogBroadcaster : public QObject {
    Q_OBJECT
public:
    static LogBroadcaster& instance();

    void broadcastLogMessage(const QString& message);

signals:
    void logMessageReceived(const QString& message);

private:
    LogBroadcaster() : QObject(nullptr) {}
    ~LogBroadcaster() = default;
    
    Q_DISABLE_COPY(LogBroadcaster) // Delete copy constructor and assignment operator
};

class LoggingConfig {
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