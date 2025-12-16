#include <QJsonDocument>
#include <QTimeZone>
#include <QDebug>

#include "Bar.h"

Bar::BarStatus Bar::barStatusFromString(const QString &barStatus)
{
    if (barStatus == "Open") {
        return BarStatus::Open;
    } else if (barStatus == "Closed") {
        return BarStatus::Closed;
    } else {
        Q_ASSERT(0);
    }
}

QString Bar::barStatusToString(BarStatus barStatus)
{
    switch (barStatus) {
        case BarStatus::Open: return "Open";
        case BarStatus::Closed: return "Closed";
        case BarStatus::Null: return "Null";
        default: Q_UNREACHABLE();
    }
}

QUrlQuery Bar::buildUrlQuery(unsigned int interval,
                             BarUnit unit,
                             unsigned int barsback,
                             BarSessionTemplate sessionTemplate,
                             QDateTime firstDate,
                             QDateTime lastDate)
{
    QUrlQuery query;
    query.addQueryItem("interval", QString::number(interval));
    query.addQueryItem("unit", [unit]() -> QString {
        switch (unit) {
        case Bar::BarUnit::Minute: return "Minute";
        case Bar::BarUnit::Daily: return "Daily";
        case Bar::BarUnit::Weekly: return "Weekly";
        case Bar::BarUnit::Monthly: return "Monthly";
        default: Q_UNREACHABLE();
        }
    }());
    query.addQueryItem("sessiontemplate", [sessionTemplate]() -> QString {
        switch (sessionTemplate) {
        case Bar::BarSessionTemplate::USEQPre: return "USEQPre";
        case Bar::BarSessionTemplate::USEQPost: return "USEQPost";
        case Bar::BarSessionTemplate::USEPreAndPost: return "USEPreAndPost";
        case Bar::BarSessionTemplate::USEQ24Hour: return "USEQ24Hour";
        case Bar::BarSessionTemplate::Default: return "Default";
        default: Q_UNREACHABLE();
        }
    }());

    if (firstDate != QDateTime() && lastDate != QDateTime()) {
        Q_ASSERT(barsback == 0);
        Q_ASSERT(firstDate.secsTo(lastDate) >= 1);
        query.addQueryItem("firstdate", firstDate.toString(Qt::ISODate));
        query.addQueryItem("lastdate", lastDate.toString(Qt::ISODate));
    } else if (firstDate == QDateTime() && lastDate == QDateTime()) {
        // nothing to do, this is the case for a stream
        if (barsback > 0) {
            query.addQueryItem("barsback", QString::number(barsback));
        }
} else {
    Q_ASSERT(0);
}

return query;
}

Bar Bar::nullBar(QDateTime dateTime)
{
    Bar bar = Bar();
    bar.barStatus = BarStatus::Null;
    bar.timeStamp = dateTime;

    return bar;
}

Bar::Bar(QDateTime ts, double o, double h, double l, double c, qint64 vol) {
    timeStamp = ts;
    open = o;
    high = h;
    low = l;
    close = c;
    totalVolume = vol;
    downTicks = 0;
    downVolume = 0;
    openInterest = 0;
    isRealtime = false;
    isEndOfHistory = false;
    totalTicks = 0;
    unchangedTicks = 0;
    unchangedVolume = 0;
    upTicks = 0;
    upVolume = 0;
    epoch = ts.toSecsSinceEpoch();
    barStatus = BarStatus::Closed;
}

Bar::Bar(const QJsonObject& jsonObj) {
    high = jsonObj["High"].toString().toDouble();
    low = jsonObj["Low"].toString().toDouble();
    open = jsonObj["Open"].toString().toDouble();
    close = jsonObj["Close"].toString().toDouble();
    timeStamp = QDateTime::fromString(jsonObj["TimeStamp"].toString(), Qt::ISODate).toTimeZone(QTimeZone("America/New_York"));
    totalVolume = (quint64) jsonObj["TotalVolume"].toString().toInt();
    downTicks =  (quint64) jsonObj["DownTicks"].toInt();
    downVolume = (quint64) jsonObj["DownVolume"].toInt();
    openInterest = jsonObj["OpenInterest"].toDouble();
    isRealtime = jsonObj["IsRealtime"].toBool();
    isEndOfHistory = jsonObj["IsEndOfHistory"].toBool();
    totalTicks = (quint64) jsonObj["TotalTicks"].toInt();
    unchangedTicks = (quint64) jsonObj["UnchangedTicks"].toInt();
    unchangedVolume = (quint64) jsonObj["UnchangedVolume"].toInt();
    upTicks = (quint64) jsonObj["UpTicks"].toInt();
    upVolume = (quint64) jsonObj["UpVolume"].toInt();
    epoch = jsonObj["Epoch"].toInteger();
    barStatus = barStatusFromString(jsonObj["BarStatus"].toString());

    qWarning()<<"bar created with timestamp "<<timeStamp;
}

bool Bar::isValid() const {

/*
    // Check that all required fields are present and have valid values
    if (high.isEmpty() || low.isEmpty() || open.isEmpty() || close.isEmpty() ||
        timeStamp.isEmpty() || totalVolume.isEmpty() || openInterest.isEmpty() ||
        barStatus.isEmpty()) {
        return false;
    }

    // Check numeric fields for valid values
    if (downTicks < 0 || downVolume < 0 || totalTicks < 0 || 
        unchangedTicks < 0 || unchangedVolume < 0 || upTicks < 0 || 
        upVolume < 0 || epoch <= 0) {
        return false;
    }

    // Validate totalTicks matches the sum of its components
    if (totalTicks != (downTicks + unchangedTicks + upTicks)) {
        return false;
    }

    // Validate totalVolume matches the sum of its components
    qint64 calculatedTotalVolume = downVolume + unchangedVolume + upVolume;
    if (totalVolume.toLongLong() != calculatedTotalVolume) {
        return false;
    }

*/
    return true;
}

QString Bar::toJsonString() const {
    QJsonObject jsonObj;
    jsonObj["High"] = high;
    jsonObj["Low"] = low;
    jsonObj["Open"] = open;
    jsonObj["Close"] = close;
    jsonObj["TimeStamp"] = timeStamp.toString();
    jsonObj["TotalVolume"] = (qint64) totalVolume;
    jsonObj["DownTicks"] = (qint64) downTicks;
    jsonObj["DownVolume"] = (qint64) downVolume;
    jsonObj["OpenInterest"] = openInterest;
    jsonObj["IsRealtime"] = isRealtime;
    jsonObj["IsEndOfHistory"] = isEndOfHistory;
    jsonObj["TotalTicks"] = (qint64) totalTicks;
    jsonObj["UnchangedTicks"] = (qint64) unchangedTicks;
    jsonObj["UnchangedVolume"] = (qint64) unchangedVolume;
    jsonObj["UpTicks"] = (qint64) upTicks;
    jsonObj["UpVolume"] = (qint64) upVolume;
    jsonObj["Epoch"] = epoch;
    jsonObj["BarStatus"] = barStatusToString(barStatus);
    
    QJsonDocument doc(jsonObj);
    return QString(doc.toJson(QJsonDocument::Indented));
}
