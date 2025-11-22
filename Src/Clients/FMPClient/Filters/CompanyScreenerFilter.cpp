#include <QJsonDocument>
#include <QFile>

#include "CompanyScreenerFilter.h"

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

// Helper function to generate URL query
QUrlQuery CompanyScreenerFilter::toUrlQuery() const {
    QUrlQuery query;

    if (marketCapMoreThan.has_value()) query.addQueryItem("marketCapMoreThan", QString::number(marketCapMoreThan.value()));
    if (marketCapLowerThan.has_value()) query.addQueryItem("marketCapLowerThan", QString::number(marketCapLowerThan.value()));
    if (sector.has_value()) query.addQueryItem("sector", QString::fromStdString(sector.value()));
    if (industry.has_value()) query.addQueryItem("industry", QString::fromStdString(industry.value()));
    if (betaMoreThan.has_value()) query.addQueryItem("betaMoreThan", QString::number(betaMoreThan.value()));
    if (betaLowerThan.has_value()) query.addQueryItem("betaLowerThan", QString::number(betaLowerThan.value()));
    if (priceMoreThan.has_value()) query.addQueryItem("priceMoreThan", QString::number(priceMoreThan.value()));
    if (priceLowerThan.has_value()) query.addQueryItem("priceLowerThan", QString::number(priceLowerThan.value()));
    if (dividendMoreThan.has_value()) query.addQueryItem("dividendMoreThan", QString::number(dividendMoreThan.value()));
    if (dividendLowerThan.has_value()) query.addQueryItem("dividendLowerThan", QString::number(dividendLowerThan.value()));
    if (volumeMoreThan.has_value()) query.addQueryItem("volumeMoreThan", QString::number(volumeMoreThan.value()));
    if (volumeLowerThan.has_value()) query.addQueryItem("volumeLowerThan", QString::number(volumeLowerThan.value()));
    if (exchange.has_value()) query.addQueryItem("exchange", QString::fromStdString(exchange.value()));
    if (country.has_value()) query.addQueryItem("country", QString::fromStdString(country.value()));
    if (isEtf.has_value()) query.addQueryItem("isEtf", isEtf.value() ? "true" : "false");
    if (isFund.has_value()) query.addQueryItem("isFund", isFund.value() ? "true" : "false");
    if (isActivelyTrading.has_value()) query.addQueryItem("isActivelyTrading", isActivelyTrading.value() ? "true" : "false");
    if (limit.has_value()) query.addQueryItem("limit", QString::number(limit.value()));
    if (includeAllShareClasses.has_value()) query.addQueryItem("includeAllShareClasses", includeAllShareClasses.value() ? "true" : "false");

    return query;
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

QString CompanyScreenerResult::toJsonString() const {
    // Construct a QJsonObject from the member variables
    QJsonObject jsonObj;
    jsonObj["symbol"] = symbol;
    jsonObj["companyName"] = companyName;
    jsonObj["marketCap"] = static_cast<qint64>(marketCap); // Use qint64 for long long
    jsonObj["sector"] = sector;
    jsonObj["industry"] = industry;
    jsonObj["beta"] = beta;
    jsonObj["price"] = price;
    jsonObj["lastAnnualDividend"] = lastAnnualDividend;
    jsonObj["volume"] = static_cast<qint64>(volume); // Use qint64 for long long
    jsonObj["exchange"] = exchange;
    jsonObj["exchangeShortName"] = exchangeShortName;
    jsonObj["country"] = country;
    jsonObj["isEtf"] = isEtf;
    jsonObj["isFund"] = isFund;
    jsonObj["isActivelyTrading"] = isActivelyTrading;

    // Convert to formatted JSON string
    QJsonDocument doc(jsonObj);
    return QString(doc.toJson(QJsonDocument::Indented)); // Indented for readability
}

bool CompanyScreenerResult::saveScreenerResults(const QVector<CompanyScreenerResult>& results, const QString& fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("Could not open file for writing");
        return false;
    }

    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_6_0); // Consistent versioning
    out << results; // QList serialization is built-in
    file.close();
    return true;
}

bool CompanyScreenerResult::loadScreenerResults(QVector<CompanyScreenerResult>& results, const QString& fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Could not open file for reading");
        return false;
    }

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_6_0); // Match save version
    in >> results; // Deserialize directly into the QList
    file.close();
    return true;
}

// Serialization operator
QDataStream& operator<<(QDataStream& out, const CompanyScreenerResult& result) {
    out << result.symbol << result.companyName << result.marketCap
        << result.sector << result.industry << result.beta
        << result.price << result.lastAnnualDividend << result.volume
        << result.exchange << result.exchangeShortName << result.country
        << result.isEtf << result.isFund << result.isActivelyTrading;
    return out;
}

// Deserialization operator
QDataStream& operator>>(QDataStream& in, CompanyScreenerResult& result) {
    in >> result.symbol >> result.companyName >> result.marketCap
        >> result.sector >> result.industry >> result.beta
        >> result.price >> result.lastAnnualDividend >> result.volume
        >> result.exchange >> result.exchangeShortName >> result.country
        >> result.isEtf >> result.isFund >> result.isActivelyTrading;
    return in;
}
