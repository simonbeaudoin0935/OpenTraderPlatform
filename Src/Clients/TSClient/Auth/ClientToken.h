#ifndef CLIENTTOKEN_H
#define CLIENTTOKEN_H

#include <QString>
#include <QSettings>
#include <QLoggingCategory>

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

  private:
    QString clientId;
    QString clientSecret;

    // Static validation methods
    static bool validateClientId(const QString& clientId);
    static bool validateClientSecret(const QString& clientSecret);
};

#endif // CLIENTTOKEN_H