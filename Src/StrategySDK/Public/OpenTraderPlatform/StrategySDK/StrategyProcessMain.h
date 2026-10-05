#pragma once

#include "OpenTraderPlatform/StrategySDK/StrategyDescription.h"

namespace OpenTraderPlatform::StrategySDK
{
    class ExternalStrategyHandler;

    [[nodiscard]] int
    runStrategyProcessMain(int p_argc, char** p_argv, const StrategyDescription& p_description, ExternalStrategyHandler& p_handler);
} // namespace OpenTraderPlatform::StrategySDK
