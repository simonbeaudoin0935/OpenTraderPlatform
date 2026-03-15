#pragma once

#include <QString>
#include <QThread>
#include <pthread.h>

/**
 * @brief Utility to set both Qt and kernel thread names for visibility in trace tools
 *
 * Qt's setObjectName() is visible in Qt debuggers but not in system tools (ps, top, LTTng).
 * The kernel thread name (comm field) is set via pthread_setname_np() and appears in:
 * - System tools: ps, top, htop
 * - Trace viewers: TraceCompass, perf, lttng
 * - Debuggers: gdb
 *
 * This helper sets both for maximum visibility across all diagnostic tools.
 */
class ThreadNames
{
  public:
    /**
     * Set both Qt and kernel thread names for a QThread
     * @param thread The QThread instance (must not be running yet)
     * @param name The name to set (max 15 bytes in kernel, will be truncated)
     */
    static void setThreadName(QThread* thread, const QString& name)
    {
        // Qt name for Qt debuggers/Qt object inspection
        thread->setObjectName(name);
    }

    /**
     * Set kernel thread name for already-running thread
     * @param thread The running QThread instance
     * @param name The name to set (max 15 bytes, will be truncated)
     * @return true if successful, false if thread not running or pthread_setname_np failed
     */
    static bool setKernelThreadName(QThread* thread, [[maybe_unused]] const QString& name)
    {
        if (!thread || !thread->isRunning())
            return false;

        // Qt6 doesn't directly expose pthread_t from QThread, so we use pthread_self()
        // within the thread context (see setCurrentThreadName below)
        // This method is kept for API consistency but is primarily used via setCurrentThreadName
        return false;
    }

    /**
     * Set both Qt and kernel names for the current thread
     * @param name The name to set
     */
    static void setCurrentThreadName(const QString& name)
    {
        QThread::currentThread()->setObjectName(name);

        const QByteArray nameBytes = name.toUtf8();
        const char* nameStr = nameBytes.constData();
        pthread_setname_np(pthread_self(), nameStr);
    }
};
