#pragma once

#include <QString>
#include <QDateTime>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <unistd.h>

class LiveStreamDB;

class StatusReporter
{
  public:
    StatusReporter(LiveStreamDB* barsDB, LiveStreamDB* marketDepthDB);

    void printStatus();

  private:
    LiveStreamDB* m_barsDB;
    LiveStreamDB* m_marketDepthDB;
    QDateTime m_startTime;

    void printStreamStatus();
    void printErrorStats(const QString& streamType, LiveStreamDB* db);
    void printRecoveryStats(const QString& streamType, LiveStreamDB* db);
    void printDatabaseStats();
    void printMemoryUsage();
    void printUptime();
};