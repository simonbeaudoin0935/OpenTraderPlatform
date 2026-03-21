#include "ConfigTab.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "FrontEnd/GUI/StockPriceChart/StockPriceChart.h"
#include "FrontEnd/GUI/Widgets/TimeAndSales/TimeAndSalesWidget.h"
#include "Misc/CONSTANTS.h"
#include "Misc/Settings.h"

namespace
{
    // Default auto-TF thresholds (lower, upper) in minutes for each timeframe
    const QMap<TimeFrame, std::pair<int, int>> DEFAULT_AUTO_TF_THRESHOLDS = {
        {TimeFrame::TEN_SECONDS, {0, 30}},        // 0min to 30min
        {TimeFrame::ONE_MINUTE, {30, 150}},       // 30min to 2.5h
        {TimeFrame::FIVE_MINUTES, {120, 480}},    // 2h to 8h
        {TimeFrame::FIFTEEN_MINUTES, {240, 960}}, // 4h to 16h
        {TimeFrame::THIRTY_MINUTES, {480, 1440}}, // 8h to 24h
        {TimeFrame::ONE_HOUR, {720, 2880}},       // 12h to 2 days
        {TimeFrame::FOUR_HOURS, {1440, 10080}},   // 1 day to 1 week
    };
} // namespace

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

    QGroupBox* closePositionsGroupBox = new QGroupBox("Close Positions Kill Switch");
    QVBoxLayout* closePositionsLayout = new QVBoxLayout(closePositionsGroupBox);

    QHBoxLayout* closePositionsOffsetLayout = new QHBoxLayout();
    QLabel* closePositionsOffsetLabel = new QLabel("Aggressive Limit Offset:");
    m_closePositionsAggressivitySpinBox = new QDoubleSpinBox();
    m_closePositionsAggressivitySpinBox->setMinimum(ClosePositionsConstants::MIN_AGGRESSIVE_LIMIT_OFFSET_CENTS);
    m_closePositionsAggressivitySpinBox->setMaximum(ClosePositionsConstants::MAX_AGGRESSIVE_LIMIT_OFFSET_CENTS);
    m_closePositionsAggressivitySpinBox->setDecimals(2);
    m_closePositionsAggressivitySpinBox->setSingleStep(ClosePositionsConstants::AGGRESSIVE_LIMIT_OFFSET_STEP_CENTS);
    m_closePositionsAggressivitySpinBox->setValue(ClosePositionsConstants::DEFAULT_AGGRESSIVE_LIMIT_OFFSET_CENTS);
    m_closePositionsAggressivitySpinBox->setSuffix(" c");
    m_closePositionsAggressivitySpinBox->setToolTip(
        "Extended-hours close-position orders use aggressive limit pricing.\n"
        "Buy-to-cover orders are priced at Ask + offset.\n"
        "Sell orders are priced at Bid - offset.\n"
        "Outside regular hours the kill switch forces Day+ time-in-force.");
    connect(m_closePositionsAggressivitySpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &ConfigTab::onClosePositionsAggressivityChanged);
    closePositionsOffsetLayout->addWidget(closePositionsOffsetLabel);
    closePositionsOffsetLayout->addWidget(m_closePositionsAggressivitySpinBox);
    closePositionsOffsetLayout->addStretch();
    closePositionsLayout->addLayout(closePositionsOffsetLayout);

    mainLayout->addWidget(closePositionsGroupBox);

    // Auto-Timeframe Configuration section
    QGroupBox* autoTfGroupBox = new QGroupBox("Auto-Timeframe Thresholds (minutes)");
    autoTfGroupBox->setToolTip("Configure the visible range (in minutes) at which the chart automatically\n"
                               "switches between timeframes when 'Auto' is enabled.\n\n"
                               "Lower: Switch to finer timeframe when visible range drops below this\n"
                               "Upper: Switch to coarser timeframe when visible range exceeds this");
    QVBoxLayout* autoTfLayout = new QVBoxLayout(autoTfGroupBox);

    // Create threshold controls for each intraday timeframe
    const QList<TimeFrame> intradayTfs = {TimeFrame::TEN_SECONDS,
                                          TimeFrame::ONE_MINUTE,
                                          TimeFrame::FIVE_MINUTES,
                                          TimeFrame::FIFTEEN_MINUTES,
                                          TimeFrame::THIRTY_MINUTES,
                                          TimeFrame::ONE_HOUR,
                                          TimeFrame::FOUR_HOURS};

    for (TimeFrame tf: intradayTfs)
    {
        QHBoxLayout* rowLayout = new QHBoxLayout();

        QString tfName = timeFrameToString(tf);
        QLabel* tfLabel = new QLabel(tfName + ":");
        tfLabel->setMinimumWidth(40);

        auto defaults = DEFAULT_AUTO_TF_THRESHOLDS.value(tf);

        QLabel* lowerLabel = new QLabel("Lower:");
        QSpinBox* lowerSpinBox = new QSpinBox();
        lowerSpinBox->setMinimum(0);
        lowerSpinBox->setMaximum(20160); // 2 weeks in minutes
        lowerSpinBox->setValue(defaults.first);
        lowerSpinBox->setSingleStep(30);
        lowerSpinBox->setSuffix(" min");
        connect(lowerSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ConfigTab::onAutoTfThresholdChanged);

        QLabel* upperLabel = new QLabel("Upper:");
        QSpinBox* upperSpinBox = new QSpinBox();
        upperSpinBox->setMinimum(0);
        upperSpinBox->setMaximum(20160);
        upperSpinBox->setValue(defaults.second);
        upperSpinBox->setSingleStep(30);
        upperSpinBox->setSuffix(" min");
        connect(upperSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ConfigTab::onAutoTfThresholdChanged);

        rowLayout->addWidget(tfLabel);
        rowLayout->addWidget(lowerLabel);
        rowLayout->addWidget(lowerSpinBox);
        rowLayout->addSpacing(20);
        rowLayout->addWidget(upperLabel);
        rowLayout->addWidget(upperSpinBox);
        rowLayout->addStretch();

        autoTfLayout->addLayout(rowLayout);

        m_autoTfThresholds[tf] = {lowerSpinBox, upperSpinBox};
    }

    mainLayout->addWidget(autoTfGroupBox);
    mainLayout->addStretch();
}

void ConfigTab::loadSettings()
{
    Q_CHECK_PTR(appStateSettings);

    int maxEntries =
        appStateSettings->value("Config/TimeAndSalesMaxEntries", TimeAndSalesConstants::DEFAULT_MAX_ENTRIES).toInt();
    m_timeAndSalesMaxEntriesSpinBox->setValue(maxEntries);

    const double closePositionsOffset = appStateSettings
                                            ->value(ClosePositionsConstants::SETTINGS_KEY_AGGRESSIVE_LIMIT_OFFSET_CENTS,
                                                    ClosePositionsConstants::DEFAULT_AGGRESSIVE_LIMIT_OFFSET_CENTS)
                                            .toDouble();
    m_closePositionsAggressivitySpinBox->setValue(closePositionsOffset);

    // Load auto-TF thresholds
    for (auto it = m_autoTfThresholds.begin(); it != m_autoTfThresholds.end(); ++it)
    {
        TimeFrame tf = it.key();
        auto& widgets = it.value();
        auto defaults = DEFAULT_AUTO_TF_THRESHOLDS.value(tf);

        QString tfKey = timeFrameToString(tf);
        int lower = appStateSettings->value("Config/AutoTF/" + tfKey + "/Lower", defaults.first).toInt();
        int upper = appStateSettings->value("Config/AutoTF/" + tfKey + "/Upper", defaults.second).toInt();

        widgets.lower->setValue(lower);
        widgets.upper->setValue(upper);
    }
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

void ConfigTab::onAutoTfThresholdChanged()
{
    // Save all thresholds
    for (auto it = m_autoTfThresholds.begin(); it != m_autoTfThresholds.end(); ++it)
    {
        TimeFrame tf = it.key();
        auto& widgets = it.value();
        QString tfKey = timeFrameToString(tf);

        saveSetting("Config/AutoTF/" + tfKey + "/Lower", widgets.lower->value());
        saveSetting("Config/AutoTF/" + tfKey + "/Upper", widgets.upper->value());
    }

    emit autoTimeFrameThresholdsChanged();
}

void ConfigTab::onClosePositionsAggressivityChanged(const double value)
{
    saveSetting(ClosePositionsConstants::SETTINGS_KEY_AGGRESSIVE_LIMIT_OFFSET_CENTS, value);
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

void ConfigTab::setStockPriceChart(StockPriceChart* p_chart)
{
    m_stockPriceChart = p_chart;
}

std::pair<int, int> ConfigTab::getAutoTimeFrameThresholds(TimeFrame tf) const
{
    if (m_autoTfThresholds.contains(tf))
    {
        const auto& widgets = m_autoTfThresholds[tf];
        return {widgets.lower->value(), widgets.upper->value()};
    }
    // Return defaults for unknown timeframes
    return DEFAULT_AUTO_TF_THRESHOLDS.value(tf, {0, INT_MAX});
}
