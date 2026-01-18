#include "MarketHours.h"

const QTimeZone MarketHours::nyZone = QTimeZone("America/New_York");


QDateTime MarketHours::toNewYorkTime(const QDateTime& localTime)
{
    return localTime.toTimeZone(nyZone);
}

MarketHours::Session MarketHours::getCurrentSession()
{
    return getSessionForDateTime(QDateTime::currentDateTime());
}

MarketHours::Session MarketHours::getSessionForDateTime(const QDateTime& localTime)
{
    QDateTime nyTime = toNewYorkTime(localTime);
    QTime time = nyTime.time();

    // Check if it's a weekday
    if (nyTime.date().dayOfWeek() >= 1 && nyTime.date().dayOfWeek() <= 5)
    {
        // Pre-market: 4:00 AM - 9:30 AM ET
        if (time >= QTime(4, 0) && time < QTime(9, 30))
        {
            return Session::PreMarket;
        }
        // Regular hours: 9:30 AM - 4:00 PM ET
        else if (time >= QTime(9, 30) && time < QTime(16, 0))
        {
            return Session::RegularHours;
        }
        // After hours: 4:00 PM - 8:00 PM ET
        else if (time >= QTime(16, 0) && time < QTime(20, 0))
        {
            return Session::AfterHours;
        }
    }

    // Closed: 8:00 PM - 4:00 AM ET and weekends
    return Session::Closed;
}

bool MarketHours::isPreMarket(const QDateTime& localTime)
{
    return getSessionForDateTime(localTime) == Session::PreMarket;
}

bool MarketHours::isRegularHours(const QDateTime& localTime)
{
    return getSessionForDateTime(localTime) == Session::RegularHours;
}

bool MarketHours::isAfterHours(const QDateTime& localTime)
{
    return getSessionForDateTime(localTime) == Session::AfterHours;
}

bool MarketHours::isClosed(const QDateTime& localTime)
{
    return getSessionForDateTime(localTime) == Session::Closed;
}

QDateTime MarketHours::getNextSessionChange(const QDateTime& localTime)
{
    QDateTime nyTime = toNewYorkTime(localTime);
    QDateTime nextChange = nyTime;

    // If it's weekend, next change is Monday 4 AM ET
    if (nyTime.date().dayOfWeek() > 5)
    {
        nextChange.setDate(nyTime.date().addDays(8 - nyTime.date().dayOfWeek()));
        nextChange.setTime(QTime(4, 0));
        return nextChange.toTimeZone(localTime.timeZone());
    }

    QTime time = nyTime.time();

    if (time < QTime(4, 0))
    {
        // Before pre-market, next change is 4 AM
        nextChange.setTime(QTime(4, 0));
    }
    else if (time < QTime(9, 30))
    {
        // During pre-market, next change is 9:30 AM
        nextChange.setTime(QTime(9, 30));
    }
    else if (time < QTime(16, 0))
    {
        // During regular hours, next change is 4 PM
        nextChange.setTime(QTime(16, 0));
    }
    else if (time < QTime(20, 0))
    {
        // During after hours, next change is 8 PM
        nextChange.setTime(QTime(20, 0));
    }
    else
    {
        // After 8 PM, next change is 4 AM next day
        nextChange = nextChange.addDays(1);
        nextChange.setTime(QTime(4, 0));
    }

    // If next change would be on weekend, move to Monday 4 AM
    if (nextChange.date().dayOfWeek() > 5)
    {
        nextChange.setDate(nextChange.date().addDays(8 - nextChange.date().dayOfWeek()));
        nextChange.setTime(QTime(4, 0));
    }

    return nextChange.toTimeZone(localTime.timeZone());
}

QString MarketHours::getFormattedTimeET(const QDateTime& localTime)
{
    QDateTime nyTime = toNewYorkTime(localTime);
    return nyTime.toString("hh:mm:ss AP") + " ET";
}

QColor MarketHours::getSessionColor(Session session)
{
    switch (session)
    {
    case Session::PreMarket:
        return getPreMarketColor();
    case Session::RegularHours:
        return getRegularHoursColor();
    case Session::AfterHours:
        return getAfterHoursColor();
    case Session::Closed:
        return getClosedColor();
    default:
        return QColor(0, 0, 0, 0); // Transparent
    }
}

QColor MarketHours::getPreMarketColor()
{
    return QColor(255, 200, 150,
                  100); // Light orange
}

QColor MarketHours::getRegularHoursColor()
{
    return QColor(0, 0, 0, 0); // Transparent
}

QColor MarketHours::getAfterHoursColor()
{
    return QColor(230, 230, 255,
                  100); // Light purple
}

QColor MarketHours::getClosedColor()
{
    return QColor(40, 40, 40, 100); // Dark gray
}

bool MarketHours::isSessionVisible(const QDateTime& startTime, const QDateTime& endTime, Session session)
{
    auto [visibleStart, visibleEnd] = getVisibleSessionRange(startTime, endTime, session);
    return visibleStart < visibleEnd;
}

QPair<QDateTime, QDateTime>
MarketHours::getVisibleSessionRange(const QDateTime& startTime, const QDateTime& endTime, Session session)
{
    QDateTime nyStartTime = toNewYorkTime(startTime);
    QDateTime nyEndTime = toNewYorkTime(endTime);

    QDateTime visibleStart = startTime;
    QDateTime visibleEnd = endTime;

    // For weekends, check if any part of the weekend is visible
    if (session == Session::Closed)
    {
        QDate currentDate = nyStartTime.date().addDays(-1); // Start from previous day
        QDate endDate = nyEndTime.date();

        while (currentDate <= endDate)
        {
            if (currentDate.dayOfWeek() > 5)
            { // Saturday or Sunday
                QDateTime dayStart = QDateTime(currentDate, QTime(0, 0), nyZone);
                QDateTime dayEnd = QDateTime(currentDate.addDays(1), QTime(0, 0), nyZone);

                if (dayStart < nyEndTime && dayEnd > nyStartTime)
                {
                    return {qMax(startTime, dayStart.toLocalTime()), qMin(endTime, dayEnd.toLocalTime())};
                }
            }
            currentDate = currentDate.addDays(1);
        }
        return {endTime, startTime}; // No visible weekend
    }

    // For weekdays, check each hour
    QDate currentDate = nyStartTime.date();
    while (currentDate <= nyEndTime.date())
    {
        if (currentDate.dayOfWeek() <= 5)
        { // Weekday
            for (int hour = 0; hour < 24; hour++)
            {
                QDateTime hourStart = QDateTime(currentDate, QTime(hour, 0), nyZone);
                QDateTime hourEnd = hour == 23 ? QDateTime(currentDate, QTime(23, 59, 59), nyZone)
                                               : QDateTime(currentDate, QTime(hour + 1, 0), nyZone);

                if (getSessionForDateTime(hourStart) == session)
                {
                    if (hourStart < nyEndTime && hourEnd > nyStartTime)
                    {
                        visibleStart = qMin(visibleStart, qMax(startTime, hourStart.toLocalTime()));
                        visibleEnd = qMax(visibleEnd, qMin(endTime, hourEnd.toLocalTime()));
                    }
                }
            }
        }
        currentDate = currentDate.addDays(1);
    }

    return {visibleStart, visibleEnd};
}
