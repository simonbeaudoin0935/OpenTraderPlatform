#pragma once

#include <memory>
#include <vector>

#include "Indicators/ChartIndicator.h"

/**
 * @brief Lightweight container for chart indicators.
 *
 * Keeps indicator ownership centralized in StockPriceChart and provides simple
 * fan-out hooks for visibility, clear, and rebuild events.
 */
class ChartIndicatorManager
{
  public:
    ChartIndicatorManager() = default;
    ~ChartIndicatorManager() = default;

    ChartIndicatorManager(const ChartIndicatorManager&) = delete;
    ChartIndicatorManager& operator=(const ChartIndicatorManager&) = delete;

    void addIndicator(std::unique_ptr<ChartIndicator> indicator);

    [[nodiscard]] bool setIndicatorVisible(const QString& indicatorId, bool visible);
    [[nodiscard]] bool isIndicatorVisible(const QString& indicatorId) const;

    void rebuildAll(const ChartIndicator::UpdateContext& context);
    void clearAll();

  private:
    std::vector<std::unique_ptr<ChartIndicator>> m_indicators;
};
