#ifndef AUTHTOKEN_H
#define AUTHTOKEN_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QSettings>
#include <QLoggingCategory>
#include <QMap>
#include <functional>

#include "CONSTANTS.h"

class SecureStorage;

Q_DECLARE_LOGGING_CATEGORY(TSAuthTokenLog);

class AuthToken
{
  public:
    // Default constructor with empty values
    AuthToken() : expiresIn(0) {}

    // Constructor from individual fields
    AuthToken(const QString& p_accessToken,
              const QString& p_refreshToken,
              const QString& p_idToken,
              const QString& p_tokenType,
              const QString& p_scope,
              int p_expiresIn,
              QDateTime p_receivedAt);

    // Copy constructor
    AuthToken(const AuthToken& other) = default;

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
    static AuthToken receiveAuthToken(const QJsonObject& json);
    static AuthToken receiveRefreshedAuthToken(const QJsonObject& p_json, const QString& p_previousRefreshToken);

    // Getters
    QString getAccessToken() const
    {
        return accessToken;
    }
    QString getRefreshToken() const
    {
        return refreshToken;
    }
    QString getIdToken() const
    {
        return idToken;
    }
    QString getTokenType() const
    {
        return tokenType;
    }
    QString getScope() const
    {
        return scope;
    }
    int getExpiresIn() const
    {
        return expiresIn;
    }
    QDateTime getReceivedAt() const
    {
        return receivedAt;
    }

    // Setters
    void setRefreshToken(const QString& token)
    {
        refreshToken = token;
    }

    // Utility methods
    int secondsUntilExpiration(); // Gives the number of seconds until expiration
    int
    secondsToNextRefreshRequest(); // Gives the number of seconds until the next refhesh should be performed (takes into account the 5s margin)
    bool isValid() const;
    bool isValidRefreshedToken() const;
    bool isExpired() const;
    QJsonObject toJson() const;
    QString toString() const; // For debugging/logging

    // Settings methods
    static AuthToken loadFromSettings();
    static bool storeToSettings(const AuthToken& token);
    static void clearSettings();

    /**
     * @brief Async load; never blocks the calling thread on the GUI-thread keyring executor
     * @param p_storage Storage object whose thread receives the callback (must outlive the job or be its parent)
     */
    static void loadFromSettingsAsync(SecureStorage& p_storage, std::function<void(const AuthToken&)> p_callback);
    /**
     * @brief Async store (refresh token first); never blocks the calling thread
     * @param p_callback Called on p_storage's thread with overall success
     */
    static void
    storeToSettingsAsync(SecureStorage& p_storage, const AuthToken& p_token, std::function<void(bool)> p_callback);

    // Static validation methods
    static bool validateScope(const QString& scope);
    static bool validateTokenType(const QString& tokenType);
    static bool validateExpiresIn(int expiresIn);

  private:
    [[nodiscard]] static AuthToken fromStoredSecrets(const QMap<QString, QString>& p_secureTokens);
    [[nodiscard]] static bool storeMetadata(const AuthToken& p_token);

    QString accessToken;
    QString refreshToken;
    QString idToken;
    QString tokenType;
    QString scope;
    int expiresIn;
    QDateTime receivedAt;
};

#endif // AUTHTOKEN_H
