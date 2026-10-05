#pragma once

#include <array>

#include <QDateTime>
#include <QMetaType>
#include <QString>

/**
 * @brief A single price level from a market depth book.
 *
 * Maps directly to Databento's BidAskPair.
 */
struct Level2Row
{
    double m_price = 0.0;
    int m_size = 0;
    int m_orderCount = 0;
};

/**
 * @brief Level 2 full 10-level book snapshot.
 *
 * Sourced from Databento Schema::Mbp10 (Mbp10Msg.levels[0..9]).
 * Full snapshot on every book event — always replace, never delta-apply.
 * m_bids[0] / m_asks[0] is the BBO (best bid/offer).
 *
 * Thread context: emitted from DBClient internal callback thread; delivered to
 * consumers via Qt AutoConnection.
 */
struct Level2
{
    QString m_symbol;
    QDateTime m_timeStamp;
    std::array<Level2Row, 10> m_bids;
    std::array<Level2Row, 10> m_asks;
};

Q_DECLARE_METATYPE(Level2Row)
Q_DECLARE_METATYPE(Level2)
