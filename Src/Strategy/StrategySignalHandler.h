#pragma once

#include <QString>
#include <memory>

class StrategyManager;

/// @brief Per-thread SIGSEGV signal handler for strategy threads
/// Catches segmentation faults and notifies StrategyManager to mark strategy as failed
/// Uses pthread_sigaltstack for alternate stack to handle stack overflow crashes
namespace StrategySignalHandler
{

    /// @brief Install SIGSEGV handler for current thread (call from strategy thread)
    /// @param p_strategyID Strategy ID to associate with this thread
    /// @param p_strategyManager StrategyManager instance for failure notification
    /// @return true if successfully installed, false on error
    [[nodiscard]] bool installSignalHandler(const QString& p_strategyID, StrategyManager* p_strategyManager);

    /// @brief Uninstall SIGSEGV handler for current thread
    void uninstallSignalHandler();

} // namespace StrategySignalHandler
