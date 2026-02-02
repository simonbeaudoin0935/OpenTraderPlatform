#include "StrategySignalHandler.h"
#include "StrategyManager.h"
#include <csignal>
#include <cstring>
#include <thread>
#include <map>
#include <pthread.h>
#include <unistd.h>
#include <QMutex>
#include <QDebug>
#include <QSocketNotifier>

// Thread-local storage for current strategy ID
static thread_local QString g_currentStrategyID;

// Global pipe for crash notifications (async-signal-safe)
// Signal handler writes to write_fd, main thread reads from read_fd
static int g_crashNotifyPipe[2] = {-1, -1};

// Crash notification message structure (must be POD, simple)
struct CrashNotification
{
    char strategyID[256];
    char errorMsg[256];
    int signal;
};

// Map: thread_id -> strategy_id for identifying which strategy crashed
static QMutex g_threadMapMutex;
static std::map<std::thread::id, QString> g_threadStrategyMap;

// Signal handler for SIGSEGV, SIGABRT, and SIGTERM
static void strategySignalHandler(int p_signal)
{
    // This handler runs on the strategy thread with an alternate stack
    // ONLY async-signal-safe operations allowed here!

    if (!g_currentStrategyID.isEmpty() && g_crashNotifyPipe[1] != -1)
    {
        // Prepare crash notification (no Qt, no memory allocation)
        CrashNotification notif;
        std::memset(&notif, 0, sizeof(notif));

        // Copy strategy ID (safe string operation - bounded)
        std::strncpy(notif.strategyID, g_currentStrategyID.toStdString().c_str(), sizeof(notif.strategyID) - 1);

        // Set error message
        switch (p_signal)
        {
        case SIGSEGV:
            std::strcpy(notif.errorMsg, "Segmentation fault (SIGSEGV)");
            break;
        case SIGABRT:
            std::strcpy(notif.errorMsg, "Abort signal (SIGABRT)");
            break;
        case SIGTERM:
            std::strcpy(notif.errorMsg, "Termination signal (SIGTERM)");
            break;
        default:
            std::strcpy(notif.errorMsg, "Unknown signal");
        }

        notif.signal = p_signal;

        // Write to pipe - this IS async-signal-safe
        ssize_t result = write(g_crashNotifyPipe[1], &notif, sizeof(notif));
        (void)result; // Suppress unused warning

        // For SIGABRT, restore default handler to prevent re-entry
        if (p_signal == SIGABRT)
        {
            signal(SIGABRT, SIG_DFL);
        }

        // Exit thread gracefully
        pthread_exit(nullptr);
    }
}

namespace StrategySignalHandler
{

    bool initialize(StrategyManager* p_strategyManager)
    {
        if (!p_strategyManager)
        {
            qWarning() << "StrategySignalHandler::initialize: Invalid StrategyManager pointer";
            return false;
        }

        // Create pipe for crash notifications
        if (pipe(g_crashNotifyPipe) == -1)
        {
            qWarning() << "Failed to create crash notification pipe:" << strerror(errno);
            return false;
        }

        qDebug() << "Initialized signal handler system with crash notification pipe";
        return true;
    }

    void cleanup()
    {
        if (g_crashNotifyPipe[0] != -1)
        {
            close(g_crashNotifyPipe[0]);
            g_crashNotifyPipe[0] = -1;
        }
        if (g_crashNotifyPipe[1] != -1)
        {
            close(g_crashNotifyPipe[1]);
            g_crashNotifyPipe[1] = -1;
        }
        qDebug() << "Cleaned up signal handler system";
    }

    int getCrashNotificationFd()
    {
        return g_crashNotifyPipe[0];
    }

    bool installSignalHandler(const QString& p_strategyID)
    {
        if (p_strategyID.isEmpty())
        {
            qWarning() << "Invalid strategy ID for installSignalHandler";
            return false;
        }

        // Store thread-local strategy ID
        g_currentStrategyID = p_strategyID;

        // Store global mapping for debugging
        {
            QMutexLocker locker(&g_threadMapMutex);
            g_threadStrategyMap[std::this_thread::get_id()] = p_strategyID;
        }

        // Set up alternate stack for signal handler (prevents stack overflow during signal delivery)
        stack_t ss;
        ss.ss_sp = new char[SIGSTKSZ]; // Allocate alternate stack
        if (ss.ss_sp == nullptr)
        {
            qWarning() << "Failed to allocate alternate signal stack";
            return false;
        }
        ss.ss_size = SIGSTKSZ;
        ss.ss_flags = 0;

        if (sigaltstack(&ss, nullptr) == -1)
        {
            qWarning() << "Failed to setup alternate signal stack:" << strerror(errno);
            delete[] static_cast<char*>(ss.ss_sp);
            return false;
        }

        // Install signal handlers with alternate stack
        struct sigaction sa;
        sigemptyset(&sa.sa_mask);
        sa.sa_handler = strategySignalHandler;
        sa.sa_flags = SA_ONSTACK; // Use alternate stack for signal delivery

        // Install handlers for SIGSEGV, SIGABRT, and SIGTERM
        if (sigaction(SIGSEGV, &sa, nullptr) == -1)
        {
            qWarning() << "Failed to install SIGSEGV handler:" << strerror(errno);
            return false;
        }

        if (sigaction(SIGABRT, &sa, nullptr) == -1)
        {
            qWarning() << "Failed to install SIGABRT handler:" << strerror(errno);
            return false;
        }

        if (sigaction(SIGTERM, &sa, nullptr) == -1)
        {
            qWarning() << "Failed to install SIGTERM handler:" << strerror(errno);
            return false;
        }

        qDebug() << "Installed signal handlers for strategy:" << p_strategyID;
        return true;
    }

    void uninstallSignalHandler()
    {
        // Restore default signal handlers
        signal(SIGSEGV, SIG_DFL);
        signal(SIGABRT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);

        // Note: We intentionally leave the alternate stack allocated
        // as freeing it could cause issues if a signal arrives after uninstall
        // The OS will clean it up when the thread terminates

        {
            QMutexLocker locker(&g_threadMapMutex);
            g_threadStrategyMap.erase(std::this_thread::get_id());
        }

        qDebug() << "Uninstalled signal handlers for strategy:" << g_currentStrategyID;

        g_currentStrategyID.clear();
    }

} // namespace StrategySignalHandler
