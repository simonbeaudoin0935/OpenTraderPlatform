#pragma once

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <QUrlQuery>

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

    enum class BarUnit
    {
        Minute,
        Daily,
        Weekly,
        Monthly
    };
    enum class BarSessionTemplate
    {
        USEQPre,
        USEQPost,
        USEPreAndPost,
        USEQ24Hour,
        Default
    };

    static BarStatus barStatusFromString(const QString& barStatus);
    static QString barStatusToString(BarStatus barStatus);

    static QUrlQuery buildUrlQuery(unsigned int interval,
                                   BarUnit unit,
                                   unsigned int barsback,
                                   BarSessionTemplate sessionTemplate,
                                   std::optional<QDateTime> firstDate = std::nullopt,
                                   std::optional<QDateTime> lastDate = std::nullopt);

    // Default constructor
    Bar() : m_flags(0) {}

    static Bar nullBar(QDateTime dateTime);

    // Constructor for database
    Bar(QDateTime ts, float o, float h, float l, float c, qint64 vol);

    // Constructor taking a QJsonObject
    Bar(const QJsonObject& jsonObj);

    // Getters
    float getHigh() const
    {
        return m_high;
    }
    float getLow() const
    {
        return m_low;
    }
    float getOpen() const
    {
        return m_open;
    }
    float getClose() const
    {
        return m_close;
    }
    QDateTime getTimeStamp() const
    {
        return m_timeStamp;
    }
    quint64 getTotalVolume() const
    {
        return m_totalVolume;
    }
    quint64 getDownTicks() const
    {
        return m_downTicks;
    }
    quint64 getDownVolume() const
    {
        return m_downVolume;
    }
    float getOpenInterest() const
    {
        return m_openInterest;
    }
    bool getIsRealtime() const
    {
        return (m_flags & FLAG_IS_REALTIME) != 0;
    }
    bool getIsEndOfHistory() const
    {
        return (m_flags & FLAG_IS_END_OF_HISTORY) != 0;
    }
    quint64 getTotalTicks() const
    {
        return m_totalTicks;
    }
    quint64 getUnchangedTicks() const
    {
        return m_unchangedTicks;
    }
    quint64 getUnchangedVolume() const
    {
        return m_unchangedVolume;
    }
    quint64 getUpTicks() const
    {
        return m_upTicks;
    }
    quint64 getUpVolume() const
    {
        return m_upVolume;
    }
    qint64 getEpoch() const
    {
        return m_epoch;
    }
    BarStatus getBarStatus() const
    {
        return static_cast<BarStatus>((m_flags >> 2) & 0x03);
    }
    QDateTime getTimestamp() const
    {
        return m_timeStamp;
    }

    // Validation
    bool isValid() const;

    // Convert to JSON string for debugging/logging
    QString toJsonString() const;

    void setClose(float c)
    {
        m_close = c;
    }

  private:
    // Bit flags for BarStatus (2 bits) and boolean fields (2 bits)
    static constexpr quint8 FLAG_IS_REALTIME = 0x01;       // bit 0
    static constexpr quint8 FLAG_IS_END_OF_HISTORY = 0x02; // bit 1
    static constexpr quint8 BARSTATUS_SHIFT = 2;           // bits 2-3 for BarStatus
    static constexpr quint8 BARSTATUS_MASK = 0x0C;         // bits 2-3 mask

    // Members ordered by size (largest to smallest) to minimize padding
    QDateTime m_timeStamp;     // 8 bytes (pointer to shared data)
    quint64 m_totalVolume;     // 8 bytes
    quint64 m_downTicks;       // 8 bytes
    quint64 m_downVolume;      // 8 bytes
    quint64 m_totalTicks;      // 8 bytes
    quint64 m_unchangedTicks;  // 8 bytes
    quint64 m_unchangedVolume; // 8 bytes
    quint64 m_upTicks;         // 8 bytes
    quint64 m_upVolume;        // 8 bytes
    qint64 m_epoch;            // 8 bytes
    float m_high;              // 4 bytes
    float m_low;               // 4 bytes
    float m_open;              // 4 bytes
    float m_close;             // 4 bytes
    float m_openInterest;      // 4 bytes
    quint8 m_flags;            // 1 byte (contains BarStatus + booleans)
    // 3 bytes padding here to align to 8-byte boundary
};

Q_DECLARE_METATYPE(Bar)
