#include "Logging.h"
#include "Logging_generated.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <QProcessEnvironment>

#include <iostream>

// Global log file stream
static QTextStream *logStream = nullptr;
static QFile logFile;

// LoggingConfig implementation
LoggingConfig::LoggingConfig()
    : m_settings(QSettings::IniFormat, QSettings::UserScope, "L2Trader", "Logging")
{
    m_settings.setFallbacksEnabled(false);
    // Populate categories list and load enabled state for all known categories
    for (int i = 0; i < logging_categories_count; ++i) {
        QString category = QString::fromUtf8(logging_categories[i]);
        m_categories.append(category);
        bool enabled = m_settings.value(QString("Categories/%1").arg(category), true).toBool();
        m_categoryEnabled[category] = enabled;
    }
}

LoggingConfig& LoggingConfig::instance() {
    static LoggingConfig instance;
    return instance;
}

bool LoggingConfig::isCategoryEnabled(const QString& category) const {
    return m_categoryEnabled.value(category, true);
}

void LoggingConfig::setCategoryEnabled(const QString& category, bool enabled) {
    m_categoryEnabled[category] = enabled;
    
    // Write all current category states to settings
    for (auto it = m_categoryEnabled.begin(); it != m_categoryEnabled.end(); ++it) {
        m_settings.setValue(QString("Categories/%1").arg(it.key()), it.value());
    }
    
    m_settings.sync();
}

QStringList LoggingConfig::getCategories() const {
    return m_categories;
}

// ANSI color codes
#define RESET_COLOR "\033[0m"
#define RED_COLOR "\033[31m"
#define GREEN_COLOR "\033[32m"
#define YELLOW_COLOR "\033[33m"
#define BLUE_COLOR "\033[34m"
#define MAGENTA_COLOR "\033[35m"
#define CYAN_COLOR "\033[36m"
#define WHITE_COLOR "\033[37m"
#define GRAY_COLOR "\033[90m"

void coloredMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString colorCode;
    QString typeText;

    switch (type) {
    case QtDebugMsg:
        colorCode = CYAN_COLOR;
        typeText = "DEBG";
        break;
    case QtInfoMsg:
        colorCode = GREEN_COLOR;
        typeText = "INFO";
        break;
    case QtWarningMsg:
        colorCode = YELLOW_COLOR;
        typeText = "WARN";
        break;
    case QtCriticalMsg:
        colorCode = RED_COLOR;
        typeText = "CRIT";
        break;
    case QtFatalMsg:
        colorCode = MAGENTA_COLOR;
        typeText = "FATAL";
        break;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString category = context.category == "default" ? "" : QString(context.category);

    QString formattedMsg = QString("%1[%2] %3 %4:%5 %6%7")
                               .arg(colorCode)
                               .arg(timestamp)
                               .arg(typeText)
                               .arg(category)
                               .arg(RESET_COLOR)
                               .arg(msg)
                               .arg(RESET_COLOR);

    // Always write to log file (without ANSI colors)
    if (logStream) {
        *logStream << formattedMsg << "\n";
        logStream->flush();
    }

    // Filter console output based on category enabled state
    if (!LoggingConfig::instance().isCategoryEnabled(category)) {
        return;
    }

    std::cout << formattedMsg.toStdString() << std::endl;
    std::cout.flush();
}

void initLogging()
{
    // Get XDG-compliant state directory for logs
    // XDG_STATE_HOME defines where user-specific state files should be stored
    // Defaults to ~/.local/state if not set
    QString xdgStateHome = QProcessEnvironment::systemEnvironment().value("XDG_STATE_HOME");
    if (xdgStateHome.isEmpty()) {
        xdgStateHome = QDir::homePath() + "/.local/state";
    }
    
    // Create application-specific state directory
    QString appStateDir = xdgStateHome + "/L2Trader";
    QDir stateDir(appStateDir);
    if (!stateDir.exists()) {
        stateDir.mkpath(".");
    }
    
    // Create logs subdirectory within state directory
    QString logsDirPath = appStateDir + "/logs";
    QDir logsDir(logsDirPath);
    if (!logsDir.exists()) {
        logsDir.mkpath(".");
    }

    // Generate timestamped log file name in XDG state directory
    QString logFileName = QString("%1/L2Trader_%2.log").arg(logsDirPath, QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss"));
    logFile.setFileName(logFileName);

    // Open log file
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logStream = new QTextStream(&logFile);
        qInfo() << "Logging initialized. Log file:" << logFileName;
    } else {
        qFatal("Could not open log file: %s", logFileName.toUtf8().constData());
    }

    // Install custom colored message handler
    qInstallMessageHandler(coloredMessageOutput);

    // Initial info: list categories (adjust to match generated symbol names)
    qInfo() << "Logging categories:";
    QStringList categories = LoggingConfig::instance().getCategories();
    for (const QString& category : categories) {
        bool enabled = LoggingConfig::instance().isCategoryEnabled(category);
        QString status = enabled ? "enabled" : "disabled";
        qInfo().noquote() << "  -" << category << "(" << status << ")";
    }

    // LoggingConfig handles loading and saving category states
}

