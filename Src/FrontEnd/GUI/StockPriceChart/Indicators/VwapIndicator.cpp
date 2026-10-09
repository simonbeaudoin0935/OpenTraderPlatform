#include "Indicators/VwapIndicator.h"

#include <algorithm>
#include <limits>

#include <QSet>

#include "Assume.h"
#include "ChartTimeUtils.h"
#include "Misc/BarUtils.h"

namespace
{
    constexpr QColor VWAP_LINE_COLOR(0, 220, 220);
    constexpr int VWAP_LINE_WIDTH = 2;
} // namespace

VwapIndicator::VwapIndicator(QCustomPlot* plot, QCPAxis* xAxis, QCPAxis* yAxis)
    : m_plot(plot), m_xAxis(xAxis), m_yAxis(yAxis)
{
    ASSUME_TRUE(m_plot != nullptr);
    ASSUME_TRUE(m_xAxis != nullptr);
    ASSUME_TRUE(m_yAxis != nullptr);
}

VwapIndicator::~VwapIndicator()
{
    if (m_plot == nullptr)
    {
        return;
    }

    for (QCPGraph* graph: m_graphs)
    {
        if (graph != nullptr)
        {
            m_plot->removePlottable(graph);
        }
    }

    m_graphs.clear();
}

void VwapIndicator::setVisible(const bool visible)
{
    m_visible = visible;
    applyVisibilityToGraphs();
}

void VwapIndicator::setSettings(const Settings& settings)
{
    m_settings.sourcePrice = settings.sourcePrice;
    m_settings.sessionResetTime = settings.sessionResetTime.isValid() ? settings.sessionResetTime : QTime(4, 0);
    m_settings.lineColor = settings.lineColor.isValid() ? settings.lineColor : VWAP_LINE_COLOR;

    for (QCPGraph* graph: m_graphs)
    {
        if (graph == nullptr)
        {
            continue;
        }

        graph->setPen(QPen(m_settings.lineColor, VWAP_LINE_WIDTH));
    }
}

void VwapIndicator::rebuild(const UpdateContext& context)
{
    if (!m_visible)
    {
        applyVisibilityToGraphs();
        return;
    }

    if (!context.index0Timestamp.isValid() || context.indexToBar.isEmpty() ||
        !BarUtils::isIntradayTimeFrame(context.displayTimeFrame))
    {
        clear();
        return;
    }

    QSet<QDate> sessionDates;
    for (auto it = context.indexToBar.cbegin(); it != context.indexToBar.cend(); ++it)
    {
        sessionDates.insert(sessionDateForTimestamp(it.value().getTimeStamp()));
    }

    QList<QDate> orderedDates = sessionDates.values();
    std::sort(orderedDates.begin(), orderedDates.end());

    QVector<SessionSeries> completeSessions;
    completeSessions.reserve(orderedDates.size());

    for (const QDate& date: orderedDates)
    {
        SessionSeries series;
        if (buildSessionSeries(context, date, series))
        {
            completeSessions.append(series);
        }
    }

    ensureGraphCount(completeSessions.size());

    for (qsizetype i = 0; i < m_graphs.size(); ++i)
    {
        QCPGraph* graph = m_graphs[i];
        ASSUME_TRUE(graph != nullptr);

        if (i < completeSessions.size())
        {
            const SessionSeries& series = completeSessions[i];
            graph->setData(series.keys, series.values, true);
            graph->setVisible(true);
        }
        else
        {
            graph->data()->clear();
            graph->setVisible(false);
        }
    }

    applyVisibilityToGraphs();
}

void VwapIndicator::clear()
{
    for (QCPGraph* graph: m_graphs)
    {
        if (graph == nullptr)
        {
            continue;
        }

        graph->data()->clear();
        graph->setVisible(false);
    }
}

int VwapIndicator::indexStepUnits(const TimeFrame tf)
{
    return BarUtils::minutesPerBar(tf);
}

double VwapIndicator::indexKeyOffset(const TimeFrame tf)
{
    return static_cast<double>(BarUtils::minutesPerBar(tf)) / 2.0;
}

QDate VwapIndicator::sessionDateForTimestamp(const QDateTime& timestamp) const
{
    const QDateTime marketTime = timestamp.toTimeZone(TradingHours::MARKET_TIMEZONE);
    QDate sessionDate = marketTime.date();
    if (marketTime.time() < m_settings.sessionResetTime)
    {
        sessionDate = sessionDate.addDays(-1);
    }
    return sessionDate;
}

double VwapIndicator::sourcePriceForBar(const Bar& bar) const
{
    if (m_settings.sourcePrice == PriceSource::Hlc3)
    {
        return (static_cast<double>(bar.getHigh()) + static_cast<double>(bar.getLow()) +
                static_cast<double>(bar.getClose())) /
               3.0;
    }

    return static_cast<double>(bar.getClose());
}

bool VwapIndicator::buildSessionSeries(const UpdateContext& context,
                                       const QDate& sessionDate,
                                       SessionSeries& sessionSeries) const
{
    ASSUME_TRUE(context.index0Timestamp.isValid());

    const int stepUnits = indexStepUnits(context.displayTimeFrame);
    if (stepUnits <= 0)
    {
        return false;
    }

    const QDateTime sessionStart(sessionDate, m_settings.sessionResetTime, TradingHours::MARKET_TIMEZONE);
    const int expectedStartIndex =
        ChartTimeUtils::timestampToChartIndex(sessionStart, context.index0Timestamp, context.displayTimeFrame);

    bool foundBar = false;
    int firstIndex = std::numeric_limits<int>::max();
    int lastIndex = std::numeric_limits<int>::lowest();

    for (auto it = context.indexToBar.cbegin(); it != context.indexToBar.cend(); ++it)
    {
        const QDate barDate = sessionDateForTimestamp(it.value().getTimeStamp());
        if (barDate != sessionDate)
        {
            continue;
        }

        foundBar = true;
        firstIndex = qMin(firstIndex, it.key());
        lastIndex = qMax(lastIndex, it.key());
    }

    if (!foundBar)
    {
        return false;
    }

    // Session must be contiguous from the configured reset time to latest loaded index for the session.
    if (firstIndex > expectedStartIndex)
    {
        return false;
    }

    for (int index = expectedStartIndex; index <= lastIndex; index += stepUnits)
    {
        if (!context.indexToBar.contains(index))
        {
            return false;
        }

        const QDate barDate = sessionDateForTimestamp(context.indexToBar[index].getTimeStamp());
        if (barDate != sessionDate)
        {
            return false;
        }
    }

    const double keyOffset = indexKeyOffset(context.displayTimeFrame);

    double cumulativePriceVolume = 0.0;
    quint64 cumulativeVolume = 0;

    sessionSeries.date = sessionDate;
    sessionSeries.keys.clear();
    sessionSeries.values.clear();

    for (int index = expectedStartIndex; index <= lastIndex; index += stepUnits)
    {
        const Bar& bar = context.indexToBar[index];
        const Bar::BarStatus status = bar.getBarStatus();
        if (status == Bar::BarStatus::Open || status == Bar::BarStatus::Closed)
        {
            const quint64 volume = bar.getTotalVolume();
            if (volume > 0)
            {
                cumulativePriceVolume += sourcePriceForBar(bar) * static_cast<double>(volume);
                cumulativeVolume += volume;
            }
        }

        if (cumulativeVolume > 0)
        {
            sessionSeries.keys.append(static_cast<double>(index) + keyOffset);
            sessionSeries.values.append(cumulativePriceVolume / static_cast<double>(cumulativeVolume));
        }
    }

    return !sessionSeries.keys.isEmpty();
}

void VwapIndicator::ensureGraphCount(const qsizetype count)
{
    while (m_graphs.size() < count)
    {
        QCPGraph* graph = m_plot->addGraph(m_xAxis, m_yAxis);
        Q_CHECK_PTR(graph);

        graph->setName(QStringLiteral("VWAP"));
        graph->setLineStyle(QCPGraph::lsLine);
        graph->setScatterStyle(QCPScatterStyle::ssNone);
        graph->setPen(QPen(m_settings.lineColor.isValid() ? m_settings.lineColor : VWAP_LINE_COLOR, VWAP_LINE_WIDTH));
        graph->setLayer("main");
        graph->setVisible(m_visible);

        m_graphs.append(graph);
    }
}

void VwapIndicator::applyVisibilityToGraphs() const
{
    for (QCPGraph* graph: m_graphs)
    {
        if (graph == nullptr)
        {
            continue;
        }

        graph->setVisible(m_visible && !graph->data()->isEmpty());
    }
}
