#include "StrategySignalHandler.h"
#include "StrategyManager.h"
#include <csignal>
#include <cstring>
#include <thread>
#include <map>
#include <pthread.h>
#include <QMutex>
#include <QDebug>
#include <QMetaObject>

// Thread-local storage for current strategy ID
static thread_local QString g_currentStrategyID;
static thread_local StrategyManager* g_currentStrategyManager = nullptr;

// Map: thread_id -> strategy_id for identifying which strategy crashed
static QMutex g_threadMapMutex;
static std::map<std::thread::id, QString> g_threadStrategyMap;
static std::map<std::thread::id, StrategyManager*> g_threadManagerMap;

// Signal handler for SIGSEGV, SIGABRT, and SIGTERM
static void strategySignalHandler(int p_signal)
{
    // This handler runs on the strategy thread with an alternate stack
    // Minimal code here to be safe - just notify StrategyManager

    if (g_currentStrategyManager && !g_currentStrategyID.isEmpty())
    {
        QString errorMsg;
        switch (p_signal)
        {
        case SIGSEGV:
            errorMsg = "Segmentation fault (SIGSEGV)";
            break;
        case SIGABRT:
            errorMsg = "Abort signal (SIGABRT)";
            break;
        case SIGTERM:
            errorMsg = "Termination signal (SIGTERM)";
            break;
        default:
            errorMsg = QString("Unknown signal (%1)").arg(p_signal);
        }

        // Use a queued invocation to safely notify from signal handler
        QString strategyID = g_currentStrategyID;
        StrategyManager* manager = g_currentStrategyManager;

        QMetaObject::invokeMethod(
            manager,
            [strategyID, manager, errorMsg]()
            {
                qCritical() << "Strategy thread crashed with signal:" << strategyID << "-" << errorMsg;
                manager->markStrategyFailed(strategyID, errorMsg);
            },
            Qt::QueuedConnection);

        // For SIGABRT, restore default handler before exiting to prevent re-entry loops
        // (abort() can retry the signal if we just exit normally)
        if (p_signal == SIGABRT)
        {
            signal(SIGABRT, SIG_DFL);
        }

        // Exit the current thread gracefully instead of raising the signal
        // This prevents the crash from propagating to the main thread
        pthread_exit(nullptr);
    }
}

namespace StrategySignalHandler
{

    bool installSignalHandler(const QString& p_strategyID, StrategyManager* p_strategyManager)
    {
        if (p_strategyID.isEmpty() || !p_strategyManager)
        {
            qWarning() << "Invalid parameters for installSignalHandler";
            return false;
        }

        // Store thread-local references
        g_currentStrategyID = p_strategyID;
        g_currentStrategyManager = p_strategyManager;

        // Store global mapping for debugging
        {
            QMutexLocker locker(&g_threadMapMutex);
            g_threadStrategyMap[std::this_thread::get_id()] = p_strategyID;
            g_threadManagerMap[std::this_thread::get_id()] = p_strategyManager;
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
            g_threadManagerMap.erase(std::this_thread::get_id());
        }

        qDebug() << "Uninstalled signal handlers for strategy:" << g_currentStrategyID;

        g_currentStrategyID.clear();
        g_currentStrategyManager = nullptr;
    }

} // namespace StrategySignalHandler
