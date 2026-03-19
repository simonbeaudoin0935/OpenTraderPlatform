#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <map>

enum class StrategyRuntimeType
{
    PluginSharedLibrary,
    ExternalProcess
};

/// @brief Complete strategy configuration with custom parameters
/// Extends the basic StrategySDK::StrategyConfig with custom parameters
/// This is used for loading/saving configurations to JSON files
struct StrategyConfig
{
    StrategyRuntimeType runtimeType = StrategyRuntimeType::ExternalProcess; ///< How the strategy is executed
    QString name;                                                           ///< Strategy display name
    QString soPath;                                                         ///< Path to .so file (relative or absolute)
    QString executablePath;                                                 ///< Path to external strategy executable
    QStringList symbols;                        ///< Symbols to monitor (e.g., ["AAPL", "TSLA"])
    int positionSize = 100;                     ///< Position size per trade
    double riskLimit = 500.0;                   ///< Max loss per strategy
    std::map<QString, QJsonValue> customParams; ///< Strategy-specific parameters

    [[nodiscard]] bool usesExternalProcess() const
    {
        return runtimeType == StrategyRuntimeType::ExternalProcess;
    }

    [[nodiscard]] QString runtimePath() const
    {
        return usesExternalProcess() ? executablePath : soPath;
    }

    /// Serialize to JSON object
    QJsonObject toJson() const;

    /// Deserialize from JSON object
    static StrategyConfig fromJson(const QJsonObject& obj);
};
