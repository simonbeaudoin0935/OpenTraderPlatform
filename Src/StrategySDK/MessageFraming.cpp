#include "OpenTraderPlatform/StrategySDK/MessageFraming.h"

#include <algorithm>
#include <limits>

#include <google/protobuf/message_lite.h>

namespace OpenTraderPlatform::StrategySDK
{
    std::vector<std::uint8_t> serializeFramedMessage(const google::protobuf::MessageLite& p_message)
    {
        const std::size_t payloadSize = p_message.ByteSizeLong();
        if (payloadSize > kMaxFramePayloadSize)
        {
            return {};
        }

        std::vector<std::uint8_t> frame(kFramePrefixSize + payloadSize);
        const auto framePrefix = encodeFrameSize(static_cast<std::uint32_t>(payloadSize));
        std::copy(framePrefix.begin(), framePrefix.end(), frame.begin());

        if (!p_message.SerializeToArray(frame.data() + kFramePrefixSize, static_cast<int>(payloadSize)))
        {
            return {};
        }

        return frame;
    }

    bool parseMessagePayload(const std::span<const std::uint8_t> p_payload, google::protobuf::MessageLite* p_message)
    {
        if (p_message == nullptr)
        {
            return false;
        }

        if (p_payload.size() > kMaxFramePayloadSize)
        {
            return false;
        }

        return p_message->ParseFromArray(p_payload.data(), static_cast<int>(p_payload.size()));
    }
} // namespace OpenTraderPlatform::StrategySDK
