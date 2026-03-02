#pragma once

#include <QObject>

#include "Assume.h"
#include "Level2.h"
#include "StreamReceiver.h"

Q_DECLARE_LOGGING_CATEGORY(Level2ReceiverLog)

/**
 * @brief Receives and processes Level 2 (full 10-level book) updates for a single symbol.
 *
 * Subscribes to Databento Schema::Mbp10 via DBClient (Phase 6).
 * Calculates bid-ask imbalance and depth-weighted prices from the book snapshot.
 *
 * Thread context: lives on MainAlgo worker thread.
 */
class Level2Receiver : public StreamReceiver
{
    Q_OBJECT
  public:
    explicit Level2Receiver(const QString& symbol, QObject* parent = nullptr);
    ~Level2Receiver() override = default;

    // Market analysis functions
    double calculateBidAskImbalance(const Level2& level2, int levels = 0) const;
    double calculateDepthWeightedPrice(const std::array<Level2Row, 10>& levels, int count) const;

    unsigned int getDepthWeightedPriceLevel() const
    {
        return m_depthWeightedPriceLevel;
    }
    void setDepthWeightedPriceLevel(unsigned int level)
    {
        ASSUME_GT(level, 0u);
        m_depthWeightedPriceLevel = qMax(1u, level);
    }

    unsigned int getBidAskImbalanceLevel() const
    {
        return m_bidAskImbalanceLevel;
    }
    void setBidAskImbalanceLevel(unsigned int level)
    {
        ASSUME_GT(level, 0u);
        m_bidAskImbalanceLevel = qMax(1u, level);
    }

  signals:
    /**
     * @brief Emitted when a new Level 2 snapshot is received and metrics computed.
     * Thread context: Emitted from MainAlgo worker thread.
     */
    void receivedNewLevel2(QString symbol, Level2 level2, double bidAskImbalance, double bidDWP, double askDWP);

  public slots:
    void onReceivedNewLevel2(Level2 level2);

  protected:
    [[nodiscard]] QPointer<Stream> getStreamBase() const override
    {
        return nullptr;
    }

  private:
    void openStream();

    QString m_symbol;
    unsigned int m_depthWeightedPriceLevel = 5;
    unsigned int m_bidAskImbalanceLevel = 5;
};
