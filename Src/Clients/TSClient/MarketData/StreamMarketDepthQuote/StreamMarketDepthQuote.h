#pragma once

#include <atomic>

#include "StreamMarketData.h"
#include "MarketDepthQuote.h"

class StreamMarketDepthQuote final : public StreamMarketData
{
    Q_OBJECT

  public:
    explicit StreamMarketDepthQuote(const QString& symbol, QNetworkReply* reply, QObject* parent = nullptr);
    ~StreamMarketDepthQuote();
    Q_DISABLE_COPY_MOVE(StreamMarketDepthQuote)

    QString getSymbol() const
    {
        return m_symbol;
    }

    /**
     * @brief Get the number of currently open market depth quote streams
     * @return Current count of active streams
     * @note Thread-safe: Uses atomic counter
     */
    static size_t getNumberOfMarketDepthStreams()
    {
        return s_numberOfMarketDepthStreams.load();
    }

    /**
     * @brief Check if a new market depth stream can be opened
     * @return true if count < MAX_CONCURRENT_STREAMS, false otherwise
     * @note Thread-safe: Can be called from any thread
     * @warning Even if this returns true, concurrent requests may still cause queuing
     */
    static bool canOpenStream();

  signals:
    void newMarketDepthQuoteReceived(MarketDepthQuote quote);

  private:
    void processJsonObject(const QJsonObject& jsonObj) override;
    QString m_symbol;

    /**
     * @brief Atomic counter for concurrent market depth streams
     *
     * Tracks the number of currently active StreamMarketDepthQuote instances.
     * Uses atomic operations for thread-safe read/write from any thread.
     * Must not exceed MAX_CONCURRENT_STREAMS (enforced by TSClient).
     */
    static std::atomic<size_t> s_numberOfMarketDepthStreams;
};
