#pragma once

#include <QString>

namespace LiveStreamDBQueries
{
    const QString CREATE_BARS_TABLE =
        "CREATE TABLE IF NOT EXISTS bars ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "stockTicker TEXT, "       // e.g. "AAPL"
        "stockTickerSeq INTEGER, " // sequence number per stock ticker
        "epochMs INTEGER, "        // milliseconds since epoch
        "jsonRawData TEXT)";       // JSON fragment, possibly incomplete due to TCP packet splitting

    const QString INSERT_BAR = "INSERT INTO bars (stockTicker, stockTickerSeq, epochMs, jsonRawData) "
                               "VALUES (?, ?, ?, ?)";

    const QString CREATE_MARKET_DEPTH_QUOTES_TABLE =
        "CREATE TABLE IF NOT EXISTS market_depth_quotes ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "stockTicker TEXT, "       // e.g. "AAPL"
        "stockTickerSeq INTEGER, " // sequence number per stock ticker
        "epochMs INTEGER, "        // milliseconds since epoch
        "jsonRawData TEXT)";       // JSON fragment, possibly incomplete due to TCP packet splitting

    const QString INSERT_MARKET_DEPTH_QUOTE =
        "INSERT INTO market_depth_quotes (stockTicker, stockTickerSeq, epochMs, jsonRawData) "
        "VALUES (?, ?, ?, ?)";

    const QString SELECT_COUNT_FROM_TABLE = "SELECT COUNT(*) FROM %1";

    // Indexes for efficient replay queries (created after recording or on first replay)
    const QString CREATE_BARS_EPOCH_INDEX = "CREATE INDEX IF NOT EXISTS idx_bars_epochMs ON bars(epochMs)";

    const QString CREATE_BARS_TICKER_INDEX = "CREATE INDEX IF NOT EXISTS idx_bars_stockTicker ON bars(stockTicker)";

    const QString CREATE_DEPTH_EPOCH_INDEX =
        "CREATE INDEX IF NOT EXISTS idx_market_depth_epochMs ON market_depth_quotes(epochMs)";

    const QString CREATE_DEPTH_TICKER_INDEX =
        "CREATE INDEX IF NOT EXISTS idx_market_depth_stockTicker ON market_depth_quotes(stockTicker)";

} // namespace LiveStreamDBQueries
