#pragma once

#include <QColor>
#include <QString>

#include "Indicators/ChartIndicator.h"
#include "qcustomplot.h"

/**
 * @brief Single EMA overlay indicator rendered on the main price pane.
 */
class EmaIndicator final : public ChartIndicator
{
  public:
    struct Settings
    {
        int period = 9;
        QColor lineColor = QColor(243, 198, 35);
    };

    EmaIndicator(QString indicatorId, QCustomPlot* plot, QCPAxis* xAxis, QCPAxis* yAxis);
    ~EmaIndicator() override;

    [[nodiscard]] QString id() const override
    {
        return m_indicatorId;
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

    void rebuild(const UpdateContext& context) override;
    void clear() override;

  private:
    [[nodiscard]] static double indexKeyOffset(TimeFrame tf);
    void applyStyle() const;
    void applyVisibility() const;

    QString m_indicatorId;
    QCustomPlot* m_plot = nullptr;
    QCPAxis* m_xAxis = nullptr;
    QCPAxis* m_yAxis = nullptr;
    bool m_visible = false;
    Settings m_settings;
    QCPGraph* m_graph = nullptr;
};
