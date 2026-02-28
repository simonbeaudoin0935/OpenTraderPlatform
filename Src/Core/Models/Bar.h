#pragma once

#include <QDateTime>
#include <QDebug>
#include <QMetaType>
#include <QString>

/**
 * @brief OHLCV bar for charting and caching.
 *
 * Source-agnostic: populated from Databento OhlcvMsg (historical),
 * accumulated TradeMsg (live forming bar), or from the SQLite bar cache.
 */
class Bar
{
  public:
    enum class BarStatus : quint8
    {
        Uninitialized = 0,
        Null,
        Open,
        Closed,
    };

    static BarStatus barStatusFromString(const QString& barStatus);
    static QString barStatusToString(BarStatus barStatus);

    Bar() = default;

    // Constructor for DB read / historical bars
    Bar(QDateTime ts, float o, float h, float l, float c, quint64 vol, BarStatus status = BarStatus::Closed);

    static Bar nullBar(QDateTime dateTime);

    bool operator==(const Bar& other) const;

    // Getters
    float getOpen() const
    {
        return m_open;
    }
    float getHigh() const
    {
        return m_high;
    }
    float getLow() const
    {
        return m_low;
    }
    float getClose() const
    {
        return m_close;
    }
    quint64 getTotalVolume() const
    {
        return m_totalVolume;
    }
    QDateTime getTimeStamp() const
    {
        return m_timeStamp;
    }
    QDateTime getTimestamp() const
    {
        return m_timeStamp;
    } // alias
    BarStatus getBarStatus() const
    {
        return m_barStatus;
    }

    void setClose(float c)
    {
        m_close = c;
    }
    void setBarStatus(BarStatus s)
    {
        m_barStatus = s;
    }

    bool isValid() const;
    QString toJsonString() const;

    friend QDebug operator<<(QDebug debug, const Bar& bar);
    friend QDebug operator<<(QDebug debug, Bar::BarStatus status)
    {
        debug.noquote() << Bar::barStatusToString(status);
        return debug;
    }

  private:
    QDateTime m_timeStamp;
    quint64 m_totalVolume = 0;
    float m_open = 0.0f;
    float m_high = 0.0f;
    float m_low = 0.0f;
    float m_close = 0.0f;
    BarStatus m_barStatus = BarStatus::Uninitialized;
};

Q_DECLARE_METATYPE(Bar)
