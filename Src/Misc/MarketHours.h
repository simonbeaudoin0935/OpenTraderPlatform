#pragma once

#include <QDateTime>
#include <QTimeZone>
#include <QColor>

/**
 * @brief Utility class to handle market hours in Eastern Time (ET)
 * 
 * All times are handled in Eastern Time (ET) and automatically adjust for
 * Daylight Saving Time (DST). Market hours are:
 * - Pre-Market:     4:00 AM - 9:30 AM ET
 * - Regular Hours:  9:30 AM - 4:00 PM ET
 * - After Hours:    4:00 PM - 8:00 PM ET
 * - Closed:         8:00 PM - 4:00 AM ET and weekends
 * 
 * DST begins on the second Sunday in March at 2:00 AM
 * DST ends on the first Sunday in November at 2:00 AM
 */
class MarketHours {
public:
    // Market session types
    enum class Session {
        PreMarket,      // 4:00 AM - 9:30 AM ET
        RegularHours,   // 9:30 AM - 4:00 PM ET
        AfterHours,     // 4:00 PM - 8:00 PM ET
        Closed          // 8:00 PM - 4:00 AM ET
    };

    // Static methods to check market status
    static Session getCurrentSession();
    static Session getSessionForDateTime(const QDateTime& localTime);

    // Individual session checks
    static bool isPreMarket(const QDateTime& localTime = QDateTime::currentDateTime());
    static bool isRegularHours(const QDateTime& localTime = QDateTime::currentDateTime());
    static bool isAfterHours(const QDateTime& localTime = QDateTime::currentDateTime());
    static bool isClosed(const QDateTime& localTime = QDateTime::currentDateTime());

    // Get next session change time
    static QDateTime getNextSessionChange(const QDateTime& localTime = QDateTime::currentDateTime());

    // Convert datetime to New York time
    static QDateTime toNewYorkTime(const QDateTime& dateTime);

    // Get formatted time string in ET
    static QString getFormattedTimeET(const QDateTime& localTime = QDateTime::currentDateTime());

    // DST-related methods
    static bool isDST(const QDateTime& localTime = QDateTime::currentDateTime());
    static int getUTCOffset(const QDateTime& localTime = QDateTime::currentDateTime());
    static QString getTimeZoneAbbreviation(const QDateTime& localTime = QDateTime::currentDateTime());

    // Background color methods
    static QColor getSessionColor(Session session);
    static QColor getPreMarketColor();
    static QColor getRegularHoursColor();
    static QColor getAfterHoursColor();
    static QColor getClosedColor();

    // Session visibility methods
    static bool isSessionVisible(const QDateTime& startTime, const QDateTime& endTime, Session session);
    static QPair<QDateTime, QDateTime> getVisibleSessionRange(const QDateTime& startTime, const QDateTime& endTime, Session session);

private:
    static const QTimeZone nyZone;
};
