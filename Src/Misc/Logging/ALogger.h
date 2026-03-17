#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>

// Asynchronous logger: producers enqueue formatted log bytes into a circular
// buffer and return immediately. A dedicated consumer thread drains the buffer
// to the underlying file descriptor either when the fill level crosses
// FLUSH_THRESHOLD_BYTES or when FLUSH_TIMEOUT_MS elapses — whichever comes first.
//
// Buffer sizes and timing constants are defined in CONSTANTS.h (namespace ALogger).
//
// Two instances are used in practice:
//   - file logger  : drains to the on-disk .log file
//   - stdout logger: drains to STDOUT_FILENO (GUI) or STDERR_FILENO (TUI)
class ALogger
{
  public:
    explicit ALogger(int fd, const char* name);
    ~ALogger(); // calls shutdown() and joins the consumer thread

    ALogger(const ALogger&) = delete;
    ALogger& operator=(const ALogger&) = delete;

    // Producer: enqueue bytes into the circular buffer. Fast — acquires the
    // internal mutex only long enough for a memcpy. If the buffer is full the
    // message is silently dropped and the lost-log counter is incremented.
    void write(const void* data, size_t length);

    // Synchronous drain — call from the crash handler where the async thread
    // cannot be trusted. Flushes all pending bytes directly to the fd.
    void syncFlush();

    // Best-effort drain for use from signal/crash handlers.
    // Spins on try_lock() up to CRASH_FLUSH_RETRIES times, sleeping
    // CRASH_FLUSH_RETRY_DELAY_NS between each attempt via nanosleep() (which is
    // async-signal-safe). If the lock is acquired the buffer is fully drained
    // and fsync'd, then released. If all retries are exhausted the drain is
    // skipped — this method never blocks indefinitely.
    void crashFlush();

    // Signal the consumer thread to drain remaining data and exit, then join it.
    // Safe to call multiple times; subsequent calls are no-ops.
    void shutdown();

    [[nodiscard]] size_t highwaterMark() const;
    [[nodiscard]] size_t lostLogs() const;

  private:
    void threadFunc();

    // Write a contiguous byte range to m_fd, retrying on EINTR / short writes.
    void writeToFd(const uint8_t* data, size_t length);

    int m_fd;
    const char* m_name;

    std::unique_ptr<uint8_t[]> m_buffer;
    size_t m_bufferSize;

    // Shared state between producer and consumer — always accessed under m_mutex.
    size_t m_head;  // next write position (producer advances)
    size_t m_tail;  // next read position  (only consumer advances)
    size_t m_count; // bytes currently in the buffer
    size_t m_highwaterMark;
    size_t m_lostLogs;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_shutdown{false};

    std::thread m_thread;
};
