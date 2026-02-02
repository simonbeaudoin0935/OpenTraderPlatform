#pragma once

#include <QString>
#include <memory>

class StrategyManager;

/// @brief Per-thread SIGSEGV signal handler for strategy threads
/// Catches segmentation faults and notifies StrategyManager to mark strategy as failed
/// Uses pthread_sigaltstack for alternate stack to handle stack overflow crashes
namespace StrategySignalHandler
{

    /// @brief Initialize signal handler system (call once from main thread)
    /// Sets up pipe for crash notifications
    /// @param p_strategyManager StrategyManager instance for failure notification
    /// @return true if successfully initialized, false on error
    [[nodiscard]] bool initialize(StrategyManager* p_strategyManager);

    /// @brief Cleanup signal handler system (call once on shutdown)
    void cleanup();

    /// @brief Get file descriptor for crash notification pipe (for monitoring)
    [[nodiscard]] int getCrashNotificationFd();

    /// @brief Install signal handlers for current thread (call from strategy thread)
    /// Catches SIGSEGV (segmentation faults), SIGABRT (aborts), and SIGTERM (termination requests)
    /// @param p_strategyID Strategy ID to associate with this thread
    /// @return true if successfully installed, false on error
    [[nodiscard]] bool installSignalHandler(const QString& p_strategyID);

    /// @brief Uninstall signal handlers for current thread
    void uninstallSignalHandler();

} // namespace StrategySignalHandler
