#include <QDebug>

#include "ClientToken.h"

Q_LOGGING_CATEGORY(tsClientToken, "tradestation.clienttoken")

ClientToken::ClientToken(const QString &clientId,
                       const QString &clientSecret)
    : clientId(clientId),
      clientSecret(clientSecret)
{
}

bool ClientToken::isValid() const
{
    return !clientId.isEmpty() &&
           !clientSecret.isEmpty() &&
           validateClientId(clientId) &&
           validateClientSecret(clientSecret);
}

QString ClientToken::toString() const
{
    return QString("Client ID: %1\n"
                  "Client Secret: %2")
        .arg(clientId, clientSecret);
}

ClientToken ClientToken::loadFromSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                      "L2Trader", "TradeStationCredentials");
    settings.setFallbacksEnabled(false);

    ClientToken token;
    token.clientId = settings.value("credentials/client_id").toString();
    token.clientSecret = settings.value("credentials/client_secret").toString();

    qCDebug(tsClientToken) << "Credentials loaded successfully";

    return token;
}

bool ClientToken::storeToSettings(const ClientToken &token)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                      "TradeStationAuth", "Credentials");
    settings.setFallbacksEnabled(false);

    // Store all fields
    settings.setValue("credentials/client_id", token.clientId);
    settings.setValue("credentials/client_secret", token.clientSecret);

    // Force an immediate write to disk
    settings.sync();
    
    if (settings.status() != QSettings::NoError) {
        qCWarning(tsClientToken) << "Failed to store credentials: settings error" << settings.status();
        return false;
    }

    qCDebug(tsClientToken) << "Credentials stored successfully";
    return true;
}

void ClientToken::clearSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                      "TradeStationAuth", "Credentials");
    settings.setFallbacksEnabled(false);

    // Remove all credential-related settings
    settings.remove("credentials/client_id");
    settings.remove("credentials/client_secret");

    // Force an immediate write to disk
    settings.sync();
    
    qCDebug(tsClientToken) << "Credential settings cleared";
}

bool ClientToken::validateClientId(const QString &clientId)
{
    if (clientId.isEmpty()) {
        qCWarning(tsClientToken) << "Client ID is empty";
        return false;
    }
    return true;
}

bool ClientToken::validateClientSecret(const QString &clientSecret)
{
    if (clientSecret.isEmpty()) {
        qCWarning(tsClientToken) << "Client Secret is empty";
        return false;
    }
    return true;
} 
