#include "StockRunUpDetector.h"
#include <QTimeZone>

StockRunUpDetector::StockRunUpDetector(const QString &symbol, QObject *parent) :
    QObject(parent),
    NYTZ("America/New_York"),
    barCache(symbol)

{}

void StockRunUpDetector::start(QDate startDate, qsizetype runUpWindowWidth)
{
    Q_ASSERT(startDate.dayOfWeek() <= 5);

    this->startDate = startDate;
    this->runUpWindowWidth = runUpWindowWidth;;

    QTime _6AM(6,0);
    QTime _10AM(10,0);

    QTime toTime = _6AM.addSecs(60 * (runUpWindowWidth - 1));

    QDateTime fromDate = QDateTime(startDate, _6AM,   NYTZ);
    QDateTime fromDateWarm = QDateTime(startDate, _10AM,   NYTZ);

    QVector<Bar> bars = barCache.getBars(fromDate, fromDateWarm);

    QDateTime toDate   = QDateTime(startDate, toTime, NYTZ);

    bars = barCache.getBars(fromDate, toDate);

    Q_ASSERT(bars.size() == runUpWindowWidth);

    for(auto &bar : bars) {
        deque.enqueue(bar);
    }

    timestampLastBarEnqued = bars.last().getTimeStamp();

    qDebug() << "deque size : " << deque.size();
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
    QDateTime fromDate = timestampLastBarEnqued.addSecs(60);
    QDateTime toDate   = fromDate;

    QVector<Bar> bars = barCache.getBars(fromDate, toDate);

    Q_ASSERT(bars.size() == 1);

    Bar bar = bars.first();

//    qDebug().noquote() << bar.toJsonString();


    deque.enqueue(bars.first());

    deque.dequeue();

    Q_ASSERT(deque.size() == runUpWindowWidth);

    timestampLastBarEnqued = bars.first().getTimeStamp();

    detectRunUp();
}

void StockRunUpDetector::detectRunUp()
{
    const int windowBars = 5; // Lookback for price surge
    const double minPriceGainPct = 5.0; // Minimum % gain
    const int volumeLookback = 20; // For average volume
    const double minVolumeMultiple = 3.0; // Volume spike threshold

    QList<Bar> barList = deque.toList();

    Q_ASSERT(barList.size() >= (windowBars + volumeLookback));

    // Find valid bars in the window (last windowBars non-void bars)
    QList<int> validIndices;
    for (int i = barList.size() - 1; i >= 0 && validIndices.size() < windowBars; --i) {
        if (barList[i].getBarStatus() != Bar::BarStatus::Void) {
            validIndices.prepend(i); // Store indices in ascending order
        }
    }


    if (validIndices.size() < windowBars) {
        qWarning() << "Not enough valid bars";
        return; // Not enough valid bars
    }


    int startIdx = validIndices.first();
    int endIdx = validIndices.last();

    // Calculate price gain using valid bars
    double startPrice = barList[startIdx].getClose();
    double endPrice = barList[endIdx].getClose();
    double priceGainPct = ((endPrice - startPrice) / startPrice) * 100.0;

    // Calculate average volume over window (include void bars as 0 volume)
    double windowVolume = 0;
    int windowCount = 0;
    for (int i = barList.size() - windowBars; i < barList.size(); ++i) {
        windowVolume += barList[i].getTotalVolume(); // Void bars contribute 0
        windowCount++;
    }
    windowVolume = windowCount > 0 ? windowVolume / windowCount : 0;

    // Calculate historical average volume (include void bars)
    double historicalVolume = 0;
    int volStartIdx = qMax(0, barList.size() - windowBars - volumeLookback);
    int volCount = barList.size() - windowBars - volStartIdx;
    for (int i = volStartIdx; i < barList.size() - windowBars; ++i) {
        historicalVolume += barList[i].getTotalVolume(); // Void bars contribute 0
    }
    historicalVolume = volCount > 0 ? historicalVolume / volCount : windowVolume;

    // Check for run-up conditions
    bool isPriceSurge = priceGainPct >= minPriceGainPct;
    bool isVolumeSpike = windowVolume >= historicalVolume * minVolumeMultiple;

    if (isPriceSurge && isVolumeSpike) {
        qDebug() << "Stock Run-Up Detected! Time : " << timestampLastBarEnqued.time() << " Gain:" << priceGainPct << "%, Volume:" << windowVolume;

        // Optional: RSI filter
        double rsi = calculateRSI(windowBars);
        if (rsi > 60.0) {
            qDebug() << "Additional RSI trigger : " << rsi;
            //executeTrade();
        }
    }
}

double StockRunUpDetector::calculateRSI(int period) {
    QList<Bar> barList = deque.toList();
    if (barList.size() < period + 1) {
        return 0.0;
    }

    // Collect valid bars for RSI
    QList<Bar> validBars;
    for (int i = barList.size() - 1; validBars.size() < period + 1 && i >= 0; --i) {
        if (barList[i].getBarStatus() != Bar::BarStatus::Void) {
            validBars.prepend(barList[i]);
        }
    }

    if (validBars.size() < period + 1) {
        return 0.0; // Not enough valid bars
    }

    double avgGain = 0, avgLoss = 0;
    int count = 0;

    for (int i = 1; i < validBars.size(); ++i) {
        double change = validBars[i].getClose() - validBars[i-1].getClose();
        if (change > 0) {
            avgGain += change;
        } else {
            avgLoss += -change;
        }
        count++;
    }

    avgGain /= count;
    avgLoss /= count;

    if (avgLoss == 0) {
        return 100.0; // Avoid division by zero
    }

    double rs = avgGain / avgLoss;
    return 100.0 - (100.0 / (1.0 + rs));
}
