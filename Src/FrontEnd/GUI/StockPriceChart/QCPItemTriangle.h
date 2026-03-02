#pragma once

#include "qcustomplot.h"

/**
 * @brief A filled triangle marker for buy/sell order visualization.
 *
 * The tip is anchored precisely to a plot coordinate — no text-rendering
 * offset artifacts. For buy markers the tip points upward (tip at bottom);
 * for sell markers the tip points downward (tip at top).
 *
 * Size is specified in pixels so it remains constant regardless of zoom.
 */
class QCPItemTriangle : public QCPAbstractItem
{

  public:
    /**
     * @param p_plot     The parent QCustomPlot
     * @param p_tipUp    true = triangle points up (buy), false = points down (sell)
     */
    explicit QCPItemTriangle(QCustomPlot* p_plot, bool p_tipUp)
        : QCPAbstractItem(p_plot), tip(createPosition(QStringLiteral("tip"))), m_tipUp(p_tipUp)
    {
        tip->setAxes(p_plot->xAxis, p_plot->axisRect()->axis(QCPAxis::atRight));
        setAntialiased(true);
    }

    QCPItemPosition* const tip; ///< Anchor: the sharp vertex of the triangle

    void setColor(const QColor& p_color)
    {
        m_color = p_color;
    }
    void setPixelSize(int p_width, int p_height)
    {
        m_halfWidth = p_width / 2.0;
        m_height = p_height;
    }

    double selectTest(const QPointF& /*pos*/, bool /*onlySelectable*/, QVariant* /*details*/) const override
    {
        return -1.0; // Not selectable
    }

  protected:
    void draw(QCPPainter* p_painter) override
    {
        const QPointF tipPx = tip->pixelPosition();

        // Build triangle in pixel space: tip is the sharp point.
        // tipUp=true  → tip at bottom, base at top  (buy ▲)
        // tipUp=false → tip at top,    base at bottom (sell ▼)
        const double dy = m_tipUp ? -m_height : m_height; // direction toward base

        QPolygonF poly(3);
        poly[0] = tipPx;                                            // tip
        poly[1] = QPointF(tipPx.x() - m_halfWidth, tipPx.y() + dy); // base left
        poly[2] = QPointF(tipPx.x() + m_halfWidth, tipPx.y() + dy); // base right

        p_painter->setPen(Qt::NoPen);
        p_painter->setBrush(QBrush(m_color));
        p_painter->drawPolygon(poly);
    }

  private:
    bool m_tipUp;
    QColor m_color = Qt::white;
    double m_halfWidth = 6.0;
    double m_height = 10.0;
};
