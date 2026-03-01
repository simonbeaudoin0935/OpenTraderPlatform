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
 * - Early Pre-Market: 4:00 AM - 5:59 AM ET
 * - Pre-Market: 6:00 AM - 9:29 AM ET
 * - Regular Hours: 9:30 AM - 3:59 PM ET
 * - After Hours: 4:00 PM - 6:59 PM ET
 */
namespace TradingHours
{
    // Market timezone
    inline const QTimeZone MARKET_TIMEZONE = QTimeZone("America/New_York");

    inline const QTime TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION = QTime(4, 0); // 4:00 AM ET
    inline const QTime TIME_LAST_CANDLE_EARLY_PRE_MARKET_SESSION = QTime(5, 59); // 5:59 AM ET

    inline const QTime TIME_FIRST_CANDLE_PRE_MARKET_SESSION = QTime(6, 0); // 6:00 AM ET
    inline const QTime TIME_LAST_CANDLE_PRE_MARKET_SESSION = QTime(9, 29); // 9:29 AM ET

    inline const QTime TIME_FIRST_CANDLE_REGULAR_SESSION = QTime(9, 30); // 9:30 AM ET
    inline const QTime TIME_LAST_CANDLE_REGULAR_SESSION = QTime(15, 59); // 3:59 PM ET

    inline const QTime TIME_FIRST_CANDLE_AFTER_MARKET_SESSION = QTime(16, 0); // 4:00 PM ET
    inline const QTime TIME_LAST_CANDLE_AFTER_MARKET_SESSION =
        QTime(18, 59); // 6:59 PM ET (XNAS.ITCH ends at midnight UTC)


    // Day of week constants (Qt uses 1-7 for Monday-Sunday)
    inline constexpr int MONDAY = 1; // Qt::Monday
    inline constexpr int FRIDAY = 5; // Qt::Friday

} // namespace TradingHours

namespace BarsConstants
{
    // Number of bars per trading day (4:00 AM to 6:59 PM, 1-minute bars)
    inline constexpr unsigned int MINUTE_BARS_PER_DAY = 15 * 60; // 900 bars

    /**
     * @brief Converts a QTime timestamp to the corresponding index in the daily bar cache vector.
     *
     * 1-minute bars are timestamped using the **open time** of the interval (Databento convention).
     * For extended hours trading (4:00 AM – 6:59 PM ET):
     *   - The first bar (covering 4:00:00 – 4:00:59) is timestamped 4:00 AM
     *   - The last bar  (covering 6:59:00 – 6:59:59 PM) is timestamped 6:59 PM
     *
     * Therefore, valid bar timestamps range from 4:00 AM to 6:59 PM inclusive.
     * The cache vector is pre-allocated with exactly 900 elements (15 hours × 60 minutes),
     * where index 0 corresponds to the 4:00 AM bar and index 899 to the 6:59 PM bar.
     *
     * @param time The timestamp of the bar (must be a valid bar open time)
     * @return size_t The zero-based index in the daily cache vector (0 to 899)
     *
     * @pre time is a valid 1-minute bar open time in extended hours:
     *      - 4:00 AM ≤ time ≤ 6:59 PM
     * @note The function asserts on invalid inputs in debug builds.
     */
    constexpr size_t timeToIndex(const QTime& time)
    {
        ASSUME_GTE(time, TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);
        ASSUME_LTE(time, TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);

        // Total minutes since 4:00 AM — open-time convention, no offset needed
        size_t index =
            (time.hour() - TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION.hour()) * 60 + time.minute();

        ASSUME_LT(index,
                  MINUTE_BARS_PER_DAY); // 0 ≤ index ≤ 899
        return index;
    }

    /**
     * @brief Converts a daily bar cache index to the corresponding bar timestamp (QTime).
     *
     * The timestamp returned is the **open time** of the 1-minute bar (Databento convention).
     * For extended hours (4:00 AM – 6:59 PM ET):
     *   - index 0   → 4:00 AM  (bar covering 4:00:00 – 4:00:59)
     *   - index 899 → 6:59 PM (bar covering 6:59:00 – 6:59:59 PM)
     *
     * @param index Zero-based index in the daily cache vector (0 to 899)
     * @return QTime The timestamp (open time) of the bar
     *
     * @pre index < MINUTE_BARS_PER_DAY (900)
     * @note Returned times are always valid bar timestamps: 4:00 AM to 6:59 PM inclusive.
     */
    constexpr QTime indexToTime(size_t index)
    {
        ASSUME_LT(index, MINUTE_BARS_PER_DAY);

        int hour = TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION.hour() + (index / 60);
        int minute = index % 60;

        // At index 899: 899 / 60 = 14 hours → 4 + 14 = 18, minute = 59 → 6:59 PM
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
    inline constexpr const char* STREAM_QUOTES = "marketdata/stream/quotes/%1";

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
 * @namespace PollingConstants
 * @brief Polling intervals for various background tasks
 */
namespace PollingConstants
{
    // Balance polling interval (milliseconds)
    inline constexpr int BALANCE_POLLING_INTERVAL_MS = 1000; // 1 second
} // namespace PollingConstants

/**
 * @namespace StreamConstants
 * @brief Constants related to streaming and heartbeat
 */
namespace StreamConstants
{
    // Heartbeat interval for mock streams in replay mode (milliseconds)
    // Should be less than the stream heartbeat timeout (10 seconds)
    inline constexpr int MOCK_HEARTBEAT_INTERVAL_MS = 5000;
} // namespace StreamConstants

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

    /**
     * @brief Delay in milliseconds before processing queued stream requests
     *
     * When a market depth stream is closed and queued requests exist, we delay processing
     * the next request to allow the TCP FIN packet to propagate to the TradeStation server.
     * This prevents the server from seeing 11 concurrent connections due to network timing.
     *
     * @note Value of 1000ms provides sufficient time for TCP close to complete
     */
    inline constexpr int QUEUE_PROCESS_DELAY_MS = 1000;
} // namespace MarketDepthConstants

/**
 * @namespace QuoteConstants
 * @brief Constants related to level 1 quote stream data
 */
namespace QuoteConstants
{
    /**
     * @brief Maximum number of symbols in a single quote stream request
     *
     * TradeStation Stream Quotes API accepts up to 100 comma-separated symbols per request.
     *
     * @doc https://api.tradestation.com/docs/specification/#tag/MarketData/operation/GetQuoteChangeStream
     */
    inline constexpr size_t MAX_SYMBOLS_PER_STREAM = 100;
} // namespace QuoteConstants

/**
 * @namespace RecorderConstants
 * @brief Constants related to the recording system
 */
namespace RecorderConstants
{
    /**
     * @brief Graduated ramp-up delays for stream opening during recorder startup
     *
     * When recording a large number of symbols, the TradeStation API has cumulative
     * rate limits that become stricter as more streams are opened. We use a graduated
     * delay strategy that slows down progressively:
     *
     * - Streams 1-100:   500ms delay (2 streams/second)
     * - Streams 101-200: 1000ms delay (1 stream/second)
     * - Streams 201+:    2000ms delay (0.5 streams/second)
     *
     * Testing shows:
     * - 50 simultaneous streams: ✅ Works
     * - 100 simultaneous streams: ❌ API rate limiting errors
     * - 100+ with 500ms ramp-up: ✅ Works up to ~100 streams
     * - 200+ requires slower ramp-up to avoid cumulative rate limits
     *
     * @note Applies to both bar and market depth streams
     */
    inline constexpr int STREAM_RAMP_UP_DELAY_TIER1_MS = 500;  // First 100 streams
    inline constexpr int STREAM_RAMP_UP_DELAY_TIER2_MS = 1000; // Streams 101-200
    inline constexpr int STREAM_RAMP_UP_DELAY_TIER3_MS = 2000; // Streams 201+

    inline constexpr int STREAM_RAMP_UP_TIER1_THRESHOLD = 100; // Switch to tier 2 after this many
    inline constexpr int STREAM_RAMP_UP_TIER2_THRESHOLD = 200; // Switch to tier 3 after this many
} // namespace RecorderConstants
