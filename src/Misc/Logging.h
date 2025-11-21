#pragma once

#include <QtGlobal>
#include <QTextStream>
#include <QMap>
#include <QSettings>
#include <QObject>

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
    LogBroadcaster(const LogBroadcaster&) = delete;
    LogBroadcaster& operator=(const LogBroadcaster&) = delete;
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
    LoggingConfig(const LoggingConfig&) = delete;
    LoggingConfig& operator=(const LoggingConfig&) = delete;

    QMap<QString, bool> m_categoryEnabled;
    QStringList m_categories;
    QSettings m_settings;
};