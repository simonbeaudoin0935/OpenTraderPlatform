#include "Bar.h"

#include <QJsonDocument>
#include <QJsonObject>

Bar::Bar(QDateTime ts, float o, float h, float l, float c, quint64 vol, BarStatus status)
    : m_timeStamp(ts), m_totalVolume(vol), m_open(o), m_high(h), m_low(l), m_close(c), m_barStatus(status)
{
}

Bar Bar::nullBar(QDateTime dateTime)
{
    Bar bar;
    bar.m_timeStamp = dateTime;
    bar.m_barStatus = BarStatus::Null;
    return bar;
}

Bar::BarStatus Bar::barStatusFromString(const QString& barStatus)
{
    if (barStatus == "Open")
        return BarStatus::Open;
    if (barStatus == "Closed")
        return BarStatus::Closed;
    if (barStatus == "Null")
        return BarStatus::Null;
    if (barStatus == "Uninitialized")
        return BarStatus::Uninitialized;
    Q_UNREACHABLE();
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

bool Bar::operator==(const Bar& other) const
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    return m_timeStamp == other.m_timeStamp && m_open == other.m_open && m_high == other.m_high &&
           m_low == other.m_low && m_close == other.m_close && m_totalVolume == other.m_totalVolume &&
           m_barStatus == other.m_barStatus;
#pragma GCC diagnostic pop
}

bool Bar::isValid() const
{
    return true;
}

QString Bar::toJsonString() const
{
    QJsonObject obj;
    obj["Open"] = static_cast<double>(m_open);
    obj["High"] = static_cast<double>(m_high);
    obj["Low"] = static_cast<double>(m_low);
    obj["Close"] = static_cast<double>(m_close);
    obj["TimeStamp"] = m_timeStamp.toString(Qt::ISODate);
    obj["TotalVolume"] = static_cast<qint64>(m_totalVolume);
    obj["BarStatus"] = barStatusToString(m_barStatus);
    return QString(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

QDebug operator<<(QDebug debug, const Bar& bar)
{
    debug << bar.getTimeStamp().toString(Qt::ISODate) << "O:" << bar.getOpen() << "H:" << bar.getHigh()
          << "L:" << bar.getLow() << "C:" << bar.getClose() << "V:" << bar.getTotalVolume()
          << "Status:" << Bar::barStatusToString(bar.getBarStatus());
    return debug;
}
