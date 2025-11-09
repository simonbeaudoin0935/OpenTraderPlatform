#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QObject>

#include "ClientToken.h"
#include "SecureStorage.h"

Q_LOGGING_CATEGORY(tsClientToken, "TSClient.token.client")

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
    ClientToken token;
    SecureStorage* storage = new SecureStorage();

    // Load client credentials from SecureStorage synchronously
    QMap<QString, QString> credentials = storage->retrieveValuesSync("TradeStation", {"client_id", "client_secret"});

    token.clientId = credentials.value("client_id");
    token.clientSecret = credentials.value("client_secret");

    storage->deleteLater();

    if (token.clientId.isEmpty() || token.clientSecret.isEmpty()) {
        qCWarning(tsClientToken) << "No credentials found in secure storage";
    } else {
        qCInfo(tsClientToken) << "Credentials loaded successfully from secure storage";
    }

    return token;
}

bool ClientToken::storeToSettings(const ClientToken &token)
{
    SecureStorage* storage = new SecureStorage();

    // Store client credentials in SecureStorage synchronously
    QMap<QString, QString> credentials;
    credentials["client_id"] = token.clientId;
    credentials["client_secret"] = token.clientSecret;

    bool success = storage->storeValuesSync("TradeStation", credentials);

    if (success) {
        qCDebug(tsClientToken) << "Credentials stored successfully in secure storage";
    } else {
        qCWarning(tsClientToken) << "Failed to store credentials in secure storage";
    }

    storage->deleteLater();
    return success;
}

void ClientToken::clearSettings()
{
    SecureStorage* storage = new SecureStorage();

    // Clear client credentials from SecureStorage synchronously
    storage->deleteValuesSync("TradeStation", {"client_id", "client_secret"});

    qCDebug(tsClientToken) << "Credential settings cleared from secure storage";

    storage->deleteLater();
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
