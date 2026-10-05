#pragma once

#include <optional>

#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <QString>

#include "MarketFlags.h"

/**
 * @brief Lightweight TradeStation quote model used for live market-status and BBO-derived tape.
 */
class Quote
{
  public:
    Quote() = default;
    explicit Quote(const QJsonObject& json)
    {
        fromJson(json);
    }

    void fromJson(const QJsonObject& json);

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] QString getSymbol() const
    {
        return m_symbol;
    }
    [[nodiscard]] QString getError() const
    {
        return m_error;
    }
    [[nodiscard]] double getLast() const
    {
        return m_last;
    }
    [[nodiscard]] unsigned int getLastSize() const
    {
        return m_lastSize;
    }
    [[nodiscard]] QDateTime getTradeTime() const
    {
        return m_tradeTime;
    }
    [[nodiscard]] double getBid() const
    {
        return m_bid;
    }
    [[nodiscard]] unsigned int getBidSize() const
    {
        return m_bidSize;
    }
    [[nodiscard]] double getAsk() const
    {
        return m_ask;
    }
    [[nodiscard]] unsigned int getAskSize() const
    {
        return m_askSize;
    }
    [[nodiscard]] const MarketFlags& getMarketFlags() const
    {
        return m_marketFlags;
    }

    [[nodiscard]] std::optional<double> getBestBid() const;
    [[nodiscard]] std::optional<double> getBestAsk() const;

    [[nodiscard]] bool isHalted() const
    {
        return m_marketFlags.isHalted();
    }
    [[nodiscard]] bool isDelayed() const
    {
        return m_marketFlags.isDelayed();
    }
    [[nodiscard]] bool isHardToBorrow() const
    {
        return m_marketFlags.isHardToBorrow();
    }

  private:
    QString m_symbol;
    QString m_error;
    QDateTime m_tradeTime;

    double m_last = 0.0;
    unsigned int m_lastSize = 0;

    double m_bid = 0.0;
    unsigned int m_bidSize = 0;
    double m_ask = 0.0;
    unsigned int m_askSize = 0;

    MarketFlags m_marketFlags;
};

Q_DECLARE_METATYPE(Quote)
