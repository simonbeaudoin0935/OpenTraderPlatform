#include "OpenTraderPlatform/StrategySDK/UnixSocketConnection.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <iostream>
#include <utility>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <google/protobuf/message_lite.h>

#include "OpenTraderPlatform/StrategySDK/MessageFraming.h"

namespace OpenTraderPlatform::StrategySDK
{
    UnixSocketConnection::~UnixSocketConnection()
    {
        close();
    }

    UnixSocketConnection::UnixSocketConnection(UnixSocketConnection&& p_other) noexcept
        : m_fd(std::exchange(p_other.m_fd, -1))
    {
    }

    UnixSocketConnection& UnixSocketConnection::operator=(UnixSocketConnection&& p_other) noexcept
    {
        if (this == &p_other)
        {
            return *this;
        }

        close();
        m_fd = std::exchange(p_other.m_fd, -1);
        return *this;
    }

    bool UnixSocketConnection::connectTo(const std::string& p_socketPath)
    {
        close();

        if (p_socketPath.empty() || p_socketPath.size() >= sizeof(sockaddr_un::sun_path))
        {
            return false;
        }

        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0)
        {
            return false;
        }

        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::strncpy(address.sun_path, p_socketPath.c_str(), sizeof(address.sun_path) - 1);

        if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
        {
            ::close(fd);
            return false;
        }

        m_fd = fd;
        return true;
    }

    void UnixSocketConnection::close()
    {
        if (!isOpen())
        {
            return;
        }

        ::close(m_fd);
        m_fd = -1;
    }

    UnixSocketConnection::WaitStatus UnixSocketConnection::waitForReadable(const int p_timeoutMs) const
    {
        if (!isOpen())
        {
            return WaitStatus::Error;
        }

        pollfd descriptor{};
        descriptor.fd = m_fd;
        descriptor.events = POLLIN;

        while (true)
        {
            const int result = ::poll(&descriptor, 1, p_timeoutMs);
            if (result > 0)
            {
                return WaitStatus::Ready;
            }

            if (result == 0)
            {
                return WaitStatus::Timeout;
            }

            if (errno == EINTR)
            {
                continue;
            }

            return WaitStatus::Error;
        }
    }

    bool UnixSocketConnection::writeMessage(const google::protobuf::MessageLite& p_message) const
    {
        const std::vector<std::uint8_t> frame = serializeFramedMessage(p_message);
        if (frame.empty())
        {
            return false;
        }

        return writeAll(frame);
    }

    bool UnixSocketConnection::readFrame(std::vector<std::uint8_t>* p_payload) const
    {
        if (p_payload == nullptr || !isOpen())
        {
            return false;
        }

        std::array<std::uint8_t, kFramePrefixSize> framePrefix{};
        if (!readAll(framePrefix))
        {
            return false;
        }

        const std::uint32_t payloadSize = decodeFrameSize(framePrefix);
        if (payloadSize > kMaxFramePayloadSize)
        {
            std::cerr << "[UnixSocketConnection] Frame exceeds the 16 MiB payload limit." << std::endl;
            return false;
        }
        p_payload->assign(payloadSize, std::uint8_t{0});
        if (payloadSize == 0)
        {
            return true;
        }

        return readAll(*p_payload);
    }

    bool UnixSocketConnection::writeAll(const std::span<const std::uint8_t> p_buffer) const
    {
        if (!isOpen())
        {
            return false;
        }

        std::size_t bytesWritten = 0;
        while (bytesWritten < p_buffer.size())
        {
            const ssize_t result = ::write(m_fd, p_buffer.data() + bytesWritten, p_buffer.size() - bytesWritten);

            if (result < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                return false;
            }

            if (result == 0)
            {
                return false;
            }

            bytesWritten += static_cast<std::size_t>(result);
        }

        return true;
    }

    bool UnixSocketConnection::readAll(const std::span<std::uint8_t> p_buffer) const
    {
        if (!isOpen())
        {
            return false;
        }

        std::size_t bytesRead = 0;
        while (bytesRead < p_buffer.size())
        {
            const ssize_t result = ::read(m_fd, p_buffer.data() + bytesRead, p_buffer.size() - bytesRead);

            if (result < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                return false;
            }

            if (result == 0)
            {
                return false;
            }

            bytesRead += static_cast<std::size_t>(result);
        }

        return true;
    }
} // namespace OpenTraderPlatform::StrategySDK
