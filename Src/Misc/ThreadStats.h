#pragma once

#include <QString>

/**
 * @brief ThreadStats - Utility to read thread CPU and memory usage from /proc filesystem
 *
 * Linux-specific: reads from /proc/[tid]/stat
 */
class ThreadStats
{
  public:
    struct Stats
    {
        double cpuUsagePercent; // CPU usage percentage (0-100+)
        qint64 memoryBytes;     // Memory usage in bytes
        bool valid;             // Whether stats were successfully read
    };

    /**
     * @brief Get CPU and memory stats for a thread
     * @param p_threadId Thread ID (native thread ID from QThread::nativeId())
     * @return Stats struct with CPU% and memory bytes, valid=false if read failed
     */
    [[nodiscard]] static Stats getThreadStats(qint64 p_threadId);

  private:
    ThreadStats() = default;

    [[nodiscard]] static double calculateCpuUsage(qint64 p_utime, qint64 p_stime);
};
