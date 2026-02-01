#include "StrategyConfigLoader.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

namespace StrategyConfigLoader
{

    QString getConfigDirectory()
    {
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        configDir = configDir + "/L2Trader/strategies";
        return configDir;
    }

    StrategyConfig loadConfig(const QString& config_file_path)
    {
        QFile file(config_file_path);
        if (!file.open(QIODevice::ReadOnly))
        {
            qWarning() << "Failed to open config file:" << config_file_path;
            return StrategyConfig();
        }

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        if (!doc.isObject())
        {
            qWarning() << "Invalid JSON in config file:" << config_file_path;
            return StrategyConfig();
        }

        return StrategyConfig::fromJson(doc.object());
    }

    bool saveConfig(const StrategyConfig& config, const QString& config_file_path)
    {
        QDir configDir(getConfigDirectory());
        if (!configDir.exists())
        {
            if (!configDir.mkpath("."))
            {
                qWarning() << "Failed to create config directory:" << getConfigDirectory();
                return false;
            }
        }

        QFile file(config_file_path);
        if (!file.open(QIODevice::WriteOnly))
        {
            qWarning() << "Failed to open config file for writing:" << config_file_path;
            return false;
        }

        QJsonDocument doc(config.toJson());
        file.write(doc.toJson());
        file.close();

        qInfo() << "Saved config:" << config_file_path;
        return true;
    }

    QStringList discoverConfigs()
    {
        QString configDirPath = getConfigDirectory();
        QDir configDir(configDirPath);

        if (!configDir.exists())
        {
            qInfo() << "Config directory does not exist yet:" << configDirPath;
            return QStringList();
        }

        QStringList filters;
        filters << "*.json";
        configDir.setNameFilters(filters);

        QStringList configs;
        for (const auto& filename: configDir.entryList(QDir::Files))
        {
            configs.append(configDir.absoluteFilePath(filename));
        }

        return configs;
    }

} // namespace StrategyConfigLoader
