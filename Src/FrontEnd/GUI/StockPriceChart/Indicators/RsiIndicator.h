#pragma once

#include <QColor>
#include <QVector>

#include "Indicators/ChartIndicator.h"
#include "qcustomplot.h"

/**
 * @brief RSI indicator rendered on a dedicated subpane.
 */
class RsiIndicator final : public ChartIndicator
{
  public:
    struct Settings
    {
        int period = 14;
        int overboughtLevel = 70;
        int oversoldLevel = 30;
        QColor lineColor = QColor(179, 136, 255);
    };

    RsiIndicator(QCustomPlot* plot, QCPAxis* xAxis, QCPAxis* yAxis);
    ~RsiIndicator() override;

    [[nodiscard]] QString id() const override
    {
        return QStringLiteral("rsi");
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
    [[nodiscard]] static double indexKeyOffset(TimeFrame tf);
    [[nodiscard]] static Settings sanitizeSettings(const Settings& settings);
    void applyStyle() const;
    void applyVisibility() const;
    void rebuildLevelLine(QCPGraph* graph, const QVector<double>& keys, double level) const;

    QCustomPlot* m_plot = nullptr;
    QCPAxis* m_xAxis = nullptr;
    QCPAxis* m_yAxis = nullptr;
    bool m_visible = false;
    Settings m_settings;

    QCPGraph* m_rsiGraph = nullptr;
    QCPGraph* m_overboughtGraph = nullptr;
    QCPGraph* m_oversoldGraph = nullptr;
};
