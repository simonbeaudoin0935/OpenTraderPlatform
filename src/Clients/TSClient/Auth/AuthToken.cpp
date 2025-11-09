#include <QJsonDocument>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QObject>

#include "AuthToken.h"
#include "SecureStorage.h"

Q_LOGGING_CATEGORY(TSAuthTokenLog, "TSClient.token.auth")

// Initialize static constants
const QString AuthToken::EXPECTED_TOKEN_TYPE = "Bearer";
const QStringList AuthToken::EXPECTED_SCOPES = {
    "openid",
    "profile",
    "MarketData",
    "ReadAccount",
    "Trade",
    "offline_access"
};

AuthToken::AuthToken(const QString &accessToken,
                    const QString &refreshToken,
                    const QString &idToken,
                    const QString &tokenType,
                    const QString &scope,
                    int expiresIn,
                    QDateTime receivedAt)
    : accessToken(accessToken),
      refreshToken(refreshToken),
      idToken(idToken),
      tokenType(tokenType),
      scope(scope),
      expiresIn(expiresIn),
      receivedAt(receivedAt)
{
}

AuthToken AuthToken::receiveAuthToken(const QJsonObject &json)
{
    AuthToken token;
    token.accessToken = json["access_token"].toString();
    token.refreshToken = json["refresh_token"].toString();
    token.idToken = json["id_token"].toString();
    token.tokenType = json["token_type"].toString();
    token.scope = json["scope"].toString();
    token.expiresIn = json["expires_in"].toInt();
    token.receivedAt = QDateTime::currentDateTime();

    return token;
}

bool AuthToken::isValid() const
{
    return !accessToken.isEmpty() &&
           !refreshToken.isEmpty() &&
           !idToken.isEmpty() &&
           validateTokenType(tokenType) &&
           validateScope(scope) &&
           validateExpiresIn(expiresIn);
}

bool AuthToken::isValidRefreshedToken() const
{
    return !accessToken.isEmpty() &&
           refreshToken.isEmpty() && // *** Note here we expect the refresh token to be empty when refreshed
           !idToken.isEmpty() &&
           validateTokenType(tokenType) &&
           validateScope(scope) &&
           validateExpiresIn(expiresIn);
}

int AuthToken::secondsUntilExpiration() {
    QDateTime expirationDate = receivedAt.addSecs(expiresIn);
    QDateTime currentTime    = QDateTime::currentDateTime();

    return currentTime.secsTo(expirationDate);
}

int AuthToken::secondsToNextRefreshRequest() {
    QDateTime expirationDate = receivedAt.addSecs(expiresIn);
    QDateTime currentTime    = QDateTime::currentDateTime();

    return currentTime.secsTo(expirationDate) - EXPIRY_BUFFER_SECONDS;
}

bool AuthToken::isExpired() const
{
    if (!receivedAt.isValid()) {
        return true;
    }

    QDateTime currentTime = QDateTime::currentDateTime();
    int secondsSinceReceived = receivedAt.secsTo(currentTime);

    return secondsSinceReceived >= (expiresIn - EXPIRY_BUFFER_SECONDS);
}

QJsonObject AuthToken::toJson() const
{
    QJsonObject json;
    json["access_token"] = accessToken;
    json["refresh_token"] = refreshToken;
    json["id_token"] = idToken;
    json["token_type"] = tokenType;
    json["scope"] = scope;
    json["expires_in"] = expiresIn;
    return json;
}

QString AuthToken::toString() const
{
    return QString("\n"
                  "  Access Token: %1\n"
                  "  Refresh Token: %2\n"
                  "  ID Token: %3\n"
                  "  Token Type: %4\n"
                  "  Scope: %5\n"
                  "  Received at: %6\n"
                  "  Expires in: %7 seconds")
        .arg(accessToken)
        .arg(refreshToken)
        .arg(idToken)
        .arg(tokenType)
        .arg(scope)
        .arg(receivedAt.toString(Qt::ISODate))
        .arg(QString::number(expiresIn));
}

bool AuthToken::validateScope(const QString &scope)
{
    if (scope.isEmpty()) {
        return false;
    }

    QStringList scopes = scope.split(" ", Qt::SkipEmptyParts);
    for (const QString &expectedScope : EXPECTED_SCOPES) {
        if (!scopes.contains(expectedScope)) {
            return false;
        }
    }
    return true;
}

bool AuthToken::validateTokenType(const QString &tokenType)
{
    return tokenType == EXPECTED_TOKEN_TYPE;
}

bool AuthToken::validateExpiresIn(int expiresIn)
{
    return expiresIn == EXPECTED_EXPIRES_IN;
}

AuthToken AuthToken::loadFromSettings()
{
    AuthToken token;
    SecureStorage* storage = new SecureStorage();

    // Load metadata from regular QSettings
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                      "L2Trader", "TradeStationTokens");
    settings.setFallbacksEnabled(false);

    token.tokenType = settings.value("Tokens/token_type").toString();
    token.scope = settings.value("Tokens/scope").toString();
    token.expiresIn = settings.value("Tokens/expires_in").toInt();
    token.receivedAt = QDateTime::fromString(settings.value("Tokens/received_at").toString(), Qt::ISODate);

    // Use event loop to make the async SecureStorage operations synchronous
    QEventLoop loop;
    QString accessToken, refreshToken, idToken;
    int completedOperations = 0;

    auto checkCompletion = [&]() {
        completedOperations++;
        if (completedOperations >= 3) {
            loop.quit();
        }
    };

    // Load sensitive tokens from SecureStorage
    storage->retrieveValue("TradeStation", "access_token", [&](const QString& value) {
        accessToken = value;
        checkCompletion();
    });

    storage->retrieveValue("TradeStation", "refresh_token", [&](const QString& value) {
        refreshToken = value;
        checkCompletion();
    });

    storage->retrieveValue("TradeStation", "id_token", [&](const QString& value) {
        idToken = value;
        checkCompletion();
    });

    // Wait for all operations to complete (with timeout)
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(5000); // 5 second timeout

    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    qCDebug(TSAuthTokenLog) << Q_FUNC_INFO << ": Waiting for credentials retrieval...";

    loop.exec();

    token.accessToken = accessToken;
    token.refreshToken = refreshToken;
    token.idToken = idToken;

    storage->deleteLater();

    if (token.isValid()) {
        qCDebug(TSAuthTokenLog) << Q_FUNC_INFO << ": Auth token loaded successfully (secure tokens from SecureStorage, metadata from QSettings)";
    } else {
        qCWarning(TSAuthTokenLog) << Q_FUNC_INFO << ": Failed to load valid auth token from settings";
    }
    
    return token;
}

bool AuthToken::storeToSettings(const AuthToken &token)
{
    SecureStorage* storage = new SecureStorage();

    // Store metadata in regular QSettings
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                      "L2Trader", "TradeStationTokens");
    settings.setFallbacksEnabled(false);

    settings.setValue("Tokens/token_type", token.tokenType);
    settings.setValue("Tokens/scope", token.scope);
    settings.setValue("Tokens/expires_in", token.expiresIn);
    settings.setValue("Tokens/received_at", token.receivedAt.toString(Qt::ISODate));

    // Use event loop to make the async SecureStorage operations synchronous
    QEventLoop loop;
    bool success = true;
    int completedOperations = 0;

    auto checkCompletion = [&]() {
        completedOperations++;
        if (completedOperations >= 3) {
            loop.quit();
        }
    };

    // Store sensitive tokens in SecureStorage
    storage->storeValue("TradeStation", "access_token", token.accessToken, [&](bool result) {
        if (!result) success = false;
        checkCompletion();
    });

    storage->storeValue("TradeStation", "refresh_token", token.refreshToken, [&](bool result) {
        if (!result) success = false;
        checkCompletion();
    });

    storage->storeValue("TradeStation", "id_token", token.idToken, [&](bool result) {
        if (!result) success = false;
        checkCompletion();
    });

    // Wait for all operations to complete (with timeout)
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(5000); // 5 second timeout

    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    loop.exec();

    // Force an immediate write to disk for QSettings
    settings.sync();

    if (settings.status() != QSettings::NoError) {
        qWarning() << "Failed to store token metadata: settings error" << settings.status();
        success = false;
    }

    if (success) {
        qDebug() << "Auth token stored successfully (secure tokens in SecureStorage, metadata in QSettings)";
    } else {
        qWarning() << "Failed to store auth token";
    }

    storage->deleteLater();
    return success;
}

void AuthToken::clearSettings()
{
    SecureStorage* storage = new SecureStorage();

    // Clear metadata from regular QSettings
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                      "L2Trader", "TradeStationTokens");
    settings.setFallbacksEnabled(false);

    settings.remove("Tokens/token_type");
    settings.remove("Tokens/scope");
    settings.remove("Tokens/expires_in");
    settings.remove("Tokens/received_at");

    // Use event loop to make the async SecureStorage operations synchronous
    QEventLoop loop;
    int completedOperations = 0;

    auto checkCompletion = [&]() {
        completedOperations++;
        if (completedOperations >= 3) {
            loop.quit();
        }
    };

    // Clear sensitive tokens from SecureStorage
    storage->deleteValue("TradeStation", "access_token", [&](bool /*result*/) {
        checkCompletion();
    });

    storage->deleteValue("TradeStation", "refresh_token", [&](bool /*result*/) {
        checkCompletion();
    });

    storage->deleteValue("TradeStation", "id_token", [&](bool /*result*/) {
        checkCompletion();
    });

    // Wait for all operations to complete (with timeout)
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(5000); // 5 second timeout

    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    loop.exec();

    // Force an immediate write to disk for QSettings
    settings.sync();

    qDebug() << "Auth token settings cleared (secure tokens from SecureStorage, metadata from QSettings)";

    storage->deleteLater();
} 
