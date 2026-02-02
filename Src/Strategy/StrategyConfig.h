#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <map>

/// @brief Complete strategy configuration with custom parameters
/// Extends the basic StrategySDK::StrategyConfig with custom parameters
/// This is used for loading/saving configurations to JSON files
struct StrategyConfig
{
    QString name;                               ///< Strategy display name
    QString soPath;                             ///< Path to .so file (relative or absolute)
    QStringList symbols;                        ///< Symbols to monitor (e.g., ["AAPL", "TSLA"])
    int positionSize = 100;                     ///< Position size per trade
    double riskLimit = 500.0;                   ///< Max loss per strategy
    std::map<QString, QJsonValue> customParams; ///< Strategy-specific parameters

    /// Serialize to JSON object
    QJsonObject toJson() const;

    /// Deserialize from JSON object
    static StrategyConfig fromJson(const QJsonObject& obj);
};
