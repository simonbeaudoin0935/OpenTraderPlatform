#include "Indicators/EmaIndicator.h"

#include <utility>

#include "Assume.h"
#include "Misc/BarUtils.h"

EmaIndicator::EmaIndicator(QString indicatorId, QCustomPlot* plot, QCPAxis* xAxis, QCPAxis* yAxis)
    : m_indicatorId(std::move(indicatorId)), m_plot(plot), m_xAxis(xAxis), m_yAxis(yAxis)
{
    ASSUME_TRUE(m_plot != nullptr);
    ASSUME_TRUE(m_xAxis != nullptr);
    ASSUME_TRUE(m_yAxis != nullptr);

    m_graph = m_plot->addGraph(m_xAxis, m_yAxis);
    Q_CHECK_PTR(m_graph);
    m_graph->setLineStyle(QCPGraph::lsLine);
    m_graph->setScatterStyle(QCPScatterStyle::ssNone);
    m_graph->setLayer("main");

    applyStyle();
    applyVisibility();
}

EmaIndicator::~EmaIndicator()
{
    if (m_plot == nullptr)
    {
        return;
    }

    if (m_graph != nullptr)
    {
        m_plot->removePlottable(m_graph);
        m_graph = nullptr;
    }
}

void EmaIndicator::setVisible(const bool visible)
{
    m_visible = visible;
    applyVisibility();
}

void EmaIndicator::setSettings(const Settings& settings)
{
    m_settings.period = qBound(1, settings.period, 5000);
    m_settings.lineColor = settings.lineColor.isValid() ? settings.lineColor : QColor(243, 198, 35);
    applyStyle();
}

void EmaIndicator::rebuild(const UpdateContext& context)
{
    ASSUME_TRUE(m_graph != nullptr);

    if (!m_visible)
    {
        applyVisibility();
        return;
    }

    if (context.indexToBar.isEmpty() || !BarUtils::isIntradayTimeFrame(context.displayTimeFrame))
    {
        clear();
        return;
    }

    QVector<double> keys;
    QVector<double> closePrices;
    keys.reserve(context.indexToBar.size());
    closePrices.reserve(context.indexToBar.size());

    const double keyOffset = indexKeyOffset(context.displayTimeFrame);
    for (auto it = context.indexToBar.cbegin(); it != context.indexToBar.cend(); ++it)
    {
        const Bar& bar = it.value();
        const Bar::BarStatus status = bar.getBarStatus();
        if (status != Bar::BarStatus::Open && status != Bar::BarStatus::Closed)
        {
            continue;
        }

        keys.append(static_cast<double>(it.key()) + keyOffset);
        closePrices.append(static_cast<double>(bar.getClose()));
    }

    if (keys.isEmpty())
    {
        clear();
        return;
    }

    QVector<double> emaValues;
    emaValues.resize(closePrices.size());
    emaValues[0] = closePrices[0];

    const double alpha = 2.0 / static_cast<double>(m_settings.period + 1);
    for (qsizetype i = 1; i < closePrices.size(); ++i)
    {
        emaValues[i] = alpha * closePrices[i] + (1.0 - alpha) * emaValues[i - 1];
    }

    m_graph->setData(keys, emaValues, true);
    applyVisibility();
}

void EmaIndicator::clear()
{
    if (m_graph != nullptr)
    {
        m_graph->data()->clear();
    }

    applyVisibility();
}

double EmaIndicator::indexKeyOffset(const TimeFrame tf)
{
    return static_cast<double>(BarUtils::minutesPerBar(tf)) / 2.0;
}

void EmaIndicator::applyStyle() const
{
    ASSUME_TRUE(m_graph != nullptr);
    m_graph->setPen(QPen(m_settings.lineColor, 2));
    m_graph->setName(QStringLiteral("EMA (%1)").arg(m_settings.period));
}

void EmaIndicator::applyVisibility() const
{
    if (m_graph != nullptr)
    {
        m_graph->setVisible(m_visible && !m_graph->data()->isEmpty());
    }
}
