#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <google/protobuf/struct.pb.h>

#include "OpenTraderPlatform/StrategyProtocol/GeneratedProtocol.h"

namespace OpenTraderPlatform::StrategySDK
{
    namespace Protocol = opentraderplatform::strategy::v1;

    [[nodiscard]] inline const google::protobuf::Value*
    findConfiguredField(const Protocol::StrategyConfiguration& p_configuration, std::string_view p_key)
    {
        const auto fieldIt = p_configuration.custom_params().fields().find(std::string(p_key));
        return fieldIt == p_configuration.custom_params().fields().end() ? nullptr : &fieldIt->second;
    }

    [[nodiscard]] inline std::string
    stringFieldOr(const Protocol::StrategyConfiguration& p_configuration,
                  std::string_view p_key,
                  std::string_view p_fallback)
    {
        const google::protobuf::Value* const value = findConfiguredField(p_configuration, p_key);
        if (value == nullptr || value->kind_case() != google::protobuf::Value::kStringValue)
        {
            return std::string(p_fallback);
        }

        return value->string_value();
    }

    [[nodiscard]] inline double
    doubleFieldOr(const Protocol::StrategyConfiguration& p_configuration, std::string_view p_key, double p_fallback)
    {
        const google::protobuf::Value* const value = findConfiguredField(p_configuration, p_key);
        if (value == nullptr || value->kind_case() != google::protobuf::Value::kNumberValue)
        {
            return p_fallback;
        }

        return value->number_value();
    }

    [[nodiscard]] inline std::int64_t
    intFieldOr(const Protocol::StrategyConfiguration& p_configuration, std::string_view p_key, std::int64_t p_fallback)
    {
        return static_cast<std::int64_t>(doubleFieldOr(p_configuration, p_key, static_cast<double>(p_fallback)));
    }

    [[nodiscard]] inline bool
    boolFieldOr(const Protocol::StrategyConfiguration& p_configuration, std::string_view p_key, bool p_fallback)
    {
        const google::protobuf::Value* const value = findConfiguredField(p_configuration, p_key);
        if (value == nullptr || value->kind_case() != google::protobuf::Value::kBoolValue)
        {
            return p_fallback;
        }

        return value->bool_value();
    }
} // namespace OpenTraderPlatform::StrategySDK
