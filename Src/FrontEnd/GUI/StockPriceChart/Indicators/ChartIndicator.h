#pragma once

#include <QDateTime>
#include <QMap>
#include <QString>

#include "Bar.h"
#include "Misc/TimeFrame.h"

/**
 * @brief Base contract for chart indicators.
 *
 * Indicator implementations own their plot items and rebuild from the current
 * chart bar map whenever requested by StockPriceChart.
 */
class ChartIndicator
{
  public:
    enum class Pane : quint8
    {
        Overlay,
        Subpane,
    };

    struct UpdateContext
    {
        const QMap<int, Bar>& indexToBar;
        QDateTime index0Timestamp;
        TimeFrame displayTimeFrame = TimeFrame::ONE_MINUTE;
    };

    virtual ~ChartIndicator() = default;

    [[nodiscard]] virtual QString id() const = 0;
    [[nodiscard]] virtual Pane pane() const = 0;

    virtual void setVisible(bool visible) = 0;
    [[nodiscard]] virtual bool isVisible() const = 0;

    virtual void rebuild(const UpdateContext& context) = 0;
    virtual void clear() = 0;
};
