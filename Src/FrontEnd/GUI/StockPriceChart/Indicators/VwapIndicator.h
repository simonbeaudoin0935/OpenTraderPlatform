#pragma once

#include <QColor>
#include <QTime>
#include <QVector>

#include "Indicators/ChartIndicator.h"
#include "qcustomplot.h"

/**
 * @brief Overlay VWAP indicator with per-session reset.
 *
 * VWAP uses cumulative close*volume / cumulative volume and resets at 04:00 ET
 * for each trading day.
 */
class VwapIndicator final : public ChartIndicator
{
  public:
    enum class PriceSource : int
    {
        Close = 0,
        Hlc3 = 1,
    };

    struct Settings
    {
        PriceSource sourcePrice = PriceSource::Close;
        QTime sessionResetTime = QTime(4, 0);
        QColor lineColor = QColor(0, 220, 220);
    };

    VwapIndicator(QCustomPlot* plot, QCPAxis* xAxis, QCPAxis* yAxis);
    ~VwapIndicator() override;

    [[nodiscard]] QString id() const override
    {
        return QStringLiteral("vwap");
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
    struct SessionSeries
    {
        QDate date;
        QVector<double> keys;
        QVector<double> values;
    };

    [[nodiscard]] static int indexStepUnits(TimeFrame tf);
    [[nodiscard]] static double indexKeyOffset(TimeFrame tf);
    [[nodiscard]] QDate sessionDateForTimestamp(const QDateTime& timestamp) const;
    [[nodiscard]] double sourcePriceForBar(const Bar& bar) const;
    [[nodiscard]] bool
    buildSessionSeries(const UpdateContext& context, const QDate& sessionDate, SessionSeries& sessionSeries) const;
    void ensureGraphCount(qsizetype count);
    void applyVisibilityToGraphs() const;

    QCustomPlot* m_plot = nullptr;
    QCPAxis* m_xAxis = nullptr;
    QCPAxis* m_yAxis = nullptr;
    bool m_visible = false;
    Settings m_settings;
    QVector<QCPGraph*> m_graphs;
};
