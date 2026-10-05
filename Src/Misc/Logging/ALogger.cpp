#include "ALogger.h"
#include "CONSTANTS.h"

#include <cstring>
#include <cerrno>
#include <cstdio>
#include <unistd.h>
#include <pthread.h>

#include <chrono>

ALogger::ALogger(int fd, const char* name)
    : m_fd(fd)
    , m_name(name)
    , m_buffer(std::make_unique<uint8_t[]>(AsyncLogger::BUFFER_SIZE_BYTES))
    , m_bufferSize(AsyncLogger::BUFFER_SIZE_BYTES)
    , m_head(0)
    , m_tail(0)
    , m_count(0)
    , m_highwaterMark(0)
    , m_lostLogs(0)
    , m_thread(&ALogger::threadFunc, this)
{
    pthread_setname_np(m_thread.native_handle(), m_name);
}

ALogger::~ALogger()
{
    shutdown();
}

void ALogger::write(const void* data, size_t length)
{
    if (length == 0)
        return;

    bool signal = false;
    size_t countAfter = 0;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_bufferSize - m_count < length)
        {
            // Buffer full — drop and count the loss
            m_lostLogs++;
            fprintf(stderr, "ALogger [%s]: buffer full, log dropped.\n", m_name);
        }
        else
        {
            const size_t remaining = m_bufferSize - m_head;

            if (remaining >= length)
            {
                std::memcpy(&m_buffer[m_head], data, length);
                m_head += length;
                if (m_head == m_bufferSize)
                    m_head = 0;
            }
            else
            {
                // Split write across the ring-buffer boundary
                std::memcpy(&m_buffer[m_head], data, remaining);
                std::memcpy(&m_buffer[0], static_cast<const uint8_t*>(data) + remaining, length - remaining);
                m_head = length - remaining;
            }

            m_count += length;

            if (m_count > m_highwaterMark)
                m_highwaterMark = m_count;

            countAfter = m_count;
            signal = true;
        }
    }

    // Wake the consumer only once enough data has accumulated — it will also
    // wake periodically on its own timeout for low-volume log streams.
    if (signal && countAfter >= AsyncLogger::FLUSH_THRESHOLD_BYTES)
        m_cv.notify_one();
}

void ALogger::syncFlush()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_count == 0)
        return;

    const size_t remaining = m_bufferSize - m_tail;

    if (remaining >= m_count)
    {
        writeToFd(&m_buffer[m_tail], m_count);
        m_tail += m_count;
        if (m_tail == m_bufferSize)
            m_tail = 0;
    }
    else
    {
        writeToFd(&m_buffer[m_tail], remaining);
        writeToFd(&m_buffer[0], m_count - remaining);
        m_tail = m_count - remaining;
    }

    m_count = 0;
    fsync(m_fd);
}

void ALogger::crashFlush()
{
    struct timespec ts
    {
        0, AsyncLogger::CRASH_FLUSH_RETRY_DELAY_NS
    };

    for (int i = 0; i < AsyncLogger::CRASH_FLUSH_RETRIES; ++i)
    {
        if (m_mutex.try_lock())
        {
            if (m_count > 0)
            {
                const size_t remaining = m_bufferSize - m_tail;

                if (remaining >= m_count)
                {
                    writeToFd(&m_buffer[m_tail], m_count);
                    m_tail += m_count;
                    if (m_tail == m_bufferSize)
                        m_tail = 0;
                }
                else
                {
                    writeToFd(&m_buffer[m_tail], remaining);
                    writeToFd(&m_buffer[0], m_count - remaining);
                    m_tail = m_count - remaining;
                }

                m_count = 0;
                fsync(m_fd);
            }

            m_mutex.unlock();
            return;
        }

        nanosleep(&ts, nullptr);
    }
    // Lock not acquired after all retries — skip drain to avoid deadlock.
}

void ALogger::shutdown()
{
    if (m_shutdown.exchange(true))
        return; // already shutting down

    m_cv.notify_all();

    if (m_thread.joinable())
        m_thread.join();
}

size_t ALogger::highwaterMark() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_highwaterMark;
}

size_t ALogger::lostLogs() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lostLogs;
}

void ALogger::writeToFd(const uint8_t* data, size_t length)
{
    size_t written = 0;
    while (written < length)
    {
        ssize_t ret = ::write(m_fd, data + written, length - written);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "ALogger [%s]: write() failed: %s\n", m_name, strerror(errno));
            return;
        }
        written += static_cast<size_t>(ret);
    }
}

void ALogger::threadFunc()
{
    while (true)
    {
        size_t chunkSize = 0;

        {
            std::unique_lock<std::mutex> lock(m_mutex);

            // Wait until threshold is reached, timeout fires, or shutdown is requested.
            m_cv.wait_for(lock,
                          std::chrono::milliseconds(AsyncLogger::FLUSH_TIMEOUT_MS),
                          [this] { return m_count >= AsyncLogger::FLUSH_THRESHOLD_BYTES || m_shutdown.load(); });

            chunkSize = m_count;
        }

        if (chunkSize == 0)
        {
            if (m_shutdown.load())
            {
                // Drain completed and shutdown requested — print stats and exit.
                char buf[256];
                int n = snprintf(buf,
                                 sizeof(buf),
                                 "ALogger [%s]: buffer=%zu bytes, highwater=%zu (%.1f%%), lost=%zu\n",
                                 m_name,
                                 m_bufferSize,
                                 m_highwaterMark,
                                 100.0 * static_cast<double>(m_highwaterMark) / static_cast<double>(m_bufferSize),
                                 m_lostLogs);
                if (n > 0)
                    writeToFd(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(n));

                fsync(m_fd);
                return;
            }
            // Timed out with nothing pending — go back to waiting.
            continue;
        }

        // Write the snapshot chunk. The tail is only ever advanced by this thread,
        // so we can read m_tail and compute the write range outside the lock.
        // Producers cannot overwrite the region we are about to read because
        // m_count (still reflecting the full chunk) blocks them from reusing it.
        size_t tail;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            tail = m_tail;
        }

        const size_t remaining = m_bufferSize - tail;

        if (remaining >= chunkSize)
        {
            writeToFd(&m_buffer[tail], chunkSize);
        }
        else
        {
            writeToFd(&m_buffer[tail], remaining);
            writeToFd(&m_buffer[0], chunkSize - remaining);
        }

        // Advance tail and release the consumed region.
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (remaining >= chunkSize)
            {
                m_tail += chunkSize;
                if (m_tail == m_bufferSize)
                    m_tail = 0;
            }
            else
            {
                m_tail = chunkSize - remaining;
            }

            m_count -= chunkSize;
        }
    }
}
