#pragma once

#include <optional>

#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include "MarketFlags.h"

class Quote
{
  public:
    Quote() = default;
    explicit Quote(const QJsonObject& json);

    // Getters
    QString getSymbol() const
    {
        return m_symbol;
    }
    QString getError() const
    {
        return m_error;
    }
    double getOpen() const
    {
        return m_open;
    }
    double getHigh() const
    {
        return m_high;
    }
    double getLow() const
    {
        return m_low;
    }
    double getPreviousClose() const
    {
        return m_previousClose;
    }
    double getLast() const
    {
        return m_last;
    }
    double getAsk() const
    {
        return m_ask;
    }
    unsigned int getAskSize() const
    {
        return m_askSize;
    }
    double getBid() const
    {
        return m_bid;
    }
    unsigned int getBidSize() const
    {
        return m_bidSize;
    }
    double getNetChange() const
    {
        return m_netChange;
    }
    double getNetChangePct() const
    {
        return m_netChangePct;
    }
    double getHigh52Week() const
    {
        return m_high52Week;
    }
    QDateTime getHigh52WeekTimestamp() const
    {
        return m_high52WeekTimestamp;
    }
    double getLow52Week() const
    {
        return m_low52Week;
    }
    QDateTime getLow52WeekTimestamp() const
    {
        return m_low52WeekTimestamp;
    }
    unsigned long long getVolume() const
    {
        return m_volume;
    }
    unsigned long long getPreviousVolume() const
    {
        return m_previousVolume;
    }
    double getClose() const
    {
        return m_close;
    }
    unsigned int getDailyOpenInterest() const
    {
        return m_dailyOpenInterest;
    }
    QDateTime getTradeTime() const
    {
        return m_tradeTime;
    }
    unsigned int getTickSizeTier() const
    {
        return m_tickSizeTier;
    }
    unsigned int getLastSize() const
    {
        return m_lastSize;
    }
    QString getLastVenue() const
    {
        return m_lastVenue;
    }
    double getVWAP() const
    {
        return m_vwap;
    }
    const std::optional<double>& getMinPrice() const
    {
        return m_minPrice;
    }
    const std::optional<double>& getMaxPrice() const
    {
        return m_maxPrice;
    }
    const std::optional<QDateTime>& getFirstNoticeDate() const
    {
        return m_firstNoticeDate;
    }
    const std::optional<QDateTime>& getLastTradingDate() const
    {
        return m_lastTradingDate;
    }
    const QStringList& getRestrictions() const
    {
        return m_restrictions;
    }
    const MarketFlags& getMarketFlags() const
    {
        return m_marketFlags;
    }
    bool isDelayed() const
    {
        return m_marketFlags.isDelayed();
    }
    bool isHardToBorrow() const
    {
        return m_marketFlags.isHardToBorrow();
    }
    bool isBats() const
    {
        return m_marketFlags.isBats();
    }
    bool isHalted() const
    {
        return m_marketFlags.isHalted();
    }
    std::optional<double> getBestBid() const;
    std::optional<double> getBestAsk() const;

    // Validation
    bool isValid() const;
    bool isEmpty() const;

    // JSON conversion
    QJsonObject toJson() const;
    QString toJsonString() const;
    void fromJson(const QJsonObject& json);

  private:
    // Members ordered by size (largest to smallest) to reduce padding
    QDateTime m_high52WeekTimestamp;
    QDateTime m_low52WeekTimestamp;
    QDateTime m_tradeTime;
    std::optional<QDateTime> m_firstNoticeDate;
    std::optional<QDateTime> m_lastTradingDate;
    QStringList m_restrictions;
    QString m_symbol;
    QString m_error;
    QString m_lastVenue;
    MarketFlags m_marketFlags;
    std::optional<double> m_minPrice;
    std::optional<double> m_maxPrice;
    unsigned long long m_volume = 0;
    unsigned long long m_previousVolume = 0;
    double m_open = 0.0;
    double m_high = 0.0;
    double m_low = 0.0;
    double m_previousClose = 0.0;
    double m_last = 0.0;
    double m_ask = 0.0;
    double m_bid = 0.0;
    double m_netChange = 0.0;
    double m_netChangePct = 0.0;
    double m_high52Week = 0.0;
    double m_low52Week = 0.0;
    double m_close = 0.0;
    double m_vwap = 0.0;
    unsigned int m_askSize = 0;
    unsigned int m_bidSize = 0;
    unsigned int m_dailyOpenInterest = 0;
    unsigned int m_tickSizeTier = 0;
    unsigned int m_lastSize = 0;
};

Q_DECLARE_METATYPE(Quote)
