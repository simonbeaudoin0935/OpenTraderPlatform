#include "StrategyRegistry.h"
#include "StrategyConfigLoader.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>

StrategyRegistry::StrategyRegistry()
{
    refresh();
}

void StrategyRegistry::refresh()
{
    m_strategies.clear();
    m_nameIndex.clear();
    m_pathIndex.clear();

    QStringList configPaths = StrategyConfigLoader::discoverConfigs();

    for (const auto& configPath: configPaths)
    {
        QFileInfo fileInfo(configPath);
        QString configName = fileInfo.fileName();

        StrategyMetadata metadata;
        metadata.configPath = configPath;
        metadata.configName = configName;

        StrategyConfig config = StrategyConfigLoader::loadConfig(configPath);
        if (config.name.isEmpty())
        {
            metadata.isValid = false;
            metadata.loadError = "Failed to load config file";
            qWarning() << "Failed to load strategy config:" << configPath;
        }
        else
        {
            metadata.isValid = true;
            metadata.config = config;
            qInfo() << "Discovered strategy:" << config.name << "from" << configPath;
        }

        int index = m_strategies.size();
        m_strategies.append(metadata);
        m_nameIndex[configName] = index;
        m_pathIndex[configPath] = index;
    }

    qInfo() << "Strategy registry refresh complete. Found" << m_strategies.size() << "strategies.";
}

QVector<StrategyMetadata> StrategyRegistry::getAvailableStrategies() const
{
    return m_strategies;
}

const StrategyMetadata* StrategyRegistry::findStrategyByName(const QString& configName) const
{
    auto it = m_nameIndex.find(configName);
    if (it != m_nameIndex.end())
    {
        return &m_strategies[it.value()];
    }
    return nullptr;
}

const StrategyMetadata* StrategyRegistry::findStrategyByPath(const QString& configPath) const
{
    auto it = m_pathIndex.find(configPath);
    if (it != m_pathIndex.end())
    {
        return &m_strategies[it.value()];
    }
    return nullptr;
}

bool StrategyRegistry::isStrategyValid(const QString& configName) const
{
    const auto* metadata = findStrategyByName(configName);
    return metadata && metadata->isValid;
}

QString StrategyRegistry::getStrategyError(const QString& configName) const
{
    const auto* metadata = findStrategyByName(configName);
    return metadata ? metadata->loadError : "Strategy not found";
}
