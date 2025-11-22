#pragma once

#include <QObject>
#include <QTimer>

class MemoryMonitor : public QObject {
    Q_OBJECT
public:
    explicit MemoryMonitor(QObject* parent = nullptr);
    ~MemoryMonitor();

    void startMonitoring(int intervalMs = 1000); // Start monitoring every intervalMs milliseconds
    void stopMonitoring();

signals:
    void memoryUsageUpdated(qsizetype memoryUsedBytes); // Signal emitted with memory usage

private slots:
    void updateMemoryUsage(); // Slot to query and emit memory usage

private:
    QTimer* timer;
    qint64 getProcessMemoryUsage(); // Platform-specific memory query
};
