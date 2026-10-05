#pragma once

#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>
#include <expected>
#include <map>

enum class ExternalStrategyFieldType
{
    String,
    Int,
    Double,
    Bool,
};

struct ExternalStrategyFieldDefinition
{
    QString key;
    ExternalStrategyFieldType type = ExternalStrategyFieldType::String;
    QString label;
    QString description;
    QJsonValue defaultValue;
    bool required = false;
};

struct ExternalStrategyDescriptionData
{
    QString name;
    QString version;
    QString executablePath;
    QVector<ExternalStrategyFieldDefinition> parameterSchema;
};

class ExternalStrategyDescription final
{
  public:
    [[nodiscard]] static std::expected<ExternalStrategyDescriptionData, QString>
    describeExecutable(const QString& p_executablePath);

    [[nodiscard]] static QStringList validateFieldValues(const ExternalStrategyDescriptionData& p_description,
                                                         const std::map<QString, QJsonValue>& p_fieldValues);
};
