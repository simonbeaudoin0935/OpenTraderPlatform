#pragma once

#include <QTime>
#include <QTimeZone>
#include <QString>
#include <QStringList>

#include "Assume.h"

/**
 * @file CONSTANTS.h
 * @brief Centralized constants for the L2Trader application
 *
 * This file contains all application-wide constants organized into namespaces.
 * When adding new constants to the codebase, they should be added here rather
 * than defined in individual files.
 */

/**
 * @namespace TradingHours
 * @brief Trading hours and timing-related constants
 *
 * All times are in America/New_York timezone (Eastern Time).
 * The trading day is divided into segments:
 * - Early Pre-Market: 4:01 AM - 6:00 AM ET
 * - Pre-Market: 6:01 AM - 9:30 AM ET
 * - Regular Hours: 9:31 AM - 4:00 PM ET
 * - After Hours: 4:01 PM - 8:00 PM ET
 */
namespace TradingHours
{
    // Market timezone
    inline const QTimeZone MARKET_TIMEZONE = QTimeZone("America/New_York");

    inline const QTime TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION = QTime(4, 1); // 4:01 AM ET
    inline const QTime TIME_LAST_CANDLE_EARLY_PRE_MARKET_SESSION = QTime(6, 0);  // 6:00 AM ET

    inline const QTime TIME_FIRST_CANDLE_PRE_MARKET_SESSION = QTime(6, 1); // 6:01 AM ET
    inline const QTime TIME_LAST_CANDLE_PRE_MARKET_SESSION = QTime(9, 30); // 9:30 AM ET

    inline const QTime TIME_FIRST_CANDLE_REGULAR_SESSION = QTime(9, 31); // 9:31 AM ET
    inline const QTime TIME_LAST_CANDLE_REGULAR_SESSION = QTime(16, 0);  // 4:00 PM ET

    inline const QTime TIME_FIRST_CANDLE_AFTER_MARKET_SESSION = QTime(16, 1); // 4:01 PM ET
    inline const QTime TIME_LAST_CANDLE_AFTER_MARKET_SESSION = QTime(20, 0);  // 8:00 PM ET


    // Day of week constants (Qt uses 1-7 for Monday-Sunday)
    inline constexpr int MONDAY = 1; // Qt::Monday
    inline constexpr int FRIDAY = 5; // Qt::Friday

} // namespace TradingHours

namespace BarsConstants
{
    // Number of bars per trading day (4:01 AM to 8:00 PM, 1-minute bars)
    inline constexpr unsigned int MINUTE_BARS_PER_DAY = 16 * 60; // 960 bars

    /**
     * @brief Converts a QTime timestamp to the corresponding index in the daily bar cache vector.
     *
     * TradeStation timestamps 1-minute bars using the **closing time** of the interval.
     * For extended hours trading (4:00 AM – 8:00 PM ET):
     *   - The first bar (covering 4:00:00 – 4:00:59) is timestamped 4:01 AM
     *   - The last bar  (covering 7:59:00 – 7:59:59) is timestamped 8:00 PM
     *
     * Therefore, valid bar timestamps range from 4:01 AM to 8:00 PM inclusive.
     * The cache vector is pre-allocated with exactly 960 elements (16 hours × 60 minutes),
     * where index 0 corresponds to the 4:01 AM bar and index 959 to the 8:00 PM bar.
     *
     * @param time The timestamp of the bar (must be a valid bar close time)
     * @return size_t The zero-based index in the daily cache vector (0 to 959)
     *
     * @pre time is a valid 1-minute bar close time in extended hours:
     *      - 4:01 AM ≤ time ≤ 8:00 PM
     * @note The function asserts on invalid inputs in debug builds.
     */
    constexpr size_t timeToIndex(const QTime& time)
    {
        // Validate: must be between 4:01 AM and 8:00 PM inclusive
        ASSUME_GTE(time, TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);
        ASSUME_LTE(time, TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);

        // Total minutes since 4:01 AM
        // Adjust by -1 because first bar is timestamped at 4:01 (index 0)
        size_t index =
            (time.hour() - TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION.hour()) * 60 + time.minute() - 1;

        ASSUME_LT(index,
                  MINUTE_BARS_PER_DAY); // 0 ≤ index ≤ 959
        return index;
    }

    /**
     * @brief Converts a daily bar cache index to the corresponding bar timestamp (QTime).
     *
     * The timestamp returned is the **close time** of the 1-minute bar, as used by TradeStation.
     * For extended hours (4:00 AM – 8:00 PM ET):
     *   - index 0   → 4:01 AM  (bar covering 4:00:00 – 4:00:59)
     *   - index 959 → 8:00 PM (bar covering 7:59:00 – 7:59:59)
     *
     * @param index Zero-based index in the daily cache vector (0 to 959)
     * @return QTime The timestamp (close time) of the bar
     *
     * @pre index < MINUTE_BARS_PER_DAY (960)
     * @note Returned times are always valid bar timestamps: 4:01 AM to 8:00 PM inclusive.
     */
    constexpr QTime indexToTime(size_t index)
    {
        ASSUME_LT(index, MINUTE_BARS_PER_DAY);

        // Add 1 to offset the fact that index 0 = 4:01, not 4:00
        size_t adjustedMinutes = index + 1;

        int hour = TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION.hour() + (adjustedMinutes / 60);
        int minute = adjustedMinutes % 60;

        // At index 959: adjustedMinutes = 960 → 960 / 60 = 16 hours → 4 + 16 = 20 (8 PM), minute = 0
        return QTime(hour, minute, 0);
    }

} // namespace BarsConstants

/**
 * @namespace TSClientHosts
 * @brief TradeStation API host URLs
 *
 * TradeStation provides two API endpoints:
 * - Simulation: For paper trading with virtual money
 * - Live: For real money trading
 */
namespace TSClientHosts
{
    inline constexpr const char* SCHEME = "https";
    inline constexpr const char* SIM_HOST = "sim-api.tradestation.com";
    inline constexpr const char* LIVE_HOST = "api.tradestation.com";
    inline constexpr const char* API_VERSION = "/v3/";
} // namespace TSClientHosts

/**
 * @namespace TSClientEndpoints
 * @brief TradeStation API endpoint constants
 *
 * These endpoints are used by TSClient to interact with the TradeStation API.
 * The %1 placeholder is replaced with specific values at runtime (e.g., symbol, account ID).
 */
namespace TSClientEndpoints
{
    // Market Data endpoints
    inline constexpr const char* GET_QUOTE_SNAPSHOTS = "marketdata/quotes/%1";
    inline constexpr const char* GET_BARS = "marketdata/barcharts/%1";
    inline constexpr const char* STREAM_BARS = "marketdata/stream/barcharts/%1";
    inline constexpr const char* STREAM_MARKET_DEPTH_QUOTE = "marketdata/stream/marketdepth/quotes/%1";

    // Brokerage endpoints
    inline constexpr const char* GET_ACCOUNTS = "brokerage/accounts";
    inline constexpr const char* GET_BALANCES = "brokerage/accounts/%1/balances";
    inline constexpr const char* STREAM_ORDERS = "brokerage/stream/accounts/%1/orders";
    inline constexpr const char* STREAM_POSITIONS = "brokerage/stream/accounts/%1/positions";

    // Order Execution endpoints
    inline constexpr const char* PLACE_ORDER = "orderexecution/orders";
    inline constexpr const char* CANCEL_ORDER = "orderexecution/orders/%1";
} // namespace TSClientEndpoints

/**
 * @namespace AuthConstants
 * @brief Authentication and token-related constants
 */
namespace AuthConstants
{
    // Token validation
    inline const QString EXPECTED_TOKEN_TYPE = "Bearer";
    inline constexpr int EXPECTED_EXPIRES_IN = 1200; // 20 minutes
    inline constexpr int EXPIRY_BUFFER_SECONDS = 5;  // Buffer time before actual expiry

    // Maximum seconds to next refresh request (expires_in - buffer)
    inline constexpr int MAX_SECONDS_TO_NEXT_REFRESH_REQUEST = 1195;

    // Expected OAuth scopes
    inline const QStringList EXPECTED_SCOPES =
        {"openid", "profile", "MarketData", "ReadAccount", "Trade", "Matrix", "offline_access"};

    // Auth handler server configuration
    inline constexpr quint16 DEFAULT_AUTH_PORT = 8080;
    inline constexpr quint16 MAX_PORT_ATTEMPTS = 10;
} // namespace AuthConstants

/**
 * @namespace ChartConstants
 * @brief Constants related to chart display and rendering
 */
namespace ChartConstants
{
    // Maximum number of bars to display in chart
    inline const int MAX_BARS = 1000;

    // Minimum spacing for chart ticks
    inline constexpr int MIN_PIXELS_PER_TICK_X = 40;
    inline constexpr int MIN_PIXELS_PER_TICK_Y = 30;

    // Candlestick rendering
    inline constexpr double CANDLESTICK_BODY_WIDTH = 0.9; // 90% of available space


} // namespace ChartConstants

/**
 * @namespace BarFlags
 * @brief Bit flags for Bar status encoding
 */
namespace BarFlags
{
    inline constexpr quint8 FLAG_IS_REALTIME = 0x01;       // bit 0
    inline constexpr quint8 FLAG_IS_END_OF_HISTORY = 0x02; // bit 1
    inline constexpr quint8 BARSTATUS_SHIFT = 2;           // bits 2-3 for BarStatus
    inline constexpr quint8 BARSTATUS_MASK = 0x0C;         // bits 2-3 mask
} // namespace BarFlags

/**
 * @namespace FileSystemConstants
 * @brief File system paths and directory names
 */
namespace FileSystemConstants
{
    // Subdirectory name for bar cache database files
    inline constexpr const char* BARS_CACHE_SUBDIR = "Bars";
} // namespace FileSystemConstants

/**
 * @namespace MarketDepthConstants
 * @brief Constants related to market depth data
 */
namespace MarketDepthConstants
{
    // Default number of market depth levels to request from TradeStation
    inline constexpr int DEFAULT_MARKET_DEPTH_LEVELS = 10;
} // namespace MarketDepthConstants
