#include "L2Trader/StrategySDK/RuntimeEnvironment.h"

#include <cstdlib>

namespace L2Trader::StrategySDK
{
    bool RuntimeEnvironment::isValid() const
    {
        return !strategyId.empty() && !socketPath.empty();
    }

    std::optional<std::string> readEnvironmentValue(const std::string_view p_name)
    {
        const std::string envName(p_name);
        const char* const value = std::getenv(envName.c_str());
        if (value == nullptr || value[0] == '\0')
        {
            return std::nullopt;
        }

        return std::string(value);
    }

    std::optional<RuntimeEnvironment> loadRuntimeEnvironment()
    {
        const auto strategyId = readEnvironmentValue(kStrategyIdEnvVar);
        const auto socketPath = readEnvironmentValue(kStrategySocketEnvVar);

        if (!strategyId.has_value() || !socketPath.has_value())
        {
            return std::nullopt;
        }

        RuntimeEnvironment runtimeEnvironment{*strategyId, *socketPath};
        if (!runtimeEnvironment.isValid())
        {
            return std::nullopt;
        }

        return runtimeEnvironment;
    }
} // namespace L2Trader::StrategySDK
