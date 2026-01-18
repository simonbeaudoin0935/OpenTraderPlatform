#pragma once

#include <QString>

namespace SqlQueries
{
const QString CREATE_BARS_TABLE = "CREATE TABLE IF NOT EXISTS bars ("
                                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                  "stockTicker TEXT, "       // e.g. "AAPL"
                                  "stockTickerSeq INTEGER, " // sequence number per stock ticker
                                  "epochMs INTEGER, "        // milliseconds since epoch
                                  "jsonRawData TEXT)"; // JSON fragment, possibly incomplete due to TCP packet splitting

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
} // namespace SqlQueries