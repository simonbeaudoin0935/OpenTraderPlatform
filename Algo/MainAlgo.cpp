#include <QThread>

#include "MainAlgo.h"
#include "../Main/Settings.h"
#include "../Clients/FMPClient/FMPClient.h"

Q_LOGGING_CATEGORY(MainAlgoLog, "MainAlgo")

MainAlgo::MainAlgo() :
    thread(new QThread())
{
    thread->setObjectName("MainAlgoThread");

    this->moveToThread(thread);

    connect(thread, &QThread::started, this, &MainAlgo::onThreadStarted);

    loadCriterias();

    thread->start();
}

void MainAlgo::start()
{
    thread->start();
}

void MainAlgo::loadCriterias()
{
    PriceRangeLow = criteriaSettings->value("Criterias/PriceRangeLow",0.0).toDouble();
    if (PriceRangeLow <= 0.0) {
        qFatal() << "Criterias/PriceRangeLow";
    }

    PriceRangeHigh = criteriaSettings->value("Criterias/PriceRangeHigh",0.0).toDouble();
    if (PriceRangeHigh <= 0.0) {
        qFatal() << "Criterias/PriceRangeHigh";
    }

    PreferedFloat = criteriaSettings->value("Criterias/PreferedFloat",0).toULongLong();
    if (PreferedFloat == 0) {
        qFatal() << "Criterias/PreferedFloat";
    }

    MaxFloat = criteriaSettings->value("Criterias/MaxFloat",0).toULongLong();
    if (MaxFloat == 0) {
        qFatal() << "Criterias/MaxFloat";
    }

    RelativeVolume = criteriaSettings->value("Criterias/RelativeVolume",0.0).toDouble();
    if (RelativeVolume <= 0.0) {
        qFatal() << "Criterias/RelativeVolume";
    }

    GapPercentage = criteriaSettings->value("Criterias/GapPercentage",0.0).toDouble();
    if (GapPercentage <= 0.0) {
        qFatal() << "Criterias/GapPercentage";
    }
}

void MainAlgo::onThreadStarted()
{
    FMPClient& client = FMPClient::getInstance();

    QVector<CompanyScreenerResult> results;
    CompanyScreenerFilter filter;

    // TODO turn this off for now
    return;

    filter.setIndustry("Biotechnology");
    filter.setPriceMoreThan(PriceRangeLow);
    filter.setPriceLowerThan(PriceRangeHigh);
    filter.setExchange("NASDAQ");
    filter.setIsActivelyTrading(true);
    filter.setIsEtf(false);
    filter.setIsFund(false);
    filter.setCountry("US");
    filter.setVolumeMoreThan(10000); // TODO shiznit

    bool success = client.fetchSyncCompanyScreener(filter, results);

    qCDebug(MainAlgoLog) << "Fetched " << results.size() << " stocks with company screener";
    Q_ASSERT(success);

    for (CompanyScreenerResult &result : results) {
        qCDebug(MainAlgoLog) << result.toJsonString();
    }

    qCDebug(MainAlgoLog) << "\n\n\nOut of those, these have a good float :";

    QVector<CompanyScreenerResult> screened_results_by_float;

    for (CompanyScreenerResult &CSresult : results) {
        struct FMPClient::SharesFloatResult floatResult;

        success = client.fetchSyncSharesFloat(CSresult.getSymbol(), floatResult);

        Q_ASSERT(success);

        if (floatResult.floatShares < (qint64) PreferedFloat) {
            screened_results_by_float.append(CSresult);

            qCDebug(MainAlgoLog) << "Float=" << floatResult.floatShares << " : " << CSresult.toJsonString();
        }
    }

}
