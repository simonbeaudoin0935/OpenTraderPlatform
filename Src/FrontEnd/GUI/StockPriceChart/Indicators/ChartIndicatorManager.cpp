#include "Indicators/ChartIndicatorManager.h"

#include "Assume.h"

void ChartIndicatorManager::addIndicator(std::unique_ptr<ChartIndicator> indicator)
{
    ASSUME_TRUE(indicator != nullptr);
    m_indicators.emplace_back(std::move(indicator));
}

bool ChartIndicatorManager::setIndicatorVisible(const QString& indicatorId, const bool visible)
{
    for (const auto& indicator: m_indicators)
    {
        if (indicator->id() == indicatorId)
        {
            indicator->setVisible(visible);
            return true;
        }
    }

    return false;
}

bool ChartIndicatorManager::isIndicatorVisible(const QString& indicatorId) const
{
    for (const auto& indicator: m_indicators)
    {
        if (indicator->id() == indicatorId)
        {
            return indicator->isVisible();
        }
    }

    return false;
}

void ChartIndicatorManager::rebuildAll(const ChartIndicator::UpdateContext& context)
{
    for (const auto& indicator: m_indicators)
    {
        indicator->rebuild(context);
    }
}

void ChartIndicatorManager::clearAll()
{
    for (const auto& indicator: m_indicators)
    {
        indicator->clear();
    }
}
