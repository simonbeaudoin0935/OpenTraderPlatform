#include "StockRunUpDetector.h"
#include <QTimeZone>

StockRunUpDetector::StockRunUpDetector(const QString &symbol, QObject *parent) :
    QObject{parent},
    barCache(symbol)

{}

void StockRunUpDetector::start(QDate startDate)
{
    Q_ASSERT(startDate.dayOfWeek() <= 5);

    this->startDate = startDate;

    computeStatsOnLastAfterMarket();
}

void StockRunUpDetector::computeStatsOnLastAfterMarket()
{
    yesterday = startDate.addDays(-1);

    while (yesterday.dayOfWeek() > 5) yesterday = yesterday.addDays(-1);

    QVector<Bar> bars = barCache.getAfterHourBars(yesterday);

    struct stats st;

    for (const Bar& bar: bars) {
        if (bar.getBarStatus() == "void") continue; // A bar where there has been zero activity

        st.nonVoidBars++;
        st.averagePriceChangePerBar += (qAbs<double>(bar.getClose().toDouble() - bar.getOpen().toDouble()) / bars.size());
        st.averageVolumePerBar      += (qAbs<double>(bar.getTotalVolume().toDouble()) / bars.size());
        st.maxPriceChange = (qMax(st.maxPriceChange, qAbs<double>(bar.getOpen().toDouble() - bar.getClose().toDouble())));
        st.maxVolumeChange = (qMax(st.maxVolumeChange, qAbs<double>(bar.getTotalVolume().toDouble())));
    }

    st.voidBarsRatio = (((double)st.nonVoidBars) / ((double) bars.size()));

    afterMarketStats = st;

    qInfo().noquote() << "Symbol : " << barCache.getSymbol() <<
        "\n  avgPriceChangePerBar : " << st.averagePriceChangePerBar <<
        "\n  avgVolumePerBar      : " << st.averageVolumePerBar <<
        "\n  maxPriceChange       : " << st.maxPriceChange <<
        "\n  maxVolumeChange      : " << st.maxVolumeChange <<
        "\n  ratio of void bars   : " << st.voidBarsRatio;
}
