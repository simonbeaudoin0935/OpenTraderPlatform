#pragma once

#include <QObject>

#include "Assume.h"
#include "StreamMarketDepthQuote.h"
#include "StreamReceiver.h"

Q_DECLARE_LOGGING_CATEGORY(MarketDepthQuoteReceiverLog)

class MarketDepthQuoteReceiver : public StreamReceiver
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
    /**
     * @brief Signal emitted when a new market depth quote is received and processed
     * 
     * Thread Safety (Architecture_Improvements.md point 1.4):
     * - Emitted from: MainAlgo worker thread (this receiver runs on MainAlgo thread)
     * - Received on: MainAlgo thread (same thread, typically uses Qt::DirectConnection)
     * - Thread-safe: Yes (receivers run on MainAlgo thread, receive from TSClient via queued connection)
     * 
     * Data Flow:
     * 1. StreamMarketDepthQuote (TSClient thread) emits newMarketDepthQuote signal
     * 2. MarketDepthQuoteReceiver slot onReceivedNewMarketDepthQuote (MainAlgo thread) receives via Qt::QueuedConnection
     * 3. MarketDepthQuoteReceiver calculates bid/ask imbalance and depth-weighted prices
     * 4. MarketDepthQuoteReceiver emits receivedNewMarketDepthQuote to MainAlgo (same MainAlgo thread)
     * 
     * @param symbol Stock ticker symbol
     * @param marketDepthQuote The market depth quote data
     * @param bidAskImbalance Calculated bid/ask imbalance ratio
     * @param bidDWP Bid depth-weighted price
     * @param askDWP Ask depth-weighted price
     */
    void receivedNewMarketDepthQuote(QString symbol,
                                     MarketDepthQuote marketDepthQuote,
                                     double bidAskImbalance,
                                     double bidDWP,
                                     double askDWP);

  private slots:
    void onReceivedNewMarketDepthQuote(MarketDepthQuote marketDepthQuote);

  protected:
    [[nodiscard]] QPointer<Stream> getStreamBase() const override
    {
        return QPointer<Stream>(m_stream.data());
    }

  private:
    void createMarketDepthQuoteStream();

    QString m_symbol;
    QPointer<StreamMarketDepthQuote> m_stream;
    unsigned int depthWeightedPriceLevel = 5; // Default value of 5
    unsigned int bidAskImbalanceLevel = 5;    // Default value of 5
};
