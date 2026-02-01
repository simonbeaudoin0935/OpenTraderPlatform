#include "ThreadStats.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

ThreadStats::Stats ThreadStats::getThreadStats(qint64 p_threadId)
{
    Stats stats{0.0, 0, false};

    if (p_threadId <= 0)
    {
        return stats;
    }

    // Read from /proc/[tid]/stat
    QString statPath = QString("/proc/%1/stat").arg(p_threadId);
    QFile file(statPath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return stats;
    }

    QString line = file.readAll();
    file.close();

    // Parse /proc/[tid]/stat
    // Format: pid (comm) state ppid pgrp session tty_nr tpgid flags minflt cminflt majflt cmajflt
    //         utime stime cutime cstime priority nice num_threads itrealvalue starttime vsize rss
    // We need: utime (field 14), stime (field 15)

    QStringList fields = line.split(" ");
    if (fields.size() < 15)
    {
        return stats;
    }

    bool ok_utime, ok_stime;
    qint64 utime = fields.at(13).toLongLong(&ok_utime); // index 13 = field 14
    qint64 stime = fields.at(14).toLongLong(&ok_stime); // index 14 = field 15

    if (!ok_utime || !ok_stime)
    {
        return stats;
    }

    // Calculate CPU usage (approximation based on jiffies)
    stats.cpuUsagePercent = calculateCpuUsage(utime, stime);

    // Read memory from /proc/[tid]/status
    QString statusPath = QString("/proc/%1/status").arg(p_threadId);
    QFile statusFile(statusPath);

    if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream stream(&statusFile);
        while (!stream.atEnd())
        {
            QString statusLine = stream.readLine();
            if (statusLine.startsWith("VmRSS:"))
            {
                // VmRSS:  12345 kB
                QStringList parts = statusLine.split(QRegularExpression("\\s+"));
                if (parts.size() >= 2)
                {
                    bool ok;
                    qint64 kb = parts.at(1).toLongLong(&ok);
                    if (ok)
                    {
                        stats.memoryBytes = kb * 1024;
                        stats.valid = true;
                    }
                }
                break;
            }
        }
        statusFile.close();
    }

    return stats;
}

double ThreadStats::calculateCpuUsage(qint64 p_utime, qint64 p_stime)
{
    // Simplified CPU calculation: total jiffies
    // In a real implementation, would need to compare against /proc/stat uptime
    // For now, return approximate percentage based on jiffies (1 jiffy = ~10ms on most systems)
    qint64 totalJiffies = p_utime + p_stime;

    // Rough approximation: assume system running for some time
    // This is a placeholder - a full implementation would track previous readings
    double cpuPercent = (totalJiffies * 100.0) / 10000.0; // Normalize to rough percentage
    if (cpuPercent > 100.0)
    {
        cpuPercent = 100.0;
    }

    return cpuPercent;
}
