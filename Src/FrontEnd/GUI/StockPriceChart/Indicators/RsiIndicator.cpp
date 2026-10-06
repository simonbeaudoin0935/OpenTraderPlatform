#include "Indicators/RsiIndicator.h"

#include <limits>

#include "Assume.h"
#include "Misc/BarUtils.h"

namespace
{
    constexpr QColor DEFAULT_RSI_LINE_COLOR(179, 136, 255);
    constexpr QColor OVERBOUGHT_LINE_COLOR(255, 140, 140, 190);
    constexpr QColor OVERSOLD_LINE_COLOR(120, 210, 150, 190);
} // namespace

RsiIndicator::RsiIndicator(QCustomPlot* plot, QCPAxis* xAxis, QCPAxis* yAxis)
    : m_plot(plot), m_xAxis(xAxis), m_yAxis(yAxis)
{
    ASSUME_TRUE(m_plot != nullptr);
    ASSUME_TRUE(m_xAxis != nullptr);
    ASSUME_TRUE(m_yAxis != nullptr);

    m_rsiGraph = m_plot->addGraph(m_xAxis, m_yAxis);
    Q_CHECK_PTR(m_rsiGraph);
    m_rsiGraph->setLineStyle(QCPGraph::lsLine);
    m_rsiGraph->setScatterStyle(QCPScatterStyle::ssNone);
    m_rsiGraph->setLayer("main");

    m_overboughtGraph = m_plot->addGraph(m_xAxis, m_yAxis);
    Q_CHECK_PTR(m_overboughtGraph);
    m_overboughtGraph->setLineStyle(QCPGraph::lsLine);
    m_overboughtGraph->setScatterStyle(QCPScatterStyle::ssNone);
    m_overboughtGraph->setLayer("main");

    m_oversoldGraph = m_plot->addGraph(m_xAxis, m_yAxis);
    Q_CHECK_PTR(m_oversoldGraph);
    m_oversoldGraph->setLineStyle(QCPGraph::lsLine);
    m_oversoldGraph->setScatterStyle(QCPScatterStyle::ssNone);
    m_oversoldGraph->setLayer("main");

    applyStyle();
    applyVisibility();
}

RsiIndicator::~RsiIndicator()
{
    if (m_plot == nullptr)
    {
        return;
    }

    if (m_rsiGraph != nullptr)
    {
        m_plot->removePlottable(m_rsiGraph);
        m_rsiGraph = nullptr;
    }

    if (m_overboughtGraph != nullptr)
    {
        m_plot->removePlottable(m_overboughtGraph);
        m_overboughtGraph = nullptr;
    }

    if (m_oversoldGraph != nullptr)
    {
        m_plot->removePlottable(m_oversoldGraph);
        m_oversoldGraph = nullptr;
    }
}

void RsiIndicator::setVisible(const bool visible)
{
    m_visible = visible;
    applyVisibility();
}

void RsiIndicator::setSettings(const Settings& settings)
{
    m_settings = sanitizeSettings(settings);
    applyStyle();
}

void RsiIndicator::rebuild(const UpdateContext& context)
{
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

    if (closePrices.size() < 2)
    {
        clear();
        return;
    }

    const int period = qBound(1, m_settings.period, 5000);

    QVector<double> rsiValues(closePrices.size(), 50.0);
    double avgGain = 0.0;
    double avgLoss = 0.0;
    bool seeded = false;
    const auto computeRsi = [](const double gainAverage, const double lossAverage) -> double
    {
        if (lossAverage <= std::numeric_limits<double>::epsilon())
        {
            return 100.0;
        }

        const double rs = gainAverage / lossAverage;
        const double rsi = 100.0 - (100.0 / (1.0 + rs));
        return qBound(0.0, rsi, 100.0);
    };

    for (qsizetype i = 1; i < closePrices.size(); ++i)
    {
        const double delta = closePrices[i] - closePrices[i - 1];
        const double gain = qMax(0.0, delta);
        const double loss = qMax(0.0, -delta);

        if (!seeded)
        {
            avgGain += gain;
            avgLoss += loss;

            if (i == static_cast<qsizetype>(period))
            {
                avgGain /= static_cast<double>(period);
                avgLoss /= static_cast<double>(period);
                const double seedRsi = computeRsi(avgGain, avgLoss);
                for (qsizetype seedIndex = 0; seedIndex <= i; ++seedIndex)
                {
                    rsiValues[seedIndex] = seedRsi;
                }
                seeded = true;
            }

            continue;
        }

        avgGain = ((avgGain * static_cast<double>(period - 1)) + gain) / static_cast<double>(period);
        avgLoss = ((avgLoss * static_cast<double>(period - 1)) + loss) / static_cast<double>(period);
        rsiValues[i] = computeRsi(avgGain, avgLoss);
    }

    if (!seeded)
    {
        const int sampleCount = qMax(1, static_cast<int>(closePrices.size()) - 1);
        avgGain /= static_cast<double>(sampleCount);
        avgLoss /= static_cast<double>(sampleCount);
        const double fallbackRsi = computeRsi(avgGain, avgLoss);
        for (qsizetype i = 0; i < rsiValues.size(); ++i)
        {
            rsiValues[i] = fallbackRsi;
        }
    }

    ASSUME_TRUE(m_rsiGraph != nullptr);
    m_rsiGraph->setData(keys, rsiValues, true);
    rebuildLevelLine(m_overboughtGraph, keys, static_cast<double>(m_settings.overboughtLevel));
    rebuildLevelLine(m_oversoldGraph, keys, static_cast<double>(m_settings.oversoldLevel));

    m_axisRange.setAutomaticRange(m_yAxis, QCPRange(0.0, 100.0));
    applyVisibility();
}

void RsiIndicator::clear()
{
    m_axisRange.reset();
    if (m_rsiGraph != nullptr)
    {
        m_rsiGraph->data()->clear();
    }
    if (m_overboughtGraph != nullptr)
    {
        m_overboughtGraph->data()->clear();
    }
    if (m_oversoldGraph != nullptr)
    {
        m_oversoldGraph->data()->clear();
    }

    applyVisibility();
}

double RsiIndicator::indexKeyOffset(const TimeFrame tf)
{
    if (tf == TimeFrame::TEN_SECONDS)
    {
        return 0.5;
    }

    return static_cast<double>(BarUtils::minutesPerBar(tf)) / 2.0;
}

RsiIndicator::Settings RsiIndicator::sanitizeSettings(const Settings& settings)
{
    Settings sanitized = settings;
    sanitized.period = qBound(1, settings.period, 5000);
    sanitized.overboughtLevel = qBound(1, settings.overboughtLevel, 100);
    sanitized.oversoldLevel = qBound(0, settings.oversoldLevel, 99);
    if (sanitized.oversoldLevel >= sanitized.overboughtLevel)
    {
        sanitized.overboughtLevel = qBound(1, sanitized.overboughtLevel, 100);
        sanitized.oversoldLevel = qBound(0, sanitized.overboughtLevel - 1, 99);
    }

    sanitized.lineColor = settings.lineColor.isValid() ? settings.lineColor : DEFAULT_RSI_LINE_COLOR;
    return sanitized;
}

void RsiIndicator::applyStyle() const
{
    if (m_rsiGraph != nullptr)
    {
        m_rsiGraph->setName(QStringLiteral("RSI (%1)").arg(m_settings.period));
        m_rsiGraph->setPen(QPen(m_settings.lineColor, 2));
    }

    if (m_overboughtGraph != nullptr)
    {
        m_overboughtGraph->setName(QStringLiteral("RSI Overbought"));
        m_overboughtGraph->setPen(QPen(OVERBOUGHT_LINE_COLOR, 1, Qt::DashLine));
    }

    if (m_oversoldGraph != nullptr)
    {
        m_oversoldGraph->setName(QStringLiteral("RSI Oversold"));
        m_oversoldGraph->setPen(QPen(OVERSOLD_LINE_COLOR, 1, Qt::DashLine));
    }
}

void RsiIndicator::applyVisibility() const
{
    const bool hasRsiData = m_rsiGraph != nullptr && !m_rsiGraph->data()->isEmpty();
    if (m_rsiGraph != nullptr)
    {
        m_rsiGraph->setVisible(m_visible && hasRsiData);
    }

    const bool levelsVisible = m_visible && hasRsiData;
    if (m_overboughtGraph != nullptr)
    {
        m_overboughtGraph->setVisible(levelsVisible && !m_overboughtGraph->data()->isEmpty());
    }
    if (m_oversoldGraph != nullptr)
    {
        m_oversoldGraph->setVisible(levelsVisible && !m_oversoldGraph->data()->isEmpty());
    }
}

void RsiIndicator::rebuildLevelLine(QCPGraph* graph, const QVector<double>& keys, const double level) const
{
    if (graph == nullptr)
    {
        return;
    }

    if (keys.isEmpty())
    {
        graph->data()->clear();
        return;
    }

    QVector<double> levelValues(keys.size(), qBound(0.0, level, 100.0));
    graph->setData(keys, levelValues, true);
}
