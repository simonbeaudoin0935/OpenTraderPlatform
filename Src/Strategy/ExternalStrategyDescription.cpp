#include "ExternalStrategyDescription.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <cmath>
#include <limits>

namespace
{
    constexpr int kDescribeTimeoutMs = 2000;

    [[nodiscard]] std::expected<ExternalStrategyFieldType, QString> fieldTypeFromString(const QString& p_type)
    {
        if (p_type == "string")
        {
            return ExternalStrategyFieldType::String;
        }
        if (p_type == "int")
        {
            return ExternalStrategyFieldType::Int;
        }
        if (p_type == "double")
        {
            return ExternalStrategyFieldType::Double;
        }
        if (p_type == "bool")
        {
            return ExternalStrategyFieldType::Bool;
        }

        return std::unexpected(QString("Unsupported strategy field type: %1").arg(p_type));
    }

    [[nodiscard]] bool isIntegralJsonNumber(const QJsonValue& p_value)
    {
        if (!p_value.isDouble())
        {
            return false;
        }

        const double value = p_value.toDouble();
        double integralPart = 0.0;
        const double fractionalPart = std::modf(value, &integralPart);
        return std::abs(fractionalPart) <= std::numeric_limits<double>::epsilon();
    }

    [[nodiscard]] bool isCompatibleValueType(const ExternalStrategyFieldDefinition& p_field, const QJsonValue& p_value)
    {
        if (p_value.isUndefined() || p_value.isNull())
        {
            return true;
        }

        switch (p_field.type)
        {
        case ExternalStrategyFieldType::String:
            return p_value.isString();
        case ExternalStrategyFieldType::Int:
            return isIntegralJsonNumber(p_value);
        case ExternalStrategyFieldType::Double:
            return p_value.isDouble();
        case ExternalStrategyFieldType::Bool:
            return p_value.isBool();
        }

        return false;
    }

    [[nodiscard]] std::expected<ExternalStrategyFieldDefinition, QString> parseFieldDefinition(const QJsonValue& p_value)
    {
        if (!p_value.isObject())
        {
            return std::unexpected("Strategy parameter schema entry is not a JSON object");
        }

        const QJsonObject object = p_value.toObject();
        const QString key = object.value("key").toString().trimmed();
        if (key.isEmpty())
        {
            return std::unexpected("Strategy parameter schema entry is missing a non-empty key");
        }

        const auto fieldType = fieldTypeFromString(object.value("type").toString().trimmed());
        if (!fieldType.has_value())
        {
            return std::unexpected(fieldType.error());
        }

        ExternalStrategyFieldDefinition field{
            .key = key,
            .type = fieldType.value(),
            .label = object.value("label").toString(key),
            .description = object.value("description").toString(),
            .defaultValue = object.value("default"),
            .required = object.value("required").toBool(false),
        };

        if (!isCompatibleValueType(field, field.defaultValue))
        {
            return std::unexpected(QString("Default value type does not match declared type for field: %1").arg(key));
        }

        return field;
    }
} // namespace

std::expected<ExternalStrategyDescriptionData, QString>
ExternalStrategyDescription::describeExecutable(const QString& p_executablePath)
{
    const QFileInfo executableInfo(p_executablePath);
    if (!executableInfo.exists() || !executableInfo.isFile())
    {
        return std::unexpected(QString("Strategy executable does not exist: %1").arg(p_executablePath));
    }

    if (!executableInfo.isExecutable())
    {
        return std::unexpected(QString("Strategy executable is not executable: %1").arg(p_executablePath));
    }

    QProcess process;
    process.setProgram(executableInfo.absoluteFilePath());
    process.setArguments({"--describe-strategy"});
    process.setWorkingDirectory(executableInfo.absolutePath());
    process.setProcessEnvironment(QProcessEnvironment::systemEnvironment());

    process.start();
    if (!process.waitForStarted(kDescribeTimeoutMs))
    {
        return std::unexpected(QString("Failed to start strategy description command: %1").arg(process.errorString()));
    }

    if (!process.waitForFinished(kDescribeTimeoutMs))
    {
        process.kill();
        process.waitForFinished();
        return std::unexpected(
            QString("Timed out waiting for %1 --describe-strategy").arg(executableInfo.absoluteFilePath()));
    }

    const QString stderrOutput = QString::fromUtf8(process.readAllStandardError()).trimmed();
    const QByteArray stdoutBytes = process.readAllStandardOutput();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        const QString suffix = stderrOutput.isEmpty() ? QString{} : QString("\n\nstderr:\n%1").arg(stderrOutput);
        return std::unexpected(QString("Strategy description command failed for %1%2")
                                   .arg(executableInfo.absoluteFilePath(), suffix));
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(stdoutBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        return std::unexpected(QString("Failed to parse strategy description JSON from %1: %2")
                                   .arg(executableInfo.absoluteFilePath(), parseError.errorString()));
    }

    if (!document.isObject())
    {
        return std::unexpected(QString("Strategy description for %1 is not a JSON object")
                                   .arg(executableInfo.absoluteFilePath()));
    }

    const QJsonObject object = document.object();
    ExternalStrategyDescriptionData description{
        .name = object.value("name").toString().trimmed(),
        .version = object.value("version").toString().trimmed(),
        .executablePath = executableInfo.absoluteFilePath(),
        .parameterSchema = {},
    };

    if (description.name.isEmpty())
    {
        return std::unexpected(QString("Strategy description for %1 is missing a non-empty name")
                                   .arg(executableInfo.absoluteFilePath()));
    }

    if (description.version.isEmpty())
    {
        return std::unexpected(QString("Strategy description for %1 is missing a non-empty version")
                                   .arg(executableInfo.absoluteFilePath()));
    }

    QStringList seenKeys;
    const QJsonArray schemaArray = object.value("parameterSchema").toArray();
    for (const QJsonValue& schemaEntry: schemaArray)
    {
        const auto parsedField = parseFieldDefinition(schemaEntry);
        if (!parsedField.has_value())
        {
            return std::unexpected(
                QString("Invalid parameter schema from %1: %2").arg(executableInfo.absoluteFilePath(), parsedField.error()));
        }

        if (seenKeys.contains(parsedField->key))
        {
            return std::unexpected(
                QString("Strategy description for %1 contains duplicate field key: %2")
                    .arg(executableInfo.absoluteFilePath(), parsedField->key));
        }

        seenKeys.append(parsedField->key);
        description.parameterSchema.append(parsedField.value());
    }

    return description;
}

QStringList ExternalStrategyDescription::validateFieldValues(const ExternalStrategyDescriptionData& p_description,
                                                             const std::map<QString, QJsonValue>& p_fieldValues)
{
    QStringList errors;
    QMap<QString, ExternalStrategyFieldDefinition> schemaByKey;
    for (const ExternalStrategyFieldDefinition& field: p_description.parameterSchema)
    {
        schemaByKey.insert(field.key, field);
    }

    for (const auto& [key, value]: p_fieldValues)
    {
        if (!schemaByKey.contains(key))
        {
            errors.append(QString("Stored field no longer exists: %1").arg(key));
            continue;
        }

        if (!isCompatibleValueType(schemaByKey.value(key), value))
        {
            errors.append(QString("Stored field has incompatible type: %1").arg(key));
        }
    }

    for (const ExternalStrategyFieldDefinition& field: p_description.parameterSchema)
    {
        const auto valueIt = p_fieldValues.find(field.key);
        const bool hasStoredValue = valueIt != p_fieldValues.end();
        const bool hasDefaultValue = !field.defaultValue.isUndefined() && !field.defaultValue.isNull();

        if (!hasStoredValue && field.required && !hasDefaultValue)
        {
            errors.append(QString("Missing required field without default: %1").arg(field.key));
            continue;
        }

        if (!hasStoredValue)
        {
            continue;
        }

        if (field.type == ExternalStrategyFieldType::String && valueIt->second.toString().trimmed().isEmpty() &&
            field.required && !hasDefaultValue)
        {
            errors.append(QString("Required field is empty: %1").arg(field.key));
        }
    }

    return errors;
}
