#pragma once

#include <optional>

#include "Assume.h"
#include "qcustomplot.h"

class IndicatorAxisRange
{
  public:
    void setAutomaticRange(QCPAxis* p_axis, const QCPRange& p_range)
    {
        ASSUME_TRUE(p_axis != nullptr);
        if (m_lastAutomaticRange.has_value() && p_axis->range() != m_lastAutomaticRange.value())
        {
            m_manuallyAdjusted = true;
        }
        if (m_manuallyAdjusted)
        {
            return;
        }

        p_axis->setRange(p_range);
        m_lastAutomaticRange = p_axis->range();
    }

    void reset()
    {
        m_lastAutomaticRange.reset();
        m_manuallyAdjusted = false;
    }

  private:
    std::optional<QCPRange> m_lastAutomaticRange;
    bool m_manuallyAdjusted = false;
};
