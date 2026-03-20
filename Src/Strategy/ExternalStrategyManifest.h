#pragma once

#include <QJsonArray>
#include <QString>
#include <expected>
#include <optional>

#include "StrategyConfig.h"

struct ExternalStrategyManifestData
{
    StrategyConfig config;
    QJsonArray parameterSchema;
    QString manifestPath;
};

class ExternalStrategyManifest final
{
  public:
    [[nodiscard]] static std::expected<ExternalStrategyManifestData, QString>
    loadFromFile(const QString& p_manifestPath);

    [[nodiscard]] static std::optional<ExternalStrategyManifestData> findForRuntime(const QString& p_runtimePath);
};
