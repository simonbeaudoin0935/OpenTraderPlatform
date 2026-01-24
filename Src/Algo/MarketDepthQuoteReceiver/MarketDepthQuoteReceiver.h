#pragma once

#include <QObject>

#include "StreamMarketDepthQuote.h"
#include "Assume.h"

Q_DECLARE_LOGGING_CATEGORY(MarketDepthQuoteReceiverLog)

class MarketDepthQuoteReceiver : public QObject
{
    Q_OBJECT
  public:
    explicit MarketDepthQuoteReceiver(const QString& symbol, QObject* parent = nullptr);
    ~MarketDepthQuoteReceiver();

    // Market analysis functions
    double calculateBidAskImbalance(const MarketDepthQuote& quote, int levels = 0) const;
    double calculateDepthWeightedPrice(const QVector<MarketDepthLevel>& levels) const;

    // Getter and setter for DepthWeightedPriceLevel
    unsigned int getDepthWeightedPriceLevel() const
    {
        return depthWeightedPriceLevel;
    }
    void setDepthWeightedPriceLevel(unsigned int level)
    {
        ASSUME_GT(level, 0u);
        depthWeightedPriceLevel = qMax(1u, level);
    }

    // Getter and setter for BidAskImbalanceLevel
    unsigned int getBidAskImbalanceLevel() const
    {
        return bidAskImbalanceLevel;
    }
    void setBidAskImbalanceLevel(unsigned int level)
    {
        ASSUME_GT(level, 0u);
        bidAskImbalanceLevel = qMax(1u, level);
    }

    QPointer<StreamMarketDepthQuote> getStream() const
    {
        return m_stream;
    }

  signals:
    void receivedNewMarketDepthQuote(QString symbol,
                                     MarketDepthQuote marketDepthQuote,
                                     double bidAskImbalance,
                                     double bidDWP,
                                     double askDWP);

  private slots:
    void onReceivedNewMarketDepthQuote(MarketDepthQuote marketDepthQuote);

  private:
    void createMarketDepthQuoteStream();

    QString m_symbol;
    QPointer<StreamMarketDepthQuote> m_stream;
    unsigned int depthWeightedPriceLevel = 5; // Default value of 5
    unsigned int bidAskImbalanceLevel = 5;    // Default value of 5
};
