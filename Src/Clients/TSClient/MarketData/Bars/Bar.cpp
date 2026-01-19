#include <QJsonDocument>
#include <QTimeZone>
#include <QDebug>

#include "Bar.h"

Bar::BarStatus Bar::barStatusFromString(const QString& barStatus)
{
    if (barStatus == "Open")
    {
        return BarStatus::Open;
    }
    else if (barStatus == "Closed")
    {
        return BarStatus::Closed;
    }
    else
    {
        Q_UNREACHABLE();
    }
}

QString Bar::barStatusToString(BarStatus barStatus)
{
    switch (barStatus)
    {
    case BarStatus::Open:
        return "Open";
    case BarStatus::Closed:
        return "Closed";
    case BarStatus::Null:
        return "Null";
    case BarStatus::Uninitialized:
        return "Uninitialized";
    default:
        Q_UNREACHABLE();
    }
}

QUrlQuery Bar::buildUrlQuery(unsigned int interval,
                             BarUnit unit,
                             unsigned int barsback,
                             BarSessionTemplate sessionTemplate,
                             std::optional<QDateTime> firstDate,
                             std::optional<QDateTime> lastDate)
{
    QUrlQuery query;
    query.addQueryItem("interval", QString::number(interval));
    query.addQueryItem("unit",
                       [unit]() -> QString
                       {
                           switch (unit)
                           {
                           case Bar::BarUnit::Minute:
                               return "Minute";
                           case Bar::BarUnit::Daily:
                               return "Daily";
                           case Bar::BarUnit::Weekly:
                               return "Weekly";
                           case Bar::BarUnit::Monthly:
                               return "Monthly";
                           default:
                               Q_UNREACHABLE();
                           }
                       }());
    query.addQueryItem("sessiontemplate",
                       [sessionTemplate]() -> QString
                       {
                           switch (sessionTemplate)
                           {
                           case Bar::BarSessionTemplate::USEQPre:
                               return "USEQPre";
                           case Bar::BarSessionTemplate::USEQPost:
                               return "USEQPost";
                           case Bar::BarSessionTemplate::USEPreAndPost:
                               return "USEPreAndPost";
                           case Bar::BarSessionTemplate::USEQ24Hour:
                               return "USEQ24Hour";
                           case Bar::BarSessionTemplate::Default:
                               return "Default";
                           default:
                               Q_UNREACHABLE();
                           }
                       }());

    if (firstDate.has_value() && lastDate.has_value())
    {
        Q_ASSERT(barsback == 0);
        Q_ASSERT(firstDate->secsTo(*lastDate) >= 1);
        query.addQueryItem("firstdate", firstDate->toString(Qt::ISODate));
        query.addQueryItem("lastdate", lastDate->toString(Qt::ISODate));
    }
    else if (!firstDate.has_value() && !lastDate.has_value())
    {
        // nothing to do, this is the case for a stream
        if (barsback > 0)
        {
            query.addQueryItem("barsback", QString::number(barsback));
        }
    }
    else
    {
        // only one of firstDate or lastDate is set - this is an error
        Q_UNREACHABLE();
    }

    return query;
}

Bar Bar::nullBar(QDateTime dateTime)
{
    Bar bar = Bar();
    bar.m_flags = (static_cast<quint8>(BarStatus::Null) << BARSTATUS_SHIFT);
    bar.m_timeStamp = dateTime;

    return bar;
}

Bar::Bar(QDateTime ts, float o, float h, float l, float c, qint64 vol)
{
    m_timeStamp = ts;
    m_open = o;
    m_high = h;
    m_low = l;
    m_close = c;
    m_totalVolume = vol;
    m_downTicks = 0;
    m_downVolume = 0;
    m_openInterest = 0.0f;
    m_totalTicks = 0;
    m_unchangedTicks = 0;
    m_unchangedVolume = 0;
    m_upTicks = 0;
    m_upVolume = 0;
    m_epoch = ts.toSecsSinceEpoch();
    m_flags = (static_cast<quint8>(BarStatus::Closed) << BARSTATUS_SHIFT);
}

Bar::Bar(const QJsonObject& jsonObj)
{
    m_high = static_cast<float>(jsonObj["High"].toString().toDouble());
    m_low = static_cast<float>(jsonObj["Low"].toString().toDouble());
    m_open = static_cast<float>(jsonObj["Open"].toString().toDouble());
    m_close = static_cast<float>(jsonObj["Close"].toString().toDouble());
    m_timeStamp =
        QDateTime::fromString(jsonObj["TimeStamp"].toString(), Qt::ISODate).toTimeZone(QTimeZone("America/New_York"));
    m_totalVolume = (quint64)jsonObj["TotalVolume"].toString().toInt();
    m_downTicks = (quint64)jsonObj["DownTicks"].toInt();
    m_downVolume = (quint64)jsonObj["DownVolume"].toInt();
    m_openInterest = static_cast<float>(jsonObj["OpenInterest"].toDouble());
    m_totalTicks = (quint64)jsonObj["TotalTicks"].toInt();
    m_unchangedTicks = (quint64)jsonObj["UnchangedTicks"].toInt();
    m_unchangedVolume = (quint64)jsonObj["UnchangedVolume"].toInt();
    m_upTicks = (quint64)jsonObj["UpTicks"].toInt();
    m_upVolume = (quint64)jsonObj["UpVolume"].toInt();
    m_epoch = jsonObj["Epoch"].toInteger();

    // Initialize flags
    m_flags = 0;
    if (jsonObj["IsRealtime"].toBool())
    {
        m_flags |= FLAG_IS_REALTIME;
    }
    if (jsonObj["IsEndOfHistory"].toBool())
    {
        m_flags |= FLAG_IS_END_OF_HISTORY;
    }
    BarStatus status = barStatusFromString(jsonObj["BarStatus"].toString());
    m_flags |= (static_cast<quint8>(status) << BARSTATUS_SHIFT);

    qDebug() << "bar created from json with timestamp " << m_timeStamp;
}

bool Bar::isValid() const
{

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

QString Bar::toJsonString() const
{
    QJsonObject jsonObj;
    jsonObj["High"] = static_cast<double>(m_high);
    jsonObj["Low"] = static_cast<double>(m_low);
    jsonObj["Open"] = static_cast<double>(m_open);
    jsonObj["Close"] = static_cast<double>(m_close);
    jsonObj["TimeStamp"] = m_timeStamp.toString();
    jsonObj["TotalVolume"] = (qint64)m_totalVolume;
    jsonObj["DownTicks"] = (qint64)m_downTicks;
    jsonObj["DownVolume"] = (qint64)m_downVolume;
    jsonObj["OpenInterest"] = static_cast<double>(m_openInterest);
    jsonObj["IsRealtime"] = (m_flags & FLAG_IS_REALTIME) != 0;
    jsonObj["IsEndOfHistory"] = (m_flags & FLAG_IS_END_OF_HISTORY) != 0;
    jsonObj["TotalTicks"] = (qint64)m_totalTicks;
    jsonObj["UnchangedTicks"] = (qint64)m_unchangedTicks;
    jsonObj["UnchangedVolume"] = (qint64)m_unchangedVolume;
    jsonObj["UpTicks"] = (qint64)m_upTicks;
    jsonObj["UpVolume"] = (qint64)m_upVolume;
    jsonObj["Epoch"] = m_epoch;
    jsonObj["BarStatus"] = barStatusToString(getBarStatus());

    QJsonDocument doc(jsonObj);
    return QString(doc.toJson(QJsonDocument::Indented));
}
