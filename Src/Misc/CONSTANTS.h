#pragma once

#include <QDate>
#include <QTime>
#include <QTimeZone>
#include <QString>
#include <QStringList>
#include <array>

#include "Assume.h"

/**
 * @file CONSTANTS.h
 * @brief Centralized constants for the OpenTraderPlatform application
 *
 * This file contains all application-wide constants organized into namespaces.
 * When adding new constants to the codebase, they should be added here rather
 * than defined in individual files.
 */

namespace GUIThemeConstants
{
    inline const QStringList UI_FONT_FAMILIES = {"Segoe UI", "Lato", "Noto Sans", "sans-serif"};
    inline const QStringList MONOSPACE_FONT_FAMILIES = {"Cascadia Code", "Consolas", "DejaVu Sans Mono", "monospace"};
    inline constexpr char APP_BACKGROUND[] = "#1e1e1e";
    inline constexpr char SIDEBAR_BACKGROUND[] = "#252526";
    inline constexpr char PANEL_BACKGROUND[] = "#2d2d30";
    inline constexpr char INPUT_BACKGROUND[] = "#3c3c3c";
    inline constexpr char BORDER[] = "#3c3c3c";
    inline constexpr char TEXT_PRIMARY[] = "#cccccc";
    inline constexpr char TEXT_MUTED[] = "#858585";
    inline constexpr char ACCENT[] = "#007acc";
    inline constexpr char SELECTION_BACKGROUND[] = "#094771";
} // namespace GUIThemeConstants

/**
 * @namespace TradingHours
 * @brief Trading hours and timing-related constants
 *
 * All times are in America/New_York timezone (Eastern Time).
 * The trading day is divided into segments:
 * - Early Pre-Market: 4:00 AM - 5:59 AM ET
 * - Pre-Market: 6:00 AM - 9:29 AM ET
 * - Regular Hours: 9:30 AM - 3:59 PM ET
 * - After Hours: 4:00 PM - 7:59 PM ET
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
        QTime(19, 59); // 7:59 PM ET (app policy: display after-hours through 8:00 PM ET)


    // Day of week constants (Qt uses 1-7 for Monday-Sunday)
    inline constexpr int MONDAY = 1; // Qt::Monday
    inline constexpr int FRIDAY = 5; // Qt::Friday

} // namespace TradingHours

namespace BarsConstants
{
    // Number of bars per trading day (4:00 AM to 7:59 PM, 1-minute bars)
    inline constexpr unsigned int MINUTE_BARS_PER_DAY = 16 * 60; // 960 bars

    /**
     * @brief Converts a QTime timestamp to the corresponding index in the daily bar cache vector.
     *
     * 1-minute bars are timestamped using the **open time** of the interval (Databento convention).
     * For extended hours trading (4:00 AM – 7:59 PM ET):
     *   - The first bar (covering 4:00:00 – 4:00:59) is timestamped 4:00 AM
     *   - The last bar  (covering 7:59:00 – 7:59:59 PM) is timestamped 7:59 PM
     *
     * Therefore, valid bar timestamps range from 4:00 AM to 7:59 PM inclusive.
     * The cache vector is pre-allocated with exactly 960 elements (16 hours × 60 minutes),
     * where index 0 corresponds to the 4:00 AM bar and index 959 to the 7:59 PM bar.
     *
     * @param time The timestamp of the bar (must be a valid bar open time)
     * @return size_t The zero-based index in the daily cache vector (0 to 959)
     *
     * @pre time is a valid 1-minute bar open time in extended hours:
     *      - 4:00 AM ≤ time ≤ 7:59 PM
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
                  MINUTE_BARS_PER_DAY); // 0 ≤ index ≤ 959
        return index;
    }

    /**
     * @brief Converts a daily bar cache index to the corresponding bar timestamp (QTime).
     *
     * The timestamp returned is the **open time** of the 1-minute bar (Databento convention).
     * For extended hours (4:00 AM – 7:59 PM ET):
     *   - index 0   → 4:00 AM  (bar covering 4:00:00 – 4:00:59)
     *   - index 959 → 7:59 PM (bar covering 7:59:00 – 7:59:59 PM)
     *
     * @param index Zero-based index in the daily cache vector (0 to 959)
     * @return QTime The timestamp (open time) of the bar
     *
     * @pre index < MINUTE_BARS_PER_DAY (960)
     * @note Returned times are always valid bar timestamps: 4:00 AM to 7:59 PM inclusive.
     */
    constexpr QTime indexToTime(size_t index)
    {
        ASSUME_LT(index, MINUTE_BARS_PER_DAY);

        int hour = TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION.hour() + (index / 60);
        int minute = index % 60;

        // At index 959: 959 / 60 = 15 hours → 4 + 15 = 19, minute = 59 → 7:59 PM
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
    inline constexpr const char* STREAM_MARKET_DEPTH_AGGREGATE = "marketdata/stream/marketdepth/aggregates/%1";
    inline constexpr const char* STREAM_QUOTES = "marketdata/stream/quotes/%1";

    // Brokerage endpoints
    inline constexpr const char* GET_ACCOUNTS = "brokerage/accounts";
    inline constexpr const char* GET_BALANCES = "brokerage/accounts/%1/balances";
    inline constexpr const char* STREAM_ORDERS = "brokerage/stream/accounts/%1/orders";
    inline constexpr const char* STREAM_POSITIONS = "brokerage/stream/accounts/%1/positions";

    // Order Execution endpoints
    inline constexpr const char* GET_ORDER_ROUTES = "orderexecution/routes";
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
 * @namespace OrderRoutingConstants
 * @brief Constants for the order entry route selector
 *
 * TradeStation's "Intelligent" route is never returned by the
 * `GET_ORDER_ROUTES` endpoint — it is implied by the *absence* of the
 * "Route" field on a placed order, and it's the only commission-free route.
 * We synthesize a sentinel entry for it in the GUI dropdown; it must never
 * be sent as a literal "Route" value to the API (see PlaceOrder::setRoute()).
 */
namespace OrderRoutingConstants
{
    inline constexpr const char* INTELLIGENT_ROUTE_ID = "Intelligent";
} // namespace OrderRoutingConstants

/**
 * @namespace StreamConstants
 * @brief Constants related to streaming and heartbeat
 */
namespace StreamConstants
{
    // Heartbeat interval for mock streams in replay mode (milliseconds)
    // Should be less than the stream heartbeat timeout (10 seconds)
    inline constexpr int MOCK_HEARTBEAT_INTERVAL_MS = 5000;
    inline constexpr int LIVE_RETRY_INITIAL_DELAY_MS = 1000;
    inline constexpr int LIVE_RETRY_MAX_DELAY_MS = 30000;
} // namespace StreamConstants

/**
 * @namespace ReplayConstants
 * @brief Constants for replay engine performance tuning
 */
namespace ReplayConstants
{
    // Wall-clock time budget (ms) for each batch of records in AsFastAsPossible mode.
    // Records are processed in a tight loop per timer tick, yielding after this budget
    // so the event loop can service GUI events and other timers.
    inline constexpr qint64 MAX_SPEED_BATCH_BUDGET_MS = 16;

    // GUI update interval (ms) when AsFastAsPossible mode is active.
    // Cross-thread signals to the GUI are buffered and emitted at this rate
    // to prevent the GUI event queue from flooding.
    inline constexpr int GUI_THROTTLE_INTERVAL_MS = 33; // ~30 fps

    // Replay clock update cadence (ms) while playback is running through an event gap.
    // Keeps the replay timeline moving smoothly even when no market data is due yet.
    inline constexpr int CLOCK_UPDATE_INTERVAL_MS = 100;
} // namespace ReplayConstants

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

namespace CredentialStorageConstants
{
    inline constexpr auto SETTINGS_KEY = "Credentials/Backend";
    inline constexpr auto VAULT_FILE = "Credentials.yubikey";
    inline constexpr auto VAULT_MAGIC = "OTPYK001";
    inline constexpr auto HKDF_INFO = "OpenTraderPlatform YubiKey credentials v1";
    inline constexpr int CHALLENGE_SIZE = 64;
    inline constexpr int RESPONSE_SIZE = 20;
    inline constexpr int KEY_SIZE = 32;
    inline constexpr int IV_SIZE = 12;
    inline constexpr int TAG_SIZE = 16;
    inline constexpr int PROCESS_TIMEOUT_MS = 30000;
    inline constexpr int MAX_VAULT_BYTES = 1024 * 1024;
    inline constexpr int VAULT_PREFIX_SIZE = 8 + CHALLENGE_SIZE + IV_SIZE + TAG_SIZE;
} // namespace CredentialStorageConstants

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
    inline constexpr double CANDLESTICK_BODY_WIDTH = 0.9;         // 90% of available space
    inline constexpr double BLENDED_VOLUME_STRIP_FRACTION = 0.20; // Bottom share used by blended volume bars
    inline constexpr int RECENTER_LOOKBACK_BARS = 30;
    inline constexpr int RECENTER_RIGHT_PADDING_BARS = 1;

} // namespace ChartConstants

/**
 * @namespace ChartFocusConstants
 * @brief Runtime-configurable recenter/focus behavior for the stock chart.
 */
namespace ChartFocusConstants
{
    inline constexpr const char* SETTINGS_KEY_LOOKBACK_BARS = "Config/ChartFocusLookbackBars";
    inline constexpr const char* SETTINGS_KEY_RIGHT_PADDING_BARS = "Config/ChartFocusRightPaddingBars";
    inline constexpr const char* SETTINGS_KEY_INCLUDE_BBO_IN_Y_RANGE = "Config/ChartFocusIncludeBboInYRange";
    inline constexpr const char* SETTINGS_KEY_Y_PADDING_PERCENT = "Config/ChartFocusYPaddingPercent";
    inline constexpr const char* SETTINGS_KEY_MIN_RANGE_PERCENT = "Config/ChartFocusMinRangePercent";
    inline constexpr const char* SETTINGS_KEY_ANCHOR_PADDING_PERCENT = "Config/ChartFocusAnchorPaddingPercent";

    inline constexpr int DEFAULT_LOOKBACK_BARS = ChartConstants::RECENTER_LOOKBACK_BARS;
    inline constexpr int MIN_LOOKBACK_BARS = 1;
    inline constexpr int MAX_LOOKBACK_BARS = 600;

    inline constexpr int DEFAULT_RIGHT_PADDING_BARS = ChartConstants::RECENTER_RIGHT_PADDING_BARS;
    inline constexpr int MIN_RIGHT_PADDING_BARS = 0;
    inline constexpr int MAX_RIGHT_PADDING_BARS = 120;

    inline constexpr bool DEFAULT_INCLUDE_BBO_IN_Y_RANGE = true;

    // Existing behavior defaults:
    // - chart-span padding was 5% of (maxPrice - minPrice)
    // - minimum range was 0.05% of anchor price
    // - anchor safety padding was 0.02% of anchor price
    inline constexpr double DEFAULT_Y_PADDING_PERCENT = 5.0;
    inline constexpr double MIN_Y_PADDING_PERCENT = 0.0;
    inline constexpr double MAX_Y_PADDING_PERCENT = 100.0;

    inline constexpr double DEFAULT_MIN_RANGE_PERCENT = 0.05;
    inline constexpr double MIN_MIN_RANGE_PERCENT = 0.0;
    inline constexpr double MAX_MIN_RANGE_PERCENT = 10.0;

    inline constexpr double DEFAULT_ANCHOR_PADDING_PERCENT = 0.02;
    inline constexpr double MIN_ANCHOR_PADDING_PERCENT = 0.0;
    inline constexpr double MAX_ANCHOR_PADDING_PERCENT = 10.0;

    inline constexpr double MIN_RANGE_ABSOLUTE_DOLLARS = 0.01;
} // namespace ChartFocusConstants

/**
 * @namespace ChartIndicatorConstants
 * @brief Runtime settings for chart indicators.
 */
namespace ChartIndicatorConstants
{
    inline constexpr const char* SETTINGS_KEY_VOLUME_AUTO_SCALE_ENABLED = "Chart/Indicators/Volume/AutoScaleEnabled";
    inline constexpr bool DEFAULT_VOLUME_AUTO_SCALE_ENABLED = true;

    inline constexpr const char* SETTINGS_KEY_VOLUME_AUTO_SCALE_MODE = "Chart/Indicators/Volume/AutoScaleMode";
    inline constexpr int DEFAULT_VOLUME_AUTO_SCALE_MODE = 1; // SecondHighestBar

    inline constexpr const char* SETTINGS_KEY_SHOW_VWAP = "Chart/ShowVWAP";
    inline constexpr bool DEFAULT_SHOW_VWAP = false;

    inline constexpr const char* SETTINGS_KEY_SHOW_LEVEL2_DEPTH_OVERLAY = "Chart/ShowLevel2DepthOverlay";
    inline constexpr bool DEFAULT_SHOW_LEVEL2_DEPTH_OVERLAY = false;

    inline constexpr const char* SETTINGS_KEY_VWAP_SOURCE_MODE = "Chart/Indicators/VWAP/SourceMode";
    inline constexpr int DEFAULT_VWAP_SOURCE_MODE = 0; // Close

    inline constexpr const char* SETTINGS_KEY_VWAP_SESSION_RESET_TIME = "Chart/Indicators/VWAP/SessionResetTime";
    inline constexpr const char* DEFAULT_VWAP_SESSION_RESET_TIME = "04:00";

    inline constexpr const char* SETTINGS_KEY_VWAP_COLOR = "Chart/Indicators/VWAP/Color";
    inline constexpr const char* DEFAULT_VWAP_COLOR = "#ff00dcdc";

    inline constexpr const char* SETTINGS_KEY_SHOW_EMA1 = "Chart/ShowEMA1";
    inline constexpr bool DEFAULT_SHOW_EMA1 = false;
    inline constexpr const char* SETTINGS_KEY_SHOW_EMA2 = "Chart/ShowEMA2";
    inline constexpr bool DEFAULT_SHOW_EMA2 = false;
    inline constexpr const char* SETTINGS_KEY_SHOW_EMA3 = "Chart/ShowEMA3";
    inline constexpr bool DEFAULT_SHOW_EMA3 = false;

    inline constexpr const char* SETTINGS_KEY_EMA1_PERIOD = "Chart/Indicators/EMA1/Period";
    inline constexpr int DEFAULT_EMA1_PERIOD = 9;
    inline constexpr const char* SETTINGS_KEY_EMA2_PERIOD = "Chart/Indicators/EMA2/Period";
    inline constexpr int DEFAULT_EMA2_PERIOD = 20;
    inline constexpr const char* SETTINGS_KEY_EMA3_PERIOD = "Chart/Indicators/EMA3/Period";
    inline constexpr int DEFAULT_EMA3_PERIOD = 200;

    inline constexpr const char* SETTINGS_KEY_EMA1_COLOR = "Chart/Indicators/EMA1/Color";
    inline constexpr const char* DEFAULT_EMA1_COLOR = "#fff3c623";
    inline constexpr const char* SETTINGS_KEY_EMA2_COLOR = "Chart/Indicators/EMA2/Color";
    inline constexpr const char* DEFAULT_EMA2_COLOR = "#ff29b6f6";
    inline constexpr const char* SETTINGS_KEY_EMA3_COLOR = "Chart/Indicators/EMA3/Color";
    inline constexpr const char* DEFAULT_EMA3_COLOR = "#fff06292";

    inline constexpr const char* SETTINGS_KEY_SHOW_MACD = "Chart/ShowMACD";
    inline constexpr bool DEFAULT_SHOW_MACD = false;

    inline constexpr const char* SETTINGS_KEY_MACD_FAST_LENGTH = "Chart/Indicators/MACD/FastLength";
    inline constexpr int DEFAULT_MACD_FAST_LENGTH = 12;

    inline constexpr const char* SETTINGS_KEY_MACD_SLOW_LENGTH = "Chart/Indicators/MACD/SlowLength";
    inline constexpr int DEFAULT_MACD_SLOW_LENGTH = 26;

    inline constexpr const char* SETTINGS_KEY_MACD_SIGNAL_LENGTH = "Chart/Indicators/MACD/SignalLength";
    inline constexpr int DEFAULT_MACD_SIGNAL_LENGTH = 9;

    inline constexpr const char* SETTINGS_KEY_MACD_MA_TYPE = "Chart/Indicators/MACD/MacdMaType";
    inline constexpr int DEFAULT_MACD_MA_TYPE = 0; // EMA

    inline constexpr const char* SETTINGS_KEY_MACD_SIGNAL_MA_TYPE = "Chart/Indicators/MACD/SignalMaType";
    inline constexpr int DEFAULT_MACD_SIGNAL_MA_TYPE = 0; // EMA

    inline constexpr const char* SETTINGS_KEY_MACD_SHOW_HISTOGRAM = "Chart/Indicators/MACD/ShowHistogram";
    inline constexpr bool DEFAULT_MACD_SHOW_HISTOGRAM = true;

    inline constexpr const char* SETTINGS_KEY_SHOW_RSI = "Chart/ShowRSI";
    inline constexpr bool DEFAULT_SHOW_RSI = false;

    inline constexpr const char* SETTINGS_KEY_SHOW_STRATEGY_STATUS_PANEL = "Chart/ShowStrategyStatusPanel";
    inline constexpr bool DEFAULT_SHOW_STRATEGY_STATUS_PANEL = true;

    inline constexpr const char* SETTINGS_KEY_RSI_PERIOD = "Chart/Indicators/RSI/Period";
    inline constexpr int DEFAULT_RSI_PERIOD = 14;

    inline constexpr const char* SETTINGS_KEY_RSI_OVERBOUGHT = "Chart/Indicators/RSI/Overbought";
    inline constexpr int DEFAULT_RSI_OVERBOUGHT = 70;

    inline constexpr const char* SETTINGS_KEY_RSI_OVERSOLD = "Chart/Indicators/RSI/Oversold";
    inline constexpr int DEFAULT_RSI_OVERSOLD = 30;

    inline constexpr const char* SETTINGS_KEY_RSI_COLOR = "Chart/Indicators/RSI/Color";
    inline constexpr const char* DEFAULT_RSI_COLOR = "#ffb388ff";
} // namespace ChartIndicatorConstants

/**
 * @namespace ChartBackgroundConstants
 * @brief Runtime settings for chart session background appearance.
 */
namespace ChartBackgroundConstants
{
    inline constexpr const char* SETTINGS_KEY_EARLY_PRE_MARKET_COLOR = "Chart/Background/EarlyPreMarketColor";
    inline constexpr const char* DEFAULT_EARLY_PRE_MARKET_COLOR = "#5affa500"; // RGBA(255,165,0,90)

    inline constexpr const char* SETTINGS_KEY_PRE_MARKET_COLOR = "Chart/Background/PreMarketColor";
    inline constexpr const char* DEFAULT_PRE_MARKET_COLOR = "#b4ffa500"; // RGBA(255,165,0,180)

    inline constexpr const char* SETTINGS_KEY_AFTER_HOURS_COLOR = "Chart/Background/AfterHoursColor";
    inline constexpr const char* DEFAULT_AFTER_HOURS_COLOR = "#b48a2be2"; // RGBA(138,43,226,180)
} // namespace ChartBackgroundConstants

/**
 * @namespace BracketWheelConstants
 * @brief Runtime-configurable wheel-adjust behavior for bracket overlays.
 */
namespace BracketWheelConstants
{
    inline constexpr const char* SETTINGS_KEY_RATIO_STEP_R = "Config/BracketWheelRatioStepR";
    inline constexpr const char* SETTINGS_KEY_STOP_STEP_PERCENT = "Config/BracketWheelStopStepPercent";
    inline constexpr const char* SETTINGS_KEY_RATIO_MIN = "Config/BracketWheelRatioMin";
    inline constexpr const char* SETTINGS_KEY_RATIO_MAX = "Config/BracketWheelRatioMax";
    inline constexpr const char* SETTINGS_KEY_STOP_PERCENT_MIN = "Config/BracketWheelStopPercentMin";
    inline constexpr const char* SETTINGS_KEY_STOP_PERCENT_MAX = "Config/BracketWheelStopPercentMax";

    inline constexpr double DEFAULT_RATIO_STEP_R = 0.10;
    inline constexpr double MIN_RATIO_STEP_R = 0.01;
    inline constexpr double MAX_RATIO_STEP_R = 5.00;

    inline constexpr double DEFAULT_STOP_STEP_PERCENT = 1.00;
    inline constexpr double MIN_STOP_STEP_PERCENT = 0.10;
    inline constexpr double MAX_STOP_STEP_PERCENT = 10.00;

    inline constexpr double DEFAULT_RATIO_MIN = 0.25;
    inline constexpr double DEFAULT_RATIO_MAX = 10.00;
    inline constexpr double MIN_RATIO_BOUND = 0.05;
    inline constexpr double MAX_RATIO_BOUND = 50.00;

    inline constexpr double DEFAULT_STOP_PERCENT_MIN = 1.00;
    inline constexpr double DEFAULT_STOP_PERCENT_MAX = 30.00;
    inline constexpr double MIN_STOP_PERCENT_BOUND = 0.10;
    inline constexpr double MAX_STOP_PERCENT_BOUND = 95.00;
} // namespace BracketWheelConstants

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
    // TradeStation limit for concurrent market-depth streams.
    inline constexpr int MAX_CONCURRENT_STREAMS = 10;

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
 * @namespace TimeAndSalesConstants
 * @brief Constants related to the Time & Sales tape widget
 */
namespace TimeAndSalesConstants
{
    /// Default maximum number of trade entries displayed in the Time & Sales widget
    inline constexpr int DEFAULT_MAX_ENTRIES = 200;
} // namespace TimeAndSalesConstants

/**
 * @namespace ClosePositionsConstants
 * @brief Constants for the close-positions kill switch configuration
 */
namespace ClosePositionsConstants
{
    /// Persistent settings key for the aggressive extended-hours limit offset, expressed in cents.
    inline constexpr const char* SETTINGS_KEY_AGGRESSIVE_LIMIT_OFFSET_CENTS =
        "Config/ClosePositionsAggressiveLimitOffsetCents";

    /// Default amount of aggressivity for close-position limit orders outside regular hours.
    inline constexpr double DEFAULT_AGGRESSIVE_LIMIT_OFFSET_CENTS = 5.0;

    /// Minimum allowed offset in cents.
    inline constexpr double MIN_AGGRESSIVE_LIMIT_OFFSET_CENTS = 0.0;

    /// Maximum allowed offset in cents.
    inline constexpr double MAX_AGGRESSIVE_LIMIT_OFFSET_CENTS = 100.0;

    /// Default step size for the Config tab spin box.
    inline constexpr double AGGRESSIVE_LIMIT_OFFSET_STEP_CENTS = 1.0;
} // namespace ClosePositionsConstants

/**
 * @namespace StrategyManualConfirmationConstants
 * @brief Constants for manual strategy-order entry confirmation.
 */
namespace StrategyManualConfirmationConstants
{
    inline constexpr const char* SETTINGS_KEY_TIMEOUT_SECONDS = "Config/StrategyManualConfirmTimeoutSec";
    inline constexpr const char* SETTINGS_KEY_MARKETABLE_OFFSET_CENTS =
        "Config/StrategyManualConfirmMarketableOffsetCents";
    inline constexpr const char* SETTINGS_KEY_MAX_CHASE_PERCENT = "Config/StrategyManualConfirmMaxChasePercent";

    inline constexpr int DEFAULT_TIMEOUT_SECONDS = 10;
    inline constexpr int MIN_TIMEOUT_SECONDS = 1;
    inline constexpr int MAX_TIMEOUT_SECONDS = 60;

    inline constexpr double DEFAULT_MARKETABLE_OFFSET_CENTS = 5.0;
    inline constexpr double MIN_MARKETABLE_OFFSET_CENTS = 0.0;
    inline constexpr double MAX_MARKETABLE_OFFSET_CENTS = 100.0;
    inline constexpr double MARKETABLE_OFFSET_STEP_CENTS = 1.0;

    inline constexpr double DEFAULT_MAX_CHASE_PERCENT = 10.0;
    inline constexpr double MIN_MAX_CHASE_PERCENT = 0.0;
    inline constexpr double MAX_MAX_CHASE_PERCENT = 100.0;
    inline constexpr double MAX_CHASE_PERCENT_STEP = 0.5;

    inline constexpr int MAX_PENDING_REQUESTS = 256;
} // namespace StrategyManualConfirmationConstants

/**
 * @namespace RiskManagementConstants
 * @brief Risk management configuration defaults, limits, and settings keys.
 */
namespace RiskManagementConstants
{
    inline constexpr const char* SETTINGS_KEY_ENABLED = "Risk/%1/Enabled";
    inline constexpr const char* SETTINGS_KEY_DAILY_DRAWDOWN_LIMIT_USD = "Risk/%1/DailyDrawdownLimitUsd";
    inline constexpr const char* SETTINGS_KEY_DAILY_DRAWDOWN_BASIS = "Risk/%1/DailyDrawdownBasis";
    inline constexpr const char* SETTINGS_KEY_MAX_PLANNED_LOSS_PER_TRADE_USD = "Risk/%1/MaxPlannedLossPerTradeUsd";
    inline constexpr const char* SETTINGS_KEY_MAX_POSITION_SHARES = "Risk/%1/MaxPositionShares";
    inline constexpr const char* SETTINGS_KEY_MAX_POSITION_NOTIONAL_USD = "Risk/%1/MaxPositionNotionalUsd";
    inline constexpr const char* SETTINGS_KEY_MAX_DAILY_ENTRY_TRADES = "Risk/%1/MaxDailyEntryTrades";
    inline constexpr const char* SETTINGS_KEY_MAX_OPEN_POSITIONS = "Risk/%1/MaxOpenPositions";
    inline constexpr const char* SETTINGS_KEY_COOLDOWN_ENABLED = "Risk/%1/CooldownEnabled";
    inline constexpr const char* SETTINGS_KEY_COOLDOWN_LOSS_TRIGGER_USD = "Risk/%1/CooldownLossTriggerUsd";
    inline constexpr const char* SETTINGS_KEY_COOLDOWN_DURATION_SEC = "Risk/%1/CooldownDurationSec";
    inline constexpr const char* SETTINGS_KEY_WARNING_AMBER_USED_PERCENT = "Risk/%1/WarningAmberUsedPercent";
    inline constexpr const char* SETTINGS_KEY_WARNING_RED_USED_PERCENT = "Risk/%1/WarningRedUsedPercent";

    inline constexpr bool DEFAULT_ENABLED = true;
    inline constexpr double DEFAULT_DAILY_DRAWDOWN_LIMIT_USD = 1000.0;
    inline constexpr double MIN_DAILY_DRAWDOWN_LIMIT_USD = 1.0;
    inline constexpr double MAX_DAILY_DRAWDOWN_LIMIT_USD = 10'000'000.0;
    inline constexpr double DAILY_DRAWDOWN_LIMIT_STEP_USD = 50.0;

    inline constexpr int DEFAULT_DAILY_DRAWDOWN_BASIS = 0; // EquityPeak

    inline constexpr double DEFAULT_MAX_PLANNED_LOSS_PER_TRADE_USD = 200.0;
    inline constexpr double MIN_MAX_PLANNED_LOSS_PER_TRADE_USD = 1.0;
    inline constexpr double MAX_MAX_PLANNED_LOSS_PER_TRADE_USD = 1'000'000.0;
    inline constexpr double MAX_PLANNED_LOSS_PER_TRADE_STEP_USD = 10.0;

    inline constexpr int DEFAULT_MAX_POSITION_SHARES = 5000;
    inline constexpr int MIN_MAX_POSITION_SHARES = 1;
    inline constexpr int MAX_MAX_POSITION_SHARES = 100'000'000;
    inline constexpr int MAX_POSITION_SHARES_STEP = 100;

    inline constexpr double DEFAULT_MAX_POSITION_NOTIONAL_USD = 100'000.0;
    inline constexpr double MIN_MAX_POSITION_NOTIONAL_USD = 1.0;
    inline constexpr double MAX_MAX_POSITION_NOTIONAL_USD = 100'000'000.0;
    inline constexpr double MAX_POSITION_NOTIONAL_STEP_USD = 1000.0;

    inline constexpr int DEFAULT_MAX_DAILY_ENTRY_TRADES = 20;
    inline constexpr int MIN_MAX_DAILY_ENTRY_TRADES = 1;
    inline constexpr int MAX_MAX_DAILY_ENTRY_TRADES = 100'000;

    inline constexpr int DEFAULT_MAX_OPEN_POSITIONS = 10;
    inline constexpr int MIN_MAX_OPEN_POSITIONS = 1;
    inline constexpr int MAX_MAX_OPEN_POSITIONS = 10'000;

    inline constexpr bool DEFAULT_COOLDOWN_ENABLED = true;
    inline constexpr double DEFAULT_COOLDOWN_LOSS_TRIGGER_USD = 200.0;
    inline constexpr double MIN_COOLDOWN_LOSS_TRIGGER_USD = 0.0;
    inline constexpr double MAX_COOLDOWN_LOSS_TRIGGER_USD = 1'000'000.0;
    inline constexpr double COOLDOWN_LOSS_TRIGGER_STEP_USD = 10.0;

    inline constexpr int DEFAULT_COOLDOWN_DURATION_SEC = 300;
    inline constexpr int MIN_COOLDOWN_DURATION_SEC = 0;
    inline constexpr int MAX_COOLDOWN_DURATION_SEC = 86'400;
    inline constexpr int COOLDOWN_DURATION_STEP_SEC = 30;

    // Thresholds are based on used drawdown percentage (0-100).
    inline constexpr int DEFAULT_WARNING_AMBER_USED_PERCENT = 50;
    inline constexpr int DEFAULT_WARNING_RED_USED_PERCENT = 80;
    inline constexpr int MIN_WARNING_USED_PERCENT = 0;
    inline constexpr int MAX_WARNING_USED_PERCENT = 100;
} // namespace RiskManagementConstants

/**
 * @namespace PlatformControlConstants
 * @brief Constants related to the local platform control socket and MCP polling helpers
 */
namespace PlatformControlConstants
{
    /// Default number of trades returned by a market-data snapshot request when no explicit maxCount is provided.
    inline constexpr int DEFAULT_TRADES_SNAPSHOT_MAX_COUNT = 200;

    /// Hard cap on trade count returned in a single control/MCP market-data snapshot response.
    inline constexpr int MAX_TRADES_SNAPSHOT_MAX_COUNT = 1000;

    /// Number of recent trades retained per symbol for control/MCP polling.
    inline constexpr int RECENT_TRADES_BUFFER_LIMIT = 1000;

    /// Keep MCP/control-requested SymbolContexts alive briefly so arbitrary-symbol polling can accumulate data.
    inline constexpr int SYMBOL_CONTEXT_LEASE_TIMEOUT_MS = 60 * 1000;

    /// Periodic cleanup interval for expired MCP/control symbol leases.
    inline constexpr int SYMBOL_CONTEXT_LEASE_CLEANUP_INTERVAL_MS = 5000;

    /// Maximum number of replay-data downloads launched concurrently by GUI/control batch requests.
    inline constexpr int REPLAY_DOWNLOAD_MAX_CONCURRENCY = 5;
} // namespace PlatformControlConstants

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
 * @namespace MarketCalendar
 * @brief NYSE market holiday calendar for 2026.
 *
 * Sources: NYSE official holiday schedule.
 *
 * Full-day closures (Mon-Fri):
 *   Jan  1  – New Year's Day
 *   Jan 19  – Martin Luther King Jr. Day
 *   Feb 16  – Presidents' Day
 *   Apr  3  – Good Friday
 *   May 25  – Memorial Day
 *   Jun 19  – Juneteenth
 *   Jul  3  – Independence Day (observed; Jul 4 falls on Saturday)
 *   Sep  7  – Labor Day
 *   Nov 26  – Thanksgiving Day
 *   Dec 25  – Christmas Day
 *
 * Early close at 1:00 PM ET:
 *   Nov 25  – Day before Thanksgiving
 *   Dec 24  – Christmas Eve
 */
namespace MarketCalendar
{
    inline const QTime EARLY_CLOSE_TIME = QTime(13, 0); // 1:00 PM ET

    // Full-day NYSE closures in 2025 (Mon–Fri holidays)
    inline const std::array<QDate, 10> HOLIDAYS_2025 = {
        QDate(2025, 1, 1),   // New Year's Day
        QDate(2025, 1, 20),  // Martin Luther King Jr. Day
        QDate(2025, 2, 17),  // Presidents' Day
        QDate(2025, 4, 18),  // Good Friday
        QDate(2025, 5, 26),  // Memorial Day
        QDate(2025, 6, 19),  // Juneteenth National Independence Day
        QDate(2025, 7, 4),   // Independence Day
        QDate(2025, 9, 1),   // Labor Day
        QDate(2025, 11, 27), // Thanksgiving Day
        // Christmas Dec 25 falls on Thursday in 2025
        QDate(2025, 12, 25), // Christmas Day
    };

    // Full-day NYSE closures in 2026 (Mon–Fri holidays)
    inline const std::array<QDate, 10> HOLIDAYS_2026 = {
        QDate(2026, 1, 1),   // New Year's Day
        QDate(2026, 1, 19),  // Martin Luther King Jr. Day
        QDate(2026, 2, 16),  // Presidents' Day
        QDate(2026, 4, 3),   // Good Friday
        QDate(2026, 5, 25),  // Memorial Day
        QDate(2026, 6, 19),  // Juneteenth National Independence Day
        QDate(2026, 7, 3),   // Independence Day (observed)
        QDate(2026, 9, 7),   // Labor Day
        QDate(2026, 11, 26), // Thanksgiving Day
        QDate(2026, 12, 25), // Christmas Day
    };

    // Days with early close at 1:00 PM ET in 2026
    inline const std::array<QDate, 2> EARLY_CLOSE_DAYS_2026 = {
        QDate(2026, 11, 25), // Day before Thanksgiving
        QDate(2026, 12, 24), // Christmas Eve
    };

    /// Returns the holiday name for a given date, or an empty string if the date
    /// is not a known NYSE holiday.
    inline QString getHolidayName(const QDate& date)
    {
        // Map of all known holiday dates → display name
        static const std::array<std::pair<QDate, QString>, 20> ALL_HOLIDAYS = {{
            // 2025
            {QDate(2025, 1, 1), "New Year's Day"},
            {QDate(2025, 1, 20), "Martin Luther King Jr. Day"},
            {QDate(2025, 2, 17), "Presidents' Day"},
            {QDate(2025, 4, 18), "Good Friday"},
            {QDate(2025, 5, 26), "Memorial Day"},
            {QDate(2025, 6, 19), "Juneteenth"},
            {QDate(2025, 7, 4), "Independence Day"},
            {QDate(2025, 9, 1), "Labor Day"},
            {QDate(2025, 11, 27), "Thanksgiving Day"},
            {QDate(2025, 12, 25), "Christmas Day"},
            // 2026
            {QDate(2026, 1, 1), "New Year's Day"},
            {QDate(2026, 1, 19), "Martin Luther King Jr. Day"},
            {QDate(2026, 2, 16), "Presidents' Day"},
            {QDate(2026, 4, 3), "Good Friday"},
            {QDate(2026, 5, 25), "Memorial Day"},
            {QDate(2026, 6, 19), "Juneteenth"},
            {QDate(2026, 7, 3), "Independence Day (observed)"},
            {QDate(2026, 9, 7), "Labor Day"},
            {QDate(2026, 11, 26), "Thanksgiving Day"},
            {QDate(2026, 12, 25), "Christmas Day"},
        }};
        for (const auto& [h, name]: ALL_HOLIDAYS)
        {
            if (h == date)
                return name;
        }
        return {};
    }

    inline bool isHoliday(const QDate& date)
    {
        for (const QDate& h: HOLIDAYS_2026)
        {
            if (h == date)
                return true;
        }
        return false;
    }

    inline bool isEarlyCloseDay(const QDate& date)
    {
        for (const QDate& d: EARLY_CLOSE_DAYS_2026)
        {
            if (d == date)
                return true;
        }
        return false;
    }
} // namespace MarketCalendar

namespace AsyncLogger
{
    // Total capacity of each circular log buffer (one for file, one for stdout/stderr).
    // Large enough to absorb bursts without dropping messages.
    inline constexpr size_t BUFFER_SIZE_BYTES = 1024 * 1024; // 1 MB

    // Wake the consumer thread immediately once this many bytes are pending.
    inline constexpr size_t FLUSH_THRESHOLD_BYTES = 512 * 1024; // 512 KB

    // Maximum time the consumer thread sleeps before flushing whatever is pending.
    inline constexpr int FLUSH_TIMEOUT_MS = 1000; // 1 s

    // crashFlush() parameters: how many times to retry try_lock() and how long
    // to sleep between attempts (via nanosleep, which is async-signal-safe).
    // The consumer holds m_mutex for only a few microseconds at a time, so
    // 10 × 1 ms is more than sufficient in practice while capping worst-case
    // wait at ~10 ms.
    inline constexpr int CRASH_FLUSH_RETRIES = 10;
    inline constexpr long CRASH_FLUSH_RETRY_DELAY_NS = 1'000'000; // 1 ms
} // namespace AsyncLogger
