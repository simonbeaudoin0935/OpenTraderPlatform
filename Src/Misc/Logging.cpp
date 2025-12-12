#include "Logging.h"
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
#include <cxxabi.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <mutex>

// Mutex to protect log file and stdout access
static std::recursive_mutex loggingMutex;

// Global log file stream
static QTextStream *logStream = nullptr;
static QFile logFile;

// Forward declaration
void printStackTrace();

// Signal handler for crashes
void crashHandler(int sig) {
    // Lock the logging mutex to prevent interleaved output
    std::lock_guard<std::recursive_mutex> lock(loggingMutex);
    
    std::string signalMsg = "\nReceived signal " + std::to_string(sig) + " - ";
    switch (sig) {
        case SIGSEGV: signalMsg += "Segmentation fault"; break;
        case SIGABRT: signalMsg += "Abort signal"; break;
        case SIGFPE: signalMsg += "Floating point exception"; break;
        case SIGILL: signalMsg += "Illegal instruction"; break;
        default: signalMsg += "Unknown signal"; break;
    }
    signalMsg += "\n";
    
    // Write signal message to both log file and stderr
    if (logStream) {
        *logStream << QString::fromStdString(signalMsg);
        logStream->flush();
    }
    std::cerr << signalMsg;
    
    printStackTrace();
    
    // Re-raise the signal to get default behavior (core dump, etc.)
    signal(sig, SIG_DFL);
    raise(sig);
}

// Function to demangle a single symbol
std::string demangleSymbol(const char* mangledName) {
    int status;
    char* demangled = abi::__cxa_demangle(mangledName, nullptr, nullptr, &status);
    if (status == 0 && demangled) {
        std::string result = demangled;
        free(demangled);
        return result;
    }
    return mangledName;
}

// Function to print stack trace with better symbol resolution
void printStackTrace()
{
    std::lock_guard<std::recursive_mutex> lock(loggingMutex);
    
    const int maxFrames = 64;
    void* frames[maxFrames];
    int numFrames = backtrace(frames, maxFrames);
    char** symbols = backtrace_symbols(frames, maxFrames);

    std::string stackTraceMsg = "\nStack trace (" + std::to_string(numFrames) + " frames):";

    if (symbols) {
        // Get executable path for addr2line
        char exePath[1024];
        ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
        if (len != -1) {
            exePath[len] = '\0';
        } else {
            exePath[0] = '\0';
        }

        for (int i = 0; i < numFrames; ++i) {
            std::string frameMsg = "  #" + std::to_string(i) + " ";

            // Always try addr2line first for better resolution
            if (exePath[0] != '\0') {
                char addr2lineCmd[2048];
                snprintf(addr2lineCmd, sizeof(addr2lineCmd),
                        "addr2line -f -C -i -e %s %p 2>/dev/null", exePath, frames[i]);

                FILE* fp = popen(addr2lineCmd, "r");
                if (fp) {
                    char function[256] = {0};
                    char fileInfo[256] = {0};

                    if (fgets(function, sizeof(function), fp)) {
                        function[strcspn(function, "\n")] = 0; // Remove newline
                        if (fgets(fileInfo, sizeof(fileInfo), fp)) {
                            fileInfo[strcspn(fileInfo, "\n")] = 0; // Remove newline

                            // Check if we got valid information
                            if (strcmp(function, "??") != 0 && strcmp(fileInfo, "??:0") != 0) {
                                std::string funcName = demangleSymbol(function);
                                frameMsg += funcName + " at " + fileInfo;
                                pclose(fp);
                                stackTraceMsg += "\n" + frameMsg;
                                continue;
                            }
                        }
                    }
                    pclose(fp);
                }
            }

            // Fallback to backtrace_symbols parsing
            // Parse the backtrace_symbols output
            // Format: "binary_path(+offset) [address]" or "binary_path(mangled_func+offset) [address]"
            char* symbol = symbols[i];
            char* leftParen = strchr(symbol, '(');
            char* plus = leftParen ? strchr(leftParen, '+') : nullptr;
            char* rightParen = plus ? strchr(plus, ')') : nullptr;

            if (leftParen && plus && rightParen) {
                // Extract binary path, function name, and offset
                *leftParen = '\0';
                *plus = '\0';
                *rightParen = '\0';

                const char* binaryPath = symbol;
                const char* mangledFunc = leftParen + 1;
                const char* offset = plus + 1;

                // Check if this is our executable
                if (strstr(binaryPath, QCoreApplication::applicationName().toUtf8().constData()) || strstr(binaryPath, "test_barcache")) {
                    if (strlen(mangledFunc) > 0) {
                        std::string funcName = demangleSymbol(mangledFunc);
                        frameMsg += funcName + " in " + binaryPath;
                        if (strlen(offset) > 0) {
                            frameMsg += " +" + std::string(offset);
                        }
                    } else {
                        frameMsg += std::string(binaryPath) + " +" + std::string(offset);
                    }
                } else {
                    // Not our executable, use normal parsing
                    std::string funcName = demangleSymbol(mangledFunc);
                    frameMsg += funcName + " in " + binaryPath;
                    if (strlen(offset) > 0) {
                        frameMsg += " +" + std::string(offset);
                    }
                }
            } else {
                // Fallback for unparseable symbols
                frameMsg += symbols[i];
            }

            stackTraceMsg += "\n" + frameMsg;
        }
        free(symbols);
    }

    stackTraceMsg += "\n\nNote: If function names appear as '\?\?' or line numbers are missing, the executable may need debug symbols.";
    stackTraceMsg += "\nEnsure CONFIG += debug is set in your .pro file and rebuild with -rdynamic flag.\n";

    // Write to both log file and stderr
    if (logStream) {
        *logStream << QString::fromStdString(stackTraceMsg);
        logStream->flush();
    }
    std::cerr << stackTraceMsg << std::endl;
}

// LogBroadcaster implementation
LogBroadcaster& LogBroadcaster::instance() {
    static LogBroadcaster instance;
    return instance;
}

void LogBroadcaster::broadcastLogMessage(const QString& message) {
    emit logMessageReceived(message);
}

// LoggingConfig implementation
LoggingConfig::LoggingConfig()
    : m_settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::applicationName(), "Logging")
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

void LoggingConfig::writeConfigToDisk() {
    // Write all current category states to settings
    for (auto it = m_categoryEnabled.begin(); it != m_categoryEnabled.end(); ++it) {
        m_settings.setValue(QString("Categories/%1").arg(it.key()), it.value());
    }
    
    // Write global settings
    m_settings.setValue("Global/DisableDebug", isDebugDisabled());
    m_settings.setValue("Global/DisableInfo", isInfoDisabled());
    
    m_settings.sync();
}

bool LoggingConfig::isDebugDisabled() const {
    return m_settings.value("Global/DisableDebug", false).toBool();
}

void LoggingConfig::setDebugDisabled(bool disabled) {
    m_settings.setValue("Global/DisableDebug", disabled);
    m_settings.sync();
}

bool LoggingConfig::isInfoDisabled() const {
    return m_settings.value("Global/DisableInfo", false).toBool();
}

void LoggingConfig::setInfoDisabled(bool disabled) {
    m_settings.setValue("Global/DisableInfo", disabled);
    m_settings.sync();
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
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString category = (strcmp(context.category, "default") == 0) ? "" : QString(context.category ? context.category : "");

    // Broadcast to GUI with HTML color formatting
    QString htmlColorCode;
    switch (type) {
    case QtDebugMsg:
        htmlColorCode = "#00CED1"; // Cyan
        break;
    case QtInfoMsg:
        htmlColorCode = "#32CD32"; // Green
        break;
    case QtWarningMsg:
        htmlColorCode = "#FFD700"; // Yellow
        break;
    case QtCriticalMsg:
        htmlColorCode = "#FF4500"; // Red
        break;
    case QtFatalMsg:
        htmlColorCode = "#FF00FF"; // Magenta
        break;
    }

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

    QString formattedMsg = QString("%1[%2] %3 %4:%5 %6%7")
                               .arg(colorCode)
                               .arg(timestamp)
                               .arg(typeText)
                               .arg(category)
                               .arg(RESET_COLOR)
                               .arg(msg)
                               .arg(RESET_COLOR);

    QString htmlMsg = QString("<span style='color:%1'>[%2] %3 %4:</span> %5")
                          .arg(htmlColorCode)
                          .arg(timestamp)
                          .arg(typeText)
                          .arg(category)
                          .arg(msg);

    std::lock_guard<std::recursive_mutex> lock(loggingMutex);

    // Always write to log file (without ANSI colors)
    if (logStream) {
        *logStream << formattedMsg << "\n";
        logStream->flush();
    }

    // Check global disable settings first
    if ((type == QtDebugMsg && LoggingConfig::instance().isDebugDisabled()) ||
        (type == QtInfoMsg && LoggingConfig::instance().isInfoDisabled())) {
        return;
    }

    // Filter console output based on category enabled state
    if (!LoggingConfig::instance().isCategoryEnabled(category)) {
        return;
    }

    std::cout << formattedMsg.toStdString() << std::endl;
    std::cout.flush();


    

    LogBroadcaster::instance().broadcastLogMessage(htmlMsg);

    // Print stack trace for fatal messages
    if (type == QtFatalMsg) {
        printStackTrace();
    }
}

void initLogging()
{
    // Install signal handlers for crash reporting
    signal(SIGSEGV, crashHandler);
    signal(SIGABRT, crashHandler);
    signal(SIGFPE, crashHandler);
    signal(SIGILL, crashHandler);

    // Get XDG-compliant state directory for logs
    // XDG_STATE_HOME defines where user-specific state files should be stored
    // Defaults to ~/.local/state if not set
    QString xdgStateHome = QProcessEnvironment::systemEnvironment().value("XDG_STATE_HOME");
    if (xdgStateHome.isEmpty()) {
        xdgStateHome = QDir::homePath() + "/.local/state";
    }
    
    // Create application-specific state directory
    QString appStateDir = xdgStateHome + "/" + QCoreApplication::applicationName();
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
    QString logFileName = QString("%1/%2_%3.log.ansi").arg(logsDirPath, QCoreApplication::applicationName(), QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss"));
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

void reinstallColoredMessageHandler()
{
    // Reinstall our custom colored message handler (useful after QTest overrides it)
    qInstallMessageHandler(coloredMessageOutput);
}

// Include the moc file for LogBroadcaster (QObject defined in this .cpp file)
#include "Logging.moc"

