#ifndef AUTHTOKEN_H
#define AUTHTOKEN_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QSettings>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(TSAuthTokenLog);

class AuthToken {
public:
    // Default constructor with empty values
    AuthToken() : expiresIn(0) {}

    // Constructor from individual fields
    AuthToken(const QString &accessToken,
             const QString &refreshToken,
             const QString &idToken,
             const QString &tokenType,
             const QString &scope,
             int expiresIn,
             QDateTime receivedAt);

    // Constructor from JSON response like this :
    // {
    //     "access_token": "...",
    //     "refresh_token": "...",
    //     "id_token": "...",
    //     "token_type": "Bearer",
    //     "scope": "...",
    //     "expires_in": 1200
    // }
    // SETS THE RECEIVED AT TIME TO THE CURRENT TIME
    static AuthToken receiveAuthToken(const QJsonObject &json);

    // Getters
    QString getAccessToken() const { return accessToken; }
    QString getRefreshToken() const { return refreshToken; }
    QString getIdToken() const { return idToken; }
    QString getTokenType() const { return tokenType; }
    QString getScope() const { return scope; }
    int getExpiresIn() const { return expiresIn; }
    QDateTime getReceivedAt() const { return receivedAt; }

    // Utility methods
    int secondsUntilExpiration(); // Gives the number of seconds until expiration
    int secondsToNextRefreshRequest(); // Gives the number of seconds until the next refhesh should be performed (takes into account the 5s margin)
    bool isValid() const;
    bool isValidRefreshedToken() const;
    bool isExpired() const;
    QJsonObject toJson() const;
    QString toString() const;  // For debugging/logging

    // Settings methods
    static AuthToken loadFromSettings();
    static bool storeToSettings(const AuthToken &token);
    static void clearSettings();

    // Static validation methods
    static bool validateScope(const QString &scope);
    static bool validateTokenType(const QString &tokenType);
    static bool validateExpiresIn(int expiresIn);

private:
    QString accessToken;
    QString refreshToken;
    QString idToken;
    QString tokenType;
    QString scope;
    int expiresIn;
    QDateTime receivedAt;

    static constexpr int EXPIRY_BUFFER_SECONDS = 5;  // Buffer time before actual expiry
    static const QString EXPECTED_TOKEN_TYPE;
    static constexpr int EXPECTED_EXPIRES_IN = 1200;
    static const QStringList EXPECTED_SCOPES;  // Still needs to be defined in cpp due to QStringList
};

#endif // AUTHTOKEN_H 
