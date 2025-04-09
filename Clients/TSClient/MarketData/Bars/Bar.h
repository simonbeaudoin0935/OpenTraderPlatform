#ifndef BAR_H
#define BAR_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <QUrlQuery>

class Bar {
public:
    enum class BarUnit { Minute, Daily, Weekly, Monthly };
    enum class BarSessionTemplate { USEQPre, USEQPost, USEPreAndPost,USEQ24Hour, Default };

    static QUrlQuery buildUrlQuery(unsigned int interval,
                                   BarUnit unit,
                                   unsigned int barsback,
                                   BarSessionTemplate sessionTemplate,
                                   QDateTime firstDate = QDateTime(),
                                   QDateTime lastDate = QDateTime());


    // Default constructor
    Bar() = default;
    
    // Constructor taking a QJsonObject
    Bar(const QJsonObject& jsonObj);

    // Getters
    QString getHigh() const { return high; }
    QString getLow() const { return low; }
    QString getOpen() const { return open; }
    QString getClose() const { return close; }
    QString getTimeStamp() const { return timeStamp; }
    QString getTotalVolume() const { return totalVolume; }
    int getDownTicks() const { return downTicks; }
    int getDownVolume() const { return downVolume; }
    QString getOpenInterest() const { return openInterest; }
    bool getIsRealtime() const { return isRealtime; }
    bool getIsEndOfHistory() const { return isEndOfHistory; }
    int getTotalTicks() const { return totalTicks; }
    int getUnchangedTicks() const { return unchangedTicks; }
    int getUnchangedVolume() const { return unchangedVolume; }
    int getUpTicks() const { return upTicks; }
    int getUpVolume() const { return upVolume; }
    qint64 getEpoch() const { return epoch; }
    QString getBarStatus() const { return barStatus; }

    // Validation
    bool isValid() const;

    // Convert to JSON string for debugging/logging
    QString toJsonString() const;

private:
    QString high;           // Required
    QString low;            // Required
    QString open;           // Required
    QString close;          // Required
    QString timeStamp;      // Required, ISO 8601 format
    QString totalVolume;    // Required
    int downTicks;         // Required
    int downVolume;        // Required
    QString openInterest;   // Required
    bool isRealtime;       // Required
    bool isEndOfHistory;   // Required
    int totalTicks;        // Required
    int unchangedTicks;    // Required
    int unchangedVolume;   // Required
    int upTicks;          // Required
    int upVolume;         // Required
    qint64 epoch;         // Required
    QString barStatus;     // Required
};

Q_DECLARE_METATYPE(Bar)

#endif // BAR_H
