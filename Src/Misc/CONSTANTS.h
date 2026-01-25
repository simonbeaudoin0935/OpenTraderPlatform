#pragma once

#include <QTime>
#include <QTimeZone>
#include <QString>
#include <QStringList>

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
 * - Pre-Market: 4:00 AM - 9:30 AM ET
 * - Regular Hours: 9:30 AM - 4:00 PM ET
 * - After Hours: 4:00 PM - 8:00 PM ET
 * - Tracked Period: 6:01 AM - 8:00 PM ET (for bar caching)
 */
namespace TradingHours
{
// Market timezone
inline const QTimeZone NY_TIMEZONE = QTimeZone("America/New_York");

// Trading start and end times for bar caching (6:01 AM - 8:00 PM ET)
inline const QTime TRADING_START_TIME = QTime(6, 1); // 6:01 AM ET
inline const QTime TRADING_END_TIME = QTime(20, 0);  // 8:00 PM ET (20:00)

// Hour boundaries for chart display
inline constexpr int TRADING_START_HOUR = 6;   // 6:00 AM ET
inline constexpr int TRADING_END_HOUR = 20;    // 8:00 PM ET (20:00)
inline constexpr int LAST_TRADING_MINUTE = 59; // Last bar is at 7:59 PM

// Day of week constants (Qt uses 1-7 for Monday-Sunday)
inline constexpr int MONDAY = 1; // Qt::Monday
inline constexpr int FRIDAY = 5; // Qt::Friday

// Number of bars per trading day (6:01 AM to 8:00 PM, 1-minute bars)
inline constexpr unsigned int BARS_PER_DAY = 840;
} // namespace TradingHours

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

// Expected OAuth scopes
inline const QStringList EXPECTED_SCOPES =
    {"openid", "profile", "MarketData", "ReadAccount", "Trade", "Crypto", "OptionSpreads"};

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
