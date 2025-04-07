#include "MarketDepthQuoteReceiver.h"
#include "Clients/TSClient/TSClient.h"

Q_LOGGING_CATEGORY(MarketDepthQuoteReceiverLog, "MarketDepthQuoteReceiver")

MarketDepthQuoteReceiver::MarketDepthQuoteReceiver(QObject *parent) :
    QObject{parent}
{

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
    
    // Determine how many levels to use
    int bidLevels = levels > 0 ? qMin(levels, bids.size()) : bids.size();
    int askLevels = levels > 0 ? qMin(levels, asks.size()) : asks.size();
    
    // Calculate total sizes
    double totalBidSize = 0.0;
    for (int i = 0; i < bidLevels; ++i) {
        totalBidSize += bids[i].getSize().toDouble();
    }
    
    double totalAskSize = 0.0;
    for (int i = 0; i < askLevels; ++i) {
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

    for (const auto& level : levels) {
        double price = level.getPrice().toDouble();
        double size = level.getSize().toDouble();
        totalVolume += size;
        weightedPriceSum += price * size;
    }

    if (totalVolume <= 0.0) {
        return 0.0; // Avoid division by zero
    }

    return weightedPriceSum / totalVolume;
}

void MarketDepthQuoteReceiver::startStream(QString &symbol)
{
    qCDebug(MarketDepthQuoteReceiverLog) << Q_FUNC_INFO << "Starting Market Depth Quote stream for " << symbol;

    StreamMarketDepthQuote *stream = TSClient::getInstance().openStreamMarketDepthQuote(symbol, 10);

    void receivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote);

    connect(stream, &StreamMarketDepthQuote::receivedNewMarketDepthQuote, this, &MarketDepthQuoteReceiver::onReceivedNewMarketDepthQuote);
    connect(stream, &Stream::streamErrorOccurred, this, &MarketDepthQuoteReceiver::onStreamError);

    streams.insert(symbol, stream);
}

void MarketDepthQuoteReceiver::startStream(const char *symbol)
{
    QString symbolStr(symbol);
    startStream(symbolStr);
}

void MarketDepthQuoteReceiver::stopStream(QString &symbol)
{
    qCDebug(MarketDepthQuoteReceiverLog) << Q_FUNC_INFO << "Stopping MarketDepthQuote stream for " << symbol;

    Q_ASSERT(streams.contains(symbol));

    disconnect(streams[symbol]);

    TSClient::getInstance().closeStreamMarketDepthQuote(streams[symbol]);

    bool removed = streams.remove(symbol);
    Q_ASSERT(removed);
}

void MarketDepthQuoteReceiver::stopStream(const char* symbol) {
    QString symbolStr(symbol);
    stopStream(symbolStr);
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

    emit currentHighlightedReceivedMarketDepthQuote(symbol, marketDepthQuote, imbalance, bidDWP, askDWP);
}

void MarketDepthQuoteReceiver::onStreamError(Stream::StreamError error, QString errorMessage)
{
    qCCritical(MarketDepthQuoteReceiverLog) << "Stream fucked";
#warning TODO complete this by retreiving the stream and close it
}
