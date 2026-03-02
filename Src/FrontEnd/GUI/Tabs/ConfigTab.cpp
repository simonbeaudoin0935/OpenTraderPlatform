#include "ConfigTab.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "FrontEnd/GUI/Widgets/TimeAndSales/TimeAndSalesWidget.h"
#include "Misc/CONSTANTS.h"
#include "Misc/Settings.h"

ConfigTab::ConfigTab(QWidget* parent) : QWidget(parent), m_timeAndSalesMaxEntriesSpinBox(nullptr)
{
    setupUI();
    loadSettings();
}

void ConfigTab::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Display Configuration section
    QGroupBox* displayGroupBox = new QGroupBox("Display Configuration");
    QVBoxLayout* displayLayout = new QVBoxLayout(displayGroupBox);

    // Time & Sales max entries control
    QHBoxLayout* tsMaxEntriesLayout = new QHBoxLayout();
    QLabel* tsMaxEntriesLabel = new QLabel("Time && Sales Max Entries:");
    m_timeAndSalesMaxEntriesSpinBox = new QSpinBox();
    m_timeAndSalesMaxEntriesSpinBox->setMinimum(50);
    m_timeAndSalesMaxEntriesSpinBox->setMaximum(2000);
    m_timeAndSalesMaxEntriesSpinBox->setValue(TimeAndSalesConstants::DEFAULT_MAX_ENTRIES);
    m_timeAndSalesMaxEntriesSpinBox->setSingleStep(50);
    m_timeAndSalesMaxEntriesSpinBox->setToolTip("Maximum number of trade entries displayed in the Time & Sales tape.\n"
                                                "Higher values use more memory. Default: 200.");
    connect(m_timeAndSalesMaxEntriesSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ConfigTab::onTimeAndSalesMaxEntriesChanged);
    tsMaxEntriesLayout->addWidget(tsMaxEntriesLabel);
    tsMaxEntriesLayout->addWidget(m_timeAndSalesMaxEntriesSpinBox);
    tsMaxEntriesLayout->addStretch();
    displayLayout->addLayout(tsMaxEntriesLayout);

    mainLayout->addWidget(displayGroupBox);
    mainLayout->addStretch();
}

void ConfigTab::loadSettings()
{
    Q_CHECK_PTR(appStateSettings);

    int maxEntries =
        appStateSettings->value("Config/TimeAndSalesMaxEntries", TimeAndSalesConstants::DEFAULT_MAX_ENTRIES).toInt();
    m_timeAndSalesMaxEntriesSpinBox->setValue(maxEntries);
}

void ConfigTab::saveSetting(const QString& key, const QVariant& value)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue(key, value);
    appStateSettings->sync();
}

void ConfigTab::onTimeAndSalesMaxEntriesChanged(int value)
{
    saveSetting("Config/TimeAndSalesMaxEntries", value);

    if (m_timeAndSalesWidget != nullptr)
    {
        m_timeAndSalesWidget->setMaxRows(value);
    }
}

void ConfigTab::setTimeAndSalesWidget(TimeAndSalesWidget* p_widget)
{
    m_timeAndSalesWidget = p_widget;

    // Apply the current setting immediately
    if (m_timeAndSalesWidget != nullptr)
    {
        m_timeAndSalesWidget->setMaxRows(m_timeAndSalesMaxEntriesSpinBox->value());
    }
}
