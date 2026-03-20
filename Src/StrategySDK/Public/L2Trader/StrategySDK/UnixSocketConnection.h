#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace google::protobuf
{
    class MessageLite;
}

namespace L2Trader::StrategySDK
{
    class UnixSocketConnection
    {
      public:
        enum class WaitStatus
        {
            Ready,
            Timeout,
            Error,
        };

        UnixSocketConnection() = default;
        ~UnixSocketConnection();

        UnixSocketConnection(const UnixSocketConnection&) = delete;
        UnixSocketConnection& operator=(const UnixSocketConnection&) = delete;
        UnixSocketConnection(UnixSocketConnection&& p_other) noexcept;
        UnixSocketConnection& operator=(UnixSocketConnection&& p_other) noexcept;

        [[nodiscard]] bool connectTo(const std::string& p_socketPath);
        void close();

        [[nodiscard]] bool isOpen() const
        {
            return m_fd >= 0;
        }

        [[nodiscard]] WaitStatus waitForReadable(int p_timeoutMs) const;
        [[nodiscard]] bool writeMessage(const google::protobuf::MessageLite& p_message) const;
        [[nodiscard]] bool readFrame(std::vector<std::uint8_t>* p_payload) const;

      private:
        [[nodiscard]] bool writeAll(std::span<const std::uint8_t> p_buffer) const;
        [[nodiscard]] bool readAll(std::span<std::uint8_t> p_buffer) const;

        int m_fd = -1;
    };
} // namespace L2Trader::StrategySDK
