#ifndef MAINALGO_H
#define MAINALGO_H

#include <QLoggingCategory>
#include <QObject>

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
    QMap<QString, QVector<StockNewsResult>> latestNewsPerSymbol;
};

#endif // MAINALGO_H
