#pragma once

#include <QVector>

#include "Indicators/ChartIndicator.h"
#include "qcustomplot.h"

/**
 * @brief MACD indicator rendered on a dedicated subpane.
 *
 * MACD line = MA(fast) - MA(slow)
 * Signal line = MA(MACD line)
 * Histogram = MACD line - Signal line
 */
class MacdIndicator final : public ChartIndicator
{
  public:
    enum class MaType : int
    {
        Ema = 0,
        Sma = 1,
    };

    struct Settings
    {
        int fastLength = 12;
        int slowLength = 26;
        int signalLength = 9;
        MaType macdMaType = MaType::Ema;
        MaType signalMaType = MaType::Ema;
        bool showHistogram = true;
    };

    MacdIndicator(QCustomPlot* plot, QCPAxis* xAxis, QCPAxis* yAxis);
    ~MacdIndicator() override;

    [[nodiscard]] QString id() const override
    {
        return QStringLiteral("macd");
    }

    [[nodiscard]] Pane pane() const override
    {
        return Pane::Subpane;
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

    void rebuild(const UpdateContext& context) override;
    void clear() override;

  private:
    [[nodiscard]] static int indexStepUnits(TimeFrame tf);
    [[nodiscard]] static double indexKeyOffset(TimeFrame tf);
    [[nodiscard]] static QVector<double>
    calculateMovingAverage(const QVector<double>& values, int length, MaType maType);
    void applyVisibility() const;
    void updateAxisRange(const QVector<double>& macdValues,
                         const QVector<double>& signalValues,
                         const QVector<double>& histogramValues) const;

    QCustomPlot* m_plot = nullptr;
    QCPAxis* m_xAxis = nullptr;
    QCPAxis* m_yAxis = nullptr;
    bool m_visible = false;
    Settings m_settings;
    QCPGraph* m_macdGraph = nullptr;
    QCPGraph* m_signalGraph = nullptr;
    QCPBars* m_histogramPositive = nullptr;
    QCPBars* m_histogramNegative = nullptr;
};
