#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace L2Trader::StrategySDK
{
    inline constexpr std::string_view kStrategyIdEnvVar = "L2TRADER_STRATEGY_ID";
    inline constexpr std::string_view kStrategySocketEnvVar = "L2TRADER_STRATEGY_SOCKET";
    inline constexpr std::string_view kStrategySdkName = "L2TraderStrategySDK";
    inline constexpr std::string_view kStrategySdkVersion = "1.0.0-alpha1";

    struct RuntimeEnvironment
    {
        std::string strategyId;
        std::string socketPath;

        [[nodiscard]] bool isValid() const;
    };

    [[nodiscard]] std::optional<std::string> readEnvironmentValue(std::string_view p_name);
    [[nodiscard]] std::optional<RuntimeEnvironment> loadRuntimeEnvironment();
} // namespace L2Trader::StrategySDK
