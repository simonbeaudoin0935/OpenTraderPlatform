#include "StockScreener.h"
#include "Misc/Settings.h"

Q_LOGGING_CATEGORY(StockScreenerLog, "StockScreener")

StockScreener::StockScreener(QObject *parent)
    : QObject(parent)
{

}

void StockScreener::start()
{
    FMPClient& client = FMPClient::getInstance();

    connect(&client, &FMPClient::sharesFloatReceived, this, &StockScreener::onSharesFloatReceived);

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


    // Execute the screener SYNChronously
    {
        bool success = client.fetchSyncCompanyScreener(filter, initialScreeningResults);
        qCDebug(StockScreenerLog) << "Fetched " << initialScreeningResults.size() << " stocks with company screener";
        Q_ASSERT(success);
    }

    qCDebug(StockScreenerLog) << "Results of the screening :";

    for (CompanyScreenerResult &result : initialScreeningResults) {
        qCDebug(StockScreenerLog) << result.toJsonString();
    }

    qCDebug(StockScreenerLog) << "Fetching the float for all of them. Waiting for all the float values to be received ";

    for (CompanyScreenerResult &result : initialScreeningResults) {
        client.fetchAsyncSharesFloat(result.getSymbol());
    }
}

void StockScreener::loadCriterias()
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



void StockScreener::onSharesFloatReceived(struct FMPClient::SharesFloatResult result)
{
    static size_t numFloatReceived = 0;

    numFloatReceived++;

    if (result.floatShares <= (signed) MaxFloat) {
        qCDebug(StockScreenerLog) << "Company : " << result.symbol << " has a float acceptable of : " << result.floatShares;

        symbolsScreenedByFloat.append(result.symbol);
    }

    if (numFloatReceived == (unsigned) initialScreeningResults.size()) {
        qCDebug(StockScreenerLog) << "Received the float for all " << initialScreeningResults.size() << " companies. Proceeding";
        qCDebug(StockScreenerLog) << "Added " << symbolsScreenedByFloat.size() << " symbols to symbolsScreenedByFloat. Proceeding";

        numFloatReceived = 0; // Reset for future use

        // IMPORTANT launch the fetching of the news, which will be the initial kick
        // to the loop of fetching
        emit finished(symbolsScreenedByFloat);
    }
}
