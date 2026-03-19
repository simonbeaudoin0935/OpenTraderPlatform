#include "StrategyConfig.h"
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>

namespace
{
    [[nodiscard]] QString expandTilde(QString p_path)
    {
        if (p_path.startsWith("~"))
        {
            p_path.replace(0, 1, QDir::homePath());
        }

        return p_path;
    }

    [[nodiscard]] QString resolvePluginPath(QString p_soPath)
    {
        if (p_soPath.isEmpty())
        {
            return p_soPath;
        }

        if (!p_soPath.contains("/") && !p_soPath.contains("~"))
        {
            const QString filename = p_soPath;
            p_soPath = "~/.local/share/L2Trader/Strategies/" + filename;

            QString expandedPath = expandTilde(p_soPath);
            if (!QFileInfo::exists(expandedPath))
            {
                p_soPath = "/usr/share/l2trader/strategies/" + filename;
            }
        }

        return expandTilde(std::move(p_soPath));
    }

    [[nodiscard]] QString resolveExecutablePath(QString p_executablePath)
    {
        return expandTilde(std::move(p_executablePath));
    }

    [[nodiscard]] QString runtimeTypeToString(const StrategyRuntimeType p_runtimeType)
    {
        switch (p_runtimeType)
        {
        case StrategyRuntimeType::ExternalProcess:
            return "external-process";
        case StrategyRuntimeType::PluginSharedLibrary:
        default:
            return "plugin-shared-library";
        }
    }

    [[nodiscard]] StrategyRuntimeType runtimeTypeFromString(const QString& p_runtimeType)
    {
        if (p_runtimeType == "external-process")
        {
            return StrategyRuntimeType::ExternalProcess;
        }

        return StrategyRuntimeType::PluginSharedLibrary;
    }
} // namespace

QJsonObject StrategyConfig::toJson() const
{
    QJsonObject obj;
    obj["runtimeType"] = runtimeTypeToString(runtimeType);
    obj["name"] = name;
    obj["soPath"] = soPath;
    obj["executablePath"] = executablePath;

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

    const QString runtimeTypeString = obj["runtimeType"].toString();
    const QString rawExecutablePath = obj["executablePath"].toString();
    QString rawSoPath = obj["soPath"].toString();
    if (rawSoPath.isEmpty())
    {
        rawSoPath = obj["plugin"].toString();
    }

    if (!runtimeTypeString.isEmpty())
    {
        config.runtimeType = runtimeTypeFromString(runtimeTypeString);
    }
    else if (!rawExecutablePath.isEmpty())
    {
        config.runtimeType = StrategyRuntimeType::ExternalProcess;
    }

    if (config.usesExternalProcess())
    {
        config.executablePath = resolveExecutablePath(!rawExecutablePath.isEmpty() ? rawExecutablePath : rawSoPath);
    }
    else
    {
        config.soPath = resolvePluginPath(rawSoPath);
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
