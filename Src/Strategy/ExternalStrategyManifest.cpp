#include "ExternalStrategyManifest.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

namespace
{
    [[nodiscard]] QString normalizePath(const QString& p_path)
    {
        if (p_path.isEmpty())
        {
            return {};
        }

        const QFileInfo fileInfo(p_path);
        if (fileInfo.exists())
        {
            return fileInfo.canonicalFilePath();
        }

        return fileInfo.absoluteFilePath();
    }

    [[nodiscard]] bool pathsMatch(const QString& p_left, const QString& p_right)
    {
        return !p_left.isEmpty() && !p_right.isEmpty() && normalizePath(p_left) == normalizePath(p_right);
    }
} // namespace

std::expected<ExternalStrategyManifestData, QString>
ExternalStrategyManifest::loadFromFile(const QString& p_manifestPath)
{
    const QFileInfo manifestInfo(p_manifestPath);
    if (!manifestInfo.exists() || !manifestInfo.isFile())
    {
        return std::unexpected(QString("Strategy manifest does not exist: %1").arg(p_manifestPath));
    }

    QFile file(manifestInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::unexpected(QString("Failed to open strategy manifest: %1").arg(file.errorString()));
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        return std::unexpected(
            QString("Failed to parse strategy manifest %1: %2").arg(p_manifestPath, parseError.errorString()));
    }

    if (!document.isObject())
    {
        return std::unexpected(QString("Strategy manifest is not a JSON object: %1").arg(p_manifestPath));
    }

    ExternalStrategyManifestData data{
        .config = StrategyConfig::fromJson(document.object()),
        .parameterSchema = document.object().value("parameterSchema").toArray(),
        .manifestPath = manifestInfo.absoluteFilePath(),
    };

    if (!data.config.usesExternalProcess())
    {
        return std::unexpected(
            QString("Strategy manifest %1 still uses the removed shared-library plugin runtime").arg(p_manifestPath));
    }

    if (data.config.executablePath.isEmpty())
    {
        return std::unexpected(QString("Strategy manifest %1 does not declare executablePath").arg(p_manifestPath));
    }

    return data;
}

std::optional<ExternalStrategyManifestData> ExternalStrategyManifest::findForRuntime(const QString& p_runtimePath)
{
    if (p_runtimePath.isEmpty())
    {
        return std::nullopt;
    }

    const QStringList candidateDirectories = {
        QFileInfo(p_runtimePath).absolutePath(),
        QDir::homePath() + "/.config/L2Trader/Strategies",
        "/usr/share/l2trader/strategies",
    };

    QStringList visitedDirectories;
    for (const QString& directoryPath: candidateDirectories)
    {
        if (directoryPath.isEmpty() || visitedDirectories.contains(directoryPath))
        {
            continue;
        }
        visitedDirectories.append(directoryPath);

        QDir directory(directoryPath);
        if (!directory.exists())
        {
            continue;
        }

        const QFileInfoList manifestFiles = directory.entryInfoList({"*.json"}, QDir::Files | QDir::Readable);
        for (const QFileInfo& manifestFile: manifestFiles)
        {
            const auto manifestResult = loadFromFile(manifestFile.absoluteFilePath());
            if (!manifestResult.has_value())
            {
                continue;
            }

            if (pathsMatch(manifestResult->config.executablePath, p_runtimePath))
            {
                return manifestResult.value();
            }
        }
    }

    return std::nullopt;
}
