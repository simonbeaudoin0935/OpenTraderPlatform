#include "CacheTab.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QDirIterator>
#include <QStandardPaths>

CacheTab::CacheTab(QWidget* parent)
    : QWidget(parent),
      cacheTable(nullptr),
      refreshButton(nullptr),
      clearSelectedButton(nullptr),
      clearAllButton(nullptr),
      totalSizeLabel(nullptr),
      refreshTimer(nullptr)
{
    setupUI();
    refreshCacheInfo();

    // Set up auto-refresh timer (every 30 seconds)
    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &CacheTab::refreshCacheInfo);
    refreshTimer->start(30000);
}

void CacheTab::setupUI() {
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
    cacheTable->setColumnWidth(0, 120);  // Category
    cacheTable->setColumnWidth(1, 200);  // File Name
    cacheTable->setColumnWidth(2, 100);  // Size

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

    // Connect signals
    connect(refreshButton, &QPushButton::clicked, this, &CacheTab::refreshCacheInfo);
    connect(clearSelectedButton, &QPushButton::clicked, this, &CacheTab::clearSelectedCache);
    connect(clearAllButton, &QPushButton::clicked, this, &CacheTab::clearAllCache);
}

void CacheTab::refreshCacheInfo() {
    populateCacheTable();
}

void CacheTab::populateCacheTable() {
    cacheTable->setRowCount(0);

    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir dir(cacheDir);

    if (!dir.exists()) {
        totalSizeLabel->setText("Total Cache Size: 0 bytes");
        return;
    }

    // Find all cache files
    QStringList filters;
    filters << "bars_cache_*.db";  // Bar cache files
    // Add more filters for other cache categories as they are implemented

    qint64 totalSize = 0;
    int row = 0;

    // Scan for bar cache files
    QStringList barCacheFiles = dir.entryList(filters, QDir::Files);

    for (const QString& fileName : barCacheFiles) {
        QFileInfo fileInfo(dir.absoluteFilePath(fileName));

        cacheTable->insertRow(row);

        // Category
        QTableWidgetItem* categoryItem = new QTableWidgetItem("Bar Data");
        categoryItem->setData(Qt::UserRole, fileInfo.absoluteFilePath());  // Store full path
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

    // Update total size
    totalSizeLabel->setText(QString("Total Cache Size: %1").arg(formatFileSize(totalSize)));
}

QString CacheTab::formatFileSize(qint64 bytes) const {
    if (bytes >= 1024 * 1024 * 1024) {
        double gigabytes = static_cast<double>(bytes) / (1024 * 1024 * 1024);
        return QString("%1 GB").arg(gigabytes, 0, 'f', 2);
    } else if (bytes >= 1024 * 1024) {
        double megabytes = static_cast<double>(bytes) / (1024 * 1024);
        return QString("%1 MB").arg(megabytes, 0, 'f', 2);
    } else if (bytes >= 1024) {
        double kilobytes = static_cast<double>(bytes) / 1024;
        return QString("%1 KB").arg(kilobytes, 0, 'f', 2);
    } else {
        return QString("%1 bytes").arg(bytes);
    }
}

void CacheTab::clearSelectedCache() {
    QList<QTableWidgetItem*> selectedItems = cacheTable->selectedItems();

    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "No Selection", "Please select cache files to clear.");
        return;
    }

    // Get unique rows
    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems) {
        selectedRows.insert(item->row());
    }

    QStringList filesToDelete;
    for (int row : selectedRows) {
        QTableWidgetItem* categoryItem = cacheTable->item(row, 0);
        if (categoryItem) {
            QString filePath = categoryItem->data(Qt::UserRole).toString();
            if (!filePath.isEmpty()) {
                filesToDelete.append(cacheTable->item(row, 1)->text());
            }
        }
    }

    if (filesToDelete.isEmpty()) {
        QMessageBox::warning(this, "Error", "Could not determine files to delete.");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Confirm Deletion",
        QString("Are you sure you want to delete the following cache files?\n\n%1").arg(filesToDelete.join("\n")),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        bool allDeleted = true;
        for (int row : selectedRows) {
            QTableWidgetItem* categoryItem = cacheTable->item(row, 0);
            if (categoryItem) {
                QString filePath = categoryItem->data(Qt::UserRole).toString();
                if (!filePath.isEmpty()) {
                    QFile file(filePath);
                    if (!file.remove()) {
                        QMessageBox::warning(this, "Deletion Failed",
                            QString("Failed to delete file: %1").arg(cacheTable->item(row, 1)->text()));
                        allDeleted = false;
                    }
                }
            }
        }

        if (allDeleted) {
            QMessageBox::information(this, "Success", "Selected cache files have been deleted.");
        }

        refreshCacheInfo();
    }
}

void CacheTab::clearAllCache() {
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir dir(cacheDir);

    if (!dir.exists()) {
        QMessageBox::information(this, "No Cache", "No cache directory found.");
        return;
    }

    // Find all cache files
    QStringList filters;
    filters << "bars_cache_*.db";  // Bar cache files
    // Add more filters for other cache categories as they are implemented

    QStringList filesToDelete;
    qint64 totalSize = 0;

    for (const QString& fileName : dir.entryList(filters, QDir::Files)) {
        QFileInfo fileInfo(dir.absoluteFilePath(fileName));
        filesToDelete.append(fileName);
        totalSize += fileInfo.size();
    }

    if (filesToDelete.isEmpty()) {
        QMessageBox::information(this, "No Cache Files", "No cache files found to delete.");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Confirm Deletion",
        QString("Are you sure you want to delete ALL cache files?\n\n"
                "Files to delete: %1\n"
                "Total size: %2").arg(filesToDelete.size()).arg(formatFileSize(totalSize)),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        bool allDeleted = true;
        for (const QString& fileName : filesToDelete) {
            QString filePath = dir.absoluteFilePath(fileName);
            QFile file(filePath);
            if (!file.remove()) {
                QMessageBox::warning(this, "Deletion Failed",
                    QString("Failed to delete file: %1").arg(fileName));
                allDeleted = false;
            }
        }

        if (allDeleted) {
            QMessageBox::information(this, "Success", "All cache files have been deleted.");
        }

        refreshCacheInfo();
    }
}