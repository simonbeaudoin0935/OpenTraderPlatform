#include "BarCache.h"

BarCache::BarCache(const QString &symbol, bool isStreaming, QObject *parent):
    QObject(parent),
    symbol(symbol),
    isStreaming(isStreaming)
{}

const Bar *BarCache::getBar(QDateTime dateTime) const
{
    Bar* bar = nullptr;

    Q_ASSERT(0);

    return bar;
}
