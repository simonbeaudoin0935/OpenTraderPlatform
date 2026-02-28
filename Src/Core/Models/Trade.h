#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>

/**
 * @brief Side of a trade (which side of the book was the aggressor).
 */
enum class TradeSide : char
{
    Ask = 'A',  ///< Sell aggressor — trade hit the ask
    Bid = 'B',  ///< Buy aggressor  — trade hit the bid
    None = 'N', ///< No side specified by the exchange
};

/**
 * @brief A single time & sales print.
 *
 * Sourced from Databento Schema::Trades (TradeMsg).
 * One record per individual trade execution.
 *
 * Thread context: emitted from DBClient internal callback thread; delivered to
 * consumers via Qt AutoConnection.
 */
struct Trade
{
    QString m_symbol;
    QDateTime m_timestamp;
    double m_price = 0.0;
    int m_size = 0;
    TradeSide m_side = TradeSide::None;
};

Q_DECLARE_METATYPE(TradeSide)
Q_DECLARE_METATYPE(Trade)
