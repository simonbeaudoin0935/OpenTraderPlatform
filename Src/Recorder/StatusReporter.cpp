#include "StatusReporter.h"
#include "LiveStreamDB.h"
#include "RecorderUtils.h"

#include <iomanip>
#include <fstream>
#include <unistd.h>

StatusReporter::StatusReporter(LiveStreamDB* barsDB, LiveStreamDB* marketDepthDB)
    : m_barsDB(barsDB), m_marketDepthDB(marketDepthDB), m_startTime(QDateTime::currentDateTime())
{
}

void StatusReporter::printStatus() {
    QDateTime now = QDateTime::currentDateTime();
    
    std::cout << "\n=== L2Trader Recorder Status (" << now.toString("HH:mm:ss").toStdString() << ") ===" << std::endl;
    
    printStreamStatus();
    printErrorStats("Bars", m_barsDB);
    printErrorStats("MarketDepth", m_marketDepthDB);
    printRecoveryStats("Bars", m_barsDB);
    printRecoveryStats("MarketDepth", m_marketDepthDB);
    printDatabaseStats();
    printMemoryUsage();
    printUptime();
}

void StatusReporter::printStreamStatus() {
    // Stream status
    int totalBarsStreams = m_barsDB ? m_barsDB->getTotalConfiguredStreams() : 0;
    int activeBarsStreams = m_barsDB ? m_barsDB->getActiveStreamCount() : 0;
    int failedBarsStreams = totalBarsStreams - activeBarsStreams;
    
    int totalDepthStreams = m_marketDepthDB ? m_marketDepthDB->getTotalConfiguredStreams() : 0;
    int activeDepthStreams = m_marketDepthDB ? m_marketDepthDB->getActiveStreamCount() : 0;
    int failedDepthStreams = totalDepthStreams - activeDepthStreams;
    
    std::cout << "Bars Streams: " << activeBarsStreams << "/" << totalBarsStreams;
    if (failedBarsStreams > 0) {
        std::cout << " (" << failedBarsStreams << " failed)";
    }
    std::cout << std::endl;
    
    std::cout << "MarketDepth Streams: " << activeDepthStreams << "/" << totalDepthStreams;
    if (failedDepthStreams > 0) {
        std::cout << " (" << failedDepthStreams << " failed)";
    }
    std::cout << std::endl;
}

void StatusReporter::printErrorStats(const QString& streamType, LiveStreamDB* db) {
    if (!db) return;
    
    auto errorCounters = db->getErrorCounters();
    int totalErrors = 0;
    QMap<Stream::StreamError, int> errorTypeCounts;
    
    for (auto symbolIt = errorCounters.begin(); symbolIt != errorCounters.end(); ++symbolIt) {
        for (auto errorIt = symbolIt.value().begin(); errorIt != symbolIt.value().end(); ++errorIt) {
            totalErrors += errorIt.value();
            errorTypeCounts[errorIt.key()] += errorIt.value();
        }
    }
    
    if (totalErrors > 0) {
        std::cout << streamType.toStdString() << " Errors (total): " << totalErrors << std::endl;
        
        // Print error types
        for (auto it = errorTypeCounts.begin(); it != errorTypeCounts.end(); ++it) {
            std::cout << "  - " << streamErrorToString(it.key()).toStdString() << ": " << it.value();
            
            // Special handling for timeouts - show recovery stats
            if (it.key() == Stream::StreamError::Timeout) {
                auto recovered = db->getRecoveredTimeouts();
                auto unrecovered = db->getUnrecoveredTimeoutCounts();
                
                int totalTimeouts = 0;
                int totalRecovered = 0;
                int totalUnrecovered = 0;
                
                for (auto rit = recovered.begin(); rit != recovered.end(); ++rit) {
                    totalRecovered += rit.value();
                }
                for (auto uit = unrecovered.begin(); uit != unrecovered.end(); ++uit) {
                    totalUnrecovered += uit.value();
                }
                totalTimeouts = totalRecovered + totalUnrecovered;
                
                if (totalTimeouts > 0) {
                    Q_ASSERT(totalTimeouts > 0);  // Ensure no division by zero
                    double recoveryRate = (static_cast<double>(totalRecovered) / totalTimeouts) * 100.0;
                    std::cout << " (" << totalRecovered << " recovered, " << totalUnrecovered << " unrecovered, " 
                              << std::fixed << std::setprecision(1) << recoveryRate << "% recovery)";
                }
            }
            std::cout << std::endl;
        }
    } else {
        std::cout << streamType.toStdString() << " Errors: None" << std::endl;
    }
}

void StatusReporter::printRecoveryStats(const QString& streamType, LiveStreamDB* db) {
    if (!db) return;

    auto recoveryAttempts = db->getRecoveryAttempts();
    auto successfulRecoveries = db->getSuccessfulRecoveries();

    int totalRecoveryAttempts = 0;
    int totalSuccessfulRecoveries = 0;

    for (auto it = recoveryAttempts.begin(); it != recoveryAttempts.end(); ++it) {
        totalRecoveryAttempts += it.value();
    }
    for (auto it = successfulRecoveries.begin(); it != successfulRecoveries.end(); ++it) {
        totalSuccessfulRecoveries += it.value();
    }

    if (totalRecoveryAttempts > 0) {
        Q_ASSERT(totalRecoveryAttempts > 0);  // Ensure no division by zero
        double recoverySuccessRate = (static_cast<double>(totalSuccessfulRecoveries) / totalRecoveryAttempts) * 100.0;
        std::cout << streamType.toStdString() << " Recovery: " << totalSuccessfulRecoveries << "/" << totalRecoveryAttempts 
                  << " successful (" << std::fixed << std::setprecision(1) << recoverySuccessRate << "% success rate)" << std::endl;
    } else {
        std::cout << streamType.toStdString() << " Recovery: No recoveries attempted" << std::endl;
    }
}

void StatusReporter::printDatabaseStats() {
    // Database statistics
    int barsRecords = m_barsDB ? m_barsDB->getRecordCount() : 0;
    int depthRecords = m_marketDepthDB ? m_marketDepthDB->getRecordCount() : 0;
    
    std::cout << "Database: Bars: " << barsRecords << " records, MarketDepth: " << depthRecords << " records" << std::endl;
}

void StatusReporter::printMemoryUsage() {
    // Memory usage (simplified - just process memory info)
    std::ifstream statm("/proc/self/statm");
    if (statm.is_open()) {
        long pages;
        statm >> pages;
        long pageSize = sysconf(_SC_PAGESIZE);
        Q_ASSERT(pageSize > 0);  // Ensure no division by zero
        long memoryKB = (pages * pageSize) / 1024;
        std::cout << "Memory: " << (memoryKB / 1024) << " MB" << std::endl;
        statm.close();
    }
}

void StatusReporter::printUptime() {
    // Uptime
    QDateTime now = QDateTime::currentDateTime();
    qint64 uptimeSeconds = m_startTime.secsTo(now);
    int hours = uptimeSeconds / 3600;
    int minutes = (uptimeSeconds % 3600) / 60;
    int seconds = uptimeSeconds % 60;
    
    std::cout << "Uptime: " << std::setfill('0') << std::setw(2) << hours << ":" 
              << std::setfill('0') << std::setw(2) << minutes << ":" 
              << std::setfill('0') << std::setw(2) << seconds << std::endl;
}