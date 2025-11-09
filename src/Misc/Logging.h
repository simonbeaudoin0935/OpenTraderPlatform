#pragma once

#include <QtGlobal>
#include <QTextStream>
#include <QMap>
#include <QSettings>

void initLogging();

class LoggingConfig {
public:
    static LoggingConfig& instance();

    bool isCategoryEnabled(const QString& category) const;
    void setCategoryEnabled(const QString& category, bool enabled);
    QStringList getCategories() const;

private:
    LoggingConfig();
    ~LoggingConfig() = default;
    LoggingConfig(const LoggingConfig&) = delete;
    LoggingConfig& operator=(const LoggingConfig&) = delete;

    QMap<QString, bool> m_categoryEnabled;
    QStringList m_categories;
    QSettings m_settings;
};