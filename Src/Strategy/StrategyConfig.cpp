#include "StrategyConfig.h"
#include <QJsonArray>
#include <QDir>

QJsonObject StrategyConfig::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;
    obj["soPath"] = soPath;

    QJsonArray symbolsArray;
    for (const auto& symbol: symbols)
    {
        symbolsArray.append(symbol);
    }
    obj["symbols"] = symbolsArray;

    obj["positionSize"] = positionSize;
    obj["riskLimit"] = riskLimit;

    QJsonObject customParamsObj;
    for (const auto& [key, value]: customParams)
    {
        customParamsObj[key] = value;
    }
    obj["customParams"] = customParamsObj;

    return obj;
}

StrategyConfig StrategyConfig::fromJson(const QJsonObject& obj)
{
    StrategyConfig config;
    config.name = obj["name"].toString();

    // Support both "soPath" and "plugin" fields for backwards compatibility
    QString soPath = obj["soPath"].toString();
    if (soPath.isEmpty())
    {
        soPath = obj["plugin"].toString();
    }

    // If soPath is just a filename (no path separators), resolve it to the Strategies directory
    if (!soPath.isEmpty() && !soPath.contains("/") && !soPath.contains("~"))
    {
        soPath = "~/.local/share/L2Trader/Strategies/" + soPath;
    }

    config.soPath = soPath;

    // Expand tilde in soPath for home directory
    if (config.soPath.startsWith("~"))
    {
        config.soPath.replace(0, 1, QDir::homePath());
    }

    QJsonArray symbolsArray = obj["symbols"].toArray();
    for (const auto& symbol: symbolsArray)
    {
        config.symbols.append(symbol.toString());
    }

    config.positionSize = obj["positionSize"].toInt(100);
    config.riskLimit = obj["riskLimit"].toDouble(500.0);

    QJsonObject customParamsObj = obj["customParams"].toObject();
    for (auto it = customParamsObj.begin(); it != customParamsObj.end(); ++it)
    {
        config.customParams[it.key()] = it.value();
    }

    return config;
}
