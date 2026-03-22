#pragma once

#include "L2Trader/StrategySDK/StrategyDescription.h"

namespace L2Trader::StrategySDK
{
    class ExternalStrategyHandler;

    [[nodiscard]] int
    runStrategyProcessMain(int p_argc, char** p_argv, const StrategyDescription& p_description, ExternalStrategyHandler& p_handler);
} // namespace L2Trader::StrategySDK
