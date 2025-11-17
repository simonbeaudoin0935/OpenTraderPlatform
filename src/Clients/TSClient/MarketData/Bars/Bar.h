#pragma once

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <QUrlQuery>

class Bar {
public:
    enum class BarStatus { Open, Closed, Null};
    enum class BarUnit { Minute, Daily, Weekly, Monthly };
    enum class BarSessionTemplate { USEQPre, USEQPost, USEPreAndPost,USEQ24Hour, Default };

    static BarStatus barStatusFromString(const QString & barStatus);
    static QString barStatusToString(BarStatus barStatus);

    static QUrlQuery buildUrlQuery(unsigned int interval,
                                   BarUnit unit,
                                   unsigned int barsback,
                                   BarSessionTemplate sessionTemplate,
                                   QDateTime firstDate = QDateTime(),
                                   QDateTime lastDate = QDateTime());


    // Default constructor
    Bar() = default;
    
    static Bar nullBar(QDateTime dateTime);

    // Constructor for database
    Bar(QDateTime ts, double o, double h, double l, double c, qint64 vol);

    // Constructor taking a QJsonObject
    Bar(const QJsonObject& jsonObj);

    // Getters
    double getHigh() const { return high; }
    double getLow() const { return low; }
    double getOpen() const { return open; }
    double getClose() const { return close; }
    QDateTime getTimeStamp() const { return timeStamp; }
    quint64 getTotalVolume() const { return totalVolume; }
    quint64 getDownTicks() const { return downTicks; }
    quint64 getDownVolume() const { return downVolume; }
    double getOpenInterest() const { return openInterest; }
    bool getIsRealtime() const { return isRealtime; }
    bool getIsEndOfHistory() const { return isEndOfHistory; }
    quint64 getTotalTicks() const { return totalTicks; }
    quint64 getUnchangedTicks() const { return unchangedTicks; }
    quint64 getUnchangedVolume() const { return unchangedVolume; }
    quint64 getUpTicks() const { return upTicks; }
    quint64 getUpVolume() const { return upVolume; }
    qint64 getEpoch() const { return epoch; }
    BarStatus getBarStatus() const { return barStatus; }

    // Validation
    bool isValid() const;

    // Convert to JSON string for debugging/logging
    QString toJsonString() const;

    void ajustTimeStampToOpeningMinute();


private:
    double    high;
    double    low;
    double    open;
    double    close;
    QDateTime timeStamp;
    quint64   totalVolume;
    quint64   downTicks;
    quint64   downVolume;
    double    openInterest;
    bool      isRealtime;
    bool      isEndOfHistory;
    quint64   totalTicks;
    quint64   unchangedTicks;
    quint64   unchangedVolume;
    quint64   upTicks;
    quint64   upVolume;
    qint64    epoch;
    BarStatus barStatus;
};

Q_DECLARE_METATYPE(Bar)
