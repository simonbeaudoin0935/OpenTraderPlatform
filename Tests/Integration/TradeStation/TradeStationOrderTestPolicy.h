#pragma once

#include <QRegularExpression>
#include <QUrl>
#include <cmath>
#include <expected>

#include "CONSTANTS.h"

namespace TradeStationOrderTestPolicy
{
    struct Configuration
    {
        QString account;
        QString symbol;
        double limitPrice;
    };

    inline std::expected<Configuration, QString> configuration(const QString& p_apiOptIn,
                                                               const QString& p_orderOptIn,
                                                               const QString& p_account,
                                                               const QString& p_symbol,
                                                               const QString& p_price)
    {
        if (p_apiOptIn != "1" || p_orderOptIn != "1")
            return std::unexpected("Both API and paper-order opt-ins must equal 1.");
        if (p_account != TradeStationApiTestConstants::ORDER_ACCOUNT)
            return std::unexpected("Paper-order tests require the exact dedicated Simulation account.");
        static const QRegularExpression equitySymbol("\\A[A-Z][A-Z0-9.]{0,9}\\z");
        if (!equitySymbol.match(p_symbol).hasMatch())
            return std::unexpected("Set an explicit uppercase equity symbol for the one-share buy order.");
        bool parsed = false;
        const double price = p_price.toDouble(&parsed);
        if (!parsed || !std::isfinite(price) || price <= 0.0)
            return std::unexpected("Set an explicit finite positive buy limit price.");
        return Configuration{p_account, p_symbol, price};
    }

    inline bool isSimulationEndpoint(const QUrl& p_url)
    {
        return p_url.scheme() == TSClientHosts::SCHEME && p_url.host() == TSClientHosts::SIM_HOST &&
               p_url.path() == TSClientHosts::API_VERSION && p_url.userInfo().isEmpty() && p_url.port(-1) == -1 &&
               !p_url.hasQuery() && !p_url.hasFragment();
    }
} // namespace TradeStationOrderTestPolicy
