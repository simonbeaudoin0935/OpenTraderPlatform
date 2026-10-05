#pragma once

#include <QString>

/**
 * @file ReplayDataQueries.h
 * @brief SQL queries for loading recorded data during replay
 *
 * These queries are designed for efficient chunk-based loading with
 * ping-pong buffering. Data is loaded by ID ranges to support seamless
 * sequential playback.
 */

namespace ReplayDataQueries
{

    /**
     * @brief Select next N bar records starting from given ID
     *
     * Used for ping-pong buffer loading. Orders by ID (which reflects
     * insertion order, i.e., arrival time).
     *
     * Parameters: 1) start ID, 2) limit count
     */
    const QString SELECT_BARS_CHUNK = "SELECT id, stockTicker, epochMs, objectType, jsonRawData FROM bars "
                                      "WHERE id >= ? "
                                      "ORDER BY id ASC "
                                      "LIMIT ?";

    /**
     * @brief Select next N market depth records starting from given ID
     *
     * Parameters: 1) start ID, 2) limit count
     */
    const QString SELECT_MARKET_DEPTH_CHUNK =
        "SELECT id, stockTicker, epochMs, objectType, jsonRawData FROM market_depth_quotes "
        "WHERE id >= ? "
        "ORDER BY id ASC "
        "LIMIT ?";

    /**
     * @brief Select bar records from a specific timestamp forward
     *
     * Used for initial load when user specifies a start time.
     *
     * Parameters: 1) start epochMs, 2) limit count
     */
    const QString SELECT_BARS_FROM_TIME = "SELECT id, stockTicker, epochMs, objectType, jsonRawData FROM bars "
                                          "WHERE epochMs >= ? "
                                          "ORDER BY id ASC "
                                          "LIMIT ?";

    /**
     * @brief Select market depth records from a specific timestamp forward
     *
     * Parameters: 1) start epochMs, 2) limit count
     */
    const QString SELECT_MARKET_DEPTH_FROM_TIME =
        "SELECT id, stockTicker, epochMs, objectType, jsonRawData FROM market_depth_quotes "
        "WHERE epochMs >= ? "
        "ORDER BY id ASC "
        "LIMIT ?";

    /**
     * @brief Get first (minimum) timestamp in bars table
     */
    const QString SELECT_FIRST_BAR_TIMESTAMP = "SELECT MIN(epochMs) FROM bars";

    /**
     * @brief Get last (maximum) timestamp in bars table
     */
    const QString SELECT_LAST_BAR_TIMESTAMP = "SELECT MAX(epochMs) FROM bars";

    /**
     * @brief Get first (minimum) timestamp in market depth table
     */
    const QString SELECT_FIRST_DEPTH_TIMESTAMP = "SELECT MIN(epochMs) FROM market_depth_quotes";

    /**
     * @brief Get last (maximum) timestamp in market depth table
     */
    const QString SELECT_LAST_DEPTH_TIMESTAMP = "SELECT MAX(epochMs) FROM market_depth_quotes";

    /**
     * @brief Get list of distinct stock tickers in bars table
     */
    const QString SELECT_AVAILABLE_STOCKS_BARS = "SELECT DISTINCT stockTicker FROM bars ORDER BY stockTicker";

    /**
     * @brief Get list of distinct stock tickers in market depth table
     */
    const QString SELECT_AVAILABLE_STOCKS_DEPTH =
        "SELECT DISTINCT stockTicker FROM market_depth_quotes ORDER BY stockTicker";

    /**
     * @brief Count total bar records (for progress calculation)
     */
    const QString COUNT_BARS = "SELECT COUNT(*) FROM bars";

    /**
     * @brief Count total market depth records (for progress calculation)
     */
    const QString COUNT_MARKET_DEPTH = "SELECT COUNT(*) FROM market_depth_quotes";

    /**
     * @brief Count bar records from a specific timestamp (for progress after start time)
     *
     * Parameter: 1) start epochMs
     */
    const QString COUNT_BARS_FROM_TIME = "SELECT COUNT(*) FROM bars WHERE epochMs >= ?";

    /**
     * @brief Count market depth records from a specific timestamp
     *
     * Parameter: 1) start epochMs
     */
    const QString COUNT_MARKET_DEPTH_FROM_TIME = "SELECT COUNT(*) FROM market_depth_quotes WHERE epochMs >= ?";

    // ============================================================================
    // Quote queries (Level 1 data)
    // ============================================================================

    /**
     * @brief Select next N quote records starting from given ID
     *
     * Parameters: 1) start ID, 2) limit count
     */
    const QString SELECT_QUOTES_CHUNK = "SELECT id, stockTicker, epochMs, objectType, jsonRawData FROM quotes "
                                        "WHERE id >= ? "
                                        "ORDER BY id ASC "
                                        "LIMIT ?";

    /**
     * @brief Select quote records from a specific timestamp forward
     *
     * Parameters: 1) start epochMs, 2) limit count
     */
    const QString SELECT_QUOTES_FROM_TIME = "SELECT id, stockTicker, epochMs, objectType, jsonRawData FROM quotes "
                                            "WHERE epochMs >= ? "
                                            "ORDER BY id ASC "
                                            "LIMIT ?";

    /**
     * @brief Get first (minimum) timestamp in quotes table
     */
    const QString SELECT_FIRST_QUOTE_TIMESTAMP = "SELECT MIN(epochMs) FROM quotes";

    /**
     * @brief Get last (maximum) timestamp in quotes table
     */
    const QString SELECT_LAST_QUOTE_TIMESTAMP = "SELECT MAX(epochMs) FROM quotes";

    /**
     * @brief Get list of distinct stock tickers in quotes table
     */
    const QString SELECT_AVAILABLE_STOCKS_QUOTES =
        "SELECT DISTINCT stockTicker FROM quotes WHERE stockTicker != '' ORDER BY stockTicker";

    /**
     * @brief Count total quote records (for progress calculation)
     */
    const QString COUNT_QUOTES = "SELECT COUNT(*) FROM quotes";

    /**
     * @brief Count quote records from a specific timestamp
     *
     * Parameter: 1) start epochMs
     */
    const QString COUNT_QUOTES_FROM_TIME = "SELECT COUNT(*) FROM quotes WHERE epochMs >= ?";

} // namespace ReplayDataQueries
