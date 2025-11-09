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

    // Use event loop to make the async operation synchronous
    QEventLoop loop;
    QString clientId, clientSecret;

    // Load client ID
    storage->retrieveValue("TradeStation", "client_id", [&](const QString& value) {
        clientId = value;
        if (!clientSecret.isEmpty() || value.isEmpty()) {
            loop.quit();
        }
    });

    // Load client secret
    storage->retrieveValue("TradeStation", "client_secret", [&](const QString& value) {
        clientSecret = value;
        if (!clientId.isEmpty() || value.isEmpty()) {
            loop.quit();
        }
    });

    // Wait for both values to be loaded (with timeout)
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(5000); // 5 second timeout

    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    qCDebug(tsClientToken) << Q_FUNC_INFO << ": Waiting for credentials retrieval...";

    loop.exec();

    token.clientId = clientId;
    token.clientSecret = clientSecret;

    storage->deleteLater();

    if (token.clientId.isEmpty() || token.clientSecret.isEmpty()) {
        qCDebug(tsClientToken) << "No credentials found in secure storage";
    } else {
        qCInfo(tsClientToken) << "Credentials loaded successfully from secure storage";
    }

    return token;
}

bool ClientToken::storeToSettings(const ClientToken &token)
{
    SecureStorage* storage = new SecureStorage();

    // Use event loop to make the async operations synchronous
    QEventLoop loop;
    bool success = true;
    int completedOperations = 0;

    auto checkCompletion = [&]() {
        completedOperations++;
        if (completedOperations >= 2) {
            loop.quit();
        }
    };

    // Store client ID
    storage->storeValue("TradeStation", "client_id", token.clientId, [&](bool result) {
        if (!result) success = false;
        checkCompletion();
    });

    // Store client secret
    storage->storeValue("TradeStation", "client_secret", token.clientSecret, [&](bool result) {
        if (!result) success = false;
        checkCompletion();
    });

    // Wait for both operations to complete (with timeout)
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(5000); // 5 second timeout

    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    loop.exec();

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

    // Use event loop to make the async operations synchronous
    QEventLoop loop;
    int completedOperations = 0;

    auto checkCompletion = [&]() {
        completedOperations++;
        if (completedOperations >= 2) {
            loop.quit();
        }
    };

    // Delete client ID
    storage->deleteValue("TradeStation", "client_id", [&](bool /*result*/) {
        checkCompletion();
    });

    // Delete client secret
    storage->deleteValue("TradeStation", "client_secret", [&](bool /*result*/) {
        checkCompletion();
    });

    // Wait for both operations to complete (with timeout)
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(5000); // 5 second timeout

    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    loop.exec();

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
