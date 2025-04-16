#include "StockRunUpDetector.h"
#include <QTimeZone>

StockRunUpDetector::StockRunUpDetector(const QString &symbol, QObject *parent) :
    QObject{parent},
    barCache(symbol)

{}

void StockRunUpDetector::start(QDate startDate, qsizetype runUpWindowWidth)
{
    Q_ASSERT(startDate.dayOfWeek() <= 5);

    this->startDate = startDate;
    this->runUpWindowWidth = runUpWindowWidth;;

    QTimeZone newYorkTimeZone("America/New_York");

    QTime _6AM(6,0);
    QTime toTime = _6AM.addSecs(60 * (runUpWindowWidth - 1));

    QDateTime fromDate = QDateTime(startDate, _6AM,   newYorkTimeZone);
    QDateTime toDate   = QDateTime(startDate, toTime, newYorkTimeZone);

    QVector<Bar> bars = barCache.getBars(fromDate, toDate);

    Q_ASSERT(bars.size() == runUpWindowWidth);

    for(auto &bar : bars) {
        deque.enqueue(bar);
        qDebug().noquote() << bar.toJsonString();
    }

    timeLastBarEnqued = bars.last().getTimeStamp().toTimeZone(newYorkTimeZone).time();

    //computeStatsOnLastAfterMarket();
}


void StockRunUpDetector::computeStatsOnLastAfterMarket()
{
    yesterday = startDate.addDays(-1);

    while (yesterday.dayOfWeek() > 5) yesterday = yesterday.addDays(-1);

    QVector<Bar> bars = barCache.getAfterHourBars(yesterday);

    struct stats st;

    for (const Bar& bar: bars) {
        if (bar.getBarStatus() == Bar::BarStatus::Void) continue; // A bar where there has been zero activity

        st.nonVoidBars++;
        st.averagePriceChangePerBar += (qAbs<double>(bar.getClose() - bar.getOpen()) / bars.size());
        st.averageVolumePerBar      += (double)(bar.getTotalVolume()) / bars.size();
        st.maxPriceChange = qMax(st.maxPriceChange, qAbs<double>(bar.getOpen() - bar.getClose()));
        st.maxVolumeChange = qMax(st.maxVolumeChange, (double) bar.getTotalVolume() );
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

void StockRunUpDetector::computeNextCandle()
{

}
