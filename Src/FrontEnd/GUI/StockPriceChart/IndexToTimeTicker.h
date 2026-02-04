#pragma once

#include <functional>
#include "qcustomplot.h"

/**
 * @class IndexToTimeTicker
 * @brief A custom QCPAxisTicker that converts index values to time labels.
 *
 * This ticker is designed for charts that use an index-based positioning system
 * (0, 1, 2, ...) but need to display time labels (e.g., "09:30", "10:15").
 * It uses a callback function to convert indices to QDateTime, allowing the
 * chart to maintain the index-to-timestamp mapping.
 */
class IndexToTimeTicker : public QCPAxisTicker
{
  public:
    using IndexToTimestampFunc = std::function<QDateTime(int)>;

    IndexToTimeTicker() = default;

    /**
     * @brief Sets the callback function that converts an index to a QDateTime.
     * @param func The function that takes an int index and returns QDateTime.
     */
    void setIndexToTimestampFunction(IndexToTimestampFunc p_func)
    {
        m_indexToTimestamp = std::move(p_func);
    }

    /**
     * @brief Sets the time format for displaying tick labels.
     * @param format Qt time format string (e.g., "hh:mm" for "09:30").
     */
    void setTimeFormat(const QString& p_format)
    {
        m_timeFormat = p_format;
    }

  protected:
    /**
     * @brief Overrides getTickLabel to convert index to time string.
     */
    QString getTickLabel(double p_tick,
                         [[maybe_unused]] const QLocale& p_locale,
                         [[maybe_unused]] QChar p_formatChar,
                         [[maybe_unused]] int p_precision) override
    {
        if (!m_indexToTimestamp)
        {
            // Fallback: just return the index as a string
            return QString::number(static_cast<int>(p_tick));
        }

        int index = static_cast<int>(std::round(p_tick));
        QDateTime timestamp = m_indexToTimestamp(index);

        if (!timestamp.isValid())
        {
            return QString::number(index);
        }

        // Subtract 1 minute for display so labels show round hours (8:00, 9:00)
        // instead of :01 minutes (8:01, 9:01) since trading data starts at 4:01 AM
        QDateTime displayTime = timestamp.addSecs(-60);
        return displayTime.toString(m_timeFormat);
    }

    /**
     * @brief Override to ensure tick steps are integers (one bar = one index unit).
     */
    double getTickStep([[maybe_unused]] const QCPRange& p_range) override
    {
        // Calculate a reasonable tick step based on range
        double rangeSize = p_range.size();

        // Target approximately 6-10 ticks
        double rawStep = rangeSize / 8.0;

        // Round to nice values: 1, 5, 10, 15, 30, 60, 120, etc.
        if (rawStep <= 1)
        {
            return 1;
        }
        if (rawStep <= 5)
        {
            return 5;
        }
        if (rawStep <= 10)
        {
            return 10;
        }
        if (rawStep <= 15)
        {
            return 15;
        }
        if (rawStep <= 30)
        {
            return 30;
        }
        if (rawStep <= 60)
        {
            return 60;
        }
        if (rawStep <= 120)
        {
            return 120;
        }

        // For larger ranges, round to nearest multiple of 60
        return std::ceil(rawStep / 60.0) * 60.0;
    }

  private:
    IndexToTimestampFunc m_indexToTimestamp;
    QString m_timeFormat = "hh:mm";
};
