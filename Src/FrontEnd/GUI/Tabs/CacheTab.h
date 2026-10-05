#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>

class OrdersDatabase;

class CacheTab : public QWidget
{
    Q_OBJECT

  public:
    explicit CacheTab(QWidget* parent = nullptr);
    ~CacheTab() override = default;

  private slots:
    /// Refresh cache information from disk
    /// Updates table with current cache file sizes
    void refreshCacheInfo();

    /// Clear selected cache files
    /// Deletes cache files selected in the table
    void clearSelectedCache();

    /// Clear all cache files
    /// Deletes all bar cache files
    void clearAllCache();

    /// Refresh orders database information
    /// Updates counts and size display
    void refreshOrdersDbInfo();

    /// Clear orders database
    /// Deletes all orders from the database
    void clearOrdersDatabase();

  private:
    void setupUI();
    void populateCacheTable();
    QString formatFileSize(qint64 bytes) const;

    QTableWidget* cacheTable;
    QPushButton* refreshButton;
    QPushButton* clearSelectedButton;
    QPushButton* clearAllButton;
    QLabel* totalSizeLabel;
    QTimer* refreshTimer;

    // Orders Database section
    QLabel* ordersDbCountLabel;
    QLabel* ordersDbSizeLabel;
    QPushButton* ordersDbRefreshButton;
    QPushButton* ordersDbClearButton;
};