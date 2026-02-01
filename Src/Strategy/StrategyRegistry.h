#pragma once

#include "StrategyConfig.h"
#include <QString>
#include <QVector>
#include <QMap>
#include <memory>

/// @brief Metadata about an available strategy from config file
struct StrategyMetadata
{
    QString configPath;    ///< Full path to config JSON file
    QString configName;    ///< Filename (e.g., "my_strategy.json")
    StrategyConfig config; ///< Loaded configuration
    bool isValid = false;  ///< True if config loaded successfully
    QString loadError;     ///< Error message if loading failed
};

/// @brief Registry of available strategy configurations
/// Scans ~/.config/L2Trader/strategies/ for strategy config files
/// Provides methods to list, search, and validate available strategies
class StrategyRegistry
{
  public:
    /// @brief Create registry and discover available strategies
    StrategyRegistry();

    /// @brief Re-scan config directory for strategies
    void refresh();

    /// @brief Get all available strategies
    [[nodiscard]] QVector<StrategyMetadata> getAvailableStrategies() const;

    /// @brief Get strategy by config filename (without path)
    /// @param configName Filename like "my_strategy.json"
    [[nodiscard]] const StrategyMetadata* findStrategyByName(const QString& configName) const;

    /// @brief Get strategy by config file path
    [[nodiscard]] const StrategyMetadata* findStrategyByPath(const QString& configPath) const;

    /// @brief Get count of available strategies
    [[nodiscard]] int strategyCount() const
    {
        return m_strategies.size();
    }

    /// @brief Check if strategy config is valid (loaded successfully)
    [[nodiscard]] bool isStrategyValid(const QString& configName) const;

    /// @brief Get error message for invalid strategy
    [[nodiscard]] QString getStrategyError(const QString& configName) const;

  private:
    QVector<StrategyMetadata> m_strategies; ///< Available strategies
    QMap<QString, int> m_nameIndex;         ///< Map configName -> index in m_strategies
    QMap<QString, int> m_pathIndex;         ///< Map configPath -> index in m_strategies
};
