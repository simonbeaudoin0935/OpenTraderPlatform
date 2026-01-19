#pragma once

#include <QString>

namespace StockPriceChartQueries
{
const QString SELECT_STOCK_TIME_RANGE = "SELECT MIN(epochMs), MAX(epochMs), COUNT(*) FROM bars "
                                        "WHERE stockTicker = ? AND epochMs >= ? AND epochMs <= ?";
} // namespace StockPriceChartQueries
