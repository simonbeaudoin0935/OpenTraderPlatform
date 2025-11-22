#pragma once

#include <string>
#include <optional>
#include <QString>
#include <QJsonObject>
#include <QUrlQuery>

class CompanyScreenerFilter {
public:
    // Constructor
    CompanyScreenerFilter();

    // Setters for each filter criterion
    void setMarketCapMoreThan(std::optional<long long> value);
    void setMarketCapLowerThan(std::optional<long long> value);
    void setSector(std::optional<std::string> value);
    void setIndustry(std::optional<std::string> value);
    void setBetaMoreThan(std::optional<double> value);
    void setBetaLowerThan(std::optional<double> value);
    void setPriceMoreThan(std::optional<double> value);
    void setPriceLowerThan(std::optional<double> value);
    void setDividendMoreThan(std::optional<double> value);
    void setDividendLowerThan(std::optional<double> value);
    void setVolumeMoreThan(std::optional<long long> value);
    void setVolumeLowerThan(std::optional<long long> value);
    void setExchange(std::optional<std::string> value);
    void setCountry(std::optional<std::string> value);
    void setIsEtf(std::optional<bool> value);
    void setIsFund(std::optional<bool> value);
    void setIsActivelyTrading(std::optional<bool> value);
    void setLimit(std::optional<int> value);
    void setIncludeAllShareClasses(std::optional<bool> value);

    // Getters for each filter criterion
    std::optional<long long> getMarketCapMoreThan() const;
    std::optional<long long> getMarketCapLowerThan() const;
    std::optional<std::string> getSector() const;
    std::optional<std::string> getIndustry() const;
    std::optional<double> getBetaMoreThan() const;
    std::optional<double> getBetaLowerThan() const;
    std::optional<double> getPriceMoreThan() const;
    std::optional<double> getPriceLowerThan() const;
    std::optional<double> getDividendMoreThan() const;
    std::optional<double> getDividendLowerThan() const;
    std::optional<long long> getVolumeMoreThan() const;
    std::optional<long long> getVolumeLowerThan() const;
    std::optional<std::string> getExchange() const;
    std::optional<std::string> getCountry() const;
    std::optional<bool> getIsEtf() const;
    std::optional<bool> getIsFund() const;
    std::optional<bool> getIsActivelyTrading() const;
    std::optional<int> getLimit() const;
    std::optional<bool> getIncludeAllShareClasses() const;

    // Helper function to generate URL query
    QUrlQuery toUrlQuery() const;

    // Implicit conversion operator
    operator QUrlQuery() const { return toUrlQuery(); }

private:
    // Member variables using std::optional for optional values
    std::optional<long long> marketCapMoreThan;
    std::optional<long long> marketCapLowerThan;
    std::optional<std::string> sector;
    std::optional<std::string> industry;
    std::optional<double> betaMoreThan;
    std::optional<double> betaLowerThan;
    std::optional<double> priceMoreThan;
    std::optional<double> priceLowerThan;
    std::optional<double> dividendMoreThan;
    std::optional<double> dividendLowerThan;
    std::optional<long long> volumeMoreThan;
    std::optional<long long> volumeLowerThan;
    std::optional<std::string> exchange;
    std::optional<std::string> country;
    std::optional<bool> isEtf;
    std::optional<bool> isFund;
    std::optional<bool> isActivelyTrading;
    std::optional<int> limit;
    std::optional<bool> includeAllShareClasses;
};

// New class for API result
class CompanyScreenerResult {
public:
    CompanyScreenerResult() = default;

    // Constructor taking a QJsonObject
    CompanyScreenerResult(const QJsonObject& jsonObj);

    // Getters for each member
    QString getSymbol() const;
    QString getCompanyName() const;
    long long getMarketCap() const;
    QString getSector() const;
    QString getIndustry() const;
    double getBeta() const;
    double getPrice() const;
    double getLastAnnualDividend() const;
    long long getVolume() const;
    QString getExchange() const;
    QString getExchangeShortName() const;
    QString getCountry() const;
    bool getIsEtf() const;
    bool getIsFund() const;
    bool getIsActivelyTrading() const;

    QString toJsonString() const;

    static bool saveScreenerResults(const QVector<CompanyScreenerResult>& results, const QString& fileName);
    static bool loadScreenerResults(QVector<CompanyScreenerResult>& results, const QString& fileName);

private:
    // Member variables corresponding to JSON fields
    QString symbol;
    QString companyName;
    long long marketCap;
    QString sector;
    QString industry;
    double beta;
    double price;
    double lastAnnualDividend;
    long long volume;
    QString exchange;
    QString exchangeShortName;
    QString country;
    bool isEtf;
    bool isFund;
    bool isActivelyTrading;

    // Friend declarations for serialization
    friend QDataStream& operator<<(QDataStream& out, const CompanyScreenerResult& result);
    friend QDataStream& operator>>(QDataStream& in, CompanyScreenerResult& result);

};

// Serialization operator
QDataStream& operator<<(QDataStream& out, const CompanyScreenerResult& result);

// Deserialization operator
QDataStream& operator>>(QDataStream& in, CompanyScreenerResult& result);

Q_DECLARE_METATYPE(CompanyScreenerResult)
