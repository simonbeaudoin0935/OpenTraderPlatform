#include "L2Trader/StrategySDK/StrategyProcessMain.h"

#include <iostream>
#include <string_view>

#include "L2Trader/StrategySDK/ExternalStrategyRuntime.h"

namespace
{
    constexpr std::string_view kDescribeStrategyArg = "--describe-strategy";
}

namespace L2Trader::StrategySDK
{
    int runStrategyProcessMain(const int p_argc,
                               char** const p_argv,
                               const StrategyDescription& p_description,
                               ExternalStrategyHandler& p_handler)
    {
        if (const auto error = validateStrategyDescription(p_description); error.has_value())
        {
            std::cerr << "[StrategySDK] Invalid strategy description: " << *error << std::endl;
            return 2;
        }

        if (p_argc == 2 && p_argv != nullptr && p_argv[1] != nullptr &&
            std::string_view(p_argv[1]) == kDescribeStrategyArg)
        {
            std::cout << serializeStrategyDescriptionJson(p_description) << std::endl;
            return 0;
        }

        if (p_argc != 1)
        {
            std::cerr << "[StrategySDK] Unsupported arguments. Use --describe-strategy for metadata output."
                      << std::endl;
            return 2;
        }

        ExternalStrategyRuntime runtime(p_description, p_handler);
        p_handler.bindRuntime(&runtime);
        return runtime.run();
    }
} // namespace L2Trader::StrategySDK
