
#include "BarCache.h"

BarCache::BarCache(QString &symbol, QObject *parent):
    QObject(parent),
    symbol(symbol)
{}

const Bar *BarCache::getBar(QDateTime dateTime) const
{
    Bar* bar = nullptr;

    Q_ASSERT(0);

    return bar;
}
