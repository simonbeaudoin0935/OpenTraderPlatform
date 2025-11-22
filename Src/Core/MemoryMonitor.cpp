#include "MemoryMonitor.h"
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h> // Link with -lpsapi on Windows
#endif

#ifdef Q_OS_LINUX
#include <unistd.h>
#include <fstream>
#endif

MemoryMonitor::MemoryMonitor(QObject* parent) : QObject(parent), timer(new QTimer(this)) {
    bool connection = connect(timer, &QTimer::timeout, this, &MemoryMonitor::updateMemoryUsage, Qt::UniqueConnection);
    Q_ASSERT_X(connection, "MemoryMonitor", "Failed to create unique connection for timer timeout");
}

MemoryMonitor::~MemoryMonitor() {
    stopMonitoring();
    delete timer;
}

void MemoryMonitor::startMonitoring(int intervalMs) {
    if (!timer->isActive()) {
        timer->start(intervalMs);
        updateMemoryUsage(); // Initial update
    }
}

void MemoryMonitor::stopMonitoring() {
    if (timer->isActive()) {
        timer->stop();
    }
}

void MemoryMonitor::updateMemoryUsage() {
    qint64 memoryUsed = getProcessMemoryUsage();
    if (memoryUsed >= 0) {
        emit memoryUsageUpdated(memoryUsed);
    } else {
        qDebug() << "Failed to retrieve memory usage";
    }
}

qint64 MemoryMonitor::getProcessMemoryUsage() {
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc;
    HANDLE hProcess = GetCurrentProcess();
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize; // Physical memory currently used by the process (in bytes)
    }
    return -1; // Error
#endif

#ifdef Q_OS_LINUX
    std::ifstream statFile("/proc/self/stat");
    if (!statFile.is_open()) {
        return -1;
    }

    std::string line;
    std::getline(statFile, line);
    statFile.close();

    // /proc/self/stat fields are space-separated; RSS (resident set size) is the 24th field (1-based index)
    QStringList fields = QString::fromStdString(line).split(' ', Qt::SkipEmptyParts);
    if (fields.size() >= 24) {
        bool ok;
        qint64 rssPages = fields[23].toLongLong(&ok); // RSS in pages
        if (ok) {
            long pageSize = sysconf(_SC_PAGESIZE); // Get system page size in bytes
            return rssPages * pageSize; // Convert to bytes
        }
    }
    return -1; // Error
#endif

#ifdef Q_OS_MAC
    // macOS implementation using task_info (requires #include <mach/mach.h>)
    // Omitted for brevity; see below for details if needed
    return -1; // Placeholder
#endif

#ifndef Q_OS_WIN
#ifndef Q_OS_LINUX
#ifndef Q_OS_MAC
    return -1; // Unsupported platform
#endif
#endif
#endif
}
