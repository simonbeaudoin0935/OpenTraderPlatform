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
#include <execinfo.h>
#include <sstream>
#include <mutex>
#include <optional>
#include <atomic>
#include <unistd.h>
#include <time.h>

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

// Raw file descriptors cached at initLogging() time — safe to read from a
// signal handler without going through Qt.
static int g_logFileFd = -1;
static int g_stdioFd = -1;

// Re-entry guard: prevents infinite recursion if a second signal fires while
// the crash handler is already running (e.g. SIGSEGV during stack unwinding).
static std::atomic_flag g_crashHandlerActive = ATOMIC_FLAG_INIT;

// Guards GUI log emission. When false, HTML formatting and the cross-thread
// signal are skipped entirely. Toggled by the "Show Logger Widget" checkbox.
static std::atomic<bool> g_guiLoggingEnabled{true};

// Lightweight mutex — only guards LoggingConfig reads and LogBroadcaster emit.
// I/O itself is handled inside ALogger's own mutex.
static std::mutex g_filterMutex;

// Replay time: −1 means "not in replay mode". Updated atomically so that
// coloredMessageOutput() never needs to take a lock just for the timestamp.
static std::atomic<qint64> g_replayTimeMs{-1};

// Forward declaration
void printStackTrace();

// Signal handler for crashes.
//
// Async-signal-safety notes:
//   - crashFlush() uses try_lock() + nanosleep() — no malloc, bounded wait.
//   - All output uses raw ::write() on cached fds — no Qt, no malloc.
//   - printStackTrace() uses backtrace() + backtrace_symbols_fd() — no malloc.
//   - g_crashHandlerActive prevents re-entry if a second signal fires.
void crashHandler(int sig)
{
    // Re-entry guard: if a second signal fires while we are already in here
    // (e.g. SIGSEGV during backtrace unwinding), reset to the default handler
    // and re-raise immediately so the OS can produce a core dump.
    if (g_crashHandlerActive.test_and_set())
    {
        signal(sig, SIG_DFL);
        raise(sig);
        return;
    }

    // Best-effort drain of any pending buffered log messages (e.g. recent
    // context leading up to a SIGSEGV). Uses bounded try_lock — never deadlocks.
    if (g_fileALogger)
        g_fileALogger->crashFlush();
    if (g_stdoutALogger)
        g_stdoutALogger->crashFlush();

    // Write crash banner directly to both destinations using cached fds.
    // No Qt calls, no heap allocation — safe in a signal handler.
    const char* sigName = (sig == SIGSEGV)   ? "Segmentation fault"
                          : (sig == SIGABRT) ? "Abort signal"
                          : (sig == SIGFPE)  ? "Floating point exception"
                          : (sig == SIGILL)  ? "Illegal instruction"
                                             : "Unknown signal";
    char msg[80];
    int n = snprintf(msg, sizeof(msg), "\nReceived signal %d - %s\n", sig, sigName);
    if (n > 0)
    {
        if (g_logFileFd >= 0)
            ::write(g_logFileFd, msg, static_cast<size_t>(n));
        if (g_stdioFd >= 0)
            ::write(g_stdioFd, msg, static_cast<size_t>(n));
    }

    printStackTrace();

    signal(sig, SIG_DFL);
    raise(sig);
}

// Print a stack trace to both the log file and stdio fd.
//
// Uses backtrace() + backtrace_symbols_fd() from <execinfo.h>:
//   - backtrace()            : fills a stack-allocated frame array, no malloc.
//   - backtrace_symbols_fd() : writes symbol strings directly to an fd, no malloc,
//                              async-signal-safe on Linux/glibc.
// The header is written with a stack-allocated buffer + ::write() — no
// std::string / std::ostringstream, no heap allocation.
void printStackTrace()
{
    static const int MAX_FRAMES = 64;
    void* frames[MAX_FRAMES];
    int nframes = backtrace(frames, MAX_FRAMES);

    char header[64];
    int n = snprintf(header, sizeof(header), "\nStack trace (%d frames):\n", nframes);
    if (n > 0)
    {
        if (g_logFileFd >= 0)
            ::write(g_logFileFd, header, static_cast<size_t>(n));
        if (g_stdioFd >= 0)
            ::write(g_stdioFd, header, static_cast<size_t>(n));
    }

    if (g_logFileFd >= 0)
        backtrace_symbols_fd(frames, nframes, g_logFileFd);
    if (g_stdioFd >= 0)
        backtrace_symbols_fd(frames, nframes, g_stdioFd);

    static const char footer[] = "\nNote: Stack trace quality depends on debug symbols. "
                                 "Build with CMAKE_BUILD_TYPE=Debug or RelWithDebInfo for best results.\n";
    if (g_logFileFd >= 0)
        ::write(g_logFileFd, footer, sizeof(footer) - 1);
    if (g_stdioFd >= 0)
        ::write(g_stdioFd, footer, sizeof(footer) - 1);
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

void LogBroadcaster::setGuiLoggingEnabled(bool enabled)
{
    g_guiLoggingEnabled.store(enabled, std::memory_order_relaxed);
}

bool LogBroadcaster::isGuiLoggingEnabled()
{
    return g_guiLoggingEnabled.load(std::memory_order_relaxed);
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
        timestamp += "]-[" + QDateTime::fromMSecsSinceEpoch(replayMs).toString("hh:mm:ss.zzz");

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
    // Read the flag once here; skip both the QString allocation and the
    // cross-thread signal when the logger widget is hidden.
    const bool guiEnabled = LogBroadcaster::isGuiLoggingEnabled();
    QString htmlMsg;
    if (guiEnabled)
    {
        htmlMsg = QString("<span style='color:%1'>[%2] %3 %4:</span> %5")
                      .arg(htmlColorCode)
                      .arg(timestamp)
                      .arg(typeText)
                      .arg(category)
                      .arg(msg);
    }

    // Always write plain text to file (unfiltered).
    if (g_fileALogger)
    {
        QByteArray bytes = plainMsg.toUtf8();
        g_fileALogger->write(bytes.constData(), static_cast<size_t>(bytes.size()));
    }

    // For fatal messages, also write directly and synchronously to both fds
    // using raw ::write() BEFORE the async enqueue above takes effect.
    // This guarantees the assertion failure reason is on disk and visible in
    // the terminal even if the crash handler subsequently deadlocks or the
    // process is killed before the async buffer is drained.
    if (type == QtFatalMsg)
    {
        QByteArray plainBytes = plainMsg.toUtf8();
        QByteArray coloredBytes = coloredMsg.toUtf8();
        if (g_logFileFd >= 0)
            ::write(g_logFileFd, plainBytes.constData(), static_cast<size_t>(plainBytes.size()));
        if (g_stdioFd >= 0)
            ::write(g_stdioFd, coloredBytes.constData(), static_cast<size_t>(coloredBytes.size()));
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

        if (guiEnabled)
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

    // Cache raw fds for use in the crash handler and printStackTrace() where
    // calling Qt methods is not async-signal-safe.
    g_logFileFd = logFileFd;
    g_stdioFd = stdioFd;

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
    // Invalidate cached fds before closing so the crash handler cannot use a
    // stale fd if a signal fires during or after shutdown.
    g_logFileFd = -1;
    g_stdioFd = -1;

    // Drain and join both logger threads before closing the file.
    g_fileALogger.reset();
    g_stdoutALogger.reset();
    g_logFile.close();
}

void reinstallColoredMessageHandler()
{
    qInstallMessageHandler(coloredMessageOutput);
}
