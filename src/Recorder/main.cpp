#include <QCoreApplication>
#include <QLoggingCategory>
#include <QDateTime>
#include <QTimer>
#include <csignal>

#include "ArgumentParser.h"
#include "Logging.h"
#include "Settings.h"
#include "BarCache.h"
#include "TSClient.h"
#include "RecorderLogic.h"
#include "LiveStreamDB.h"

#include <QtGlobal>

#include <iostream>

// Global pointers for signal handler
LiveStreamDB* g_liveBarsDB = nullptr;
LiveStreamDB* g_liveMarketDepthQuoteDB = nullptr;

QString streamErrorToString(Stream::StreamError error) {
    switch (error) {
        case Stream::StreamError::Timeout: return "Timeout";
        case Stream::StreamError::BadRequest: return "BadRequest";
        case Stream::StreamError::DualLogon: return "DualLogon";
        case Stream::StreamError::GoAway: return "GoAway";
        case Stream::StreamError::InternalServerError: return "InternalServerError";
        case Stream::StreamError::InvalidSymbol: return "InvalidSymbol";
        case Stream::StreamError::Unknown: return "Unknown";
        default: return "Unknown";
    }
}

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived SIGINT (Ctrl+C). Displaying error counters before exit:\n" << std::endl;
        
        // Finalize any unrecovered timeouts
        if (g_liveBarsDB) {
            g_liveBarsDB->finalizeUnrecoveredTimeouts();
        }
        if (g_liveMarketDepthQuoteDB) {
            g_liveMarketDepthQuoteDB->finalizeUnrecoveredTimeouts();
        }
        
        if (g_liveBarsDB) {
            std::cout << "Bars Stream Error Counters:" << std::endl;
            auto barsErrors = g_liveBarsDB->getErrorCounters();
            bool hasErrors = false;
            for (auto symbolIt = barsErrors.begin(); symbolIt != barsErrors.end(); ++symbolIt) {
                const QString& symbol = symbolIt.key();
                const auto& errorMap = symbolIt.value();
                if (!errorMap.isEmpty()) {
                    hasErrors = true;
                    std::cout << "  " << symbol.toStdString() << ":" << std::endl;
                    for (auto errorIt = errorMap.begin(); errorIt != errorMap.end(); ++errorIt) {
                        std::cout << "    " << streamErrorToString(errorIt.key()).toStdString() 
                                  << ": " << errorIt.value() << " errors" << std::endl;
                    }
                }
            }
            if (!hasErrors) {
                std::cout << "  No errors recorded" << std::endl;
            }
            
            // Display timeout recovery statistics
            auto recoveredTimeouts = g_liveBarsDB->getRecoveredTimeouts();
            auto unrecoveredTimeouts = g_liveBarsDB->getUnrecoveredTimeoutCounts();
            if (!recoveredTimeouts.isEmpty() || !unrecoveredTimeouts.isEmpty()) {
                std::cout << "  Timeout Recovery Statistics:" << std::endl;
                QSet<QString> allSymbols;
                for (auto it = recoveredTimeouts.begin(); it != recoveredTimeouts.end(); ++it) {
                    allSymbols.insert(it.key());
                }
                for (auto it = unrecoveredTimeouts.begin(); it != unrecoveredTimeouts.end(); ++it) {
                    allSymbols.insert(it.key());
                }
                
                for (const QString& symbol : allSymbols) {
                    int recovered = recoveredTimeouts.value(symbol, 0);
                    int unrecovered = unrecoveredTimeouts.value(symbol, 0);
                    std::cout << "    " << symbol.toStdString() << ": " 
                              << recovered << " recovered, " << unrecovered << " unrecovered" << std::endl;
                }
            }
            std::cout << std::endl;
        }
        
        if (g_liveMarketDepthQuoteDB) {
            std::cout << "Market Depth Stream Error Counters:" << std::endl;
            auto depthErrors = g_liveMarketDepthQuoteDB->getErrorCounters();
            bool hasErrors = false;
            for (auto symbolIt = depthErrors.begin(); symbolIt != depthErrors.end(); ++symbolIt) {
                const QString& symbol = symbolIt.key();
                const auto& errorMap = symbolIt.value();
                if (!errorMap.isEmpty()) {
                    hasErrors = true;
                    std::cout << "  " << symbol.toStdString() << ":" << std::endl;
                    for (auto errorIt = errorMap.begin(); errorIt != errorMap.end(); ++errorIt) {
                        std::cout << "    " << streamErrorToString(errorIt.key()).toStdString() 
                                  << ": " << errorIt.value() << " errors" << std::endl;
                    }
                }
            }
            if (!hasErrors) {
                std::cout << "  No errors recorded" << std::endl;
            }
            
            // Display timeout recovery statistics
            auto recoveredTimeouts = g_liveMarketDepthQuoteDB->getRecoveredTimeouts();
            auto unrecoveredTimeouts = g_liveMarketDepthQuoteDB->getUnrecoveredTimeoutCounts();
            if (!recoveredTimeouts.isEmpty() || !unrecoveredTimeouts.isEmpty()) {
                std::cout << "  Timeout Recovery Statistics:" << std::endl;
                QSet<QString> allSymbols;
                for (auto it = recoveredTimeouts.begin(); it != recoveredTimeouts.end(); ++it) {
                    allSymbols.insert(it.key());
                }
                for (auto it = unrecoveredTimeouts.begin(); it != unrecoveredTimeouts.end(); ++it) {
                    allSymbols.insert(it.key());
                }
                
                for (const QString& symbol : allSymbols) {
                    int recovered = recoveredTimeouts.value(symbol, 0);
                    int unrecovered = unrecoveredTimeouts.value(symbol, 0);
                    std::cout << "    " << symbol.toStdString() << ": " 
                              << recovered << " recovered, " << unrecovered << " unrecovered" << std::endl;
                }
            }
            std::cout << std::endl;
        }
        
        std::cout << "Exiting gracefully..." << std::endl;
        QCoreApplication::quit();
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QCoreApplication::setApplicationName("Recorder");
    QString version = QString("%1 ~ %2@%3").arg(GIT_TAG, GIT_BRANCH, GIT_HASH);
    QCoreApplication::setApplicationVersion(version);

    // Set up signal handler for graceful shutdown
    std::signal(SIGINT, signalHandler);

    // Initialize logging (opens file and installs handler)
    initLogging();

    qInfo() << "Qt version:" << QT_VERSION_STR;

    qInfo() << "Version:" << version;

    parseArguments(app.arguments());

    qInfo() << "Cache root directory:" << getCacheLocation();
    
    if (stockCsvFile.isEmpty()) {
        qFatal("Stock CSV file not specified");
    } else {
        qInfo() << "Stock CSV file:" << stockCsvFile;
    }

    QStringList stockTickers = loadStockTickers(stockCsvFile);
    qInfo() << "Loaded" << stockTickers.size() << "stock tickers from CSV.";

    QString recordedDataPath = createRecordingFolders(getCacheLocation());
    qInfo() << "Recorded data folder:" << recordedDataPath;

    TSClient::getInstancePtr()->start();

    QString dateStr = QDate::currentDate().toString("yyyy-MM-dd");
    QString barsDbPath = recordedDataPath + "/Bars/RecordedLiveBars_" + dateStr + ".db";
    LiveStreamDB* liveBarsDB = new LiveStreamDB(LiveStreamDB::StreamType::Bars, barsDbPath, stockTickers);

    liveBarsDB->startRecording();

    qInfo() << "------ Recorder for Bars started - recording market data...";

    QString marketDepthDbPath = recordedDataPath + "/MarketDepthQuotes/RecordedLiveMarketDepthQuotes_" + dateStr + ".db";
    LiveStreamDB* liveMarketDepthQuoteDB = new LiveStreamDB(LiveStreamDB::StreamType::MarketDepthQuotes, marketDepthDbPath, stockTickers);

    liveMarketDepthQuoteDB->startRecording();

    qInfo() << "------ Recorder for Market Depth Quotes started - recording market data...";

    // Set global pointers for signal handler
    g_liveBarsDB = liveBarsDB;
    g_liveMarketDepthQuoteDB = liveMarketDepthQuoteDB;

    // Write logging configuration to disk if this is the first run
    LoggingConfig::instance().writeConfigToDisk();

    return app.exec();
}