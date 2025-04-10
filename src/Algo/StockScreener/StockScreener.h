#pragma once

#include <QObject>
#include <QLoggingCategory>

#include "FMPClient.h"

Q_DECLARE_LOGGING_CATEGORY(StockScreenerLog)

class StockScreener : public QObject
{
    Q_OBJECT
public:
    explicit StockScreener(QObject *parent = nullptr);

    void start();

    QVector<QString>& getStockScreeningResult() {return symbolsScreenedByFloat;};

signals:

    void finished(QVector<QString> watchlist);

private slots:
    void onSharesFloatReceived(struct FMPClient::SharesFloatResult result);

private:
    void loadCriterias();

    // Criterias
    double PriceRangeLow;
    double PriceRangeHigh;
    unsigned long long PreferedFloat;
    unsigned long long MaxFloat;
    double RelativeVolume;
    double GapPercentage;

    QVector<CompanyScreenerResult> initialScreeningResults;
    QVector<QString> symbolsScreenedByFloat;

};
