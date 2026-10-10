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

AuthToken AuthToken::receiveRefreshedAuthToken(const QJsonObject& p_json, const QString& p_previousRefreshToken)
{
    AuthToken token = receiveAuthToken(p_json);
    if (!p_json.contains("refresh_token"))
    {
        token.setRefreshToken(p_previousRefreshToken);
    }
    return token;
}

bool AuthToken::isValidRefreshedToken() const
{
    return !accessToken.isEmpty() && !idToken.isEmpty() && validateTokenType(tokenType) && validateScope(scope) &&
           validateExpiresIn(expiresIn);
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
    SecureStorage storage;
    return fromStoredSecrets(storage.retrieveValuesSync("TradeStation", {"access_token", "refresh_token", "id_token"}));
}

void AuthToken::loadFromSettingsAsync(SecureStorage& p_storage, std::function<void(const AuthToken&)> p_callback)
{
    p_storage.retrieveValues("TradeStation",
                             {"access_token", "refresh_token", "id_token"},
                             [p_callback = std::move(p_callback)](const QMap<QString, QString>& p_values)
                             { p_callback(fromStoredSecrets(p_values)); });
}

AuthToken AuthToken::fromStoredSecrets(const QMap<QString, QString>& p_secureTokens)
{
    AuthToken token;

    // Load metadata from regular QSettings
    QSettings settings(QSettings::IniFormat,
                       QSettings::UserScope,
                       "OpenTraderPlatform",
                       SecureStorage::activeBackend() == SecureStorage::Backend::YubiKey ? "TradeStationTokensYubiKey"
                                                                                         : "TradeStationTokens");
    settings.setFallbacksEnabled(false);

    token.tokenType = settings.value("Tokens/token_type").toString();
    token.scope = settings.value("Tokens/scope").toString();
    token.expiresIn = settings.value("Tokens/expires_in").toInt();
    token.receivedAt = QDateTime::fromString(settings.value("Tokens/received_at").toString(), Qt::ISODate);

    token.accessToken = p_secureTokens.value("access_token");
    token.refreshToken = p_secureTokens.value("refresh_token");
    token.idToken = p_secureTokens.value("id_token");

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

    // Persist a rotated refresh token before other values so a later write failure cannot lose it.
    if (!storage.storeValuesSync("TradeStation", {{"refresh_token", token.refreshToken}}) ||
        !storage.storeValuesSync("TradeStation", {{"access_token", token.accessToken}, {"id_token", token.idToken}}))
    {
        qCWarning(TSAuthTokenLog) << "Failed to store auth token in" << SecureStorage::backendName();
        return false;
    }

    return storeMetadata(token);
}

void AuthToken::storeToSettingsAsync(SecureStorage& p_storage,
                                     const AuthToken& p_token,
                                     std::function<void(bool)> p_callback)
{
    // Persist a rotated refresh token before other values so a later write failure cannot lose it.
    p_storage.storeValues(
        "TradeStation",
        {{"refresh_token", p_token.refreshToken}, {"access_token", p_token.accessToken}, {"id_token", p_token.idToken}},
        [p_token, p_callback = std::move(p_callback)](const bool p_success)
        {
            if (!p_success)
            {
                qCWarning(TSAuthTokenLog) << "Failed to store auth token in" << SecureStorage::backendName();
                p_callback(false);
                return;
            }
            p_callback(storeMetadata(p_token));
        });
}

bool AuthToken::storeMetadata(const AuthToken& token)
{
    // Store metadata in regular QSettings
    QSettings settings(QSettings::IniFormat,
                       QSettings::UserScope,
                       "OpenTraderPlatform",
                       SecureStorage::activeBackend() == SecureStorage::Backend::YubiKey ? "TradeStationTokensYubiKey"
                                                                                         : "TradeStationTokens");
    settings.setFallbacksEnabled(false);

    settings.setValue("Tokens/token_type", token.tokenType);
    settings.setValue("Tokens/scope", token.scope);
    settings.setValue("Tokens/expires_in", token.expiresIn);
    settings.setValue("Tokens/received_at", token.receivedAt.toString(Qt::ISODate));

    // Force an immediate write to disk for QSettings
    settings.sync();

    if (settings.status() != QSettings::NoError)
    {
        qWarning() << "Failed to store token metadata: settings error" << settings.status();
        return false;
    }

    qCDebug(TSAuthTokenLog) << "Auth token stored successfully in" << SecureStorage::backendName();
    return true;
}

void AuthToken::clearSettings()
{
    SecureStorage storage;

    // Clear metadata from regular QSettings
    QSettings settings(QSettings::IniFormat,
                       QSettings::UserScope,
                       "OpenTraderPlatform",
                       SecureStorage::activeBackend() == SecureStorage::Backend::YubiKey ? "TradeStationTokensYubiKey"
                                                                                         : "TradeStationTokens");
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
