#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace google::protobuf
{
    class MessageLite;
}

namespace L2Trader::StrategySDK
{
    inline constexpr std::size_t kFramePrefixSize = 4;

    [[nodiscard]] constexpr std::array<std::uint8_t, kFramePrefixSize> encodeFrameSize(std::uint32_t p_size)
    {
        return {
            static_cast<std::uint8_t>((p_size >> 24U) & 0xFFU),
            static_cast<std::uint8_t>((p_size >> 16U) & 0xFFU),
            static_cast<std::uint8_t>((p_size >> 8U) & 0xFFU),
            static_cast<std::uint8_t>(p_size & 0xFFU),
        };
    }

    [[nodiscard]] constexpr std::uint32_t decodeFrameSize(const std::array<std::uint8_t, kFramePrefixSize>& p_prefix)
    {
        return (static_cast<std::uint32_t>(p_prefix[0]) << 24U) | (static_cast<std::uint32_t>(p_prefix[1]) << 16U) |
               (static_cast<std::uint32_t>(p_prefix[2]) << 8U) | static_cast<std::uint32_t>(p_prefix[3]);
    }

    [[nodiscard]] std::vector<std::uint8_t> serializeFramedMessage(const google::protobuf::MessageLite& p_message);
    [[nodiscard]] bool parseMessagePayload(std::span<const std::uint8_t> p_payload,
                                           google::protobuf::MessageLite* p_message);
} // namespace L2Trader::StrategySDK
