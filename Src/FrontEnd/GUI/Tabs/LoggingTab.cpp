#include "LoggingTab.h"
#include <QGroupBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QDir>

#include "Misc/Logging.h"
#include "Misc/Settings.h"

LoggingTab::LoggingTab(QWidget* parent)
    : QWidget(parent)
    , categoryCheckBoxLayout(nullptr)
    , loggerVisibilityCheckBox(nullptr)
    , logDepthSpinBox(nullptr)
    , globalDebugDisableCheckBox(nullptr)
    , globalInfoDisableCheckBox(nullptr)
    , clearLogsButton(nullptr)
{
    setupUI();
    populateCategoryCheckboxes();
    loadSettings();
}

void LoggingTab::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Logger Widget Controls section
    QGroupBox* loggerControlsGroupBox = new QGroupBox("Logger Widget Controls");
    QVBoxLayout* loggerControlsLayout = new QVBoxLayout(loggerControlsGroupBox);

    // Visibility checkbox
    loggerVisibilityCheckBox = new QCheckBox("Show Logger Widget");
    loggerVisibilityCheckBox->setChecked(true); // Default to visible
    connect(loggerVisibilityCheckBox, &QCheckBox::toggled, this, &LoggingTab::onLoggerVisibilityToggled);
    loggerControlsLayout->addWidget(loggerVisibilityCheckBox);

    // Log depth control
    QHBoxLayout* logDepthLayout = new QHBoxLayout();
    QLabel* logDepthLabel = new QLabel("Maximum Log Lines:");
    logDepthSpinBox = new QSpinBox();
    logDepthSpinBox->setMinimum(100);
    logDepthSpinBox->setMaximum(10000);
    logDepthSpinBox->setValue(1000); // Default value
    logDepthSpinBox->setSingleStep(100);
    connect(logDepthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &LoggingTab::onLogDepthValueChanged);
    logDepthLayout->addWidget(logDepthLabel);
    logDepthLayout->addWidget(logDepthSpinBox);
    logDepthLayout->addStretch();
    loggerControlsLayout->addLayout(logDepthLayout);

    // Global debug disable checkbox
    globalDebugDisableCheckBox = new QCheckBox("Disable Debug Messages Globally");
    globalDebugDisableCheckBox->setChecked(LoggingConfig::instance().isDebugDisabled());
    connect(globalDebugDisableCheckBox, &QCheckBox::toggled, this, &LoggingTab::onGlobalDebugDisableToggled);
    loggerControlsLayout->addWidget(globalDebugDisableCheckBox);

    // Global info disable checkbox
    globalInfoDisableCheckBox = new QCheckBox("Disable Info Messages Globally");
    globalInfoDisableCheckBox->setChecked(LoggingConfig::instance().isInfoDisabled());
    connect(globalInfoDisableCheckBox, &QCheckBox::toggled, this, &LoggingTab::onGlobalInfoDisableToggled);
    loggerControlsLayout->addWidget(globalInfoDisableCheckBox);

    // Clear log folder button
    clearLogsButton = new QPushButton("Clear Log Folder");
    clearLogsButton->setStyleSheet("QPushButton { background-color: #FF4444; color: white; }");
    connect(clearLogsButton, &QPushButton::clicked, this, &LoggingTab::onClearLogsFolderClicked, Qt::UniqueConnection);
    loggerControlsLayout->addWidget(clearLogsButton);

    mainLayout->addWidget(loggerControlsGroupBox);

    // Category management section
    QGroupBox* categoryGroupBox = new QGroupBox("Logging Categories");
    QVBoxLayout* categoryLayout = new QVBoxLayout(categoryGroupBox);

    QScrollArea* categoryScrollArea = new QScrollArea();
    categoryScrollArea->setWidgetResizable(true);

    QWidget* categoryScrollWidget = new QWidget();
    categoryCheckBoxLayout = new QVBoxLayout(categoryScrollWidget);
    categoryCheckBoxLayout->setAlignment(Qt::AlignTop);

    categoryScrollArea->setWidget(categoryScrollWidget);
    categoryLayout->addWidget(categoryScrollArea);

    // Add category section to main layout
    mainLayout->addWidget(categoryGroupBox);
}

void LoggingTab::loadSettings()
{
    Q_CHECK_PTR(appStateSettings);

    // Load logger visibility
    bool loggerVisible = appStateSettings->value("Logging/LoggerVisible", true).toBool();
    loggerVisibilityCheckBox->setChecked(loggerVisible);

    // Load log depth
    int logDepth = appStateSettings->value("Logging/LogDepth", 1000).toInt();
    logDepthSpinBox->setValue(logDepth);
}

void LoggingTab::saveSetting(const QString& key, const QVariant& value)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue(key, value);
    appStateSettings->sync();
}

void LoggingTab::populateCategoryCheckboxes()
{
    // Clear existing checkboxes
    QLayoutItem* item;
    while ((item = categoryCheckBoxLayout->takeAt(0)) != nullptr)
    {
        if (item->widget())
        {
            delete item->widget();
        }
        delete item;
    }
    categoryCheckBoxes.clear();

    // Populate category checkboxes
    QStringList categories = LoggingConfig::instance().getCategories();
    for (const QString& category: categories)
    {
        QCheckBox* checkBox = new QCheckBox(category);
        checkBox->setChecked(LoggingConfig::instance().isCategoryEnabled(category));
        checkBox->setProperty("category", category);

        connect(checkBox, &QCheckBox::toggled, this, &LoggingTab::onCategoryCheckBoxToggled);

        categoryCheckBoxes[category] = checkBox;
        categoryCheckBoxLayout->addWidget(checkBox);
    }

    // Add stretch to push checkboxes to the top
    categoryCheckBoxLayout->addStretch();
}

void LoggingTab::onCategoryCheckBoxToggled(bool checked)
{
    QCheckBox* checkBox = qobject_cast<QCheckBox*>(sender());
    if (checkBox)
    {
        QString category = checkBox->property("category").toString();
        LoggingConfig::instance().setCategoryEnabled(category, checked);

        QString status = checked ? "enabled" : "disabled";
        qInfo() << "Category" << category << status;
    }
}

void LoggingTab::onLoggerVisibilityToggled(bool checked)
{
    saveSetting("Logging/LoggerVisible", checked);
    emit loggerVisibilityChanged(checked);
}

void LoggingTab::onLogDepthValueChanged(int value)
{
    saveSetting("Logging/LogDepth", value);
    emit logDepthChanged(value);
}

void LoggingTab::onGlobalDebugDisableToggled(bool checked)
{
    LoggingConfig::instance().setDebugDisabled(checked);
    qInfo() << "Global debug messages" << (checked ? "disabled" : "enabled");
}

void LoggingTab::onGlobalInfoDisableToggled(bool checked)
{
    LoggingConfig::instance().setInfoDisabled(checked);
    qInfo() << "Global info messages" << (checked ? "disabled" : "enabled");
}

void LoggingTab::onClearLogsFolderClicked()
{
    QString logsDirPath = getLogsFolderPath();
    QDir logsDir(logsDirPath);

    if (!logsDir.exists())
    {
        QMessageBox::information(this, "Log Folder", "Log folder does not exist.");
        return;
    }

    QStringList logFiles = logsDir.entryList({"*.log.ansi"}, QDir::Files);

    if (logFiles.isEmpty())
    {
        QMessageBox::information(this, "Log Folder", "No log files found to delete.");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Confirm Deletion",
        QString("Are you sure you want to delete %1 log file(s) from:\n%2\n\nThe current session log will be skipped.")
            .arg(logFiles.size())
            .arg(logsDirPath),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
    {
        return;
    }

    int deletedCount = 0;
    int skippedCount = 0;
    for (const QString& fileName: logFiles)
    {
        QString filePath = logsDir.filePath(fileName);
        if (!QFile::remove(filePath))
        {
            skippedCount++;
        }
        else
        {
            deletedCount++;
        }
    }

    qInfo() << "Log folder cleared:" << deletedCount << "file(s) deleted," << skippedCount << "file(s) skipped.";

    if (skippedCount > 0)
    {
        QMessageBox::information(
            this,
            "Log Folder Cleared",
            QString("Deleted %1 log file(s). %2 file(s) could not be deleted (current session log is kept).")
                .arg(deletedCount)
                .arg(skippedCount));
    }
    else
    {
        QMessageBox::information(this,
                                 "Log Folder Cleared",
                                 QString("Successfully deleted %1 log file(s).").arg(deletedCount));
    }
}