#pragma once

#include <cstdint>
#include <string_view>

#include "L2Trader/StrategyProtocol/GeneratedProtocol.h"

namespace L2Trader::StrategyProtocol
{
    inline constexpr std::uint32_t kProtocolMajor = 1;
    inline constexpr std::uint32_t kProtocolMinor = 0;
    inline constexpr std::uint32_t kProtocolPatch = 0;
    inline constexpr std::string_view kProtocolPrerelease = "alpha1";
    inline constexpr std::string_view kProtocolVersionString = "1.0.0-alpha1";

    void populateProtocolVersion(l2trader::strategy::v1::ProtocolVersion* p_version);
    [[nodiscard]] bool isCompatibleProtocol(const l2trader::strategy::v1::ProtocolVersion& p_version);
} // namespace L2Trader::StrategyProtocol
