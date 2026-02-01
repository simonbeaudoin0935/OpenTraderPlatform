#ifndef STRATEGY_CONFIG_LOADER_H
#define STRATEGY_CONFIG_LOADER_H

#include "StrategyConfig.h"
#include <QString>
#include <QStringList>

namespace StrategyConfigLoader
{

    // Load a strategy config from a JSON file
    // Returns empty config if file not found or invalid JSON
    [[nodiscard]] StrategyConfig loadConfig(const QString& config_file_path);

    // Save a strategy config to a JSON file
    // Returns true if successful, false otherwise
    [[nodiscard]] bool saveConfig(const StrategyConfig& config, const QString& config_file_path);

    // Discover all strategy configs in ~/.config/L2Trader/strategies/
    // Returns list of full file paths to .json config files
    [[nodiscard]] QStringList discoverConfigs();

    // Get the default config directory path
    // Returns ~/.config/L2Trader/strategies/
    [[nodiscard]] QString getConfigDirectory();

} // namespace StrategyConfigLoader

#endif // STRATEGY_CONFIG_LOADER_H
