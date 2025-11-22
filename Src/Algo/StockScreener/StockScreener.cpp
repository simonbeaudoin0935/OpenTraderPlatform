#include <QFile>

#include "StockScreener.h"
#include "Settings.h"

Q_LOGGING_CATEGORY(StockScreenerLog, "StockScreener")

StockScreener::StockScreener(QObject *parent)
    : QObject(parent)
{

}

void StockScreener::start()
{
    FMPClient& client = FMPClient::getInstance();

    bool connection = connect(&client, &FMPClient::sharesFloatReceived, this, &StockScreener::onSharesFloatReceived, Qt::UniqueConnection);
    Q_ASSERT_X(connection, "StockScreener::start", "Failed to create unique connection for sharesFloatReceived");

    loadCriterias();


    CompanyScreenerFilter filter;

    // Set criterias
    {
        filter.setIndustry("Biotechnology");
        filter.setPriceMoreThan(PriceRangeLow);
        filter.setPriceLowerThan(PriceRangeHigh);
        filter.setExchange("NASDAQ");
        filter.setIsActivelyTrading(true);
        filter.setIsEtf(false);
        filter.setIsFund(false);
        filter.setCountry("US");
        filter.setVolumeMoreThan(10000); // TODO shiznit
    }

    QString fileName = QString("Biotechnology_screener_results_%1.dat").arg(QDate::currentDate().toString("yyyyMMdd"));

    // Check if cache file exists
    if (QFile::exists(fileName) && CompanyScreenerResult::loadScreenerResults(biotechScreeningResults, fileName)) {
        qDebug() << "Successfully loaded cached data";
    } else {
        qDebug() << "No cache file found for" << fileName << ", fetching fresh data";

        // Execute the screener SYNChronously
        {
            bool success = client.fetchSyncCompanyScreener(filter, biotechScreeningResults);
            qCDebug(StockScreenerLog) << "Fetched " << biotechScreeningResults.size() << " stocks with company screener";
            Q_ASSERT(success);
        }

        CompanyScreenerResult::saveScreenerResults(biotechScreeningResults, fileName);
    }

    qCDebug(StockScreenerLog) << "Results of the screening :";

    for (CompanyScreenerResult &result : biotechScreeningResults) {
        qCDebug(StockScreenerLog) << result.toJsonString();
    }

    fileName = QString("HealthTechnology_screener_results_%1.dat").arg(QDate::currentDate().toString("yyyyMMdd"));


    filter.setIndustry("Health Technology");


    // Check if cache file exists
    if (QFile::exists(fileName) && CompanyScreenerResult::loadScreenerResults(healthtechnologyScreeningResults, fileName)) {
        qDebug() << "Successfully loaded cached data";
    } else {
        qDebug() << "No cache file found for" << fileName << ", fetching fresh data";

        // Execute the screener SYNChronously
        {
            bool success = client.fetchSyncCompanyScreener(filter, healthtechnologyScreeningResults);
            qCDebug(StockScreenerLog) << "Fetched " << healthtechnologyScreeningResults.size() << " stocks with company screener";
            Q_ASSERT(success);
        }

        CompanyScreenerResult::saveScreenerResults(healthtechnologyScreeningResults, fileName);
    }

    qCDebug(StockScreenerLog) << "Results of the screening :";

    for (CompanyScreenerResult &result : healthtechnologyScreeningResults) {
        qCDebug(StockScreenerLog) << result.toJsonString();
    }

/*
    qCDebug(StockScreenerLog) << "Fetching the float for all of them. Waiting for all the float values to be received ";

    for (CompanyScreenerResult &result : initialScreeningResults) {
        client.fetchAsyncSharesFloat(result.getSymbol());
    }
*/
}

void StockScreener::loadCriterias()
{
    PriceRangeLow = criteriaSettings->value("Criterias/PriceRangeLow",0.0).toDouble();
    if (PriceRangeLow <= 0.0) {
        qFatal("Invalid or missing Criterias/PriceRangeLow configuration");
    }

    PriceRangeHigh = criteriaSettings->value("Criterias/PriceRangeHigh",0.0).toDouble();
    if (PriceRangeHigh <= 0.0) {
        qFatal("Invalid or missing Criterias/PriceRangeHigh configuration");
    }

    PreferedFloat = criteriaSettings->value("Criterias/PreferedFloat",0).toULongLong();
    if (PreferedFloat == 0) {
        qFatal("Invalid or missing Criterias/PreferedFloat configuration");
    }

    MaxFloat = criteriaSettings->value("Criterias/MaxFloat",0).toULongLong();
    if (MaxFloat == 0) {
        qFatal("Invalid or missing Criterias/MaxFloat configuration");
    }

    RelativeVolume = criteriaSettings->value("Criterias/RelativeVolume",0.0).toDouble();
    if (RelativeVolume <= 0.0) {
        qFatal("Invalid or missing Criterias/RelativeVolume configuration");
    }

    GapPercentage = criteriaSettings->value("Criterias/GapPercentage",0.0).toDouble();
    if (GapPercentage <= 0.0) {
        qFatal("Invalid or missing Criterias/GapPercentage configuration");
    }
}



void StockScreener::onSharesFloatReceived(struct FMPClient::SharesFloatResult result)
{
    static size_t numFloatReceived = 0;

    numFloatReceived++;

    if (result.floatShares <= (signed) MaxFloat) {
        qCDebug(StockScreenerLog) << "Company : " << result.symbol << " has a float acceptable of : " << result.floatShares;

        symbolsScreenedByFloat.append(result.symbol);
    }

    if (numFloatReceived == (unsigned) biotechScreeningResults.size()) {
        qCDebug(StockScreenerLog) << "Received the float for all " << biotechScreeningResults.size() << " companies. Proceeding";
        qCDebug(StockScreenerLog) << "Added " << symbolsScreenedByFloat.size() << " symbols to symbolsScreenedByFloat. Proceeding";

        numFloatReceived = 0; // Reset for future use

        // IMPORTANT launch the fetching of the news, which will be the initial kick
        // to the loop of fetching
        emit finished(symbolsScreenedByFloat);
    }
}
