#include "MarketDepthQuoteReceiver.h"
#include "TSClient.h"

Q_LOGGING_CATEGORY(MarketDepthQuoteReceiverLog, "MarketDepthQuoteReceiver")

MarketDepthQuoteReceiver::MarketDepthQuoteReceiver(const QString &symbol, QObject *parent) :
    QObject(parent),
    symbol(symbol)
{
    qCDebug(MarketDepthQuoteReceiverLog) << Q_FUNC_INFO << "Starting Market Depth Quote stream for " << symbol;

    StreamMarketDepthQuote *stream = TSClient::getInstance().openStreamMarketDepthQuote(symbol, 10);

    connect(stream, &StreamMarketDepthQuote::receivedNewMarketDepthQuote, this, &MarketDepthQuoteReceiver::onReceivedNewMarketDepthQuote);
    connect(stream, &Stream::streamErrorOccurred, this, &MarketDepthQuoteReceiver::onStreamError);
}

MarketDepthQuoteReceiver::~MarketDepthQuoteReceiver() {
    stopStream();
}

// Calculate Bid-Ask Imbalance (BAI)
// BAI = (Total Bid Size - Total Ask Size) / (Total Bid Size + Total Ask Size)
// Result range: [-1, 1]
// Positive: More buying pressure (bids > asks)
// Negative: More selling pressure (asks > bids)
// Zero: Equal pressure
double MarketDepthQuoteReceiver::calculateBidAskImbalance(const MarketDepthQuote& quote, int levels) const
{
    if (quote.isEmpty() || quote.getBids().isEmpty() || quote.getAsks().isEmpty()) {
        return 0.0; // No imbalance when there's no data
    }

    const QVector<MarketDepthLevel>& bids = quote.getBids();
    const QVector<MarketDepthLevel>& asks = quote.getAsks();
    
    // Use bidAskImbalanceLevel if no specific level is provided
    unsigned int levelsToUse = levels > 0 ? static_cast<unsigned int>(levels) : bidAskImbalanceLevel;
    
    // Determine how many levels to use
    unsigned int bidLevels = qMin(levelsToUse, static_cast<unsigned int>(bids.size()));
    unsigned int askLevels = qMin(levelsToUse, static_cast<unsigned int>(asks.size()));
    
    // Calculate total sizes
    double totalBidSize = 0.0;
    for (unsigned int i = 0; i < bidLevels; ++i) {
        totalBidSize += bids[i].getSize().toDouble();
    }
    
    double totalAskSize = 0.0;
    for (unsigned int i = 0; i < askLevels; ++i) {
        totalAskSize += asks[i].getSize().toDouble();
    }
    
    // Calculate imbalance
    double totalSize = totalBidSize + totalAskSize;
    if (totalSize <= 0.0) {
        return 0.0; // Avoid division by zero
    }
    
    return (totalBidSize - totalAskSize) / totalSize;
}

// Calculate Depth-Weighted Price (DWP)
double MarketDepthQuoteReceiver::calculateDepthWeightedPrice(const QVector<MarketDepthLevel>& levels) const {
    if (levels.isEmpty()) {
        return 0.0; // No data available
    }

    double totalVolume = 0.0;
    double weightedPriceSum = 0.0;

    // Only process up to depthWeightedPriceLevel levels
    unsigned int levelsToProcess = qMin(depthWeightedPriceLevel, static_cast<unsigned int>(levels.size()));

    for (unsigned int i = 0; i < levelsToProcess; ++i) {
        double price = levels[i].getPrice().toDouble();
        double size = levels[i].getSize().toDouble();
        totalVolume += size;
        weightedPriceSum += price * size;
    }

    if (totalVolume <= 0.0) {
        return 0.0; // Avoid division by zero
    }

    return weightedPriceSum / totalVolume;
}


void MarketDepthQuoteReceiver::stopStream()
{
    qCDebug(MarketDepthQuoteReceiverLog) << Q_FUNC_INFO << "Stopping MarketDepthQuote stream for " << symbol;

    Q_ASSERT(stream != nullptr);

    disconnect(stream);

    TSClient::getInstance().closeStreamMarketDepthQuote(stream);
}

void MarketDepthQuoteReceiver::onReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote marketDepthQuote)
{
    qCDebug(MarketDepthQuoteReceiverLog).noquote() << marketDepthQuote.toJsonString();
    
    // Calculate and log bid-ask imbalance
    double imbalance = calculateBidAskImbalance(marketDepthQuote);
    double imbalanceTopLevels = calculateBidAskImbalance(marketDepthQuote, 3); // Top 3 levels
    
    // Calculate Depth-Weighted Prices
    double bidDWP = calculateDepthWeightedPrice(marketDepthQuote.getBids());
    double askDWP = calculateDepthWeightedPrice(marketDepthQuote.getAsks());
    
    qCDebug(MarketDepthQuoteReceiverLog) << "Bid-Ask Imbalance for" << symbol
                                         << "- All levels:" << imbalance
                                         << "- Top 3 levels:" << imbalanceTopLevels
                                         << "- Bid DWP:" << bidDWP
                                         << "- Ask DWP:" << askDWP;

    emit receivedNewMarketDepthQuote(symbol, marketDepthQuote, imbalance, bidDWP, askDWP);
}

void MarketDepthQuoteReceiver::onStreamError(Stream::StreamError error, QString errorMessage)
{
    Q_UNUSED(error);

    qCFatal(MarketDepthQuoteReceiverLog) << "Market Depth Quote Receiver stream error : " << errorMessage;
}
