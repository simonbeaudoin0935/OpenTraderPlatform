#include "Level2Receiver.h"

#include "Logging.h"

#define LOGGING_CATEGORY Level2ReceiverLog
Q_LOGGING_CATEGORY(LOGGING_CATEGORY, "Level2Receiver")

Level2Receiver::Level2Receiver(const QString& symbol, QObject* parent) : StreamReceiver(parent), m_symbol(symbol)
{
    setObjectName("Level2Receiver::" + symbol);
}

void Level2Receiver::openStream()
{
    // Subscriptions are managed by StockInstruments via DBClient::subscribeLive()
    DEBUG << "Level2Receiver ready for" << m_symbol << "(subscription via StockInstruments)";
}

// BAI = (Total Bid Size - Total Ask Size) / (Total Bid Size + Total Ask Size)
// Result: [-1, 1]  Positive = more bid pressure, Negative = more ask pressure
double Level2Receiver::calculateBidAskImbalance(const Level2& level2, int levels) const
{
    unsigned int levelsToUse = levels > 0 ? static_cast<unsigned int>(levels) : m_bidAskImbalanceLevel;
    levelsToUse = qMin(levelsToUse, static_cast<unsigned int>(level2.m_bids.size()));

    double totalBid = 0.0;
    double totalAsk = 0.0;
    for (unsigned int i = 0; i < levelsToUse; ++i)
    {
        totalBid += level2.m_bids[i].m_size;
        totalAsk += level2.m_asks[i].m_size;
    }

    double total = totalBid + totalAsk;
    return total > 0.0 ? (totalBid - totalAsk) / total : 0.0;
}

double Level2Receiver::calculateDepthWeightedPrice(const std::array<Level2Row, 10>& rows, int count) const
{
    unsigned int n = qMin(m_depthWeightedPriceLevel, static_cast<unsigned int>(count));
    double totalSize = 0.0;
    double weightedSum = 0.0;
    for (unsigned int i = 0; i < n; ++i)
    {
        totalSize += rows[i].m_size;
        weightedSum += rows[i].m_price * rows[i].m_size;
    }
    return totalSize > 0.0 ? weightedSum / totalSize : 0.0;
}

void Level2Receiver::onReceivedNewLevel2(Level2 level2)
{
    double imbalance = calculateBidAskImbalance(level2);
    double bidDWP = calculateDepthWeightedPrice(level2.m_bids, static_cast<int>(level2.m_bids.size()));
    double askDWP = calculateDepthWeightedPrice(level2.m_asks, static_cast<int>(level2.m_asks.size()));

    emit receivedNewLevel2(m_symbol, level2, imbalance, bidDWP, askDWP);
}
