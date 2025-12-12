#include "RecorderTab.h"
#include "LiveStreamDB.h"
#include "RecorderUtils.h"
#include "Settings.h"
#include "TSClient.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QDate>
#include <QGroupBox>
#include <QFile>
#include <QTextStream>

#ifdef Q_OS_LINUX
#include <fstream>
#include <unistd.h>
#endif

RecorderTab::RecorderTab(QWidget* p_parent)
    : QWidget(p_parent),
      m_streamTable(nullptr),
      m_errorTable(nullptr),
      m_startButton(nullptr),
      m_stopButton(nullptr),
      m_refreshButton(nullptr),
      m_statusLabel(nullptr),
      m_uptimeLabel(nullptr),
      m_barsRecordCountLabel(nullptr),
      m_depthRecordCountLabel(nullptr),
      m_memoryUsageLabel(nullptr),
      m_refreshTimer(nullptr),
      m_isRecording(false),
      m_liveBarsDB(nullptr),
      m_liveMarketDepthQuoteDB(nullptr)
{
    setupUI();

    // Set up auto-refresh timer (every 5 seconds)
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &RecorderTab::refreshRecorderStats);
}

RecorderTab::~RecorderTab() {
    if (m_isRecording) {
        onStopRecording();
    }
}

void RecorderTab::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Status section
    QGroupBox* statusGroupBox = new QGroupBox("Recording Status");
    QVBoxLayout* statusLayout = new QVBoxLayout(statusGroupBox);

    // Status labels
    m_statusLabel = new QLabel("Status: Not Recording");
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: #FFA500; }");
    statusLayout->addWidget(m_statusLabel);

    m_uptimeLabel = new QLabel("Uptime: --:--:--");
    statusLayout->addWidget(m_uptimeLabel);

    m_barsRecordCountLabel = new QLabel("Bars Records: 0");
    statusLayout->addWidget(m_barsRecordCountLabel);

    m_depthRecordCountLabel = new QLabel("Market Depth Records: 0");
    statusLayout->addWidget(m_depthRecordCountLabel);

    m_memoryUsageLabel = new QLabel("Memory Usage: N/A");
    statusLayout->addWidget(m_memoryUsageLabel);

    // Control buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_startButton = new QPushButton("Start Recording");
    m_startButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; }");
    
    m_stopButton = new QPushButton("Stop Recording");
    m_stopButton->setStyleSheet("QPushButton { background-color: #FF4444; color: white; }");
    m_stopButton->setEnabled(false);

    m_refreshButton = new QPushButton("Refresh Stats");

    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_stopButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_refreshButton);

    statusLayout->addLayout(buttonLayout);

    mainLayout->addWidget(statusGroupBox);

    // Stream status table
    QGroupBox* streamGroupBox = new QGroupBox("Stream Status");
    QVBoxLayout* streamLayout = new QVBoxLayout(streamGroupBox);

    m_streamTable = new QTableWidget();
    m_streamTable->setColumnCount(4);
    m_streamTable->setHorizontalHeaderLabels({"Stream Type", "Active Streams", "Failed Streams", "Total Configured"});
    m_streamTable->horizontalHeader()->setStretchLastSection(true);
    m_streamTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_streamTable->setAlternatingRowColors(true);
    m_streamTable->setMaximumHeight(150);

    streamLayout->addWidget(m_streamTable);
    mainLayout->addWidget(streamGroupBox);

    // Error statistics table
    QGroupBox* errorGroupBox = new QGroupBox("Error Statistics");
    QVBoxLayout* errorLayout = new QVBoxLayout(errorGroupBox);

    m_errorTable = new QTableWidget();
    m_errorTable->setColumnCount(5);
    m_errorTable->setHorizontalHeaderLabels({"Stream Type", "Error Type", "Count", "Recovered", "Recovery Rate"});
    m_errorTable->horizontalHeader()->setStretchLastSection(true);
    m_errorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_errorTable->setAlternatingRowColors(true);

    errorLayout->addWidget(m_errorTable);
    mainLayout->addWidget(errorGroupBox);

    // Connect signals
    connect(m_startButton, &QPushButton::clicked, this, &RecorderTab::onStartRecording);
    connect(m_stopButton, &QPushButton::clicked, this, &RecorderTab::onStopRecording);
    connect(m_refreshButton, &QPushButton::clicked, this, &RecorderTab::refreshRecorderStats);
}

void RecorderTab::onStartRecording() {
    // Load stock tickers from CSV
    // Note: We don't use loadStockTickers() utility because it uses qFatal() on error
    // which would crash the GUI. Instead, we handle errors gracefully with message boxes.
    if (stockCsvFile.isEmpty()) {
        QMessageBox::warning(this, "Configuration Error", 
                           "Stock CSV file not configured. Please specify it in the application settings.");
        return;
    }

    QFile file(stockCsvFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "File Error", 
                            QString("Cannot open stock CSV file: %1").arg(stockCsvFile));
        return;
    }

    m_stockTickers.clear();
    QTextStream in(&file);
    QString header = in.readLine(); // Skip header line
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList fields = line.split(',');
        if (!fields.isEmpty() && !fields[0].isEmpty()) {
            m_stockTickers.append(fields[0]);
        }
    }
    file.close();

    if (m_stockTickers.isEmpty()) {
        QMessageBox::warning(this, "Configuration Error", 
                           "No stock tickers found in CSV file.");
        return;
    }

    // Create recording folders
    // Note: We don't use createRecordingFolders() utility because it uses qFatal() on error
    // which would crash the GUI. Instead, we handle errors gracefully with message boxes.
    QString cacheLocation = getCacheLocation();
    QDir baseDir(cacheLocation);
    if (!baseDir.exists()) {
        if (!baseDir.mkpath(".")) {
            QMessageBox::critical(this, "Directory Error", 
                                QString("Cannot create cache directory: %1").arg(cacheLocation));
            return;
        }
    }

    QString recordedDataPath = cacheLocation + "/RecordedLiveData";
    QDir recordedDir(recordedDataPath);
    if (!recordedDir.exists()) {
        if (!recordedDir.mkpath(".")) {
            QMessageBox::critical(this, "Directory Error", 
                                QString("Cannot create RecordedLiveData directory: %1").arg(recordedDataPath));
            return;
        }
    }

    QString barsPath = recordedDataPath + "/Bars";
    QDir barsDir(barsPath);
    if (!barsDir.exists()) {
        if (!barsDir.mkpath(".")) {
            QMessageBox::critical(this, "Directory Error", 
                                QString("Cannot create Bars directory: %1").arg(barsPath));
            return;
        }
    }

    QString marketDepthPath = recordedDataPath + "/MarketDepthQuotes";
    QDir mdDir(marketDepthPath);
    if (!mdDir.exists()) {
        if (!mdDir.mkpath(".")) {
            QMessageBox::critical(this, "Directory Error", 
                                QString("Cannot create MarketDepthQuotes directory: %1").arg(marketDepthPath));
            return;
        }
    }

    // Create database instances
    QString dateStr = QDate::currentDate().toString("yyyy-MM-dd");
    QString barsDbPath = barsPath + "/RecordedLiveBars_" + dateStr + ".db";
    QString marketDepthDbPath = marketDepthPath + "/RecordedLiveMarketDepthQuotes_" + dateStr + ".db";

    m_liveBarsDB = new LiveStreamDB(LiveStreamDB::StreamType::Bars, barsDbPath, m_stockTickers);
    m_liveMarketDepthQuoteDB = new LiveStreamDB(LiveStreamDB::StreamType::MarketDepthQuotes, marketDepthDbPath, m_stockTickers);

    // Start recording
    m_liveBarsDB->startRecording();
    m_liveMarketDepthQuoteDB->startRecording();

    // Update state
    m_isRecording = true;
    m_startTime = QDateTime::currentDateTime();

    // Update UI
    m_statusLabel->setText("Status: Recording");
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: #4CAF50; }");
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);

    // Start refresh timer
    m_refreshTimer->start(5000); // 5 seconds

    // Initial refresh
    refreshRecorderStats();

    qInfo() << "Recording started with" << m_stockTickers.size() << "symbols";
}

void RecorderTab::onStopRecording() {
    if (!m_isRecording) {
        return;
    }

    // Stop refresh timer
    m_refreshTimer->stop();

    // Finalize databases
    if (m_liveBarsDB) {
        m_liveBarsDB->finalizeUnrecoveredTimeouts();
        delete m_liveBarsDB;
        m_liveBarsDB = nullptr;
    }

    if (m_liveMarketDepthQuoteDB) {
        m_liveMarketDepthQuoteDB->finalizeUnrecoveredTimeouts();
        delete m_liveMarketDepthQuoteDB;
        m_liveMarketDepthQuoteDB = nullptr;
    }

    // Update state
    m_isRecording = false;

    // Update UI
    m_statusLabel->setText("Status: Not Recording");
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: #FFA500; }");
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);

    qInfo() << "Recording stopped";
}

void RecorderTab::refreshRecorderStats() {
    if (!m_isRecording) {
        return;
    }

    updateStatsDisplay();
    updateStreamTable();
    updateErrorTable();
}

void RecorderTab::updateStatsDisplay() {
    // Update uptime
    if (m_isRecording) {
        qint64 uptimeSeconds = m_startTime.secsTo(QDateTime::currentDateTime());
        m_uptimeLabel->setText(QString("Uptime: %1").arg(formatUptime(uptimeSeconds)));
    }

    // Update record counts
    if (m_liveBarsDB) {
        int barsCount = m_liveBarsDB->getRecordCount();
        m_barsRecordCountLabel->setText(QString("Bars Records: %1").arg(barsCount));
    }

    if (m_liveMarketDepthQuoteDB) {
        int depthCount = m_liveMarketDepthQuoteDB->getRecordCount();
        m_depthRecordCountLabel->setText(QString("Market Depth Records: %1").arg(depthCount));
    }

    // Update memory usage
#ifdef Q_OS_LINUX
    std::ifstream statm("/proc/self/statm");
    if (statm.is_open()) {
        long pages;
        statm >> pages;
        long pageSize = sysconf(_SC_PAGESIZE);
        long memoryBytes = pages * pageSize;
        m_memoryUsageLabel->setText(QString("Memory Usage: %1").arg(formatFileSize(memoryBytes)));
        statm.close();
    } else {
        m_memoryUsageLabel->setText("Memory Usage: N/A");
    }
#else
    // For non-Linux platforms, we don't have a simple way to get memory usage
    m_memoryUsageLabel->setText("Memory Usage: N/A (platform not supported)");
#endif
}

void RecorderTab::updateStreamTable() {
    m_streamTable->setRowCount(0);

    if (!m_isRecording || !m_liveBarsDB || !m_liveMarketDepthQuoteDB) {
        return;
    }

    // Bars stream row
    int row = 0;
    m_streamTable->insertRow(row);
    m_streamTable->setItem(row, 0, new QTableWidgetItem("Bars"));
    
    int totalBarsStreams = m_liveBarsDB->getTotalConfiguredStreams();
    int activeBarsStreams = m_liveBarsDB->getActiveStreamCount();
    int failedBarsStreams = totalBarsStreams - activeBarsStreams;
    
    m_streamTable->setItem(row, 1, new QTableWidgetItem(QString::number(activeBarsStreams)));
    m_streamTable->setItem(row, 2, new QTableWidgetItem(QString::number(failedBarsStreams)));
    m_streamTable->setItem(row, 3, new QTableWidgetItem(QString::number(totalBarsStreams)));

    // Market Depth stream row
    row++;
    m_streamTable->insertRow(row);
    m_streamTable->setItem(row, 0, new QTableWidgetItem("Market Depth"));
    
    int totalDepthStreams = m_liveMarketDepthQuoteDB->getTotalConfiguredStreams();
    int activeDepthStreams = m_liveMarketDepthQuoteDB->getActiveStreamCount();
    int failedDepthStreams = totalDepthStreams - activeDepthStreams;
    
    m_streamTable->setItem(row, 1, new QTableWidgetItem(QString::number(activeDepthStreams)));
    m_streamTable->setItem(row, 2, new QTableWidgetItem(QString::number(failedDepthStreams)));
    m_streamTable->setItem(row, 3, new QTableWidgetItem(QString::number(totalDepthStreams)));
}

void RecorderTab::updateErrorTable() {
    m_errorTable->setRowCount(0);

    if (!m_isRecording || !m_liveBarsDB || !m_liveMarketDepthQuoteDB) {
        return;
    }

    int row = 0;

    // Process Bars errors
    auto barsErrors = m_liveBarsDB->getErrorCounters();
    QMap<Stream::StreamError, int> barsErrorTypeCounts;
    
    for (auto symbolIt = barsErrors.begin(); symbolIt != barsErrors.end(); ++symbolIt) {
        for (auto errorIt = symbolIt.value().begin(); errorIt != symbolIt.value().end(); ++errorIt) {
            barsErrorTypeCounts[errorIt.key()] += errorIt.value();
        }
    }

    for (auto it = barsErrorTypeCounts.begin(); it != barsErrorTypeCounts.end(); ++it) {
        m_errorTable->insertRow(row);
        m_errorTable->setItem(row, 0, new QTableWidgetItem("Bars"));
        m_errorTable->setItem(row, 1, new QTableWidgetItem(streamErrorToString(it.key())));
        m_errorTable->setItem(row, 2, new QTableWidgetItem(QString::number(it.value())));

        // For timeout errors, show recovery stats
        if (it.key() == Stream::StreamError::Timeout) {
            auto recovered = m_liveBarsDB->getRecoveredTimeouts();
            auto unrecovered = m_liveBarsDB->getUnrecoveredTimeoutCounts();
            
            int totalRecovered = 0;
            int totalUnrecovered = 0;
            
            for (auto rit = recovered.begin(); rit != recovered.end(); ++rit) {
                totalRecovered += rit.value();
            }
            for (auto uit = unrecovered.begin(); uit != unrecovered.end(); ++uit) {
                totalUnrecovered += uit.value();
            }
            
            int totalTimeouts = totalRecovered + totalUnrecovered;
            double recoveryRate = totalTimeouts > 0 ? (static_cast<double>(totalRecovered) / totalTimeouts) * 100.0 : 0.0;
            
            m_errorTable->setItem(row, 3, new QTableWidgetItem(QString::number(totalRecovered)));
            m_errorTable->setItem(row, 4, new QTableWidgetItem(QString("%1%").arg(recoveryRate, 0, 'f', 1)));
        } else {
            m_errorTable->setItem(row, 3, new QTableWidgetItem("N/A"));
            m_errorTable->setItem(row, 4, new QTableWidgetItem("N/A"));
        }

        row++;
    }

    // Process Market Depth errors
    auto depthErrors = m_liveMarketDepthQuoteDB->getErrorCounters();
    QMap<Stream::StreamError, int> depthErrorTypeCounts;
    
    for (auto symbolIt = depthErrors.begin(); symbolIt != depthErrors.end(); ++symbolIt) {
        for (auto errorIt = symbolIt.value().begin(); errorIt != symbolIt.value().end(); ++errorIt) {
            depthErrorTypeCounts[errorIt.key()] += errorIt.value();
        }
    }

    for (auto it = depthErrorTypeCounts.begin(); it != depthErrorTypeCounts.end(); ++it) {
        m_errorTable->insertRow(row);
        m_errorTable->setItem(row, 0, new QTableWidgetItem("Market Depth"));
        m_errorTable->setItem(row, 1, new QTableWidgetItem(streamErrorToString(it.key())));
        m_errorTable->setItem(row, 2, new QTableWidgetItem(QString::number(it.value())));

        // For timeout errors, show recovery stats
        if (it.key() == Stream::StreamError::Timeout) {
            auto recovered = m_liveMarketDepthQuoteDB->getRecoveredTimeouts();
            auto unrecovered = m_liveMarketDepthQuoteDB->getUnrecoveredTimeoutCounts();
            
            int totalRecovered = 0;
            int totalUnrecovered = 0;
            
            for (auto rit = recovered.begin(); rit != recovered.end(); ++rit) {
                totalRecovered += rit.value();
            }
            for (auto uit = unrecovered.begin(); uit != unrecovered.end(); ++uit) {
                totalUnrecovered += uit.value();
            }
            
            int totalTimeouts = totalRecovered + totalUnrecovered;
            double recoveryRate = totalTimeouts > 0 ? (static_cast<double>(totalRecovered) / totalTimeouts) * 100.0 : 0.0;
            
            m_errorTable->setItem(row, 3, new QTableWidgetItem(QString::number(totalRecovered)));
            m_errorTable->setItem(row, 4, new QTableWidgetItem(QString("%1%").arg(recoveryRate, 0, 'f', 1)));
        } else {
            m_errorTable->setItem(row, 3, new QTableWidgetItem("N/A"));
            m_errorTable->setItem(row, 4, new QTableWidgetItem("N/A"));
        }

        row++;
    }
}

QString RecorderTab::formatFileSize(qint64 p_bytes) const {
    if (p_bytes >= 1024 * 1024 * 1024) {
        double gigabytes = static_cast<double>(p_bytes) / (1024 * 1024 * 1024);
        return QString("%1 GB").arg(gigabytes, 0, 'f', 2);
    } else if (p_bytes >= 1024 * 1024) {
        double megabytes = static_cast<double>(p_bytes) / (1024 * 1024);
        return QString("%1 MB").arg(megabytes, 0, 'f', 2);
    } else if (p_bytes >= 1024) {
        double kilobytes = static_cast<double>(p_bytes) / 1024;
        return QString("%1 KB").arg(kilobytes, 0, 'f', 2);
    } else {
        return QString("%1 bytes").arg(p_bytes);
    }
}

QString RecorderTab::formatUptime(qint64 p_seconds) const {
    int hours = p_seconds / 3600;
    int minutes = (p_seconds % 3600) / 60;
    int seconds = p_seconds % 60;
    
    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}
