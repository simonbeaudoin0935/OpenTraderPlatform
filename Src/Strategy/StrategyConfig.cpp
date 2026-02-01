#include "StrategyConfig.h"
#include <QJsonArray>

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
    config.soPath = obj["soPath"].toString();

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
