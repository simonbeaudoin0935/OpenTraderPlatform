#include "BarTapeReconstructor.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "CONSTANTS.h"

std::optional<Trade> BarTapeReconstructor::onBar(const QString& p_symbol,
                                                 const Bar& p_bar,
                                                 const std::optional<Bbo>& p_bbo,
                                                 const QDateTime& p_receivedAt)
{
    const QDateTime minute = p_bar.getTimeStamp();
    const quint64 volume = p_bar.getTotalVolume();

    if (!m_hasBaseline)
    {
        m_minute = minute;
        m_volume = volume;
        m_hasBaseline = true;
        return std::nullopt;
    }

    if (minute < m_minute)
    {
        return std::nullopt;
    }

    quint64 delta = 0;
    if (minute > m_minute)
    {
        // Every share of a brand new bar traded since the previous update.
        delta = volume;
        m_minute = minute;
        m_volume = volume;
    }
    else
    {
        if (volume <= m_volume)
        {
            return std::nullopt;
        }
        delta = volume - m_volume;
        m_volume = volume;
    }

    if (delta == 0)
    {
        return std::nullopt;
    }

    const double price =
        std::round(static_cast<double>(p_bar.getClose()) * TapeReconstructionConstants::PRICE_ROUNDING_FACTOR) /
        TapeReconstructionConstants::PRICE_ROUNDING_FACTOR;

    Trade trade;
    trade.m_symbol = p_symbol;
    trade.m_timestamp = p_receivedAt;
    trade.m_price = price;
    trade.m_size = static_cast<int>(std::min<quint64>(delta, static_cast<quint64>(std::numeric_limits<int>::max())));
    trade.m_side = classify(price, p_bbo);
    return trade;
}

void BarTapeReconstructor::reset()
{
    m_minute = QDateTime();
    m_volume = 0;
    m_hasBaseline = false;
}

TradeSide BarTapeReconstructor::classify(const double p_price, const std::optional<Bbo>& p_bbo)
{
    if (!p_bbo.has_value() || p_bbo->m_bid <= 0.0 || p_bbo->m_ask <= 0.0)
    {
        return TradeSide::None;
    }

    constexpr double epsilon = TapeReconstructionConstants::PRICE_COMPARE_EPSILON;
    if (p_price >= p_bbo->m_ask - epsilon)
    {
        return TradeSide::Bid;
    }
    if (p_price <= p_bbo->m_bid + epsilon)
    {
        return TradeSide::Ask;
    }
    return TradeSide::None;
}
