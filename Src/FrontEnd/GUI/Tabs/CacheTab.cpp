#include "CacheTab.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QDirIterator>
#include <QStandardPaths>
#include "Settings.h"
#include "OrdersDatabase.h"

CacheTab::CacheTab(QWidget* parent)
    : QWidget(parent)
    , cacheTable(nullptr)
    , refreshButton(nullptr)
    , clearSelectedButton(nullptr)
    , clearAllButton(nullptr)
    , totalSizeLabel(nullptr)
    , refreshTimer(nullptr)
    , ordersDbCountLabel(nullptr)
    , ordersDbSizeLabel(nullptr)
    , ordersDbRefreshButton(nullptr)
    , ordersDbClearButton(nullptr)
{
    setupUI();
    refreshCacheInfo();
    refreshOrdersDbInfo();

    // Set up auto-refresh timer (every 30 seconds)
    refreshTimer = new QTimer(this);
    connect(refreshTimer,
            &QTimer::timeout,
            this,
            [this]()
            {
                refreshCacheInfo();
                refreshOrdersDbInfo();
            });
    refreshTimer->start(30000);
}

void CacheTab::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Cache information section
    QGroupBox* cacheGroupBox = new QGroupBox("Cache Management");
    QVBoxLayout* cacheLayout = new QVBoxLayout(cacheGroupBox);

    // Table for cache files
    cacheTable = new QTableWidget();
    cacheTable->setColumnCount(4);
    cacheTable->setHorizontalHeaderLabels({"Category", "File Name", "Size", "Last Modified"});
    cacheTable->horizontalHeader()->setStretchLastSection(true);
    cacheTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    cacheTable->setAlternatingRowColors(true);
    cacheTable->setSortingEnabled(true);

    // Set column widths
    cacheTable->setColumnWidth(0,
                               120); // Category
    cacheTable->setColumnWidth(1,
                               200);    // File Name
    cacheTable->setColumnWidth(2, 100); // Size

    cacheLayout->addWidget(cacheTable);

    // Control buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    refreshButton = new QPushButton("Refresh");
    clearSelectedButton = new QPushButton("Clear Selected");
    clearAllButton = new QPushButton("Clear All Cache");

    // Style buttons
    clearSelectedButton->setStyleSheet("QPushButton { background-color: #FFA500; color: black; }");
    clearAllButton->setStyleSheet("QPushButton { background-color: #FF4444; color: white; }");

    buttonLayout->addWidget(refreshButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(clearSelectedButton);
    buttonLayout->addWidget(clearAllButton);

    cacheLayout->addLayout(buttonLayout);

    // Total size display
    QHBoxLayout* sizeLayout = new QHBoxLayout();
    totalSizeLabel = new QLabel("Total Cache Size: Calculating...");
    sizeLayout->addWidget(totalSizeLabel);
    sizeLayout->addStretch();

    cacheLayout->addLayout(sizeLayout);

    mainLayout->addWidget(cacheGroupBox);

    // Orders Database section
    QGroupBox* ordersDbGroupBox = new QGroupBox("Orders Database");
    QVBoxLayout* ordersDbLayout = new QVBoxLayout(ordersDbGroupBox);

    // Statistics
    QHBoxLayout* ordersStatsLayout = new QHBoxLayout();
    ordersDbCountLabel = new QLabel("Order Count: Calculating...");
    ordersDbSizeLabel = new QLabel("Database Size: Calculating...");

    ordersStatsLayout->addWidget(ordersDbCountLabel);
    ordersStatsLayout->addSpacing(20);
    ordersStatsLayout->addWidget(ordersDbSizeLabel);
    ordersStatsLayout->addStretch();

    ordersDbLayout->addLayout(ordersStatsLayout);

    // Control buttons for Orders DB
    QHBoxLayout* ordersDbButtonLayout = new QHBoxLayout();

    ordersDbRefreshButton = new QPushButton("Refresh");
    ordersDbClearButton = new QPushButton("Clear Orders Database");
    ordersDbClearButton->setStyleSheet("QPushButton { background-color: #FF4444; color: white; }");

    ordersDbButtonLayout->addWidget(ordersDbRefreshButton);
    ordersDbButtonLayout->addStretch();
    ordersDbButtonLayout->addWidget(ordersDbClearButton);

    ordersDbLayout->addLayout(ordersDbButtonLayout);

    mainLayout->addWidget(ordersDbGroupBox);

    // Connect signals
    connect(refreshButton, &QPushButton::clicked, this, &CacheTab::refreshCacheInfo);
    connect(clearSelectedButton, &QPushButton::clicked, this, &CacheTab::clearSelectedCache);
    connect(clearAllButton, &QPushButton::clicked, this, &CacheTab::clearAllCache);
    connect(ordersDbRefreshButton, &QPushButton::clicked, this, &CacheTab::refreshOrdersDbInfo);
    connect(ordersDbClearButton, &QPushButton::clicked, this, &CacheTab::clearOrdersDatabase);
}

void CacheTab::refreshCacheInfo()
{
    populateCacheTable();
}

void CacheTab::populateCacheTable()
{
    cacheTable->setRowCount(0);

    QString cacheDir = getCacheLocation();
    QDir dir(cacheDir);

    if (!dir.exists())
    {
        totalSizeLabel->setText("Total Cache Size: 0 bytes");
        return;
    }

    qint64 totalSize = 0;
    int row = 0;

    // Scan for bar cache files in Bars subdirectory
    QString barsDir = cacheDir + "/" + BARS_CACHE_SUBDIR;
    QDir barsDirObj(barsDir);

    if (barsDirObj.exists())
    {
        QStringList filters;
        filters << "*.db"; // All .db files in Bars directory

        QStringList barCacheFiles = barsDirObj.entryList(filters, QDir::Files);

        for (const QString& fileName: barCacheFiles)
        {
            QFileInfo fileInfo(barsDirObj.absoluteFilePath(fileName));

            cacheTable->insertRow(row);

            // Category
            QTableWidgetItem* categoryItem = new QTableWidgetItem("Bar Data");
            categoryItem->setData(Qt::UserRole,
                                  fileInfo.absoluteFilePath()); // Store full path
            cacheTable->setItem(row, 0, categoryItem);

            // File Name
            cacheTable->setItem(row, 1, new QTableWidgetItem(fileName));

            // Size
            qint64 fileSize = fileInfo.size();
            totalSize += fileSize;
            cacheTable->setItem(row, 2, new QTableWidgetItem(formatFileSize(fileSize)));

            // Last Modified
            cacheTable->setItem(row, 3, new QTableWidgetItem(fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss")));

            row++;
        }
    }

    // Update total size
    totalSizeLabel->setText(QString("Total Cache Size: %1").arg(formatFileSize(totalSize)));
}

QString CacheTab::formatFileSize(qint64 bytes) const
{
    if (bytes >= 1024 * 1024 * 1024)
    {
        double gigabytes = static_cast<double>(bytes) / (1024 * 1024 * 1024);
        return QString("%1 GB").arg(gigabytes, 0, 'f', 2);
    }
    else if (bytes >= 1024 * 1024)
    {
        double megabytes = static_cast<double>(bytes) / (1024 * 1024);
        return QString("%1 MB").arg(megabytes, 0, 'f', 2);
    }
    else if (bytes >= 1024)
    {
        double kilobytes = static_cast<double>(bytes) / 1024;
        return QString("%1 KB").arg(kilobytes, 0, 'f', 2);
    }
    else
    {
        return QString("%1 bytes").arg(bytes);
    }
}

void CacheTab::clearSelectedCache()
{
    QList<QTableWidgetItem*> selectedItems = cacheTable->selectedItems();

    if (selectedItems.isEmpty())
    {
        QMessageBox::information(this, "No Selection", "Please select cache files to clear.");
        return;
    }

    // Get unique rows
    QSet<int> selectedRows;
    for (QTableWidgetItem* item: selectedItems)
    {
        selectedRows.insert(item->row());
    }

    QStringList filesToDelete;
    for (int row: selectedRows)
    {
        QTableWidgetItem* categoryItem = cacheTable->item(row, 0);
        if (categoryItem)
        {
            QString filePath = categoryItem->data(Qt::UserRole).toString();
            if (!filePath.isEmpty())
            {
                filesToDelete.append(cacheTable->item(row, 1)->text());
            }
        }
    }

    if (filesToDelete.isEmpty())
    {
        QMessageBox::warning(this, "Error", "Could not determine files to delete.");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Confirm Deletion",
        QString("Are you sure you want to delete the following cache files?\n\n%1").arg(filesToDelete.join("\n")),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        bool allDeleted = true;
        for (int row: selectedRows)
        {
            QTableWidgetItem* categoryItem = cacheTable->item(row, 0);
            if (categoryItem)
            {
                QString filePath = categoryItem->data(Qt::UserRole).toString();
                if (!filePath.isEmpty())
                {
                    QFile file(filePath);
                    if (!file.remove())
                    {
                        QMessageBox::warning(
                            this,
                            "Deletion Failed",
                            QString("Failed to delete file: %1").arg(cacheTable->item(row, 1)->text()));
                        allDeleted = false;
                    }
                }
            }
        }

        if (allDeleted)
        {
            QMessageBox::information(this, "Success", "Selected cache files have been deleted.");
        }

        refreshCacheInfo();
    }
}

void CacheTab::clearAllCache()
{
    QString cacheDir = getCacheLocation();
    QDir dir(cacheDir);

    if (!dir.exists())
    {
        QMessageBox::information(this, "No Cache", "No cache directory found.");
        return;
    }

    QStringList filesToDelete;
    qint64 totalSize = 0;

    // Scan for bar cache files in Bars subdirectory
    QString barsDir = cacheDir + "/" + BARS_CACHE_SUBDIR;
    QDir barsDirObj(barsDir);

    if (barsDirObj.exists())
    {
        QStringList filters;
        filters << "*.db"; // All .db files in Bars directory

        for (const QString& fileName: barsDirObj.entryList(filters, QDir::Files))
        {
            QFileInfo fileInfo(barsDirObj.absoluteFilePath(fileName));
            filesToDelete.append(barsDirObj.absoluteFilePath(fileName));
            totalSize += fileInfo.size();
        }
    }

    if (filesToDelete.isEmpty())
    {
        QMessageBox::information(this, "No Cache Files", "No cache files found to delete.");
        return;
    }

    QMessageBox::StandardButton reply =
        QMessageBox::question(this,
                              "Confirm Deletion",
                              QString("Are you sure you want to delete ALL cache files?\n\n"
                                      "Files to delete: %1\n"
                                      "Total size: %2")
                                  .arg(filesToDelete.size())
                                  .arg(formatFileSize(totalSize)),
                              QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        bool allDeleted = true;
        for (const QString& filePath: filesToDelete)
        {
            QFile file(filePath);
            if (!file.remove())
            {
                QMessageBox::warning(this, "Deletion Failed", QString("Failed to delete file: %1").arg(filePath));
                allDeleted = false;
            }
        }

        if (allDeleted)
        {
            QMessageBox::information(this, "Success", "All cache files have been deleted.");
        }

        refreshCacheInfo();
    }
}

void CacheTab::refreshOrdersDbInfo()
{
    QString cacheDir = getCacheLocation();
    QString dbPath = cacheDir + "/orders.db";

    QFileInfo dbFileInfo(dbPath);

    if (!dbFileInfo.exists())
    {
        ordersDbCountLabel->setText("Order Count: N/A (database not created yet)");
        ordersDbSizeLabel->setText("Database Size: 0 bytes");
        return;
    }

    // Get database size
    qint64 dbSize = dbFileInfo.size();
    ordersDbSizeLabel->setText(QString("Database Size: %1").arg(formatFileSize(dbSize)));

    // Get order count using singleton instance
    OrdersDatabase* db = OrdersDatabase::getInstance();
    if (db && db->isOpen())
    {
        int orderCount = db->getOrderCount();
        ordersDbCountLabel->setText(QString("Order Count: %1").arg(orderCount));
    }
    else
    {
        ordersDbCountLabel->setText("Order Count: Error reading database");
    }
}

void CacheTab::clearOrdersDatabase()
{
    QString cacheDir = getCacheLocation();
    QString dbPath = cacheDir + "/orders.db";

    QFileInfo dbFileInfo(dbPath);

    if (!dbFileInfo.exists())
    {
        QMessageBox::information(this, "No Database", "Orders database does not exist.");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Confirm Deletion",
        "Are you sure you want to clear ALL orders from the database?\n\n"
        "This will permanently delete all order history including received and filled timestamps.\n"
        "This action cannot be undone.",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        // Use singleton instance to clear database
        OrdersDatabase* db = OrdersDatabase::getInstance();
        if (db && db->isOpen())
        {
            if (db->clearAllOrders())
            {
                QMessageBox::information(this, "Success", "Orders database has been cleared.");
                refreshOrdersDbInfo();
            }
            else
            {
                QMessageBox::warning(this, "Error", "Failed to clear orders database.");
            }
        }
        else
        {
            QMessageBox::warning(this, "Error", "Failed to open orders database.");
        }
    }
}