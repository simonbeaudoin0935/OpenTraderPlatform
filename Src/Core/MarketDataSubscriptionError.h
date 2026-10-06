#pragma once

#include <QMetaType>
#include <QString>

struct MarketDataSubscriptionError
{
    QString symbol;
    QString feed;
    QString reason;
    QString message;
    bool terminal = false;
    int retryDelayMs = 0;
};

Q_DECLARE_METATYPE(MarketDataSubscriptionError)
