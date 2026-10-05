#pragma once

#include <cstdint>
#include <string_view>

#include "OpenTraderPlatform/StrategyProtocol/GeneratedProtocol.h"

namespace OpenTraderPlatform::StrategyProtocol
{
    inline constexpr std::uint32_t kProtocolMajor = 1;
    inline constexpr std::uint32_t kProtocolMinor = 1;
    inline constexpr std::uint32_t kProtocolPatch = 0;
    inline constexpr std::string_view kProtocolPrerelease = "alpha1";
    inline constexpr std::string_view kProtocolVersionString = "1.1.0-alpha1";

    void populateProtocolVersion(opentraderplatform::strategy::v1::ProtocolVersion* p_version);
    [[nodiscard]] bool isCompatibleProtocol(const opentraderplatform::strategy::v1::ProtocolVersion& p_version);
} // namespace OpenTraderPlatform::StrategyProtocol
