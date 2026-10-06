#pragma once

#include <QVector>
#include <optional>

#include "Bar.h"

namespace BarHistoryBackfill
{
    // The anchor must come from the saved snapshot, before new live bars advance its tail.
    [[nodiscard]] inline std::optional<QDateTime>
    start(const QVector<Bar>& p_savedBars,
          const QDateTime& p_dayStart,
          const QDateTime& p_latestCompleted,
          const std::optional<QDateTime>& p_verifiedThrough = std::nullopt)
    {
        if (p_latestCompleted < p_dayStart ||
            (p_verifiedThrough.has_value() && *p_verifiedThrough >= p_latestCompleted))
        {
            return std::nullopt;
        }
        std::optional<QDateTime> latestClosed;
        for (const Bar& bar: p_savedBars)
        {
            if (bar.getBarStatus() == Bar::BarStatus::Closed &&
                (!latestClosed.has_value() || bar.getTimeStamp() > *latestClosed))
            {
                latestClosed = bar.getTimeStamp();
            }
        }
        if (latestClosed.has_value() && *latestClosed >= p_latestCompleted)
        {
            return std::nullopt;
        }
        // Include the last saved candle to finalize the overlap at the restart boundary.
        const QDateTime anchor = latestClosed.value_or(p_dayStart);
        return p_verifiedThrough.has_value() && *p_verifiedThrough > anchor ? *p_verifiedThrough : anchor;
    }

    [[nodiscard]] inline bool shouldReplace(const Bar& p_existing, const Bar& p_incoming)
    {
        if (p_incoming.getBarStatus() == Bar::BarStatus::Null ||
            p_incoming.getBarStatus() == Bar::BarStatus::Uninitialized)
        {
            return p_existing.getBarStatus() == Bar::BarStatus::Null ||
                   p_existing.getBarStatus() == Bar::BarStatus::Uninitialized;
        }
        return p_incoming.getBarStatus() != Bar::BarStatus::Open ||
               (p_existing.getBarStatus() != Bar::BarStatus::Closed &&
                p_existing.getBarStatus() != Bar::BarStatus::Open);
    }
} // namespace BarHistoryBackfill
