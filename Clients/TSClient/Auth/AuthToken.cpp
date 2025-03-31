#include <QJsonDocument>
#include <QDebug>

#include "AuthToken.h"

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
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                      "TradeStationAuth", "Tokens");
    settings.setFallbacksEnabled(false);

    AuthToken token;
    // Load all required fields
    token.accessToken  = settings.value("access_token").toString();
    token.refreshToken = settings.value("refresh_token").toString();
    token.idToken      = settings.value("id_token").toString();
    token.tokenType    = settings.value("token_type").toString();
    token.scope        = settings.value("scope").toString();
    token.expiresIn    = settings.value("expires_in").toInt();
    token.receivedAt   = QDateTime::fromString(settings.value("received_at").toString(), Qt::ISODate);

    return token;
}

bool AuthToken::storeToSettings(const AuthToken &token)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                      "TradeStationAuth", "Tokens");
    settings.setFallbacksEnabled(false);

    // Store all fields
    settings.setValue("access_token", token.accessToken);
    settings.setValue("refresh_token", token.refreshToken);
    settings.setValue("id_token", token.idToken);
    settings.setValue("token_type", token.tokenType);
    settings.setValue("scope", token.scope);
    settings.setValue("expires_in", token.expiresIn);
    settings.setValue("received_at", token.receivedAt.toString(Qt::ISODate));

    // Force an immediate write to disk
    settings.sync();
    
    if (settings.status() != QSettings::NoError) {
        qWarning() << "Failed to store token: settings error" << settings.status();
        return false;
    }

    return true;
}

void AuthToken::clearSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                      "TradeStationAuth", "Tokens");
    settings.setFallbacksEnabled(false);

    // Remove all token-related settings
    settings.remove("access_token");
    settings.remove("refresh_token");
    settings.remove("id_token");
    settings.remove("token_type");
    settings.remove("scope");
    settings.remove("expires_in");
    settings.remove("received_at");

    // Force an immediate write to disk
    settings.sync();
} 
