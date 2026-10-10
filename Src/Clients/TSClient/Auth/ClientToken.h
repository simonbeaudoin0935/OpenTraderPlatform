#ifndef CLIENTTOKEN_H
#define CLIENTTOKEN_H

#include <QString>
#include <QSettings>
#include <QLoggingCategory>
#include <QMap>
#include <functional>

#include "SecureStorage.h"

Q_DECLARE_LOGGING_CATEGORY(tsClientToken)

class ClientToken
{
  public:
    // Default constructor with empty values
    ClientToken() {}

    // Constructor from individual fields
    ClientToken(const QString& p_clientId, const QString& p_clientSecret);

    // Getters
    QString getClientId() const
    {
        return clientId;
    }
    QString getClientSecret() const
    {
        return clientSecret;
    }

    // Utility methods
    bool isValid() const;
    QString toString() const; // For debugging/logging

    // Settings methods
    static ClientToken loadFromSettings();
    static bool storeToSettings(const ClientToken& token);
    static void clearSettings();

    /**
     * @brief Async load; never blocks the calling thread on the GUI-thread keyring executor
     * @param p_callback Called on p_storage's thread
     */
    static void loadFromSettingsAsync(SecureStorage& p_storage, std::function<void(const ClientToken&)> p_callback);

  private:
    [[nodiscard]] static ClientToken fromStoredCredentials(const QMap<QString, QString>& p_credentials);

    QString clientId;
    QString clientSecret;

    // Static validation methods
    static bool validateClientId(const QString& clientId);
    static bool validateClientSecret(const QString& clientSecret);
};

#endif // CLIENTTOKEN_H