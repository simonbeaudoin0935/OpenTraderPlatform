#pragma once

#include <QByteArray>
#include <QString>
#include <algorithm>
#include <expected>
#include <optional>

#include "OpenTraderPlatform/StrategySDK/MessageFraming.h"

namespace StrategyFrameReader
{
    [[nodiscard]] inline std::expected<std::optional<std::vector<std::uint8_t>>, QString>
    takeFrame(QByteArray& p_buffer)
    {
        namespace SDK = OpenTraderPlatform::StrategySDK;
        if (p_buffer.size() < static_cast<qsizetype>(SDK::kFramePrefixSize))
            return std::nullopt;
        std::array<std::uint8_t, SDK::kFramePrefixSize> prefix{};
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(p_buffer.constData());
        std::copy_n(bytes, prefix.size(), prefix.begin());
        const auto length = SDK::decodeFrameSize(prefix);
        if (length > SDK::kMaxFramePayloadSize)
            return std::unexpected("Strategy frame exceeds the 16 MiB payload limit");
        const qsizetype totalSize = static_cast<qsizetype>(SDK::kFramePrefixSize + length);
        if (p_buffer.size() < totalSize)
            return std::nullopt;
        std::vector<std::uint8_t> payload(bytes + SDK::kFramePrefixSize, bytes + totalSize);
        p_buffer.remove(0, totalSize);
        return payload;
    }
} // namespace StrategyFrameReader
