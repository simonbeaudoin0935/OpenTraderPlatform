#pragma once

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

  private:
    void setupUI();
    void loadSettings();
    void saveSetting(const QString& key, const QVariant& value);

    QSpinBox* m_timeAndSalesMaxEntriesSpinBox;
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
