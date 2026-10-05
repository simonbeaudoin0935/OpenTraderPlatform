#include "ConfigTab.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>

#include "FrontEnd/GUI/StockPriceChart/StockPriceChart.h"
#include "FrontEnd/GUI/Widgets/TimeAndSales/TimeAndSalesWidget.h"
#include "Misc/CONSTANTS.h"
#include "Misc/Logging/Logging.h"
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

    QGroupBox* chartFocusGroupBox = new QGroupBox("Chart Focus (Recenter)");
    QVBoxLayout* chartFocusLayout = new QVBoxLayout(chartFocusGroupBox);

    QHBoxLayout* chartFocusLookbackLayout = new QHBoxLayout();
    QLabel* chartFocusLookbackLabel = new QLabel("Lookback Bars:");
    m_chartFocusLookbackBarsSpinBox = new QSpinBox();
    m_chartFocusLookbackBarsSpinBox->setMinimum(ChartFocusConstants::MIN_LOOKBACK_BARS);
    m_chartFocusLookbackBarsSpinBox->setMaximum(ChartFocusConstants::MAX_LOOKBACK_BARS);
    m_chartFocusLookbackBarsSpinBox->setValue(ChartFocusConstants::DEFAULT_LOOKBACK_BARS);
    m_chartFocusLookbackBarsSpinBox->setSingleStep(1);
    m_chartFocusLookbackBarsSpinBox->setToolTip(
        "How many bars to include to the left of the now-line when recentering.");
    connect(m_chartFocusLookbackBarsSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ConfigTab::onChartFocusLookbackBarsChanged);
    chartFocusLookbackLayout->addWidget(chartFocusLookbackLabel);
    chartFocusLookbackLayout->addWidget(m_chartFocusLookbackBarsSpinBox);
    chartFocusLookbackLayout->addStretch();
    chartFocusLayout->addLayout(chartFocusLookbackLayout);

    QHBoxLayout* chartFocusRightPaddingLayout = new QHBoxLayout();
    QLabel* chartFocusRightPaddingLabel = new QLabel("Right Padding Bars:");
    m_chartFocusRightPaddingBarsSpinBox = new QSpinBox();
    m_chartFocusRightPaddingBarsSpinBox->setMinimum(ChartFocusConstants::MIN_RIGHT_PADDING_BARS);
    m_chartFocusRightPaddingBarsSpinBox->setMaximum(ChartFocusConstants::MAX_RIGHT_PADDING_BARS);
    m_chartFocusRightPaddingBarsSpinBox->setValue(ChartFocusConstants::DEFAULT_RIGHT_PADDING_BARS);
    m_chartFocusRightPaddingBarsSpinBox->setSingleStep(1);
    m_chartFocusRightPaddingBarsSpinBox->setToolTip(
        "How many bars to include to the right of the now-line when recentering.");
    connect(m_chartFocusRightPaddingBarsSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ConfigTab::onChartFocusRightPaddingBarsChanged);
    chartFocusRightPaddingLayout->addWidget(chartFocusRightPaddingLabel);
    chartFocusRightPaddingLayout->addWidget(m_chartFocusRightPaddingBarsSpinBox);
    chartFocusRightPaddingLayout->addStretch();
    chartFocusLayout->addLayout(chartFocusRightPaddingLayout);

    QHBoxLayout* chartFocusIncludeBboLayout = new QHBoxLayout();
    m_chartFocusIncludeBboCheckBox = new QCheckBox("Include BBO in Y range");
    m_chartFocusIncludeBboCheckBox->setChecked(ChartFocusConstants::DEFAULT_INCLUDE_BBO_IN_Y_RANGE);
    m_chartFocusIncludeBboCheckBox->setToolTip(
        "When enabled, recenter/focus expands Y range to include latest best bid and ask prices.");
    connect(m_chartFocusIncludeBboCheckBox, &QCheckBox::toggled, this, &ConfigTab::onChartFocusIncludeBboChanged);
    chartFocusIncludeBboLayout->addWidget(m_chartFocusIncludeBboCheckBox);
    chartFocusIncludeBboLayout->addStretch();
    chartFocusLayout->addLayout(chartFocusIncludeBboLayout);

    QHBoxLayout* chartFocusYPaddingLayout = new QHBoxLayout();
    QLabel* chartFocusYPaddingLabel = new QLabel("Y Padding:");
    m_chartFocusYPaddingPercentSpinBox = new QDoubleSpinBox();
    m_chartFocusYPaddingPercentSpinBox->setMinimum(ChartFocusConstants::MIN_Y_PADDING_PERCENT);
    m_chartFocusYPaddingPercentSpinBox->setMaximum(ChartFocusConstants::MAX_Y_PADDING_PERCENT);
    m_chartFocusYPaddingPercentSpinBox->setValue(ChartFocusConstants::DEFAULT_Y_PADDING_PERCENT);
    m_chartFocusYPaddingPercentSpinBox->setDecimals(2);
    m_chartFocusYPaddingPercentSpinBox->setSingleStep(0.25);
    m_chartFocusYPaddingPercentSpinBox->setSuffix(" %");
    m_chartFocusYPaddingPercentSpinBox->setToolTip("Percent padding applied around the focused candle/BBO range.");
    connect(m_chartFocusYPaddingPercentSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &ConfigTab::onChartFocusYPaddingPercentChanged);
    chartFocusYPaddingLayout->addWidget(chartFocusYPaddingLabel);
    chartFocusYPaddingLayout->addWidget(m_chartFocusYPaddingPercentSpinBox);
    chartFocusYPaddingLayout->addStretch();
    chartFocusLayout->addLayout(chartFocusYPaddingLayout);

    QHBoxLayout* chartFocusMinRangeLayout = new QHBoxLayout();
    QLabel* chartFocusMinRangeLabel = new QLabel("Min Range:");
    m_chartFocusMinRangePercentSpinBox = new QDoubleSpinBox();
    m_chartFocusMinRangePercentSpinBox->setMinimum(ChartFocusConstants::MIN_MIN_RANGE_PERCENT);
    m_chartFocusMinRangePercentSpinBox->setMaximum(ChartFocusConstants::MAX_MIN_RANGE_PERCENT);
    m_chartFocusMinRangePercentSpinBox->setValue(ChartFocusConstants::DEFAULT_MIN_RANGE_PERCENT);
    m_chartFocusMinRangePercentSpinBox->setDecimals(3);
    m_chartFocusMinRangePercentSpinBox->setSingleStep(0.01);
    m_chartFocusMinRangePercentSpinBox->setSuffix(" %");
    m_chartFocusMinRangePercentSpinBox->setToolTip("Minimum focused Y range as a percent of the anchor price.");
    connect(m_chartFocusMinRangePercentSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &ConfigTab::onChartFocusMinRangePercentChanged);
    chartFocusMinRangeLayout->addWidget(chartFocusMinRangeLabel);
    chartFocusMinRangeLayout->addWidget(m_chartFocusMinRangePercentSpinBox);
    chartFocusMinRangeLayout->addStretch();
    chartFocusLayout->addLayout(chartFocusMinRangeLayout);

    QHBoxLayout* chartFocusAnchorPaddingLayout = new QHBoxLayout();
    QLabel* chartFocusAnchorPaddingLabel = new QLabel("Anchor Padding:");
    m_chartFocusAnchorPaddingPercentSpinBox = new QDoubleSpinBox();
    m_chartFocusAnchorPaddingPercentSpinBox->setMinimum(ChartFocusConstants::MIN_ANCHOR_PADDING_PERCENT);
    m_chartFocusAnchorPaddingPercentSpinBox->setMaximum(ChartFocusConstants::MAX_ANCHOR_PADDING_PERCENT);
    m_chartFocusAnchorPaddingPercentSpinBox->setValue(ChartFocusConstants::DEFAULT_ANCHOR_PADDING_PERCENT);
    m_chartFocusAnchorPaddingPercentSpinBox->setDecimals(3);
    m_chartFocusAnchorPaddingPercentSpinBox->setSingleStep(0.01);
    m_chartFocusAnchorPaddingPercentSpinBox->setSuffix(" %");
    m_chartFocusAnchorPaddingPercentSpinBox->setToolTip(
        "Minimum safety padding around the anchor price used during recenter.");
    connect(m_chartFocusAnchorPaddingPercentSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &ConfigTab::onChartFocusAnchorPaddingPercentChanged);
    chartFocusAnchorPaddingLayout->addWidget(chartFocusAnchorPaddingLabel);
    chartFocusAnchorPaddingLayout->addWidget(m_chartFocusAnchorPaddingPercentSpinBox);
    chartFocusAnchorPaddingLayout->addStretch();
    chartFocusLayout->addLayout(chartFocusAnchorPaddingLayout);

    mainLayout->addWidget(chartFocusGroupBox);

    QGroupBox* strategyConfirmationGroupBox = new QGroupBox("Strategy Entry Confirmation");
    QVBoxLayout* strategyConfirmationLayout = new QVBoxLayout(strategyConfirmationGroupBox);

    QHBoxLayout* confirmationTimeoutLayout = new QHBoxLayout();
    QLabel* confirmationTimeoutLabel = new QLabel("Manual Confirmation Timeout:");
    m_strategyConfirmationTimeoutSpinBox = new QSpinBox();
    m_strategyConfirmationTimeoutSpinBox->setMinimum(StrategyManualConfirmationConstants::MIN_TIMEOUT_SECONDS);
    m_strategyConfirmationTimeoutSpinBox->setMaximum(StrategyManualConfirmationConstants::MAX_TIMEOUT_SECONDS);
    m_strategyConfirmationTimeoutSpinBox->setValue(StrategyManualConfirmationConstants::DEFAULT_TIMEOUT_SECONDS);
    m_strategyConfirmationTimeoutSpinBox->setSingleStep(1);
    m_strategyConfirmationTimeoutSpinBox->setSuffix(" s");
    m_strategyConfirmationTimeoutSpinBox->setToolTip(
        "Wall-clock timeout in seconds for strategy orders that require manual user confirmation.\n"
        "Applies in both live and replay modes.");
    connect(m_strategyConfirmationTimeoutSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ConfigTab::onStrategyConfirmationTimeoutChanged);
    confirmationTimeoutLayout->addWidget(confirmationTimeoutLabel);
    confirmationTimeoutLayout->addWidget(m_strategyConfirmationTimeoutSpinBox);
    confirmationTimeoutLayout->addStretch();
    strategyConfirmationLayout->addLayout(confirmationTimeoutLayout);

    QHBoxLayout* marketableOffsetLayout = new QHBoxLayout();
    QLabel* marketableOffsetLabel = new QLabel("Marketable Offset (Ext Hours):");
    m_strategyMarketableOffsetSpinBox = new QDoubleSpinBox();
    m_strategyMarketableOffsetSpinBox->setMinimum(StrategyManualConfirmationConstants::MIN_MARKETABLE_OFFSET_CENTS);
    m_strategyMarketableOffsetSpinBox->setMaximum(StrategyManualConfirmationConstants::MAX_MARKETABLE_OFFSET_CENTS);
    m_strategyMarketableOffsetSpinBox->setDecimals(2);
    m_strategyMarketableOffsetSpinBox->setSingleStep(StrategyManualConfirmationConstants::MARKETABLE_OFFSET_STEP_CENTS);
    m_strategyMarketableOffsetSpinBox->setValue(StrategyManualConfirmationConstants::DEFAULT_MARKETABLE_OFFSET_CENTS);
    m_strategyMarketableOffsetSpinBox->setSuffix(" c");
    m_strategyMarketableOffsetSpinBox->setToolTip(
        "When a strategy uses marketable-on-accept confirmation mode in extended hours,\n"
        "the platform submits a Day+ limit priced from the book:\n"
        "Buy/BuyToCover: Ask + offset, Sell/SellShort: Bid - offset.");
    connect(m_strategyMarketableOffsetSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &ConfigTab::onStrategyMarketableOffsetChanged);
    marketableOffsetLayout->addWidget(marketableOffsetLabel);
    marketableOffsetLayout->addWidget(m_strategyMarketableOffsetSpinBox);
    marketableOffsetLayout->addStretch();
    strategyConfirmationLayout->addLayout(marketableOffsetLayout);

    QHBoxLayout* maxChaseLayout = new QHBoxLayout();
    QLabel* maxChaseLabel = new QLabel("Max Chase:");
    m_strategyMaxChasePercentSpinBox = new QDoubleSpinBox();
    m_strategyMaxChasePercentSpinBox->setMinimum(StrategyManualConfirmationConstants::MIN_MAX_CHASE_PERCENT);
    m_strategyMaxChasePercentSpinBox->setMaximum(StrategyManualConfirmationConstants::MAX_MAX_CHASE_PERCENT);
    m_strategyMaxChasePercentSpinBox->setDecimals(2);
    m_strategyMaxChasePercentSpinBox->setSingleStep(StrategyManualConfirmationConstants::MAX_CHASE_PERCENT_STEP);
    m_strategyMaxChasePercentSpinBox->setValue(StrategyManualConfirmationConstants::DEFAULT_MAX_CHASE_PERCENT);
    m_strategyMaxChasePercentSpinBox->setSuffix(" %");
    m_strategyMaxChasePercentSpinBox->setToolTip(
        "Guardrail for marketable-on-accept pricing.\n"
        "If the accept-time executable price drifts beyond this percentage from the\n"
        "strategy anchor limit price, the confirmation is rejected.");
    connect(m_strategyMaxChasePercentSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &ConfigTab::onStrategyMaxChasePercentChanged);
    maxChaseLayout->addWidget(maxChaseLabel);
    maxChaseLayout->addWidget(m_strategyMaxChasePercentSpinBox);
    maxChaseLayout->addStretch();
    strategyConfirmationLayout->addLayout(maxChaseLayout);

    mainLayout->addWidget(strategyConfirmationGroupBox);

    QGroupBox* bracketWheelGroupBox = new QGroupBox("Bracket Wheel Adjustment");
    QVBoxLayout* bracketWheelLayout = new QVBoxLayout(bracketWheelGroupBox);

    QHBoxLayout* ratioStepLayout = new QHBoxLayout();
    QLabel* ratioStepLabel = new QLabel("R Step (per wheel notch):");
    m_bracketWheelRatioStepSpinBox = new QDoubleSpinBox();
    m_bracketWheelRatioStepSpinBox->setMinimum(BracketWheelConstants::MIN_RATIO_STEP_R);
    m_bracketWheelRatioStepSpinBox->setMaximum(BracketWheelConstants::MAX_RATIO_STEP_R);
    m_bracketWheelRatioStepSpinBox->setDecimals(2);
    m_bracketWheelRatioStepSpinBox->setSingleStep(0.05);
    m_bracketWheelRatioStepSpinBox->setValue(BracketWheelConstants::DEFAULT_RATIO_STEP_R);
    m_bracketWheelRatioStepSpinBox->setToolTip(
        "When bracket wheel mode is RATIO, each wheel notch changes target R by this amount.\n"
        "R = |take-entry| / |entry-stop|.");
    connect(m_bracketWheelRatioStepSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &ConfigTab::onBracketWheelSettingChanged);
    ratioStepLayout->addWidget(ratioStepLabel);
    ratioStepLayout->addWidget(m_bracketWheelRatioStepSpinBox);
    ratioStepLayout->addStretch();
    bracketWheelLayout->addLayout(ratioStepLayout);

    QHBoxLayout* stopStepLayout = new QHBoxLayout();
    QLabel* stopStepLabel = new QLabel("Stop % Step (per wheel notch):");
    m_bracketWheelStopStepPercentSpinBox = new QDoubleSpinBox();
    m_bracketWheelStopStepPercentSpinBox->setMinimum(BracketWheelConstants::MIN_STOP_STEP_PERCENT);
    m_bracketWheelStopStepPercentSpinBox->setMaximum(BracketWheelConstants::MAX_STOP_STEP_PERCENT);
    m_bracketWheelStopStepPercentSpinBox->setDecimals(2);
    m_bracketWheelStopStepPercentSpinBox->setSingleStep(0.25);
    m_bracketWheelStopStepPercentSpinBox->setSuffix(" %");
    m_bracketWheelStopStepPercentSpinBox->setValue(BracketWheelConstants::DEFAULT_STOP_STEP_PERCENT);
    m_bracketWheelStopStepPercentSpinBox->setToolTip(
        "When bracket wheel mode is STOP, each wheel notch changes stop-loss percent from entry by this amount.");
    connect(m_bracketWheelStopStepPercentSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &ConfigTab::onBracketWheelSettingChanged);
    stopStepLayout->addWidget(stopStepLabel);
    stopStepLayout->addWidget(m_bracketWheelStopStepPercentSpinBox);
    stopStepLayout->addStretch();
    bracketWheelLayout->addLayout(stopStepLayout);

    QHBoxLayout* ratioBoundsLayout = new QHBoxLayout();
    QLabel* ratioMinLabel = new QLabel("Min R:");
    m_bracketWheelMinRatioSpinBox = new QDoubleSpinBox();
    m_bracketWheelMinRatioSpinBox->setMinimum(BracketWheelConstants::MIN_RATIO_BOUND);
    m_bracketWheelMinRatioSpinBox->setMaximum(BracketWheelConstants::MAX_RATIO_BOUND);
    m_bracketWheelMinRatioSpinBox->setDecimals(2);
    m_bracketWheelMinRatioSpinBox->setSingleStep(0.05);
    m_bracketWheelMinRatioSpinBox->setValue(BracketWheelConstants::DEFAULT_RATIO_MIN);
    connect(m_bracketWheelMinRatioSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &ConfigTab::onBracketWheelSettingChanged);

    QLabel* ratioMaxLabel = new QLabel("Max R:");
    m_bracketWheelMaxRatioSpinBox = new QDoubleSpinBox();
    m_bracketWheelMaxRatioSpinBox->setMinimum(BracketWheelConstants::MIN_RATIO_BOUND);
    m_bracketWheelMaxRatioSpinBox->setMaximum(BracketWheelConstants::MAX_RATIO_BOUND);
    m_bracketWheelMaxRatioSpinBox->setDecimals(2);
    m_bracketWheelMaxRatioSpinBox->setSingleStep(0.10);
    m_bracketWheelMaxRatioSpinBox->setValue(BracketWheelConstants::DEFAULT_RATIO_MAX);
    connect(m_bracketWheelMaxRatioSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &ConfigTab::onBracketWheelSettingChanged);

    ratioBoundsLayout->addWidget(ratioMinLabel);
    ratioBoundsLayout->addWidget(m_bracketWheelMinRatioSpinBox);
    ratioBoundsLayout->addSpacing(20);
    ratioBoundsLayout->addWidget(ratioMaxLabel);
    ratioBoundsLayout->addWidget(m_bracketWheelMaxRatioSpinBox);
    ratioBoundsLayout->addStretch();
    bracketWheelLayout->addLayout(ratioBoundsLayout);

    QHBoxLayout* stopBoundsLayout = new QHBoxLayout();
    QLabel* stopMinLabel = new QLabel("Min Stop %:");
    m_bracketWheelMinStopPercentSpinBox = new QDoubleSpinBox();
    m_bracketWheelMinStopPercentSpinBox->setMinimum(BracketWheelConstants::MIN_STOP_PERCENT_BOUND);
    m_bracketWheelMinStopPercentSpinBox->setMaximum(BracketWheelConstants::MAX_STOP_PERCENT_BOUND);
    m_bracketWheelMinStopPercentSpinBox->setDecimals(2);
    m_bracketWheelMinStopPercentSpinBox->setSingleStep(0.25);
    m_bracketWheelMinStopPercentSpinBox->setSuffix(" %");
    m_bracketWheelMinStopPercentSpinBox->setValue(BracketWheelConstants::DEFAULT_STOP_PERCENT_MIN);
    connect(m_bracketWheelMinStopPercentSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &ConfigTab::onBracketWheelSettingChanged);

    QLabel* stopMaxLabel = new QLabel("Max Stop %:");
    m_bracketWheelMaxStopPercentSpinBox = new QDoubleSpinBox();
    m_bracketWheelMaxStopPercentSpinBox->setMinimum(BracketWheelConstants::MIN_STOP_PERCENT_BOUND);
    m_bracketWheelMaxStopPercentSpinBox->setMaximum(BracketWheelConstants::MAX_STOP_PERCENT_BOUND);
    m_bracketWheelMaxStopPercentSpinBox->setDecimals(2);
    m_bracketWheelMaxStopPercentSpinBox->setSingleStep(0.50);
    m_bracketWheelMaxStopPercentSpinBox->setSuffix(" %");
    m_bracketWheelMaxStopPercentSpinBox->setValue(BracketWheelConstants::DEFAULT_STOP_PERCENT_MAX);
    connect(m_bracketWheelMaxStopPercentSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &ConfigTab::onBracketWheelSettingChanged);

    stopBoundsLayout->addWidget(stopMinLabel);
    stopBoundsLayout->addWidget(m_bracketWheelMinStopPercentSpinBox);
    stopBoundsLayout->addSpacing(20);
    stopBoundsLayout->addWidget(stopMaxLabel);
    stopBoundsLayout->addWidget(m_bracketWheelMaxStopPercentSpinBox);
    stopBoundsLayout->addStretch();
    bracketWheelLayout->addLayout(stopBoundsLayout);

    mainLayout->addWidget(bracketWheelGroupBox);

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

    const int strategyConfirmationTimeout =
        appStateSettings
            ->value(StrategyManualConfirmationConstants::SETTINGS_KEY_TIMEOUT_SECONDS,
                    StrategyManualConfirmationConstants::DEFAULT_TIMEOUT_SECONDS)
            .toInt();
    m_strategyConfirmationTimeoutSpinBox->setValue(
        std::clamp(strategyConfirmationTimeout,
                   StrategyManualConfirmationConstants::MIN_TIMEOUT_SECONDS,
                   StrategyManualConfirmationConstants::MAX_TIMEOUT_SECONDS));

    const double marketableOffsetCents =
        appStateSettings
            ->value(StrategyManualConfirmationConstants::SETTINGS_KEY_MARKETABLE_OFFSET_CENTS,
                    StrategyManualConfirmationConstants::DEFAULT_MARKETABLE_OFFSET_CENTS)
            .toDouble();
    m_strategyMarketableOffsetSpinBox->setValue(
        std::clamp(marketableOffsetCents,
                   StrategyManualConfirmationConstants::MIN_MARKETABLE_OFFSET_CENTS,
                   StrategyManualConfirmationConstants::MAX_MARKETABLE_OFFSET_CENTS));

    const double maxChasePercent = appStateSettings
                                       ->value(StrategyManualConfirmationConstants::SETTINGS_KEY_MAX_CHASE_PERCENT,
                                               StrategyManualConfirmationConstants::DEFAULT_MAX_CHASE_PERCENT)
                                       .toDouble();
    m_strategyMaxChasePercentSpinBox->setValue(std::clamp(maxChasePercent,
                                                          StrategyManualConfirmationConstants::MIN_MAX_CHASE_PERCENT,
                                                          StrategyManualConfirmationConstants::MAX_MAX_CHASE_PERCENT));

    const int chartFocusLookbackBars =
        appStateSettings
            ->value(ChartFocusConstants::SETTINGS_KEY_LOOKBACK_BARS, ChartFocusConstants::DEFAULT_LOOKBACK_BARS)
            .toInt();
    m_chartFocusLookbackBarsSpinBox->setValue(std::clamp(chartFocusLookbackBars,
                                                         ChartFocusConstants::MIN_LOOKBACK_BARS,
                                                         ChartFocusConstants::MAX_LOOKBACK_BARS));

    const int chartFocusRightPaddingBars = appStateSettings
                                               ->value(ChartFocusConstants::SETTINGS_KEY_RIGHT_PADDING_BARS,
                                                       ChartFocusConstants::DEFAULT_RIGHT_PADDING_BARS)
                                               .toInt();
    m_chartFocusRightPaddingBarsSpinBox->setValue(std::clamp(chartFocusRightPaddingBars,
                                                             ChartFocusConstants::MIN_RIGHT_PADDING_BARS,
                                                             ChartFocusConstants::MAX_RIGHT_PADDING_BARS));

    const bool chartFocusIncludeBbo = appStateSettings
                                          ->value(ChartFocusConstants::SETTINGS_KEY_INCLUDE_BBO_IN_Y_RANGE,
                                                  ChartFocusConstants::DEFAULT_INCLUDE_BBO_IN_Y_RANGE)
                                          .toBool();
    m_chartFocusIncludeBboCheckBox->setChecked(chartFocusIncludeBbo);

    const double chartFocusYPaddingPercent =
        appStateSettings
            ->value(ChartFocusConstants::SETTINGS_KEY_Y_PADDING_PERCENT, ChartFocusConstants::DEFAULT_Y_PADDING_PERCENT)
            .toDouble();
    m_chartFocusYPaddingPercentSpinBox->setValue(std::clamp(chartFocusYPaddingPercent,
                                                            ChartFocusConstants::MIN_Y_PADDING_PERCENT,
                                                            ChartFocusConstants::MAX_Y_PADDING_PERCENT));

    const double chartFocusMinRangePercent =
        appStateSettings
            ->value(ChartFocusConstants::SETTINGS_KEY_MIN_RANGE_PERCENT, ChartFocusConstants::DEFAULT_MIN_RANGE_PERCENT)
            .toDouble();
    m_chartFocusMinRangePercentSpinBox->setValue(std::clamp(chartFocusMinRangePercent,
                                                            ChartFocusConstants::MIN_MIN_RANGE_PERCENT,
                                                            ChartFocusConstants::MAX_MIN_RANGE_PERCENT));

    const double chartFocusAnchorPaddingPercent = appStateSettings
                                                      ->value(ChartFocusConstants::SETTINGS_KEY_ANCHOR_PADDING_PERCENT,
                                                              ChartFocusConstants::DEFAULT_ANCHOR_PADDING_PERCENT)
                                                      .toDouble();
    m_chartFocusAnchorPaddingPercentSpinBox->setValue(std::clamp(chartFocusAnchorPaddingPercent,
                                                                 ChartFocusConstants::MIN_ANCHOR_PADDING_PERCENT,
                                                                 ChartFocusConstants::MAX_ANCHOR_PADDING_PERCENT));

    const double ratioStepR =
        appStateSettings
            ->value(BracketWheelConstants::SETTINGS_KEY_RATIO_STEP_R, BracketWheelConstants::DEFAULT_RATIO_STEP_R)
            .toDouble();
    m_bracketWheelRatioStepSpinBox->setValue(
        std::clamp(ratioStepR, BracketWheelConstants::MIN_RATIO_STEP_R, BracketWheelConstants::MAX_RATIO_STEP_R));

    const double stopStepPercent = appStateSettings
                                       ->value(BracketWheelConstants::SETTINGS_KEY_STOP_STEP_PERCENT,
                                               BracketWheelConstants::DEFAULT_STOP_STEP_PERCENT)
                                       .toDouble();
    m_bracketWheelStopStepPercentSpinBox->setValue(std::clamp(stopStepPercent,
                                                              BracketWheelConstants::MIN_STOP_STEP_PERCENT,
                                                              BracketWheelConstants::MAX_STOP_STEP_PERCENT));

    const double ratioMin =
        appStateSettings->value(BracketWheelConstants::SETTINGS_KEY_RATIO_MIN, BracketWheelConstants::DEFAULT_RATIO_MIN)
            .toDouble();
    const double ratioMax =
        appStateSettings->value(BracketWheelConstants::SETTINGS_KEY_RATIO_MAX, BracketWheelConstants::DEFAULT_RATIO_MAX)
            .toDouble();
    const double ratioMinClamped =
        std::clamp(ratioMin, BracketWheelConstants::MIN_RATIO_BOUND, BracketWheelConstants::MAX_RATIO_BOUND);
    const double ratioMaxClamped =
        std::clamp(ratioMax, BracketWheelConstants::MIN_RATIO_BOUND, BracketWheelConstants::MAX_RATIO_BOUND);
    const double normalizedRatioMin = std::min(ratioMinClamped, ratioMaxClamped);
    const double normalizedRatioMax = std::max(ratioMinClamped, ratioMaxClamped);
    m_bracketWheelMinRatioSpinBox->setValue(normalizedRatioMin);
    m_bracketWheelMaxRatioSpinBox->setValue(normalizedRatioMax);

    const double stopMin = appStateSettings
                               ->value(BracketWheelConstants::SETTINGS_KEY_STOP_PERCENT_MIN,
                                       BracketWheelConstants::DEFAULT_STOP_PERCENT_MIN)
                               .toDouble();
    const double stopMax = appStateSettings
                               ->value(BracketWheelConstants::SETTINGS_KEY_STOP_PERCENT_MAX,
                                       BracketWheelConstants::DEFAULT_STOP_PERCENT_MAX)
                               .toDouble();
    const double stopMinClamped = std::clamp(stopMin,
                                             BracketWheelConstants::MIN_STOP_PERCENT_BOUND,
                                             BracketWheelConstants::MAX_STOP_PERCENT_BOUND);
    const double stopMaxClamped = std::clamp(stopMax,
                                             BracketWheelConstants::MIN_STOP_PERCENT_BOUND,
                                             BracketWheelConstants::MAX_STOP_PERCENT_BOUND);
    const double normalizedStopMin = std::min(stopMinClamped, stopMaxClamped);
    const double normalizedStopMax = std::max(stopMinClamped, stopMaxClamped);
    m_bracketWheelMinStopPercentSpinBox->setValue(normalizedStopMin);
    m_bracketWheelMaxStopPercentSpinBox->setValue(normalizedStopMax);

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
    if (isVisible())
    {
        logInputEvent(u"ConfigTab", u"set-time-and-sales-max-entries", {inputDetail(u"value", value)});
    }

    if (m_timeAndSalesWidget != nullptr)
    {
        m_timeAndSalesWidget->setMaxRows(value);
    }
}

void ConfigTab::onAutoTfThresholdChanged()
{
    // Save all thresholds
    QStringList details;
    for (auto it = m_autoTfThresholds.begin(); it != m_autoTfThresholds.end(); ++it)
    {
        TimeFrame tf = it.key();
        auto& widgets = it.value();
        QString tfKey = timeFrameToString(tf);

        saveSetting("Config/AutoTF/" + tfKey + "/Lower", widgets.lower->value());
        saveSetting("Config/AutoTF/" + tfKey + "/Upper", widgets.upper->value());
        details << inputDetail(tfKey, QString("%1-%2").arg(widgets.lower->value()).arg(widgets.upper->value()));
    }

    if (isVisible())
    {
        logInputEvent(u"ConfigTab", u"update-auto-timeframe-thresholds", details);
    }
    emit autoTimeFrameThresholdsChanged();
}

void ConfigTab::onClosePositionsAggressivityChanged(const double value)
{
    saveSetting(ClosePositionsConstants::SETTINGS_KEY_AGGRESSIVE_LIMIT_OFFSET_CENTS, value);
    if (isVisible())
    {
        logInputEvent(u"ConfigTab", u"set-close-positions-offset-cents", {inputDetail(u"value", value)});
    }
}

void ConfigTab::onStrategyConfirmationTimeoutChanged(const int value)
{
    saveSetting(StrategyManualConfirmationConstants::SETTINGS_KEY_TIMEOUT_SECONDS, value);
    if (isVisible())
    {
        logInputEvent(u"ConfigTab", u"set-strategy-manual-confirm-timeout", {inputDetail(u"valueSec", value)});
    }
}

void ConfigTab::onStrategyMarketableOffsetChanged(const double value)
{
    saveSetting(StrategyManualConfirmationConstants::SETTINGS_KEY_MARKETABLE_OFFSET_CENTS, value);
    if (isVisible())
    {
        logInputEvent(u"ConfigTab",
                      u"set-strategy-manual-confirm-marketable-offset",
                      {inputDetail(u"valueCents", value)});
    }
}

void ConfigTab::onStrategyMaxChasePercentChanged(const double value)
{
    saveSetting(StrategyManualConfirmationConstants::SETTINGS_KEY_MAX_CHASE_PERCENT, value);
    if (isVisible())
    {
        logInputEvent(u"ConfigTab", u"set-strategy-manual-confirm-max-chase", {inputDetail(u"valuePercent", value)});
    }
}

void ConfigTab::onBracketWheelSettingChanged()
{
    const QSignalBlocker ratioMinBlocker(m_bracketWheelMinRatioSpinBox);
    const QSignalBlocker ratioMaxBlocker(m_bracketWheelMaxRatioSpinBox);
    const QSignalBlocker stopMinBlocker(m_bracketWheelMinStopPercentSpinBox);
    const QSignalBlocker stopMaxBlocker(m_bracketWheelMaxStopPercentSpinBox);

    const double ratioMin = std::clamp(m_bracketWheelMinRatioSpinBox->value(),
                                       BracketWheelConstants::MIN_RATIO_BOUND,
                                       BracketWheelConstants::MAX_RATIO_BOUND);
    const double ratioMax = std::clamp(m_bracketWheelMaxRatioSpinBox->value(),
                                       BracketWheelConstants::MIN_RATIO_BOUND,
                                       BracketWheelConstants::MAX_RATIO_BOUND);
    const double normalizedRatioMin = std::min(ratioMin, ratioMax);
    const double normalizedRatioMax = std::max(ratioMin, ratioMax);
    m_bracketWheelMinRatioSpinBox->setValue(normalizedRatioMin);
    m_bracketWheelMaxRatioSpinBox->setValue(normalizedRatioMax);

    const double stopMin = std::clamp(m_bracketWheelMinStopPercentSpinBox->value(),
                                      BracketWheelConstants::MIN_STOP_PERCENT_BOUND,
                                      BracketWheelConstants::MAX_STOP_PERCENT_BOUND);
    const double stopMax = std::clamp(m_bracketWheelMaxStopPercentSpinBox->value(),
                                      BracketWheelConstants::MIN_STOP_PERCENT_BOUND,
                                      BracketWheelConstants::MAX_STOP_PERCENT_BOUND);
    const double normalizedStopMin = std::min(stopMin, stopMax);
    const double normalizedStopMax = std::max(stopMin, stopMax);
    m_bracketWheelMinStopPercentSpinBox->setValue(normalizedStopMin);
    m_bracketWheelMaxStopPercentSpinBox->setValue(normalizedStopMax);

    const double ratioStep = std::clamp(m_bracketWheelRatioStepSpinBox->value(),
                                        BracketWheelConstants::MIN_RATIO_STEP_R,
                                        BracketWheelConstants::MAX_RATIO_STEP_R);
    const double stopStep = std::clamp(m_bracketWheelStopStepPercentSpinBox->value(),
                                       BracketWheelConstants::MIN_STOP_STEP_PERCENT,
                                       BracketWheelConstants::MAX_STOP_STEP_PERCENT);

    saveSetting(BracketWheelConstants::SETTINGS_KEY_RATIO_STEP_R, ratioStep);
    saveSetting(BracketWheelConstants::SETTINGS_KEY_STOP_STEP_PERCENT, stopStep);
    saveSetting(BracketWheelConstants::SETTINGS_KEY_RATIO_MIN, normalizedRatioMin);
    saveSetting(BracketWheelConstants::SETTINGS_KEY_RATIO_MAX, normalizedRatioMax);
    saveSetting(BracketWheelConstants::SETTINGS_KEY_STOP_PERCENT_MIN, normalizedStopMin);
    saveSetting(BracketWheelConstants::SETTINGS_KEY_STOP_PERCENT_MAX, normalizedStopMax);

    if (isVisible())
    {
        logInputEvent(
            u"ConfigTab",
            u"set-bracket-wheel-settings",
            {inputDetail(u"ratioStepR", ratioStep),
             inputDetail(u"stopStepPercent", stopStep),
             inputDetail(u"ratioRange",
                         QString("%1-%2").arg(normalizedRatioMin, 0, 'f', 2).arg(normalizedRatioMax, 0, 'f', 2)),
             inputDetail(u"stopPctRange",
                         QString("%1-%2").arg(normalizedStopMin, 0, 'f', 2).arg(normalizedStopMax, 0, 'f', 2))});
    }
}

void ConfigTab::onChartFocusLookbackBarsChanged(const int value)
{
    saveSetting(ChartFocusConstants::SETTINGS_KEY_LOOKBACK_BARS, value);
    if (isVisible())
    {
        logInputEvent(u"ConfigTab", u"set-chart-focus-lookback-bars", {inputDetail(u"value", value)});
    }
}

void ConfigTab::onChartFocusRightPaddingBarsChanged(const int value)
{
    saveSetting(ChartFocusConstants::SETTINGS_KEY_RIGHT_PADDING_BARS, value);
    if (isVisible())
    {
        logInputEvent(u"ConfigTab", u"set-chart-focus-right-padding-bars", {inputDetail(u"value", value)});
    }
}

void ConfigTab::onChartFocusIncludeBboChanged(const bool value)
{
    saveSetting(ChartFocusConstants::SETTINGS_KEY_INCLUDE_BBO_IN_Y_RANGE, value);
    if (isVisible())
    {
        logInputEvent(u"ConfigTab", u"set-chart-focus-include-bbo", {inputDetail(u"value", value)});
    }
}

void ConfigTab::onChartFocusYPaddingPercentChanged(const double value)
{
    saveSetting(ChartFocusConstants::SETTINGS_KEY_Y_PADDING_PERCENT, value);
    if (isVisible())
    {
        logInputEvent(u"ConfigTab", u"set-chart-focus-y-padding-percent", {inputDetail(u"value", value)});
    }
}

void ConfigTab::onChartFocusMinRangePercentChanged(const double value)
{
    saveSetting(ChartFocusConstants::SETTINGS_KEY_MIN_RANGE_PERCENT, value);
    if (isVisible())
    {
        logInputEvent(u"ConfigTab", u"set-chart-focus-min-range-percent", {inputDetail(u"value", value)});
    }
}

void ConfigTab::onChartFocusAnchorPaddingPercentChanged(const double value)
{
    saveSetting(ChartFocusConstants::SETTINGS_KEY_ANCHOR_PADDING_PERCENT, value);
    if (isVisible())
    {
        logInputEvent(u"ConfigTab", u"set-chart-focus-anchor-padding-percent", {inputDetail(u"value", value)});
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
