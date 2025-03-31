#ifndef MAINALGO_H
#define MAINALGO_H

#include <QLoggingCategory>
#include <QObject>
#include <QFile>


#include "FMPClient.h"
#include "Filters/StockNewsFilter.h"

// Define the logging category
Q_DECLARE_LOGGING_CATEGORY(MainAlgoLog)


class QThread;

class MainAlgo : public QObject
{
    Q_OBJECT
public:
    MainAlgo();

    void start();

private slots:
    void onThreadStarted();

    void onSharesFloatReceived(struct FMPClient::SharesFloatResult result);
    void onStockNewsReceived(QVector<StockNewsResult> results);

private:
    void loadCriterias();

    void fetchSyncNewsStockScreenedByFloat();
    void fetchAsyncNewsStockScreenedByFloat();

    void processReceivedNews();
    void processReceivedNewsPingPongBuffers();

    void alternateNewsPerSymbolPingPong();

    QThread *thread;

    // Criterias
    double PriceRangeLow;
    double PriceRangeHigh;
    unsigned long long PreferedFloat;
    unsigned long long MaxFloat;
    double RelativeVolume;
    double GapPercentage;


    // Variables for the algorithm :
    QVector<CompanyScreenerResult> screeningResults;
    QVector<QString> symbolsScreenedByFloat;

    QMap<QString, QVector<StockNewsResult>> latestNewsPerSymbolPingPong1;
    QMap<QString, QVector<StockNewsResult>> latestNewsPerSymbolPingPong2;

    QMap<QString, QVector<StockNewsResult>> *latestNewsPerSymbolPingPongPtr;

    QTextStream *out;
    QFile *file;
};

#endif // MAINALGO_H
