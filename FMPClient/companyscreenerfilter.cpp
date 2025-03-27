#include "companyscreenerfilter.h"

CompanyScreenerFilter::CompanyScreenerFilter() {
    // Default constructor leaves all fields unset (std::nullopt)
}

// Setters (unchanged, omitted for brevity)
void CompanyScreenerFilter::setMarketCapMoreThan(std::optional<long long> value) { marketCapMoreThan = value; }
void CompanyScreenerFilter::setMarketCapLowerThan(std::optional<long long> value) { marketCapLowerThan = value; }
void CompanyScreenerFilter::setSector(std::optional<std::string> value) { sector = value; }
void CompanyScreenerFilter::setIndustry(std::optional<std::string> value) { industry = value; }
void CompanyScreenerFilter::setBetaMoreThan(std::optional<double> value) { betaMoreThan = value; }
void CompanyScreenerFilter::setBetaLowerThan(std::optional<double> value) { betaLowerThan = value; }
void CompanyScreenerFilter::setPriceMoreThan(std::optional<double> value) { priceMoreThan = value; }
void CompanyScreenerFilter::setPriceLowerThan(std::optional<double> value) { priceLowerThan = value; }
void CompanyScreenerFilter::setDividendMoreThan(std::optional<double> value) { dividendMoreThan = value; }
void CompanyScreenerFilter::setDividendLowerThan(std::optional<double> value) { dividendLowerThan = value; }
void CompanyScreenerFilter::setVolumeMoreThan(std::optional<long long> value) { volumeMoreThan = value; }
void CompanyScreenerFilter::setVolumeLowerThan(std::optional<long long> value) { volumeLowerThan = value; }
void CompanyScreenerFilter::setExchange(std::optional<std::string> value) { exchange = value; }
void CompanyScreenerFilter::setCountry(std::optional<std::string> value) { country = value; }
void CompanyScreenerFilter::setIsEtf(std::optional<bool> value) { isEtf = value; }
void CompanyScreenerFilter::setIsFund(std::optional<bool> value) { isFund = value; }
void CompanyScreenerFilter::setIsActivelyTrading(std::optional<bool> value) { isActivelyTrading = value; }
void CompanyScreenerFilter::setLimit(std::optional<int> value) { limit = value; }
void CompanyScreenerFilter::setIncludeAllShareClasses(std::optional<bool> value) { includeAllShareClasses = value; }

// Getters (unchanged, omitted for brevity)
std::optional<long long> CompanyScreenerFilter::getMarketCapMoreThan() const { return marketCapMoreThan; }
std::optional<long long> CompanyScreenerFilter::getMarketCapLowerThan() const { return marketCapLowerThan; }
std::optional<std::string> CompanyScreenerFilter::getSector() const { return sector; }
std::optional<std::string> CompanyScreenerFilter::getIndustry() const { return industry; }
std::optional<double> CompanyScreenerFilter::getBetaMoreThan() const { return betaMoreThan; }
std::optional<double> CompanyScreenerFilter::getBetaLowerThan() const { return betaLowerThan; }
std::optional<double> CompanyScreenerFilter::getPriceMoreThan() const { return priceMoreThan; }
std::optional<double> CompanyScreenerFilter::getPriceLowerThan() const { return priceLowerThan; }
std::optional<double> CompanyScreenerFilter::getDividendMoreThan() const { return dividendMoreThan; }
std::optional<double> CompanyScreenerFilter::getDividendLowerThan() const { return dividendLowerThan; }
std::optional<long long> CompanyScreenerFilter::getVolumeMoreThan() const { return volumeMoreThan; }
std::optional<long long> CompanyScreenerFilter::getVolumeLowerThan() const { return volumeLowerThan; }
std::optional<std::string> CompanyScreenerFilter::getExchange() const { return exchange; }
std::optional<std::string> CompanyScreenerFilter::getCountry() const { return country; }
std::optional<bool> CompanyScreenerFilter::getIsEtf() const { return isEtf; }
std::optional<bool> CompanyScreenerFilter::getIsFund() const { return isFund; }
std::optional<bool> CompanyScreenerFilter::getIsActivelyTrading() const { return isActivelyTrading; }
std::optional<int> CompanyScreenerFilter::getLimit() const { return limit; }
std::optional<bool> CompanyScreenerFilter::getIncludeAllShareClasses() const { return includeAllShareClasses; }

// Helper function to generate URL parameters (unchanged, included for completeness)
QString CompanyScreenerFilter::getURLParameters() const  {
    QString params;
    bool firstParam = true;

    auto appendParam = [&](const QString& key, const QString& value) {
        if (!firstParam) params.append('&');
        params.append(key + '=' + value);
        firstParam = false;
    };

    if (marketCapMoreThan.has_value()) appendParam("marketCapMoreThan", QString::number(marketCapMoreThan.value()));
    if (marketCapLowerThan.has_value()) appendParam("marketCapLowerThan", QString::number(marketCapLowerThan.value()));
    if (sector.has_value()) appendParam("sector", QString::fromStdString(sector.value()));
    if (industry.has_value()) appendParam("industry", QString::fromStdString(industry.value()));
    if (betaMoreThan.has_value()) appendParam("betaMoreThan", QString::number(betaMoreThan.value()));
    if (betaLowerThan.has_value()) appendParam("betaLowerThan", QString::number(betaLowerThan.value()));
    if (priceMoreThan.has_value()) appendParam("priceMoreThan", QString::number(priceMoreThan.value()));
    if (priceLowerThan.has_value()) appendParam("priceLowerThan", QString::number(priceLowerThan.value()));
    if (dividendMoreThan.has_value()) appendParam("dividendMoreThan", QString::number(dividendMoreThan.value()));
    if (dividendLowerThan.has_value()) appendParam("dividendLowerThan", QString::number(dividendLowerThan.value()));
    if (volumeMoreThan.has_value()) appendParam("volumeMoreThan", QString::number(volumeMoreThan.value()));
    if (volumeLowerThan.has_value()) appendParam("volumeLowerThan", QString::number(volumeLowerThan.value()));
    if (exchange.has_value()) appendParam("exchange", QString::fromStdString(exchange.value()));
    if (country.has_value()) appendParam("country", QString::fromStdString(country.value()));
    if (isEtf.has_value()) appendParam("isEtf", isEtf.value() ? "true" : "false");
    if (isFund.has_value()) appendParam("isFund", isFund.value() ? "true" : "false");
    if (isActivelyTrading.has_value()) appendParam("isActivelyTrading", isActivelyTrading.value() ? "true" : "false");
    if (limit.has_value()) appendParam("limit", QString::number(limit.value()));
    if (includeAllShareClasses.has_value()) appendParam("includeAllShareClasses", includeAllShareClasses.value() ? "true" : "false");

    return params;
}

// Implementation of CompanyScreenerResult
CompanyScreenerResult::CompanyScreenerResult(const QJsonObject& jsonObj) {
    symbol = jsonObj["symbol"].toString();
    companyName = jsonObj["companyName"].toString();
    marketCap = jsonObj["marketCap"].toVariant().toLongLong(); // JSON may use int or double, convert to long long
    sector = jsonObj["sector"].toString();
    industry = jsonObj["industry"].toString();
    beta = jsonObj["beta"].toDouble();
    price = jsonObj["price"].toDouble();
    lastAnnualDividend = jsonObj["lastAnnualDividend"].toDouble();
    volume = jsonObj["volume"].toVariant().toLongLong(); // JSON may use int or double, convert to long long
    exchange = jsonObj["exchange"].toString();
    exchangeShortName = jsonObj["exchangeShortName"].toString();
    country = jsonObj["country"].toString();
    isEtf = jsonObj["isEtf"].toBool();
    isFund = jsonObj["isFund"].toBool();
    isActivelyTrading = jsonObj["isActivelyTrading"].toBool();
}

// Getters for CompanyScreenerResult
QString CompanyScreenerResult::getSymbol() const { return symbol; }
QString CompanyScreenerResult::getCompanyName() const { return companyName; }
long long CompanyScreenerResult::getMarketCap() const { return marketCap; }
QString CompanyScreenerResult::getSector() const { return sector; }
QString CompanyScreenerResult::getIndustry() const { return industry; }
double CompanyScreenerResult::getBeta() const { return beta; }
double CompanyScreenerResult::getPrice() const { return price; }
double CompanyScreenerResult::getLastAnnualDividend() const { return lastAnnualDividend; }
long long CompanyScreenerResult::getVolume() const { return volume; }
QString CompanyScreenerResult::getExchange() const { return exchange; }
QString CompanyScreenerResult::getExchangeShortName() const { return exchangeShortName; }
QString CompanyScreenerResult::getCountry() const { return country; }
bool CompanyScreenerResult::getIsEtf() const { return isEtf; }
bool CompanyScreenerResult::getIsFund() const { return isFund; }
bool CompanyScreenerResult::getIsActivelyTrading() const { return isActivelyTrading; }
