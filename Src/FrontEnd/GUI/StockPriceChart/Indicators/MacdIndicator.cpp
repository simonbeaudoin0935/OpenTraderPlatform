#include "Indicators/MacdIndicator.h"

#include <algorithm>
#include <limits>

#include "Assume.h"
#include "Misc/BarUtils.h"

namespace
{
    constexpr QColor MACD_LINE_COLOR(90, 190, 255);
    constexpr QColor SIGNAL_LINE_COLOR(255, 180, 75);
    constexpr QColor HISTOGRAM_POSITIVE_COLOR(90, 210, 120);
    constexpr QColor HISTOGRAM_NEGATIVE_COLOR(230, 105, 105);
} // namespace

MacdIndicator::MacdIndicator(QCustomPlot* plot, QCPAxis* xAxis, QCPAxis* yAxis)
    : m_plot(plot), m_xAxis(xAxis), m_yAxis(yAxis)
{
    ASSUME_TRUE(m_plot != nullptr);
    ASSUME_TRUE(m_xAxis != nullptr);
    ASSUME_TRUE(m_yAxis != nullptr);

    m_macdGraph = m_plot->addGraph(m_xAxis, m_yAxis);
    Q_CHECK_PTR(m_macdGraph);
    m_macdGraph->setName(QStringLiteral("MACD"));
    m_macdGraph->setPen(QPen(MACD_LINE_COLOR, 2));
    m_macdGraph->setLineStyle(QCPGraph::lsLine);
    m_macdGraph->setScatterStyle(QCPScatterStyle::ssNone);
    m_macdGraph->setLayer("main");

    m_signalGraph = m_plot->addGraph(m_xAxis, m_yAxis);
    Q_CHECK_PTR(m_signalGraph);
    m_signalGraph->setName(QStringLiteral("Signal"));
    m_signalGraph->setPen(QPen(SIGNAL_LINE_COLOR, 2));
    m_signalGraph->setLineStyle(QCPGraph::lsLine);
    m_signalGraph->setScatterStyle(QCPScatterStyle::ssNone);
    m_signalGraph->setLayer("main");

    m_histogramPositive = new QCPBars(m_xAxis, m_yAxis);
    Q_CHECK_PTR(m_histogramPositive);
    m_histogramPositive->setName(QStringLiteral("MACD Histogram +"));
    m_histogramPositive->setPen(Qt::NoPen);
    m_histogramPositive->setBrush(HISTOGRAM_POSITIVE_COLOR);
    m_histogramPositive->setLayer("main");

    m_histogramNegative = new QCPBars(m_xAxis, m_yAxis);
    Q_CHECK_PTR(m_histogramNegative);
    m_histogramNegative->setName(QStringLiteral("MACD Histogram -"));
    m_histogramNegative->setPen(Qt::NoPen);
    m_histogramNegative->setBrush(HISTOGRAM_NEGATIVE_COLOR);
    m_histogramNegative->setLayer("main");

    applyVisibility();
}

MacdIndicator::~MacdIndicator()
{
    if (m_plot == nullptr)
    {
        return;
    }

    if (m_macdGraph != nullptr)
    {
        m_plot->removePlottable(m_macdGraph);
        m_macdGraph = nullptr;
    }
    if (m_signalGraph != nullptr)
    {
        m_plot->removePlottable(m_signalGraph);
        m_signalGraph = nullptr;
    }
    if (m_histogramPositive != nullptr)
    {
        m_plot->removePlottable(m_histogramPositive);
        m_histogramPositive = nullptr;
    }
    if (m_histogramNegative != nullptr)
    {
        m_plot->removePlottable(m_histogramNegative);
        m_histogramNegative = nullptr;
    }
}

void MacdIndicator::setVisible(const bool visible)
{
    m_visible = visible;
    applyVisibility();
}

void MacdIndicator::setSettings(const Settings& settings)
{
    Settings sanitized = settings;
    sanitized.fastLength = qBound(1, settings.fastLength, 499);
    sanitized.slowLength = qBound(sanitized.fastLength + 1, settings.slowLength, 500);
    sanitized.signalLength = qBound(1, settings.signalLength, 500);
    m_settings = sanitized;
}

void MacdIndicator::rebuild(const UpdateContext& context)
{
    if (!m_visible)
    {
        applyVisibility();
        return;
    }

    if (!context.index0Timestamp.isValid() || context.indexToBar.isEmpty() ||
        !BarUtils::isIntradayTimeFrame(context.displayTimeFrame))
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

    const QVector<double> fastAverage =
        calculateMovingAverage(closePrices, m_settings.fastLength, m_settings.macdMaType);
    const QVector<double> slowAverage =
        calculateMovingAverage(closePrices, m_settings.slowLength, m_settings.macdMaType);

    QVector<double> macdValues;
    macdValues.resize(closePrices.size());
    for (qsizetype i = 0; i < macdValues.size(); ++i)
    {
        macdValues[i] = fastAverage[i] - slowAverage[i];
    }

    const QVector<double> signalValues =
        calculateMovingAverage(macdValues, m_settings.signalLength, m_settings.signalMaType);

    QVector<double> histogramValues;
    histogramValues.resize(macdValues.size());
    for (qsizetype i = 0; i < histogramValues.size(); ++i)
    {
        histogramValues[i] = macdValues[i] - signalValues[i];
    }

    ASSUME_TRUE(m_macdGraph != nullptr);
    ASSUME_TRUE(m_signalGraph != nullptr);
    ASSUME_TRUE(m_histogramPositive != nullptr);
    ASSUME_TRUE(m_histogramNegative != nullptr);

    m_macdGraph->setData(keys, macdValues, true);
    m_signalGraph->setData(keys, signalValues, true);

    QVector<double> positiveKeys;
    QVector<double> positiveValues;
    QVector<double> negativeKeys;
    QVector<double> negativeValues;
    positiveKeys.reserve(histogramValues.size());
    positiveValues.reserve(histogramValues.size());
    negativeKeys.reserve(histogramValues.size());
    negativeValues.reserve(histogramValues.size());

    for (qsizetype i = 0; i < histogramValues.size(); ++i)
    {
        const double value = histogramValues[i];
        if (value >= 0.0)
        {
            positiveKeys.append(keys[i]);
            positiveValues.append(value);
        }
        else
        {
            negativeKeys.append(keys[i]);
            negativeValues.append(value);
        }
    }

    const double histogramWidth = qMax(0.2, static_cast<double>(indexStepUnits(context.displayTimeFrame)) * 0.7);
    m_histogramPositive->setWidth(histogramWidth);
    m_histogramNegative->setWidth(histogramWidth);
    m_histogramPositive->setData(positiveKeys, positiveValues, true);
    m_histogramNegative->setData(negativeKeys, negativeValues, true);

    updateAxisRange(macdValues, signalValues, histogramValues);
    applyVisibility();
}

void MacdIndicator::clear()
{
    m_axisRange.reset();
    if (m_macdGraph != nullptr)
    {
        m_macdGraph->data()->clear();
    }
    if (m_signalGraph != nullptr)
    {
        m_signalGraph->data()->clear();
    }
    if (m_histogramPositive != nullptr)
    {
        m_histogramPositive->data()->clear();
    }
    if (m_histogramNegative != nullptr)
    {
        m_histogramNegative->data()->clear();
    }

    applyVisibility();
}

int MacdIndicator::indexStepUnits(const TimeFrame tf)
{
    return BarUtils::minutesPerBar(tf);
}

double MacdIndicator::indexKeyOffset(const TimeFrame tf)
{
    return static_cast<double>(BarUtils::minutesPerBar(tf)) / 2.0;
}

QVector<double> MacdIndicator::calculateMovingAverage(const QVector<double>& values, int length, const MaType maType)
{
    QVector<double> averages(values.size(), 0.0);
    if (values.isEmpty())
    {
        return averages;
    }

    length = qMax(1, length);

    if (maType == MaType::Sma)
    {
        double rollingSum = 0.0;
        for (qsizetype i = 0; i < values.size(); ++i)
        {
            rollingSum += values[i];
            if (i >= length)
            {
                rollingSum -= values[i - length];
            }

            const int divisor = static_cast<int>(qMin(i + 1, static_cast<qsizetype>(length)));
            averages[i] = rollingSum / static_cast<double>(divisor);
        }

        return averages;
    }

    const double alpha = 2.0 / static_cast<double>(length + 1);
    double ema = values[0];
    averages[0] = ema;

    for (qsizetype i = 1; i < values.size(); ++i)
    {
        ema = alpha * values[i] + (1.0 - alpha) * ema;
        averages[i] = ema;
    }

    return averages;
}

void MacdIndicator::applyVisibility() const
{
    if (m_macdGraph != nullptr)
    {
        m_macdGraph->setVisible(m_visible && !m_macdGraph->data()->isEmpty());
    }
    if (m_signalGraph != nullptr)
    {
        m_signalGraph->setVisible(m_visible && !m_signalGraph->data()->isEmpty());
    }

    const bool histogramVisible = m_visible && m_settings.showHistogram;
    if (m_histogramPositive != nullptr)
    {
        m_histogramPositive->setVisible(histogramVisible && !m_histogramPositive->data()->isEmpty());
    }
    if (m_histogramNegative != nullptr)
    {
        m_histogramNegative->setVisible(histogramVisible && !m_histogramNegative->data()->isEmpty());
    }
}

void MacdIndicator::updateAxisRange(const QVector<double>& macdValues,
                                    const QVector<double>& signalValues,
                                    const QVector<double>& histogramValues)
{
    double minValue = std::numeric_limits<double>::max();
    double maxValue = std::numeric_limits<double>::lowest();

    const auto includeValues = [&](const QVector<double>& values)
    {
        for (const double value: values)
        {
            minValue = qMin(minValue, value);
            maxValue = qMax(maxValue, value);
        }
    };

    includeValues(macdValues);
    includeValues(signalValues);
    if (m_settings.showHistogram)
    {
        includeValues(histogramValues);
    }

    if (minValue > maxValue)
    {
        m_axisRange.setAutomaticRange(m_yAxis, QCPRange(-1.0, 1.0));
        return;
    }

    if (qFuzzyCompare(minValue + 1.0, maxValue + 1.0))
    {
        const double delta = qMax(0.1, qAbs(minValue) * 0.1);
        const double extent = qAbs(minValue) + delta;
        m_axisRange.setAutomaticRange(m_yAxis, QCPRange(-extent, extent));
        return;
    }

    const double padding = qMax(0.02, (maxValue - minValue) * 0.15);
    const double extent = qMax(qAbs(minValue), qAbs(maxValue)) + padding;
    m_axisRange.setAutomaticRange(m_yAxis, QCPRange(-extent, extent));
}
