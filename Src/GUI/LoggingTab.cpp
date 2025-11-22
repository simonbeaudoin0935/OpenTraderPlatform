#include "LoggingTab.h"
#include <QGroupBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

#include "Misc/Logging.h"

LoggingTab::LoggingTab(QWidget* parent)
    : QWidget(parent),
      categoryCheckBoxLayout(nullptr),
      loggerVisibilityCheckBox(nullptr),
      logDepthSpinBox(nullptr),
      globalDebugDisableCheckBox(nullptr),
      globalInfoDisableCheckBox(nullptr)
{
    setupUI();
    populateCategoryCheckboxes();
}

void LoggingTab::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Logger Widget Controls section
    QGroupBox* loggerControlsGroupBox = new QGroupBox("Logger Widget Controls");
    QVBoxLayout* loggerControlsLayout = new QVBoxLayout(loggerControlsGroupBox);

    // Visibility checkbox
    loggerVisibilityCheckBox = new QCheckBox("Show Logger Widget");
    loggerVisibilityCheckBox->setChecked(true);  // Default to visible
    bool connection1 = connect(loggerVisibilityCheckBox, &QCheckBox::toggled, this, &LoggingTab::onLoggerVisibilityToggled, Qt::UniqueConnection);
    Q_ASSERT_X(connection1, "LoggingTab", "Failed to create unique connection for loggerVisibilityCheckBox toggled");
    loggerControlsLayout->addWidget(loggerVisibilityCheckBox);

    // Log depth control
    QHBoxLayout* logDepthLayout = new QHBoxLayout();
    QLabel* logDepthLabel = new QLabel("Maximum Log Lines:");
    logDepthSpinBox = new QSpinBox();
    logDepthSpinBox->setMinimum(100);
    logDepthSpinBox->setMaximum(10000);
    logDepthSpinBox->setValue(1000);  // Default value
    logDepthSpinBox->setSingleStep(100);
    bool connection2 = connect(logDepthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &LoggingTab::onLogDepthValueChanged, Qt::UniqueConnection);
    Q_ASSERT_X(connection2, "LoggingTab", "Failed to create unique connection for logDepthSpinBox valueChanged");
    logDepthLayout->addWidget(logDepthLabel);
    logDepthLayout->addWidget(logDepthSpinBox);
    logDepthLayout->addStretch();
    loggerControlsLayout->addLayout(logDepthLayout);

    // Global debug disable checkbox
    globalDebugDisableCheckBox = new QCheckBox("Disable Debug Messages Globally");
    globalDebugDisableCheckBox->setChecked(LoggingConfig::instance().isDebugDisabled());
    bool connection3 = connect(globalDebugDisableCheckBox, &QCheckBox::toggled, this, &LoggingTab::onGlobalDebugDisableToggled, Qt::UniqueConnection);
    Q_ASSERT_X(connection3, "LoggingTab", "Failed to create unique connection for globalDebugDisableCheckBox toggled");
    loggerControlsLayout->addWidget(globalDebugDisableCheckBox);

    // Global info disable checkbox
    globalInfoDisableCheckBox = new QCheckBox("Disable Info Messages Globally");
    globalInfoDisableCheckBox->setChecked(LoggingConfig::instance().isInfoDisabled());
    bool connection4 = connect(globalInfoDisableCheckBox, &QCheckBox::toggled, this, &LoggingTab::onGlobalInfoDisableToggled, Qt::UniqueConnection);
    Q_ASSERT_X(connection4, "LoggingTab", "Failed to create unique connection for globalInfoDisableCheckBox toggled");
    loggerControlsLayout->addWidget(globalInfoDisableCheckBox);

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

void LoggingTab::populateCategoryCheckboxes() {
    // Clear existing checkboxes
    QLayoutItem* item;
    while ((item = categoryCheckBoxLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    categoryCheckBoxes.clear();

    // Populate category checkboxes
    QStringList categories = LoggingConfig::instance().getCategories();
    for (const QString& category : categories) {
        QCheckBox* checkBox = new QCheckBox(category);
        checkBox->setChecked(LoggingConfig::instance().isCategoryEnabled(category));
        checkBox->setProperty("category", category);

        bool connection = connect(checkBox, &QCheckBox::toggled, this, &LoggingTab::onCategoryCheckBoxToggled, Qt::UniqueConnection);
        Q_ASSERT_X(connection, "LoggingTab::populateCategoryCheckboxes", "Failed to create unique connection for category checkbox toggled");

        categoryCheckBoxes[category] = checkBox;
        categoryCheckBoxLayout->addWidget(checkBox);
    }

    // Add stretch to push checkboxes to the top
    categoryCheckBoxLayout->addStretch();
}

void LoggingTab::onCategoryCheckBoxToggled(bool checked) {
    QCheckBox* checkBox = qobject_cast<QCheckBox*>(sender());
    if (checkBox) {
        QString category = checkBox->property("category").toString();
        LoggingConfig::instance().setCategoryEnabled(category, checked);

        QString status = checked ? "enabled" : "disabled";
        qInfo() << "Category" << category << status;
    }
}

void LoggingTab::onLoggerVisibilityToggled(bool checked) {
    emit loggerVisibilityChanged(checked);
}

void LoggingTab::onLogDepthValueChanged(int value) {
    emit logDepthChanged(value);
}

void LoggingTab::onGlobalDebugDisableToggled(bool checked) {
    LoggingConfig::instance().setDebugDisabled(checked);
    qInfo() << "Global debug messages" << (checked ? "disabled" : "enabled");
}

void LoggingTab::onGlobalInfoDisableToggled(bool checked) {
    LoggingConfig::instance().setInfoDisabled(checked);
    qInfo() << "Global info messages" << (checked ? "disabled" : "enabled");
}