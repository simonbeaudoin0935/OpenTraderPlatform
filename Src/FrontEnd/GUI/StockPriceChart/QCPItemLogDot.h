#pragma once

#include "qcustomplot.h"

/**
 * @brief A fixed-size filled circle marker for strategy log visualization.
 *
 * Unlike QCPItemEllipse, this item anchors its X coordinate to the plot axis but
 * always renders at a fixed pixel distance above the bottom of the axis rect,
 * independent of the current Y-axis range or zoom level.
 *
 * Size is specified in pixels so it remains constant regardless of zoom or pan.
 */
class QCPItemLogDot : public QCPAbstractItem
{
  public:
    /**
     * @param p_plot         The parent QCustomPlot.
     * @param p_radius       Circle radius in pixels (default 4 px).
     * @param p_bottomOffset Pixels above the bottom of the axis rect (default 12 px).
     */
    explicit QCPItemLogDot(QCustomPlot* p_plot, int p_radius = 4, int p_bottomOffset = 12)
        : QCPAbstractItem(p_plot)
        , center(createPosition(QStringLiteral("center")))
        , m_radius(p_radius)
        , m_bottomOffset(p_bottomOffset)
    {
        // Only the X axis is meaningful — Y is overridden in draw()
        center->setAxes(p_plot->xAxis, p_plot->axisRect()->axis(QCPAxis::atRight));
        setAntialiased(true);
    }

    QCPItemPosition* const center; ///< Anchor: only the X coordinate is used

    void setColor(const QColor& p_color) { m_color = p_color; }
    void setRadius(int p_radius) { m_radius = p_radius; }
    void setBottomOffset(int p_bottomOffset) { m_bottomOffset = p_bottomOffset; }

    /** @brief Returns the pixel position where the dot is drawn (useful for tooltips). */
    QPointF dotPixelPosition() const
    {
        const double xPx = center->pixelPosition().x();
        const double yPx = parentPlot()->axisRect()->bottom() - m_bottomOffset;
        return {xPx, yPx};
    }

    double selectTest(const QPointF& /*pos*/, bool /*onlySelectable*/, QVariant* /*details*/) const override
    {
        return -1.0; // Not selectable
    }

  protected:
    void draw(QCPPainter* p_painter) override
    {
        const QPointF pos = dotPixelPosition();

        p_painter->setPen(Qt::NoPen);
        p_painter->setBrush(QBrush(m_color));
        p_painter->drawEllipse(pos, static_cast<double>(m_radius), static_cast<double>(m_radius));
    }

  private:
    QColor m_color{100, 140, 255, 220}; // Blue-purple default
    int m_radius = 4;
    int m_bottomOffset = 12;
};
