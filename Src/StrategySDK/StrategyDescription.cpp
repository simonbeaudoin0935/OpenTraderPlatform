#include "L2Trader/StrategySDK/StrategyDescription.h"

#include <iomanip>
#include <optional>
#include <sstream>
#include <type_traits>
#include <unordered_set>

namespace
{
    using namespace L2Trader::StrategySDK;

    [[nodiscard]] std::string_view fieldTypeToString(const StrategyFieldType p_type)
    {
        switch (p_type)
        {
        case StrategyFieldType::String:
            return "string";
        case StrategyFieldType::Int:
            return "int";
        case StrategyFieldType::Double:
            return "double";
        case StrategyFieldType::Bool:
            return "bool";
        }

        return "string";
    }

    void appendEscapedJsonString(const std::string_view p_value, std::string* const p_output)
    {
        if (p_output == nullptr)
        {
            return;
        }

        p_output->push_back('"');
        for (const char ch: p_value)
        {
            switch (ch)
            {
            case '\\':
                p_output->append("\\\\");
                break;
            case '"':
                p_output->append("\\\"");
                break;
            case '\b':
                p_output->append("\\b");
                break;
            case '\f':
                p_output->append("\\f");
                break;
            case '\n':
                p_output->append("\\n");
                break;
            case '\r':
                p_output->append("\\r");
                break;
            case '\t':
                p_output->append("\\t");
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20U)
                {
                    std::ostringstream escapeStream;
                    escapeStream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                                 << static_cast<int>(static_cast<unsigned char>(ch));
                    p_output->append(escapeStream.str());
                }
                else
                {
                    p_output->push_back(ch);
                }
                break;
            }
        }
        p_output->push_back('"');
    }

    void appendDefaultValue(const StrategyFieldDefaultValue& p_value, std::string* const p_output)
    {
        if (p_output == nullptr)
        {
            return;
        }

        std::visit(
            [p_output](const auto& p_default)
            {
                using ValueType = std::decay_t<decltype(p_default)>;
                if constexpr (std::is_same_v<ValueType, std::monostate>)
                {
                    p_output->append("null");
                }
                else if constexpr (std::is_same_v<ValueType, bool>)
                {
                    p_output->append(p_default ? "true" : "false");
                }
                else if constexpr (std::is_same_v<ValueType, std::int64_t>)
                {
                    p_output->append(std::to_string(p_default));
                }
                else if constexpr (std::is_same_v<ValueType, double>)
                {
                    std::ostringstream stream;
                    stream << p_default;
                    p_output->append(stream.str());
                }
                else if constexpr (std::is_same_v<ValueType, std::string>)
                {
                    appendEscapedJsonString(p_default, p_output);
                }
            },
            p_value);
    }

    [[nodiscard]] bool defaultMatchesType(const StrategyFieldDefinition& p_field)
    {
        if (std::holds_alternative<std::monostate>(p_field.defaultValue))
        {
            return true;
        }

        switch (p_field.type)
        {
        case StrategyFieldType::String:
            return std::holds_alternative<std::string>(p_field.defaultValue);
        case StrategyFieldType::Int:
            return std::holds_alternative<std::int64_t>(p_field.defaultValue);
        case StrategyFieldType::Double:
            return std::holds_alternative<double>(p_field.defaultValue);
        case StrategyFieldType::Bool:
            return std::holds_alternative<bool>(p_field.defaultValue);
        }

        return false;
    }
} // namespace

namespace L2Trader::StrategySDK
{
    StrategyFieldDefinition stringField(std::string p_key,
                                        std::string p_label,
                                        std::string p_defaultValue,
                                        std::string p_description,
                                        const bool p_required)
    {
        return StrategyFieldDefinition{
            .key = std::move(p_key),
            .type = StrategyFieldType::String,
            .label = std::move(p_label),
            .description = std::move(p_description),
            .defaultValue = std::move(p_defaultValue),
            .required = p_required,
        };
    }

    StrategyFieldDefinition intField(std::string p_key,
                                     std::string p_label,
                                     const std::int64_t p_defaultValue,
                                     std::string p_description,
                                     const bool p_required)
    {
        return StrategyFieldDefinition{
            .key = std::move(p_key),
            .type = StrategyFieldType::Int,
            .label = std::move(p_label),
            .description = std::move(p_description),
            .defaultValue = p_defaultValue,
            .required = p_required,
        };
    }

    StrategyFieldDefinition doubleField(std::string p_key,
                                        std::string p_label,
                                        const double p_defaultValue,
                                        std::string p_description,
                                        const bool p_required)
    {
        return StrategyFieldDefinition{
            .key = std::move(p_key),
            .type = StrategyFieldType::Double,
            .label = std::move(p_label),
            .description = std::move(p_description),
            .defaultValue = p_defaultValue,
            .required = p_required,
        };
    }

    StrategyFieldDefinition boolField(std::string p_key,
                                      std::string p_label,
                                      const bool p_defaultValue,
                                      std::string p_description,
                                      const bool p_required)
    {
        return StrategyFieldDefinition{
            .key = std::move(p_key),
            .type = StrategyFieldType::Bool,
            .label = std::move(p_label),
            .description = std::move(p_description),
            .defaultValue = p_defaultValue,
            .required = p_required,
        };
    }

    std::optional<std::string> validateStrategyDescription(const StrategyDescription& p_description)
    {
        if (p_description.name.empty())
        {
            return "Strategy description name is empty";
        }

        if (p_description.version.empty())
        {
            return "Strategy description version is empty";
        }

        std::unordered_set<std::string> seenKeys;
        for (const StrategyFieldDefinition& field: p_description.parameterSchema)
        {
            if (field.key.empty())
            {
                return "Strategy description contains a field with an empty key";
            }

            if (!seenKeys.insert(field.key).second)
            {
                return "Strategy description contains duplicate field key: " + field.key;
            }

            if (!defaultMatchesType(field))
            {
                return "Strategy description default value type does not match field type for key: " + field.key;
            }
        }

        return std::nullopt;
    }

    std::string serializeStrategyDescriptionJson(const StrategyDescription& p_description)
    {
        std::string json;
        json.reserve(256U + (p_description.parameterSchema.size() * 128U));
        json.push_back('{');
        json.append("\"name\":");
        appendEscapedJsonString(p_description.name, &json);
        json.append(",\"version\":");
        appendEscapedJsonString(p_description.version, &json);
        json.append(",\"parameterSchema\":[");

        bool firstField = true;
        for (const StrategyFieldDefinition& field: p_description.parameterSchema)
        {
            if (!firstField)
            {
                json.push_back(',');
            }
            firstField = false;

            json.push_back('{');
            json.append("\"key\":");
            appendEscapedJsonString(field.key, &json);
            json.append(",\"type\":");
            appendEscapedJsonString(fieldTypeToString(field.type), &json);
            json.append(",\"label\":");
            appendEscapedJsonString(field.label.empty() ? field.key : field.label, &json);
            if (!field.description.empty())
            {
                json.append(",\"description\":");
                appendEscapedJsonString(field.description, &json);
            }
            if (!std::holds_alternative<std::monostate>(field.defaultValue))
            {
                json.append(",\"default\":");
                appendDefaultValue(field.defaultValue, &json);
            }
            if (field.required)
            {
                json.append(",\"required\":true");
            }
            json.push_back('}');
        }

        json.append("]}");
        return json;
    }
} // namespace L2Trader::StrategySDK
