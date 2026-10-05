#pragma once

#include "Indicators/ChartIndicator.h"
#include "qcustomplot.h"

/**
 * @brief Volume bars indicator blended into the main candlestick pane.
 */
class VolumeIndicator final : public ChartIndicator
{
  public:
    enum class AutoScaleMode : int
    {
        HighestBar = 0,
        SecondHighestBar = 1,
    };

    struct Settings
    {
        bool autoScaleEnabled = true;
        AutoScaleMode autoScaleMode = AutoScaleMode::SecondHighestBar;
    };

    VolumeIndicator(QCustomPlot* plot, QCPAxis* xAxis, QCPAxis* yAxis);
    ~VolumeIndicator() override;

    [[nodiscard]] QString id() const override
    {
        return QStringLiteral("volume");
    }

    [[nodiscard]] Pane pane() const override
    {
        return Pane::Overlay;
    }

    void setVisible(bool visible) override;
    [[nodiscard]] bool isVisible() const override
    {
        return m_visible;
    }

    void setSettings(const Settings& settings);
    [[nodiscard]] Settings settings() const
    {
        return m_settings;
    }

    void setBarWidth(double width);

    void rebuild(const UpdateContext& context) override;
    void clear() override;

    void rescaleVisibleRange(const QMap<int, Bar>& indexToBar, const QCPRange& xRange);

  private:
    static QVector<double> buildStripTickVector(double visibleTop);
    static QString formatStripTickLabel(double value);
    void applyStripTickLabels(double visibleTop);
    void clearStripTickLabels();
    [[nodiscard]] static double indexKeyOffset(TimeFrame tf);
    void applyVisibility() const;

    QCustomPlot* m_plot = nullptr;
    QCPAxis* m_xAxis = nullptr;
    QCPAxis* m_yAxis = nullptr;
    bool m_visible = true;
    Settings m_settings;

    QCPBars* m_volumePos = nullptr;
    QCPBars* m_volumeNeg = nullptr;
};
