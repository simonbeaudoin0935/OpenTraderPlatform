#include "Indicators/VolumeIndicator.h"

#include <array>
#include <QtMath>
#include <cmath>

#include "Assume.h"
#include "CONSTANTS.h"
#include "Misc/BarUtils.h"

namespace
{
    constexpr QColor VOLUME_POSITIVE_COLOR(100, 180, 110, 210);
    constexpr QColor VOLUME_NEGATIVE_COLOR(180, 90, 90, 210);
    constexpr double VOLUME_TOP_PADDING_MULTIPLIER = 1.05;
} // namespace

VolumeIndicator::VolumeIndicator(QCustomPlot* plot, QCPAxis* xAxis, QCPAxis* yAxis)
    : m_plot(plot), m_xAxis(xAxis), m_yAxis(yAxis)
{
    ASSUME_TRUE(m_plot != nullptr);
    ASSUME_TRUE(m_xAxis != nullptr);
    ASSUME_TRUE(m_yAxis != nullptr);

    m_volumePos = new QCPBars(m_xAxis, m_yAxis);
    Q_CHECK_PTR(m_volumePos);
    m_volumePos->setName(QStringLiteral("Volume Positive"));
    m_volumePos->setPen(Qt::NoPen);
    m_volumePos->setBrush(VOLUME_POSITIVE_COLOR);
    if (!m_volumePos->setLayer(QStringLiteral("volume-bars")))
    {
        m_volumePos->setLayer("main");
    }

    m_volumeNeg = new QCPBars(m_xAxis, m_yAxis);
    Q_CHECK_PTR(m_volumeNeg);
    m_volumeNeg->setName(QStringLiteral("Volume Negative"));
    m_volumeNeg->setPen(Qt::NoPen);
    m_volumeNeg->setBrush(VOLUME_NEGATIVE_COLOR);
    if (!m_volumeNeg->setLayer(QStringLiteral("volume-bars")))
    {
        m_volumeNeg->setLayer("main");
    }

    applyVisibility();
}

VolumeIndicator::~VolumeIndicator()
{
    if (m_plot == nullptr)
    {
        return;
    }

    if (m_volumePos != nullptr)
    {
        m_plot->removePlottable(m_volumePos);
        m_volumePos = nullptr;
    }

    if (m_volumeNeg != nullptr)
    {
        m_plot->removePlottable(m_volumeNeg);
        m_volumeNeg = nullptr;
    }
}

void VolumeIndicator::setVisible(const bool visible)
{
    m_visible = visible;
    applyVisibility();
    if (!m_visible)
    {
        clearStripTickLabels();
    }
}

void VolumeIndicator::setSettings(const Settings& settings)
{
    m_settings.autoScaleEnabled = settings.autoScaleEnabled;
    switch (settings.autoScaleMode)
    {
    case AutoScaleMode::HighestBar:
    case AutoScaleMode::SecondHighestBar:
        m_settings.autoScaleMode = settings.autoScaleMode;
        break;
    default:
        m_settings.autoScaleMode = AutoScaleMode::SecondHighestBar;
        break;
    }
}

void VolumeIndicator::setBarWidth(const double width)
{
    ASSUME_TRUE(m_volumePos != nullptr);
    ASSUME_TRUE(m_volumeNeg != nullptr);

    m_volumePos->setWidth(width);
    m_volumeNeg->setWidth(width);
}

void VolumeIndicator::rebuild(const UpdateContext& context)
{
    ASSUME_TRUE(m_volumePos != nullptr);
    ASSUME_TRUE(m_volumeNeg != nullptr);

    m_volumePos->data()->clear();
    m_volumeNeg->data()->clear();

    if (!m_visible)
    {
        applyVisibility();
        return;
    }

    if (context.indexToBar.isEmpty() || !BarUtils::isIntradayTimeFrame(context.displayTimeFrame))
    {
        applyVisibility();
        return;
    }

    const double keyOffset = indexKeyOffset(context.displayTimeFrame);

    for (auto it = context.indexToBar.begin(); it != context.indexToBar.end(); ++it)
    {
        const int index = it.key();
        const Bar& bar = it.value();
        const Bar::BarStatus status = bar.getBarStatus();
        if (status != Bar::BarStatus::Open && status != Bar::BarStatus::Closed)
        {
            continue;
        }

        const bool isUp = bar.getClose() >= bar.getOpen();
        const qint64 volume = static_cast<qint64>(bar.getTotalVolume());
        const double displayKey = static_cast<double>(index) + keyOffset;

        if (isUp)
        {
            m_volumePos->addData(displayKey, volume);
        }
        else
        {
            m_volumeNeg->addData(displayKey, volume);
        }
    }

    applyVisibility();
}

void VolumeIndicator::clear()
{
    if (m_volumePos != nullptr)
    {
        m_volumePos->data()->clear();
    }
    if (m_volumeNeg != nullptr)
    {
        m_volumeNeg->data()->clear();
    }

    applyVisibility();
    if (m_visible)
    {
        const double stripFraction = qBound(0.05, ChartConstants::BLENDED_VOLUME_STRIP_FRACTION, 0.95);
        const QCPRange range = m_yAxis->range();
        applyStripTickLabels(qMax(0.0, range.upper * stripFraction));
    }
    else
    {
        clearStripTickLabels();
    }
}

void VolumeIndicator::rescaleVisibleRange(const QMap<int, Bar>& indexToBar, const QCPRange& xRange)
{
    if (!m_visible)
    {
        clearStripTickLabels();
        return;
    }

    const double stripFraction = qBound(0.05, ChartConstants::BLENDED_VOLUME_STRIP_FRACTION, 0.95);
    if (!m_settings.autoScaleEnabled || indexToBar.isEmpty())
    {
        const QCPRange currentRange = m_yAxis->range();
        const double currentVisibleTop = qMax(0.0, currentRange.upper * stripFraction);
        applyStripTickLabels(currentVisibleTop);
        return;
    }

    const int visibleStart = qMax(static_cast<int>(qFloor(xRange.lower)), indexToBar.firstKey());
    const int visibleEnd = qMin(static_cast<int>(qCeil(xRange.upper)), indexToBar.lastKey());

    qint64 highestVolume = 0;
    qint64 secondHighestVolume = 0;
    qint64 highestOpenVolume = 0;

    for (int i = visibleStart; i <= visibleEnd; ++i)
    {
        if (!indexToBar.contains(i))
        {
            continue;
        }

        const Bar& bar = indexToBar[i];
        const Bar::BarStatus status = bar.getBarStatus();
        if (status != Bar::BarStatus::Open && status != Bar::BarStatus::Closed)
        {
            continue;
        }

        const qint64 volume = static_cast<qint64>(bar.getTotalVolume());
        if (status == Bar::BarStatus::Open)
        {
            highestOpenVolume = qMax(highestOpenVolume, volume);
        }

        if (volume > highestVolume)
        {
            secondHighestVolume = highestVolume;
            highestVolume = volume;
        }
        else if (volume > secondHighestVolume)
        {
            secondHighestVolume = volume;
        }
    }

    qint64 scaleVolume = highestVolume;
    if (m_settings.autoScaleMode == AutoScaleMode::SecondHighestBar && secondHighestVolume > 0)
    {
        scaleVolume = secondHighestVolume;
    }
    // Keep the active/open bar inside the blended strip even when SecondHighest
    // mode is selected and the current bar outgrows recent closed bars.
    if (highestOpenVolume > scaleVolume)
    {
        scaleVolume = highestOpenVolume;
    }

    if (scaleVolume > 0)
    {
        const double targetVisibleTop = static_cast<double>(scaleVolume) * VOLUME_TOP_PADDING_MULTIPLIER;
        m_yAxis->setRange(0.0, targetVisibleTop / stripFraction);
        applyStripTickLabels(targetVisibleTop);
        return;
    }

    const QCPRange currentRange = m_yAxis->range();
    const double currentVisibleTop = qMax(0.0, currentRange.upper * stripFraction);
    applyStripTickLabels(currentVisibleTop);
}

QVector<double> VolumeIndicator::buildStripTickVector(const double visibleTop)
{
    if (visibleTop <= 0.0)
    {
        return {};
    }

    constexpr int targetIntervals = 4; // ~5 labeled ticks including zero
    const double rawStep = visibleTop / static_cast<double>(targetIntervals);
    if (rawStep <= 0.0)
    {
        return {0.0, visibleTop};
    }

    const double magnitude = std::pow(10.0, std::floor(std::log10(rawStep)));
    const double normalized = rawStep / magnitude;

    double niceStep = magnitude;
    if (normalized > 5.0)
    {
        niceStep = 10.0 * magnitude;
    }
    else if (normalized > 2.0)
    {
        niceStep = 5.0 * magnitude;
    }
    else if (normalized > 1.0)
    {
        niceStep = 2.0 * magnitude;
    }

    QVector<double> ticks;
    ticks.reserve(8);
    ticks.push_back(0.0);

    for (double tick = niceStep; tick <= visibleTop + (niceStep * 0.01) && ticks.size() < 8; tick += niceStep)
    {
        ticks.push_back(tick);
    }

    const double lastTick = ticks.isEmpty() ? 0.0 : ticks.back();
    if (lastTick < visibleTop * 0.85 && ticks.size() < 8)
    {
        ticks.push_back(visibleTop);
    }

    return ticks;
}

QString VolumeIndicator::formatStripTickLabel(const double value)
{
    if (value <= 0.0)
    {
        return QStringLiteral("0");
    }

    struct Unit
    {
        double divisor;
        QString suffix;
    };

    static const std::array<Unit, 4> units{
        Unit{1e9, QStringLiteral("B")},
        Unit{1e6, QStringLiteral("M")},
        Unit{1e3, QStringLiteral("K")},
        Unit{1.0, QStringLiteral("")},
    };

    for (const Unit& unit: units)
    {
        if (value >= unit.divisor)
        {
            const double scaled = value / unit.divisor;
            const int decimals = scaled < 10.0 ? 2 : (scaled < 100.0 ? 1 : 0);
            QString text = QString::number(scaled, 'f', decimals);

            while (text.endsWith('0'))
            {
                text.chop(1);
            }
            if (text.endsWith('.'))
            {
                text.chop(1);
            }

            return text + unit.suffix;
        }
    }

    return QString::number(value, 'f', 0);
}

void VolumeIndicator::applyStripTickLabels(const double visibleTop)
{
    if (m_yAxis == nullptr)
    {
        return;
    }

    const QVector<double> ticks = buildStripTickVector(visibleTop);
    if (ticks.isEmpty())
    {
        clearStripTickLabels();
        return;
    }

    QVector<QString> labels;
    labels.reserve(ticks.size());
    for (const double tick: ticks)
    {
        labels.append(formatStripTickLabel(tick));
    }

    QSharedPointer<QCPAxisTickerText> ticker(new QCPAxisTickerText);
    ticker->setTicks(ticks, labels);
    m_yAxis->setTicker(ticker);
}

void VolumeIndicator::clearStripTickLabels()
{
    if (m_yAxis == nullptr)
    {
        return;
    }

    m_yAxis->setTicker(QSharedPointer<QCPAxisTicker>(new QCPAxisTicker));
}

double VolumeIndicator::indexKeyOffset(const TimeFrame tf)
{
    if (tf == TimeFrame::TEN_SECONDS)
    {
        return 0.5;
    }

    return static_cast<double>(BarUtils::minutesPerBar(tf)) / 2.0;
}

void VolumeIndicator::applyVisibility() const
{
    if (m_volumePos != nullptr)
    {
        m_volumePos->setVisible(m_visible);
    }
    if (m_volumeNeg != nullptr)
    {
        m_volumeNeg->setVisible(m_visible);
    }
}
