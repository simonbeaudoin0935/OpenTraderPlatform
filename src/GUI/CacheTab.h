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

class CacheTab : public QWidget {
    Q_OBJECT

public:
    explicit CacheTab(QWidget* parent = nullptr);
    ~CacheTab() override = default;

private slots:
    void refreshCacheInfo();
    void clearSelectedCache();
    void clearAllCache();

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
};