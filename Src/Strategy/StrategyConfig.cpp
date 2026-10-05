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
            p_soPath = "~/.local/share/OpenTraderPlatform/Strategies/" + filename;

            QString expandedPath = expandTilde(p_soPath);
            if (!QFileInfo::exists(expandedPath))
            {
                p_soPath = "/usr/share/opentraderplatform/strategies/" + filename;
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
        if (p_runtimeType == "plugin-shared-library")
        {
            return StrategyRuntimeType::PluginSharedLibrary;
        }

        if (p_runtimeType == "external-process")
        {
            return StrategyRuntimeType::ExternalProcess;
        }

        return StrategyRuntimeType::ExternalProcess;
    }
} // namespace

QJsonObject StrategyConfig::toJson() const
{
    QJsonObject obj;
    obj["runtimeType"] = runtimeTypeToString(runtimeType);
    obj["name"] = name;
    obj["version"] = version;
    obj["soPath"] = soPath;
    obj["executablePath"] = executablePath;

    QJsonObject fieldValuesObject;
    for (const auto& [key, value]: fieldValues)
    {
        fieldValuesObject[key] = value;
    }
    obj["fieldValues"] = fieldValuesObject;

    return obj;
}

StrategyConfig StrategyConfig::fromJson(const QJsonObject& obj)
{
    StrategyConfig config;
    config.name = obj["name"].toString();
    config.version = obj["version"].toString();

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
    else if (!rawSoPath.isEmpty())
    {
        config.runtimeType = StrategyRuntimeType::PluginSharedLibrary;
    }

    if (config.usesExternalProcess())
    {
        config.executablePath = resolveExecutablePath(rawExecutablePath);
    }
    else
    {
        config.soPath = resolvePluginPath(rawSoPath);
    }

    const QJsonObject fieldValuesObject =
        obj.contains("fieldValues") ? obj["fieldValues"].toObject() : obj["customParams"].toObject();
    for (auto it = fieldValuesObject.begin(); it != fieldValuesObject.end(); ++it)
    {
        config.fieldValues[it.key()] = it.value();
    }

    if (!obj.contains("fieldValues"))
    {
        const QJsonArray symbolsArray = obj["symbols"].toArray();
        if (symbolsArray.size() == 1 && !config.fieldValues.contains("symbol"))
        {
            config.fieldValues["symbol"] = symbolsArray.first();
        }
        else if (symbolsArray.size() > 1 && !config.fieldValues.contains("symbol"))
        {
            config.fieldValues["_legacySymbols"] = symbolsArray;
        }

        if (obj.contains("positionSize") && !config.fieldValues.contains("positionSize"))
        {
            config.fieldValues["positionSize"] = obj["positionSize"];
        }

        if (obj.contains("riskLimit") && !config.fieldValues.contains("riskLimit"))
        {
            config.fieldValues["riskLimit"] = obj["riskLimit"];
        }
    }

    return config;
}
