#pragma once

#include <QString>
#include <QDateTime>
#include <QJsonObject>

class Quote
{
public:
    Quote();
    explicit Quote(const QJsonObject& json);

    // Getters
    QString getSymbol() const { return m_symbol; }
    double getOpen() const { return m_open; }
    double getHigh() const { return m_high; }
    double getLow() const { return m_low; }
    double getPreviousClose() const { return m_previousClose; }
    double getLast() const { return m_last; }
    double getAsk() const { return m_ask; }
    unsigned int getAskSize() const { return m_askSize; }
    double getBid() const { return m_bid; }
    unsigned int getBidSize() const { return m_bidSize; }
    double getNetChange() const { return m_netChange; }
    double getNetChangePct() const { return m_netChangePct; }
    double getHigh52Week() const { return m_high52Week; }
    QDateTime getHigh52WeekTimestamp() const { return m_high52WeekTimestamp; }
    double getLow52Week() const { return m_low52Week; }
    QDateTime getLow52WeekTimestamp() const { return m_low52WeekTimestamp; }
    unsigned long long getVolume() const { return m_volume; }
    unsigned long long getPreviousVolume() const { return m_previousVolume; }
    double getClose() const { return m_close; }
    unsigned int getDailyOpenInterest() const { return m_dailyOpenInterest; }
    QDateTime getTradeTime() const { return m_tradeTime; }
    unsigned int getTickSizeTier() const { return m_tickSizeTier; }
    bool isDelayed() const { return m_isDelayed; }
    bool isHardToBorrow() const { return m_isHardToBorrow; }
    bool isBats() const { return m_isBats; }
    bool isHalted() const { return m_isHalted; }
    unsigned int getLastSize() const { return m_lastSize; }
    QString getLastVenue() const { return m_lastVenue; }
    double getVWAP() const { return m_vwap; }

    // Validation
    bool isValid() const;

    // JSON conversion
    QJsonObject toJson() const;
    QString toJsonString() const;
    void fromJson(const QJsonObject& json);

private:
    QString m_symbol;
    double m_open = 0.0;
    double m_high = 0.0;
    double m_low = 0.0;
    double m_previousClose = 0.0;
    double m_last = 0.0;
    double m_ask = 0.0;
    unsigned int m_askSize = 0;
    double m_bid = 0.0;
    unsigned int m_bidSize = 0;
    double m_netChange = 0.0;
    double m_netChangePct = 0.0;
    double m_high52Week = 0.0;
    QDateTime m_high52WeekTimestamp;
    double m_low52Week = 0.0;
    QDateTime m_low52WeekTimestamp;
    unsigned long long m_volume = 0;
    unsigned long long m_previousVolume = 0;
    double m_close = 0.0;
    unsigned int m_dailyOpenInterest = 0;
    QDateTime m_tradeTime;
    unsigned int m_tickSizeTier = 0;
    bool m_isDelayed = false;
    bool m_isHardToBorrow = false;
    bool m_isBats = false;
    bool m_isHalted = false;
    unsigned int m_lastSize = 0;
    QString m_lastVenue;
    double m_vwap = 0.0;
};
