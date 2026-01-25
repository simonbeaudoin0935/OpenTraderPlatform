#include <QJsonDocument>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QObject>

#include "AuthToken.h"
#include "SecureStorage.h"
#include "CONSTANTS.h"

Q_LOGGING_CATEGORY(TSAuthTokenLog, "TSClient.token.auth")

AuthToken::AuthToken(const QString& p_accessToken,
                     const QString& p_refreshToken,
                     const QString& p_idToken,
                     const QString& p_tokenType,
                     const QString& p_scope,
                     int p_expiresIn,
                     QDateTime p_receivedAt)
    : accessToken(p_accessToken)
    , refreshToken(p_refreshToken)
    , idToken(p_idToken)
    , tokenType(p_tokenType)
    , scope(p_scope)
    , expiresIn(p_expiresIn)
    , receivedAt(p_receivedAt)
{
}

AuthToken AuthToken::receiveAuthToken(const QJsonObject& json)
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
    return !accessToken.isEmpty() && !refreshToken.isEmpty() && !idToken.isEmpty() && validateTokenType(tokenType) &&
           validateScope(scope) && validateExpiresIn(expiresIn);
}

bool AuthToken::isValidRefreshedToken() const
{
    return !accessToken.isEmpty() &&
           refreshToken.isEmpty() && // *** Note here we expect the refresh token to be empty when refreshed
           !idToken.isEmpty() && validateTokenType(tokenType) && validateScope(scope) && validateExpiresIn(expiresIn);
}

int AuthToken::secondsUntilExpiration()
{
    QDateTime expirationDate = receivedAt.addSecs(expiresIn);
    QDateTime currentTime = QDateTime::currentDateTime();

    return currentTime.secsTo(expirationDate);
}

int AuthToken::secondsToNextRefreshRequest()
{
    QDateTime expirationDate = receivedAt.addSecs(expiresIn);
    QDateTime currentTime = QDateTime::currentDateTime();

    return currentTime.secsTo(expirationDate) - AuthConstants::EXPIRY_BUFFER_SECONDS;
}

bool AuthToken::isExpired() const
{
    if (!receivedAt.isValid())
    {
        return true;
    }

    QDateTime currentTime = QDateTime::currentDateTime();
    int secondsSinceReceived = receivedAt.secsTo(currentTime);

    return secondsSinceReceived >= (expiresIn - AuthConstants::EXPIRY_BUFFER_SECONDS);
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
                   "  Access Token: [REDACTED]\n"
                   "  Refresh Token: [REDACTED]\n"
                   "  ID Token: [REDACTED]\n"
                   "  Token Type: %1\n"
                   "  Scope: %2\n"
                   "  Received at: %3\n"
                   "  Expires in: %4 seconds")
        .arg(tokenType)
        .arg(scope)
        .arg(receivedAt.toString(Qt::ISODate))
        .arg(QString::number(expiresIn));
}

bool AuthToken::validateScope(const QString& scope)
{
    if (scope.isEmpty())
    {
        return false;
    }

    QStringList scopes = scope.split(" ", Qt::SkipEmptyParts);
    for (const QString& expectedScope: AuthConstants::EXPECTED_SCOPES)
    {
        if (!scopes.contains(expectedScope))
        {
            return false;
        }
    }
    return true;
}

bool AuthToken::validateTokenType(const QString& tokenType)
{
    return tokenType == AuthConstants::EXPECTED_TOKEN_TYPE;
}

bool AuthToken::validateExpiresIn(int expiresIn)
{
    return expiresIn == AuthConstants::EXPECTED_EXPIRES_IN;
}

AuthToken AuthToken::loadFromSettings()
{
    AuthToken token;
    SecureStorage storage;

    // Load metadata from regular QSettings
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "L2Trader", "TradeStationTokens");
    settings.setFallbacksEnabled(false);

    token.tokenType = settings.value("Tokens/token_type").toString();
    token.scope = settings.value("Tokens/scope").toString();
    token.expiresIn = settings.value("Tokens/expires_in").toInt();
    token.receivedAt = QDateTime::fromString(settings.value("Tokens/received_at").toString(), Qt::ISODate);

    // Load sensitive tokens from SecureStorage synchronously
    QMap<QString, QString> secureTokens =
        storage.retrieveValuesSync("TradeStation", {"access_token", "refresh_token", "id_token"});

    token.accessToken = secureTokens.value("access_token");
    token.refreshToken = secureTokens.value("refresh_token");
    token.idToken = secureTokens.value("id_token");

    if (token.isValid())
    {
        qCInfo(TSAuthTokenLog) << "Credentials loaded successfully from secure storage";
    }
    else
    {
        qCWarning(TSAuthTokenLog) << "No credentials found in secure storage";
        qCDebug(TSAuthTokenLog) << "Debug: accessToken empty:" << token.accessToken.isEmpty()
                                << "refreshToken empty:" << token.refreshToken.isEmpty()
                                << "idToken empty:" << token.idToken.isEmpty()
                                << "tokenType valid:" << validateTokenType(token.tokenType)
                                << "scope valid:" << validateScope(token.scope)
                                << "expiresIn valid:" << validateExpiresIn(token.expiresIn)
                                << "(tokenType=" << token.tokenType << ", scope=" << token.scope
                                << ", expiresIn=" << token.expiresIn << ")";
    }

    return token;
}

bool AuthToken::storeToSettings(const AuthToken& token)
{
    SecureStorage storage;

    // Store metadata in regular QSettings
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "L2Trader", "TradeStationTokens");
    settings.setFallbacksEnabled(false);

    settings.setValue("Tokens/token_type", token.tokenType);
    settings.setValue("Tokens/scope", token.scope);
    settings.setValue("Tokens/expires_in", token.expiresIn);
    settings.setValue("Tokens/received_at", token.receivedAt.toString(Qt::ISODate));

    // Store sensitive tokens in SecureStorage synchronously
    QMap<QString, QString> secureTokens;
    secureTokens["access_token"] = token.accessToken;
    secureTokens["refresh_token"] = token.refreshToken;
    secureTokens["id_token"] = token.idToken;

    bool success = storage.storeValuesSync("TradeStation", secureTokens);

    // Force an immediate write to disk for QSettings
    settings.sync();

    if (settings.status() != QSettings::NoError)
    {
        qWarning() << "Failed to store token metadata: settings error" << settings.status();
        success = false;
    }

    if (success)
    {
        qDebug() << "Auth token stored successfully (secure tokens in SecureStorage, metadata in QSettings)";
    }
    else
    {
        qWarning() << "Failed to store auth token";
    }

    return success;
}

void AuthToken::clearSettings()
{
    SecureStorage storage;

    // Clear metadata from regular QSettings
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "L2Trader", "TradeStationTokens");
    settings.setFallbacksEnabled(false);

    settings.remove("Tokens/token_type");
    settings.remove("Tokens/scope");
    settings.remove("Tokens/expires_in");
    settings.remove("Tokens/received_at");

    // Clear sensitive tokens from SecureStorage synchronously
    storage.deleteValuesSync("TradeStation", {"access_token", "refresh_token", "id_token"});

    // Force an immediate write to disk for QSettings
    settings.sync();

    qDebug() << "Auth token settings cleared (secure tokens from SecureStorage, metadata from QSettings)";
}
