#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace OpenTraderPlatform::StrategySDK
{
    enum class StrategyFieldType
    {
        String,
        Int,
        Double,
        Bool,
    };

    using StrategyFieldDefaultValue = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

    struct StrategyFieldDefinition
    {
        std::string key;
        StrategyFieldType type = StrategyFieldType::String;
        std::string label;
        std::string description;
        StrategyFieldDefaultValue defaultValue;
        bool required = false;
    };

    struct StrategyDescription
    {
        std::string name;
        std::string version;
        std::vector<StrategyFieldDefinition> parameterSchema;
    };

    [[nodiscard]] StrategyFieldDefinition stringField(std::string p_key,
                                                      std::string p_label,
                                                      std::string p_defaultValue = {},
                                                      std::string p_description = {},
                                                      bool p_required = false);

    [[nodiscard]] StrategyFieldDefinition intField(std::string p_key,
                                                   std::string p_label,
                                                   std::int64_t p_defaultValue = 0,
                                                   std::string p_description = {},
                                                   bool p_required = false);

    [[nodiscard]] StrategyFieldDefinition doubleField(std::string p_key,
                                                      std::string p_label,
                                                      double p_defaultValue = 0.0,
                                                      std::string p_description = {},
                                                      bool p_required = false);

    [[nodiscard]] StrategyFieldDefinition boolField(std::string p_key,
                                                    std::string p_label,
                                                    bool p_defaultValue = false,
                                                    std::string p_description = {},
                                                    bool p_required = false);

    [[nodiscard]] std::optional<std::string> validateStrategyDescription(const StrategyDescription& p_description);

    [[nodiscard]] std::string serializeStrategyDescriptionJson(const StrategyDescription& p_description);
} // namespace OpenTraderPlatform::StrategySDK
