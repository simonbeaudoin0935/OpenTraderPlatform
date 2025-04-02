#include "Bar.h"
#include <QJsonDocument>

Bar::Bar(const QJsonObject& jsonObj) {
    high = jsonObj["High"].toString();
    low = jsonObj["Low"].toString();
    open = jsonObj["Open"].toString();
    close = jsonObj["Close"].toString();
    timeStamp = jsonObj["TimeStamp"].toString();
    totalVolume = jsonObj["TotalVolume"].toString();
    downTicks = jsonObj["DownTicks"].toInt();
    downVolume = jsonObj["DownVolume"].toInt();
    openInterest = jsonObj["OpenInterest"].toString();
    isRealtime = jsonObj["IsRealtime"].toBool();
    isEndOfHistory = jsonObj["IsEndOfHistory"].toBool();
    totalTicks = jsonObj["TotalTicks"].toInt();
    unchangedTicks = jsonObj["UnchangedTicks"].toInt();
    unchangedVolume = jsonObj["UnchangedVolume"].toInt();
    upTicks = jsonObj["UpTicks"].toInt();
    upVolume = jsonObj["UpVolume"].toInt();
    epoch = jsonObj["Epoch"].toInteger();
    barStatus = jsonObj["BarStatus"].toString();
}

bool Bar::isValid() const {
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

    return true;
}

QString Bar::toJsonString() const {
    QJsonObject jsonObj;
    jsonObj["High"] = high;
    jsonObj["Low"] = low;
    jsonObj["Open"] = open;
    jsonObj["Close"] = close;
    jsonObj["TimeStamp"] = timeStamp;
    jsonObj["TotalVolume"] = totalVolume;
    jsonObj["DownTicks"] = downTicks;
    jsonObj["DownVolume"] = downVolume;
    jsonObj["OpenInterest"] = openInterest;
    jsonObj["IsRealtime"] = isRealtime;
    jsonObj["IsEndOfHistory"] = isEndOfHistory;
    jsonObj["TotalTicks"] = totalTicks;
    jsonObj["UnchangedTicks"] = unchangedTicks;
    jsonObj["UnchangedVolume"] = unchangedVolume;
    jsonObj["UpTicks"] = upTicks;
    jsonObj["UpVolume"] = upVolume;
    jsonObj["Epoch"] = epoch;
    jsonObj["BarStatus"] = barStatus;
    
    QJsonDocument doc(jsonObj);
    return QString(doc.toJson(QJsonDocument::Indented));
}
