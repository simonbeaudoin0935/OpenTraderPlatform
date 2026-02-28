#pragma once

#include <QDateTime>
#include <QTimeZone>

#include <databento/constants.hpp>
#include <databento/record.hpp>

#include "CONSTANTS.h"
#include "Core/Models/Bar.h"
#include "Core/Models/Level2.h"
#include "Core/Models/Trade.h"

/**
 * @file DBRecordTranslator.h
 * @brief Header-only static functions to convert Databento records into L2Trader domain types.
 *
 * Each function is a pure translation with no side effects. Thread-safe by design.
 */
namespace DBRecordTranslator
{

    // ── Helpers ────────────────────────────────────────────────────────────────

    /** Convert Databento fixed-point price (int64 / 1e9) to double. */
    [[nodiscard]] inline double toDouble(std::int64_t p_fixedPrice)
    {
        return static_cast<double>(p_fixedPrice) / databento::kFixedPriceScale;
    }

    /** Convert Databento UnixNanos timestamp to QDateTime in market timezone. */
    [[nodiscard]] inline QDateTime toDateTime(databento::UnixNanos p_ts)
    {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(p_ts.time_since_epoch()).count();
        return QDateTime::fromMSecsSinceEpoch(ms, TradingHours::MARKET_TIMEZONE);
    }

    // ── Record → Domain conversions ────────────────────────────────────────────

    /**
 * @brief Translate a Databento Mbp10Msg into a Level2 snapshot.
 * @param p_symbol Resolved symbol string (from PitSymbolMap)
 * @param p_msg    The 10-level market-by-price message
 */
    [[nodiscard]] inline Level2 toLevel2(const QString& p_symbol, const databento::Mbp10Msg& p_msg)
    {
        Level2 l2;
        l2.m_symbol = p_symbol;
        l2.m_timeStamp = toDateTime(p_msg.hd.ts_event);

        for (std::size_t i = 0; i < 10; ++i)
        {
            const auto& level = p_msg.levels[i];
            l2.m_bids[i] = {toDouble(level.bid_px), static_cast<int>(level.bid_sz), static_cast<int>(level.bid_ct)};
            l2.m_asks[i] = {toDouble(level.ask_px), static_cast<int>(level.ask_sz), static_cast<int>(level.ask_ct)};
        }
        return l2;
    }

    /**
 * @brief Translate a Databento Mbp1Msg into a Level1 BBO snapshot.
 * @param p_symbol Resolved symbol string (from PitSymbolMap)
 * @param p_msg    The 1-level market-by-price message
 */
    [[nodiscard]] inline Level1 toLevel1(const QString& p_symbol, const databento::Mbp1Msg& p_msg)
    {
        const auto& level = p_msg.levels[0];
        Level1 l1;
        l1.m_symbol = p_symbol;
        l1.m_timeStamp = toDateTime(p_msg.hd.ts_event);
        l1.m_bid = {toDouble(level.bid_px), static_cast<int>(level.bid_sz), static_cast<int>(level.bid_ct)};
        l1.m_ask = {toDouble(level.ask_px), static_cast<int>(level.ask_sz), static_cast<int>(level.ask_ct)};
        return l1;
    }

    /**
 * @brief Translate a Databento TradeMsg into a Trade.
 * @param p_symbol Resolved symbol string (from PitSymbolMap)
 * @param p_msg    The trade message
 */
    [[nodiscard]] inline Trade toTrade(const QString& p_symbol, const databento::TradeMsg& p_msg)
    {
        Trade t;
        t.m_symbol = p_symbol;
        t.m_timestamp = toDateTime(p_msg.hd.ts_event);
        t.m_price = toDouble(p_msg.price);
        t.m_size = static_cast<int>(p_msg.size);

        switch (p_msg.side)
        {
        case databento::Side::Ask:
            t.m_side = TradeSide::Ask;
            break;
        case databento::Side::Bid:
            t.m_side = TradeSide::Bid;
            break;
        default:
            t.m_side = TradeSide::None;
            break;
        }
        return t;
    }

    /**
 * @brief Translate a Databento OhlcvMsg into a Bar.
 * @param p_symbol Resolved symbol string (from PitSymbolMap)
 * @param p_msg    The OHLCV bar message
 */
    [[nodiscard]] inline Bar toBar(const QString& p_symbol, const databento::OhlcvMsg& p_msg)
    {
        Q_UNUSED(p_symbol);
        return Bar(toDateTime(p_msg.hd.ts_event),
                   static_cast<float>(toDouble(p_msg.open)),
                   static_cast<float>(toDouble(p_msg.high)),
                   static_cast<float>(toDouble(p_msg.low)),
                   static_cast<float>(toDouble(p_msg.close)),
                   p_msg.volume,
                   Bar::BarStatus::Closed);
    }

} // namespace DBRecordTranslator
