#pragma once

#include <QObject>

#include "StreamMarketDepthQuote.h"

Q_DECLARE_LOGGING_CATEGORY(MarketDepthQuoteReceiverLog)

class MarketDepthQuoteReceiver : public QObject
{
    Q_OBJECT
public:
    explicit MarketDepthQuoteReceiver(const QString &symbol, QObject *parent = nullptr);
    ~MarketDepthQuoteReceiver();

    void stopStream();

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
    void receivedNewMarketDepthQuote(QString symbol, MarketDepthQuote marketDepthQuote, double bidAskImbalance, double bidDWP, double askDWP);

private slots:
    void onReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote marketDepthQuote);

private:
    QString symbol;
    StreamMarketDepthQuote* stream = nullptr;
    unsigned int depthWeightedPriceLevel = 5; // Default value of 5
    unsigned int bidAskImbalanceLevel = 5; // Default value of 5
};
