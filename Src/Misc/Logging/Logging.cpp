#include "Logging.h"
#include "ALogger.h"
#include "CONSTANTS.h"
#include "Logging_generated.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <QProcessEnvironment>
#include <QCoreApplication>

#include <iostream>
#include <csignal>
#include <stacktrace>
#include <sstream>
#include <mutex>
#include <optional>
#include <atomic>
#include <unistd.h>

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

// Async logger instances — file receives plain text, stdout/stderr receives ANSI-coloured text.
static std::optional<ALogger> g_fileALogger;
static std::optional<ALogger> g_stdoutALogger;

// Underlying file handle kept open for the lifetime of the process.
static QFile g_logFile;

// Lightweight mutex — only guards LoggingConfig reads and LogBroadcaster emit.
// I/O itself is handled inside ALogger's own mutex.
static std::mutex g_filterMutex;

// Replay time: −1 means "not in replay mode". Updated atomically so that
// coloredMessageOutput() never needs to take a lock just for the timestamp.
static std::atomic<qint64> g_replayTimeMs{-1};

// Forward declaration
void printStackTrace();

// Signal handler for crashes
void crashHandler(int sig)
{
    // Drain pending async buffers first so no log lines are lost before the crash message.
    if (g_fileALogger)
        g_fileALogger->syncFlush();
    if (g_stdoutALogger)
        g_stdoutALogger->syncFlush();

    std::string signalMsg = "\nReceived signal " + std::to_string(sig) + " - ";
    switch (sig)
    {
    case SIGSEGV:
        signalMsg += "Segmentation fault";
        break;
    case SIGABRT:
        signalMsg += "Abort signal";
        break;
    case SIGFPE:
        signalMsg += "Floating point exception";
        break;
    case SIGILL:
        signalMsg += "Illegal instruction";
        break;
    default:
        signalMsg += "Unknown signal";
        break;
    }
    signalMsg += "\n";

    // Write crash message directly to both destinations (bypass async — we are crashing).
    if (g_logFile.isOpen())
    {
        int fd = g_logFile.handle();
        if (fd >= 0)
            ::write(fd, signalMsg.data(), signalMsg.size());
    }
    ::write(STDERR_FILENO, signalMsg.data(), signalMsg.size());

    printStackTrace();

    signal(sig, SIG_DFL);
    raise(sig);
}

// Function to print stack trace using C++23 stacktrace API
void printStackTrace()
{
    std::stacktrace trace = std::stacktrace::current();

    std::ostringstream stackTraceMsg;
    stackTraceMsg << "\nStack trace (" << trace.size() << " frames):\n";

    for (size_t i = 0; i < trace.size(); ++i)
    {
        const auto& entry = trace[i];
        stackTraceMsg << "  #" << i << " ";
        stackTraceMsg << std::to_string(entry) << "\n";
    }

    stackTraceMsg << "\nNote: Stack trace quality depends on debug symbols being present in the binary.\n";
    stackTraceMsg << "Build with CMAKE_BUILD_TYPE=Debug or RelWithDebInfo for best results.\n";

    std::string msg = stackTraceMsg.str();

    if (g_logFile.isOpen())
    {
        int fd = g_logFile.handle();
        if (fd >= 0)
            ::write(fd, msg.data(), msg.size());
    }
    ::write(STDERR_FILENO, msg.data(), msg.size());
}

// LogBroadcaster implementation
LogBroadcaster& LogBroadcaster::instance()
{
    static LogBroadcaster instance;
    return instance;
}

void LogBroadcaster::broadcastLogMessage(const QString& message)
{
    emit logMessageReceived(message);
}

// LoggingConfig implementation
LoggingConfig::LoggingConfig()
    : m_settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::applicationName(), "Logging")
{
    m_settings.setFallbacksEnabled(false);
    for (int i = 0; i < logging_categories_count; ++i)
    {
        QString category = QString::fromUtf8(logging_categories[i]);
        m_categories.append(category);
        bool enabled = m_settings.value(QString("Categories/%1").arg(category), true).toBool();
        m_categoryEnabled[category] = enabled;
    }
}

LoggingConfig& LoggingConfig::instance()
{
    static LoggingConfig instance;
    return instance;
}

bool LoggingConfig::isCategoryEnabled(const QString& category) const
{
    return m_categoryEnabled.value(category, true);
}

void LoggingConfig::setCategoryEnabled(const QString& category, bool enabled)
{
    m_categoryEnabled[category] = enabled;

    for (auto it = m_categoryEnabled.begin(); it != m_categoryEnabled.end(); ++it)
    {
        m_settings.setValue(QString("Categories/%1").arg(it.key()), it.value());
    }

    m_settings.sync();
}

QStringList LoggingConfig::getCategories() const
{
    return m_categories;
}

void LoggingConfig::writeConfigToDisk()
{
    for (auto it = m_categoryEnabled.begin(); it != m_categoryEnabled.end(); ++it)
    {
        m_settings.setValue(QString("Categories/%1").arg(it.key()), it.value());
    }

    m_settings.setValue("Global/DisableDebug", isDebugDisabled());
    m_settings.setValue("Global/DisableInfo", isInfoDisabled());

    m_settings.sync();
}

bool LoggingConfig::isDebugDisabled() const
{
    return m_settings.value("Global/DisableDebug", false).toBool();
}

void LoggingConfig::setDebugDisabled(bool disabled)
{
    m_settings.setValue("Global/DisableDebug", disabled);
    m_settings.sync();
}

bool LoggingConfig::isInfoDisabled() const
{
    return m_settings.value("Global/DisableInfo", false).toBool();
}

void LoggingConfig::setInfoDisabled(bool disabled)
{
    m_settings.setValue("Global/DisableInfo", disabled);
    m_settings.sync();
}

void setCurrentReplayTime(const QDateTime& dt)
{
    g_replayTimeMs.store(dt.toMSecsSinceEpoch(), std::memory_order_relaxed);
}

void clearReplayTime()
{
    g_replayTimeMs.store(-1, std::memory_order_relaxed);
}

void coloredMessageOutput(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    // Build the timestamp. In replay mode append the simulated market time.
    qint64 replayMs = g_replayTimeMs.load(std::memory_order_relaxed);
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    if (replayMs >= 0)
        timestamp += "-[" + QDateTime::fromMSecsSinceEpoch(replayMs).toString("hh:mm:ss.zzz") + "]";

    QString category =
        (strcmp(context.category, "default") == 0) ? "" : QString(context.category ? context.category : "");

    QString colorCode;
    QString typeText;
    QString htmlColorCode;

    switch (type)
    {
    case QtDebugMsg:
        colorCode = CYAN_COLOR;
        typeText = "DEBG";
        htmlColorCode = "#00CED1";
        break;
    case QtInfoMsg:
        colorCode = GREEN_COLOR;
        typeText = "INFO";
        htmlColorCode = "#32CD32";
        break;
    case QtWarningMsg:
        colorCode = YELLOW_COLOR;
        typeText = "WARN";
        htmlColorCode = "#FFD700";
        break;
    case QtCriticalMsg:
        colorCode = RED_COLOR;
        typeText = "CRIT";
        htmlColorCode = "#FF4500";
        break;
    case QtFatalMsg:
        colorCode = MAGENTA_COLOR;
        typeText = "FATAL";
        htmlColorCode = "#FF00FF";
        break;
    }

    // Plain text — written to the log file (no ANSI codes, readable with cat/grep).
    QString plainMsg = QString("[%1] %2 %3: %4\n").arg(timestamp).arg(typeText).arg(category).arg(msg);

    // ANSI-coloured — written to stdout/stderr for terminal display.
    QString coloredMsg = QString("%1[%2] %3 %4:%5 %6%7\n")
                             .arg(colorCode)
                             .arg(timestamp)
                             .arg(typeText)
                             .arg(category)
                             .arg(RESET_COLOR)
                             .arg(msg)
                             .arg(RESET_COLOR);

    // HTML — broadcast to the GUI log widget.
    QString htmlMsg = QString("<span style='color:%1'>[%2] %3 %4:</span> %5")
                          .arg(htmlColorCode)
                          .arg(timestamp)
                          .arg(typeText)
                          .arg(category)
                          .arg(msg);

    // Always write plain text to file (unfiltered).
    if (g_fileALogger)
    {
        QByteArray bytes = plainMsg.toUtf8();
        g_fileALogger->write(bytes.constData(), static_cast<size_t>(bytes.size()));
    }

    // Filtered paths: check category/level before writing to stdout and GUI.
    {
        std::lock_guard<std::mutex> lock(g_filterMutex);

        if ((type == QtDebugMsg && LoggingConfig::instance().isDebugDisabled()) ||
            (type == QtInfoMsg && LoggingConfig::instance().isInfoDisabled()))
        {
            return;
        }

        if (!LoggingConfig::instance().isCategoryEnabled(category))
            return;

        if (g_stdoutALogger)
        {
            QByteArray bytes = coloredMsg.toUtf8();
            g_stdoutALogger->write(bytes.constData(), static_cast<size_t>(bytes.size()));
        }

        LogBroadcaster::instance().broadcastLogMessage(htmlMsg);
    }
}

QString getLogsFolderPath()
{
    QString xdgStateHome = QProcessEnvironment::systemEnvironment().value("XDG_STATE_HOME");
    if (xdgStateHome.isEmpty())
    {
        xdgStateHome = QDir::homePath() + "/.local/state";
    }
    return xdgStateHome + "/" + QCoreApplication::applicationName() + "/AppLogs";
}

void initLogging()
{
    signal(SIGSEGV, crashHandler);
    signal(SIGABRT, crashHandler);
    signal(SIGFPE, crashHandler);
    signal(SIGILL, crashHandler);

    QString logsDirPath = getLogsFolderPath();
    QDir logsDir(logsDirPath);
    if (!logsDir.exists())
        logsDir.mkpath(".");

    // Plain text log file — no .ansi extension needed.
    QString logFileName = QString("%1/%2_%3.log")
                              .arg(logsDirPath,
                                   QCoreApplication::applicationName(),
                                   QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss"));
    g_logFile.setFileName(logFileName);

    if (!g_logFile.open(QIODevice::WriteOnly | QIODevice::Text))
        qFatal("Could not open log file: %s", logFileName.toUtf8().constData());

    int logFileFd = g_logFile.handle();

#ifdef GUI_ENABLED
    int stdioFd = STDOUT_FILENO;
#else
    int stdioFd = STDERR_FILENO;
#endif

    g_fileALogger.emplace(logFileFd, "file");
    g_stdoutALogger.emplace(stdioFd, "stdout");

    qInstallMessageHandler(coloredMessageOutput);

    qInfo() << "Logging initialized. Log file:" << logFileName;

    qInfo() << "Logging categories:";
    QStringList categories = LoggingConfig::instance().getCategories();
    for (const QString& category: categories)
    {
        bool enabled = LoggingConfig::instance().isCategoryEnabled(category);
        qInfo().noquote() << "  -" << category << "(" << (enabled ? "enabled" : "disabled") << ")";
    }
}

void shutdownLogging()
{
    // Drain and join both logger threads before closing the file.
    g_fileALogger.reset();
    g_stdoutALogger.reset();
    g_logFile.close();
}

void reinstallColoredMessageHandler()
{
    qInstallMessageHandler(coloredMessageOutput);
}
