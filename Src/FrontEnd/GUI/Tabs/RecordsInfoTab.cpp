#include "RecordsInfoTab.h"
#include "Logging.h"
#include "Settings.h"
#include "Assume.h"

#include <QHeaderView>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QTimeZone>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QMessageBox>

RecordsInfoTab::RecordsInfoTab(QWidget* p_parent)
    : QWidget(p_parent)
    , m_daysTable(nullptr)
    , m_refreshButton(nullptr)
    , m_stocksTable(nullptr)
    , m_symbolLabel(nullptr)
    , m_barsGroupBox(nullptr)
    , m_barsCountLabel(nullptr)
    , m_barsFirstTimeLabel(nullptr)
    , m_barsLastTimeLabel(nullptr)
    , m_barsDurationLabel(nullptr)
    , m_depthGroupBox(nullptr)
    , m_depthStatusLabel(nullptr)
    , m_depthCountLabel(nullptr)
    , m_depthFirstTimeLabel(nullptr)
    , m_depthLastTimeLabel(nullptr)
    , m_depthDurationLabel(nullptr)
{
    setupUI();
    scanRecordedDays();
}

void RecordsInfoTab::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Top bar with refresh button
    QHBoxLayout* topBarLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel("Recorded Market Data");
    titleLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 14pt; }");
    topBarLayout->addWidget(titleLabel);
    topBarLayout->addStretch();

    m_refreshButton = new QPushButton("Refresh");
    m_refreshButton->setMaximumWidth(100);
    topBarLayout->addWidget(m_refreshButton);

    mainLayout->addLayout(topBarLayout);

    // Three-column layout with splitter
    QSplitter* splitter = new QSplitter(Qt::Horizontal);

    // Left column: Recorded days table
    QWidget* leftWidget = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* daysLabel = new QLabel("Recorded Days");
    daysLabel->setStyleSheet("QLabel { font-weight: bold; }");
    leftLayout->addWidget(daysLabel);

    m_daysTable = new QTableWidget();
    m_daysTable->setColumnCount(3);
    m_daysTable->setHorizontalHeaderLabels({"Date", "Bars", "Size"});
    m_daysTable->horizontalHeader()->setStretchLastSection(true);
    m_daysTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_daysTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_daysTable->setAlternatingRowColors(true);
    m_daysTable->setSortingEnabled(true);
    leftLayout->addWidget(m_daysTable);

    splitter->addWidget(leftWidget);

    // Middle column: Stocks table
    QWidget* middleWidget = new QWidget();
    QVBoxLayout* middleLayout = new QVBoxLayout(middleWidget);
    middleLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* stocksLabel = new QLabel("Stocks in Selected Day");
    stocksLabel->setStyleSheet("QLabel { font-weight: bold; }");
    middleLayout->addWidget(stocksLabel);

    m_stocksTable = new QTableWidget();
    m_stocksTable->setColumnCount(3);
    m_stocksTable->setHorizontalHeaderLabels({"Symbol", "Bars", "Depth"});
    m_stocksTable->horizontalHeader()->setStretchLastSection(true);
    m_stocksTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_stocksTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_stocksTable->setAlternatingRowColors(true);
    m_stocksTable->setSortingEnabled(true);
    middleLayout->addWidget(m_stocksTable);

    splitter->addWidget(middleWidget);

    // Right column: Stock details
    QWidget* rightWidget = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* detailsLabel = new QLabel("Stock Details");
    detailsLabel->setStyleSheet("QLabel { font-weight: bold; }");
    rightLayout->addWidget(detailsLabel);

    m_symbolLabel = new QLabel("No stock selected");
    m_symbolLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 12pt; }");
    rightLayout->addWidget(m_symbolLabel);

    // Bars data group box
    m_barsGroupBox = new QGroupBox("Bars Data");
    QVBoxLayout* barsLayout = new QVBoxLayout(m_barsGroupBox);

    m_barsCountLabel = new QLabel("Count: --");
    m_barsFirstTimeLabel = new QLabel("First: --");
    m_barsLastTimeLabel = new QLabel("Last: --");
    m_barsDurationLabel = new QLabel("Duration: --");

    barsLayout->addWidget(m_barsCountLabel);
    barsLayout->addWidget(m_barsFirstTimeLabel);
    barsLayout->addWidget(m_barsLastTimeLabel);
    barsLayout->addWidget(m_barsDurationLabel);

    rightLayout->addWidget(m_barsGroupBox);

    // Market depth data group box
    m_depthGroupBox = new QGroupBox("Market Depth Data");
    QVBoxLayout* depthLayout = new QVBoxLayout(m_depthGroupBox);

    m_depthStatusLabel = new QLabel("Status: --");
    m_depthCountLabel = new QLabel("Count: --");
    m_depthFirstTimeLabel = new QLabel("First: --");
    m_depthLastTimeLabel = new QLabel("Last: --");
    m_depthDurationLabel = new QLabel("Duration: --");

    depthLayout->addWidget(m_depthStatusLabel);
    depthLayout->addWidget(m_depthCountLabel);
    depthLayout->addWidget(m_depthFirstTimeLabel);
    depthLayout->addWidget(m_depthLastTimeLabel);
    depthLayout->addWidget(m_depthDurationLabel);

    rightLayout->addWidget(m_depthGroupBox);
    rightLayout->addStretch();

    splitter->addWidget(rightWidget);

    // Set initial splitter sizes (30% / 30% / 40%)
    splitter->setSizes({300, 300, 400});

    mainLayout->addWidget(splitter);

    // Connect signals
    bool isConnectionUnique;

    isConnectionUnique =
        connect(m_refreshButton, &QPushButton::clicked, this, &RecordsInfoTab::onRefreshClicked, Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(isConnectionUnique);

    isConnectionUnique = connect(m_daysTable,
                                 &QTableWidget::itemSelectionChanged,
                                 this,
                                 &RecordsInfoTab::onDaySelected,
                                 Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(isConnectionUnique);

    isConnectionUnique = connect(m_stocksTable,
                                 &QTableWidget::itemSelectionChanged,
                                 this,
                                 &RecordsInfoTab::onStockSelected,
                                 Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(isConnectionUnique);
}

void RecordsInfoTab::onRefreshClicked()
{
    qDebug() << "Refreshing recorded days list";
    clearStocksList();
    clearDetailsDisplay();
    m_depthAvailabilityCache.clear();
    scanRecordedDays();
}

void RecordsInfoTab::onDaySelected()
{
    QList<QTableWidgetItem*> selectedItems = m_daysTable->selectedItems();
    if (selectedItems.isEmpty())
    {
        clearStocksList();
        clearDetailsDisplay();
        return;
    }

    // Get date from first column of selected row
    int row = selectedItems.first()->row();
    QTableWidgetItem* dateItem = m_daysTable->item(row, 0);
    OBJ_ASSUME_DIFF(dateItem, nullptr);

    QDate date = QDate::fromString(dateItem->text(), "yyyy-MM-dd");
    if (!date.isValid())
    {
        qWarning() << "Invalid date selected:" << dateItem->text();
        return;
    }

    m_selectedDate = date;
    clearDetailsDisplay();
    loadStocksForDay(date);
}

void RecordsInfoTab::onStockSelected()
{
    QList<QTableWidgetItem*> selectedItems = m_stocksTable->selectedItems();
    if (selectedItems.isEmpty() || !m_selectedDate.isValid())
    {
        clearDetailsDisplay();
        return;
    }

    // Get symbol from first column of selected row
    int row = selectedItems.first()->row();
    QTableWidgetItem* symbolItem = m_stocksTable->item(row, 0);
    OBJ_ASSUME_DIFF(symbolItem, nullptr);

    QString symbol = symbolItem->text();
    m_selectedSymbol = symbol;
    loadStockDetails(m_selectedDate, symbol);
}

void RecordsInfoTab::scanRecordedDays()
{
    m_daysTable->setRowCount(0);
    m_daysTable->setSortingEnabled(false);

    QString cacheLocation = getCacheLocation();
    QString barsPath = cacheLocation + "/RecordedLiveData/Bars";
    QDir barsDir(barsPath);

    if (!barsDir.exists())
    {
        qInfo() << "No recorded data directory found at:" << barsPath;
        m_daysTable->setSortingEnabled(true);
        return;
    }

    // Get all .db files
    QStringList filters;
    filters << "*.db";
    QFileInfoList fileList = barsDir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo& fileInfo: fileList)
    {
        QString baseName = fileInfo.baseName();
        QDate date = QDate::fromString(baseName, "yyyy-MM-dd");

        if (!date.isValid())
        {
            continue; // Skip files that don't match expected format
        }

        // Get bar count from database
        QString dbPath = fileInfo.absoluteFilePath();
        QStringList stocks = getStocksFromDatabase(dbPath);
        qint64 barCount = 0;

        // Quick count query
        QString connectionName = QString("scan_%1").arg(date.toString("yyyyMMdd"));
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
            db.setDatabaseName(dbPath);

            if (db.open())
            {
                QSqlQuery query(db);
                if (query.exec("SELECT COUNT(*) FROM bars"))
                {
                    if (query.next())
                    {
                        barCount = query.value(0).toLongLong();
                    }
                }
                query.finish();
                db.close();
            }
        } // db goes out of scope here
        QSqlDatabase::removeDatabase(connectionName);

        // Get file size
        qint64 fileSize = fileInfo.size();

        // Add row to table
        int row = m_daysTable->rowCount();
        m_daysTable->insertRow(row);

        QTableWidgetItem* dateItem = new QTableWidgetItem(date.toString("yyyy-MM-dd"));
        QTableWidgetItem* barsItem = new QTableWidgetItem(QString::number(barCount));
        QTableWidgetItem* sizeItem = new QTableWidgetItem(formatFileSize(fileSize));

        // Store raw values for sorting
        dateItem->setData(Qt::UserRole, date);
        barsItem->setData(Qt::UserRole, barCount);
        sizeItem->setData(Qt::UserRole, fileSize);

        m_daysTable->setItem(row, 0, dateItem);
        m_daysTable->setItem(row, 1, barsItem);
        m_daysTable->setItem(row, 2, sizeItem);
    }

    m_daysTable->setSortingEnabled(true);
    m_daysTable->sortItems(0, Qt::DescendingOrder); // Most recent first

    qInfo() << "Found" << m_daysTable->rowCount() << "recorded days";
}

void RecordsInfoTab::loadStocksForDay(const QDate& p_date)
{
    clearStocksList();
    m_stocksTable->setSortingEnabled(false);

    QString barsDbPath = getBarsDbPath(p_date);
    QString depthDbPath = getMarketDepthDbPath(p_date);

    // Check if bars database exists
    if (!QFileInfo::exists(barsDbPath))
    {
        qWarning() << "Bars database not found:" << barsDbPath;
        m_stocksTable->setSortingEnabled(true);
        return;
    }

    // Get stocks from bars database
    QStringList stocks = getStocksFromDatabase(barsDbPath);

    if (stocks.isEmpty())
    {
        qInfo() << "No stocks found in database for" << p_date.toString("yyyy-MM-dd");
        m_stocksTable->setSortingEnabled(true);
        return;
    }

    // Check market depth availability for each stock
    bool depthDbExists = QFileInfo::exists(depthDbPath);
    QMap<QString, bool> depthAvailability;

    if (depthDbExists)
    {
        for (const QString& symbol: stocks)
        {
            bool hasDepth = checkStockInDatabase(depthDbPath, symbol);
            depthAvailability[symbol] = hasDepth;
        }
    }
    else
    {
        for (const QString& symbol: stocks)
        {
            depthAvailability[symbol] = false;
        }
    }

    // Cache the results
    m_depthAvailabilityCache[p_date] = depthAvailability;

    // Get bar counts for each stock
    QString connectionName = QString("stocks_%1").arg(p_date.toString("yyyyMMdd"));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(barsDbPath);

        if (!db.open())
        {
            qCritical() << "Failed to open bars database:" << db.lastError().text();
            QSqlDatabase::removeDatabase(connectionName);
            m_stocksTable->setSortingEnabled(true);
            return;
        }

        for (const QString& symbol: stocks)
        {
            qint64 barCount = 0;
            QSqlQuery query(db);
            query.prepare("SELECT COUNT(*) FROM bars WHERE stockTicker = ?");
            query.addBindValue(symbol);

            if (query.exec() && query.next())
            {
                barCount = query.value(0).toLongLong();
            }
            query.finish();

            int row = m_stocksTable->rowCount();
            m_stocksTable->insertRow(row);

            QTableWidgetItem* symbolItem = new QTableWidgetItem(symbol);
            QTableWidgetItem* barsItem = new QTableWidgetItem(QString::number(barCount));
            QTableWidgetItem* depthItem = new QTableWidgetItem(depthAvailability[symbol] ? "Yes" : "No");

            barsItem->setData(Qt::UserRole, barCount);

            m_stocksTable->setItem(row, 0, symbolItem);
            m_stocksTable->setItem(row, 1, barsItem);
            m_stocksTable->setItem(row, 2, depthItem);
        }

        db.close();
    } // db goes out of scope here
    QSqlDatabase::removeDatabase(connectionName);

    m_stocksTable->setSortingEnabled(true);
    qInfo() << "Loaded" << stocks.size() << "stocks for" << p_date.toString("yyyy-MM-dd");
}

void RecordsInfoTab::loadStockDetails(const QDate& p_date, const QString& p_symbol)
{
    QString barsDbPath = getBarsDbPath(p_date);
    QString depthDbPath = getMarketDepthDbPath(p_date);

    StockMetrics metrics = queryStockMetrics(barsDbPath, depthDbPath, p_symbol);
    updateDetailsDisplay(metrics);
}

void RecordsInfoTab::updateDetailsDisplay(const StockMetrics& p_metrics)
{
    m_symbolLabel->setText(p_metrics.symbol);

    // Update bars data
    m_barsCountLabel->setText(QString("Count: %1 bars").arg(p_metrics.barCount));
    m_barsFirstTimeLabel->setText(QString("First: %1").arg(formatTimestamp(p_metrics.firstTimestampMs)));
    m_barsLastTimeLabel->setText(QString("Last: %1").arg(formatTimestamp(p_metrics.lastTimestampMs)));

    qint64 duration = p_metrics.lastTimestampMs - p_metrics.firstTimestampMs;
    m_barsDurationLabel->setText(QString("Duration: %1").arg(formatDuration(duration)));

    // Update market depth data
    if (p_metrics.hasMarketDepth)
    {
        m_depthStatusLabel->setText("Status: Available");
        m_depthStatusLabel->setStyleSheet("QLabel { color: #4CAF50; font-weight: bold; }");
        m_depthCountLabel->setText(QString("Count: %1 quotes").arg(p_metrics.depthCount));
        m_depthFirstTimeLabel->setText(QString("First: %1").arg(formatTimestamp(p_metrics.depthFirstTimestampMs)));
        m_depthLastTimeLabel->setText(QString("Last: %1").arg(formatTimestamp(p_metrics.depthLastTimestampMs)));

        qint64 depthDuration = p_metrics.depthLastTimestampMs - p_metrics.depthFirstTimestampMs;
        m_depthDurationLabel->setText(QString("Duration: %1").arg(formatDuration(depthDuration)));

        m_depthCountLabel->setVisible(true);
        m_depthFirstTimeLabel->setVisible(true);
        m_depthLastTimeLabel->setVisible(true);
        m_depthDurationLabel->setVisible(true);
    }
    else
    {
        m_depthStatusLabel->setText("Status: Not Available");
        m_depthStatusLabel->setStyleSheet("QLabel { color: #FF4444; font-weight: bold; }");
        m_depthCountLabel->setVisible(false);
        m_depthFirstTimeLabel->setVisible(false);
        m_depthLastTimeLabel->setVisible(false);
        m_depthDurationLabel->setVisible(false);
    }
}

void RecordsInfoTab::clearStocksList()
{
    m_stocksTable->setRowCount(0);
    m_selectedSymbol.clear();
}

void RecordsInfoTab::clearDetailsDisplay()
{
    m_symbolLabel->setText("No stock selected");
    m_barsCountLabel->setText("Count: --");
    m_barsFirstTimeLabel->setText("First: --");
    m_barsLastTimeLabel->setText("Last: --");
    m_barsDurationLabel->setText("Duration: --");
    m_depthStatusLabel->setText("Status: --");
    m_depthStatusLabel->setStyleSheet("");
    m_depthCountLabel->setText("Count: --");
    m_depthFirstTimeLabel->setText("First: --");
    m_depthLastTimeLabel->setText("Last: --");
    m_depthDurationLabel->setText("Duration: --");
    m_depthCountLabel->setVisible(true);
    m_depthFirstTimeLabel->setVisible(true);
    m_depthLastTimeLabel->setVisible(true);
    m_depthDurationLabel->setVisible(true);
}

QString RecordsInfoTab::getBarsDbPath(const QDate& p_date) const
{
    QString cacheLocation = getCacheLocation();
    return QString("%1/RecordedLiveData/Bars/%2.db").arg(cacheLocation, p_date.toString("yyyy-MM-dd"));
}

QString RecordsInfoTab::getMarketDepthDbPath(const QDate& p_date) const
{
    QString cacheLocation = getCacheLocation();
    return QString("%1/RecordedLiveData/MarketDepthQuotes/%2.db").arg(cacheLocation, p_date.toString("yyyy-MM-dd"));
}

QString RecordsInfoTab::formatFileSize(qint64 p_bytes) const
{
    if (p_bytes >= 1024 * 1024 * 1024)
    {
        double gigabytes = static_cast<double>(p_bytes) / (1024 * 1024 * 1024);
        return QString("%1 GB").arg(gigabytes, 0, 'f', 2);
    }
    else if (p_bytes >= 1024 * 1024)
    {
        double megabytes = static_cast<double>(p_bytes) / (1024 * 1024);
        return QString("%1 MB").arg(megabytes, 0, 'f', 2);
    }
    else if (p_bytes >= 1024)
    {
        double kilobytes = static_cast<double>(p_bytes) / 1024;
        return QString("%1 KB").arg(kilobytes, 0, 'f', 2);
    }
    else
    {
        return QString("%1 bytes").arg(p_bytes);
    }
}

QString RecordsInfoTab::formatDuration(qint64 p_durationMs) const
{
    qint64 seconds = p_durationMs / 1000;
    qint64 minutes = seconds / 60;
    qint64 hours = minutes / 60;

    seconds = seconds % 60;
    minutes = minutes % 60;

    if (hours > 0)
    {
        return QString("%1h %2m %3s").arg(hours).arg(minutes).arg(seconds);
    }
    else if (minutes > 0)
    {
        return QString("%1m %2s").arg(minutes).arg(seconds);
    }
    else
    {
        return QString("%1s").arg(seconds);
    }
}

QString RecordsInfoTab::formatTimestamp(qint64 p_epochMs) const
{
    if (p_epochMs == 0)
    {
        return "--";
    }

    QDateTime dt = QDateTime::fromMSecsSinceEpoch(p_epochMs, QTimeZone("America/New_York"));
    return dt.toString("yyyy-MM-dd hh:mm:ss.zzz");
}

QStringList RecordsInfoTab::getStocksFromDatabase(const QString& p_dbPath)
{
    QStringList stocks;

    QString connectionName = QString("getStocks_%1").arg(QDateTime::currentMSecsSinceEpoch());
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(p_dbPath);

        if (!db.open())
        {
            qWarning() << "Failed to open database:" << p_dbPath << "-" << db.lastError().text();
            QSqlDatabase::removeDatabase(connectionName);
            return stocks;
        }

        QSqlQuery query(db);
        if (query.exec("SELECT DISTINCT stockTicker FROM bars ORDER BY stockTicker"))
        {
            while (query.next())
            {
                stocks.append(query.value(0).toString());
            }
        }
        else
        {
            qWarning() << "Failed to query stocks:" << query.lastError().text();
        }
        query.finish();
        db.close();
    } // db goes out of scope here
    QSqlDatabase::removeDatabase(connectionName);

    return stocks;
}

bool RecordsInfoTab::checkStockInDatabase(const QString& p_dbPath, const QString& p_symbol)
{
    if (!QFileInfo::exists(p_dbPath))
    {
        return false;
    }

    QString connectionName = QString("checkStock_%1_%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(p_symbol);
    bool hasStock = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(p_dbPath);

        if (!db.open())
        {
            qWarning() << "Failed to open database:" << p_dbPath << "-" << db.lastError().text();
            QSqlDatabase::removeDatabase(connectionName);
            return false;
        }

        QSqlQuery query(db);
        query.prepare("SELECT COUNT(*) FROM market_depth_quotes WHERE stockTicker = ? LIMIT 1");
        query.addBindValue(p_symbol);

        if (query.exec() && query.next())
        {
            hasStock = query.value(0).toInt() > 0;
        }
        query.finish();
        db.close();
    } // db goes out of scope here
    QSqlDatabase::removeDatabase(connectionName);

    return hasStock;
}

RecordsInfoTab::StockMetrics
RecordsInfoTab::queryStockMetrics(const QString& p_barsDbPath, const QString& p_depthDbPath, const QString& p_symbol)
{
    StockMetrics metrics;
    metrics.symbol = p_symbol;

    // Query bars database
    QString barsConnectionName = QString("barsMetrics_%1_%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(p_symbol);
    {
        QSqlDatabase barsDb = QSqlDatabase::addDatabase("QSQLITE", barsConnectionName);
        barsDb.setDatabaseName(p_barsDbPath);

        if (barsDb.open())
        {
            QSqlQuery query(barsDb);
            query.prepare("SELECT COUNT(*), MIN(epochMs), MAX(epochMs) FROM bars WHERE stockTicker = ?");
            query.addBindValue(p_symbol);

            if (query.exec() && query.next())
            {
                metrics.barCount = query.value(0).toLongLong();
                metrics.firstTimestampMs = query.value(1).toLongLong();
                metrics.lastTimestampMs = query.value(2).toLongLong();
            }
            query.finish();
            barsDb.close();
        }
        else
        {
            qCritical() << "Failed to open bars database:" << barsDb.lastError().text();
        }
    } // barsDb goes out of scope here
    QSqlDatabase::removeDatabase(barsConnectionName);

    // Query market depth database if it exists
    if (QFileInfo::exists(p_depthDbPath))
    {
        QString depthConnectionName =
            QString("depthMetrics_%1_%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(p_symbol);
        {
            QSqlDatabase depthDb = QSqlDatabase::addDatabase("QSQLITE", depthConnectionName);
            depthDb.setDatabaseName(p_depthDbPath);

            if (depthDb.open())
            {
                QSqlQuery query(depthDb);
                query.prepare(
                    "SELECT COUNT(*), MIN(epochMs), MAX(epochMs) FROM market_depth_quotes WHERE stockTicker = ?");
                query.addBindValue(p_symbol);

                if (query.exec() && query.next())
                {
                    qint64 count = query.value(0).toLongLong();
                    if (count > 0)
                    {
                        metrics.hasMarketDepth = true;
                        metrics.depthCount = count;
                        metrics.depthFirstTimestampMs = query.value(1).toLongLong();
                        metrics.depthLastTimestampMs = query.value(2).toLongLong();
                    }
                }
                query.finish();
                depthDb.close();
            }
        } // depthDb goes out of scope here
        QSqlDatabase::removeDatabase(depthConnectionName);
    }

    return metrics;
}
