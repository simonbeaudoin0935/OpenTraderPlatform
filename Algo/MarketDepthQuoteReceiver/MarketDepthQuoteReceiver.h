#pragma once

#include <QObject>

#include "Clients/TSClient/MarketData/StreamMarketDepthQuote/StreamMarketDepthQuote.h"

Q_DECLARE_LOGGING_CATEGORY(MarketDepthQuoteReceiverLog)

class MarketDepthQuoteReceiver : public QObject
{
    Q_OBJECT
public:
    explicit MarketDepthQuoteReceiver(QObject *parent = nullptr);

    void startStream(QString &symbol);
    void startStream(const char* symbol);

    void stopStream(QString &symbol);
    void stopStream(const char* symbol);

    // Market analysis functions
    double calculateBidAskImbalance(const MarketDepthQuote& quote, int levels = 0) const;
    double calculateDepthWeightedPrice(const QVector<MarketDepthLevel>& levels) const;

    // Getter and setter for DepthWeightedPriceLevel
    unsigned int getDepthWeightedPriceLevel() const { return depthWeightedPriceLevel; }
    void setDepthWeightedPriceLevel(unsigned int level) { Q_ASSERT(level > 0); depthWeightedPriceLevel = qMax(1u, level); }

    // Getter and setter for BidAskImbalanceLevel
    unsigned int getBidAskImbalanceLevel() const { return bidAskImbalanceLevel; }
    void setBidAskImbalanceLevel(unsigned int level) { Q_ASSERT(level > 0); bidAskImbalanceLevel = qMax(1u, level); }

signals:
    void currentHighlightedReceivedMarketDepthQuote(QString symbol, MarketDepthQuote marketDepthQuote, double bidAskImbalance, double bidDWP, double askDWP);

private slots:
    void onReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote marketDepthQuote);
    void onStreamError(Stream::StreamError error, QString errorMessage);

private:
    QMap<QString, StreamMarketDepthQuote*> streams;
    unsigned int depthWeightedPriceLevel = 5; // Default value of 5
    unsigned int bidAskImbalanceLevel = 5; // Default value of 5
};
