#pragma once

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>
#include <QMap>
#include "Misc/TimeFrame.h"

class TimeAndSalesWidget;
class StockPriceChart;

class ConfigTab : public QWidget
{
    Q_OBJECT

  public:
    explicit ConfigTab(QWidget* parent = nullptr);
    ~ConfigTab() override = default;

    /// Set the T&S widget to update when max entries changes
    void setTimeAndSalesWidget(TimeAndSalesWidget* p_widget);

    /// Set the chart widget to update when auto-TF thresholds change
    void setStockPriceChart(StockPriceChart* p_chart);

    /// Get the auto-timeframe thresholds (lower, upper) in minutes for a given timeframe
    std::pair<int, int> getAutoTimeFrameThresholds(TimeFrame tf) const;

  signals:
    /// Emitted when auto-TF thresholds are changed
    void autoTimeFrameThresholdsChanged();

  private slots:
    void onTimeAndSalesMaxEntriesChanged(int value);
    void onAutoTfThresholdChanged();
    void onClosePositionsAggressivityChanged(double value);
    void onStrategyConfirmationTimeoutChanged(int value);
    void onStrategyMarketableOffsetChanged(double value);
    void onStrategyMaxChasePercentChanged(double value);
    void onChartFocusLookbackBarsChanged(int value);
    void onChartFocusRightPaddingBarsChanged(int value);
    void onChartFocusIncludeBboChanged(bool value);
    void onChartFocusYPaddingPercentChanged(double value);
    void onChartFocusMinRangePercentChanged(double value);
    void onChartFocusAnchorPaddingPercentChanged(double value);
    void onBracketWheelSettingChanged();

  private:
    void setupUI();
    void loadSettings();
    void saveSetting(const QString& key, const QVariant& value);

    QSpinBox* m_timeAndSalesMaxEntriesSpinBox;
    QSpinBox* m_strategyConfirmationTimeoutSpinBox = nullptr;
    QDoubleSpinBox* m_strategyMarketableOffsetSpinBox = nullptr;
    QDoubleSpinBox* m_strategyMaxChasePercentSpinBox = nullptr;
    QSpinBox* m_chartFocusLookbackBarsSpinBox = nullptr;
    QSpinBox* m_chartFocusRightPaddingBarsSpinBox = nullptr;
    QCheckBox* m_chartFocusIncludeBboCheckBox = nullptr;
    QDoubleSpinBox* m_chartFocusYPaddingPercentSpinBox = nullptr;
    QDoubleSpinBox* m_chartFocusMinRangePercentSpinBox = nullptr;
    QDoubleSpinBox* m_chartFocusAnchorPaddingPercentSpinBox = nullptr;
    QDoubleSpinBox* m_bracketWheelRatioStepSpinBox = nullptr;
    QDoubleSpinBox* m_bracketWheelStopStepPercentSpinBox = nullptr;
    QDoubleSpinBox* m_bracketWheelMinRatioSpinBox = nullptr;
    QDoubleSpinBox* m_bracketWheelMaxRatioSpinBox = nullptr;
    QDoubleSpinBox* m_bracketWheelMinStopPercentSpinBox = nullptr;
    QDoubleSpinBox* m_bracketWheelMaxStopPercentSpinBox = nullptr;
    QDoubleSpinBox* m_closePositionsAggressivitySpinBox = nullptr;
    TimeAndSalesWidget* m_timeAndSalesWidget = nullptr;
    StockPriceChart* m_stockPriceChart = nullptr;

    // Auto-timeframe threshold spinboxes: lower and upper for each intraday timeframe
    struct TfThresholdWidgets
    {
        QSpinBox* lower;
        QSpinBox* upper;
    };
    QMap<TimeFrame, TfThresholdWidgets> m_autoTfThresholds;
};
