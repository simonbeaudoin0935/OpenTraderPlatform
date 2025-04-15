#include "StockRunUpDetector.h"
#include <QTimeZone>

StockRunUpDetector::StockRunUpDetector(QObject *parent)
    : QObject{parent}
{}

void StockRunUpDetector::start(QVector<QString> &watchlist)
{
    for (const QString &symbol: watchlist) {
        watchlistBarCache.insert(symbol, new BarCache(symbol));
    }
}

void StockRunUpDetector::computeStatsOnLastAfterMarket()
{


    for (BarCache *cache : watchlistBarCache) {

        QVector<Bar> bars = cache->getPreviousDayAfterHourBars();

        struct stats st;

        for (const Bar& bar: bars) {
            st.averagePriceChangePerBar += qAbs<double>(bar.getHigh().toDouble() - bar.getClose().toDouble()) / bars.size();
            st.averageVolumePerBar      += qAbs<double>(bar.getTotalVolume().toDouble()) / bars.size();
            st.maxPriceChange = qMax(st.maxPriceChange, qAbs<double>(bar.getHigh().toDouble() - bar.getClose().toDouble()));
            st.maxVolumeChange = qMax(st.maxVolumeChange, qAbs<double>(bar.getTotalVolume().toDouble()));

            statsPerSymbol[cache->getSymbol()] = st;
        }

        qDebug().noquote() << "Symbol : " << cache->getSymbol() <<
            "\n  avgPriceChangePerBar : " << st.averagePriceChangePerBar <<
            "\n  avgVolumePerBar      : " << st.averageVolumePerBar <<
            "\n  maxPriceChange       : " << st.maxPriceChange <<
            "\n  maxVolumeChange      : " << st.maxVolumeChange;
    }
}
