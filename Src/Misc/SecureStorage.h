#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QMap>
#include <QStringList>
#include <functional>

#ifdef QT_KEYCHAIN_LIB
#include <qt6keychain/keychain.h>
#endif

/**
 * @brief Secure storage utility for sensitive data like API keys and credentials
 *
 * This class provides encrypted storage using the system's native keychain/keyring.
 * It falls back to QSettings if QKeychain is not available.
 */
class SecureStorage : public QObject
{
    Q_OBJECT

public:
    explicit SecureStorage(QObject* parent = nullptr);
    ~SecureStorage();

    /**
     * @brief Store a value securely (async)
     * @param service The service name (e.g., "FMP_API")
     * @param key The key name (e.g., "access_token")
     * @param value The value to store
     * @param callback Callback function called with success status
     */
    void storeValue(const QString& service, const QString& key, const QString& value,
                   std::function<void(bool)> callback = nullptr);

    /**
     * @brief Retrieve a value securely (async)
     * @param service The service name
     * @param key The key name
     * @param callback Callback function called with the retrieved value (empty if not found)
     */
    void retrieveValue(const QString& service, const QString& key,
                      std::function<void(const QString&)> callback);

    /**
     * @brief Delete a stored value (async)
     * @param service The service name
     * @param key The key name
     * @param callback Callback function called with success status
     */
    void deleteValue(const QString& service, const QString& key,
                    std::function<void(bool)> callback = nullptr);

    /**
     * @brief Store multiple values securely (synchronous)
     * @param service The service name
     * @param keyValues Map of key-value pairs to store
     * @param timeoutMs Timeout in milliseconds (default 5000)
     * @return true if all operations succeeded
     */
    bool storeValuesSync(const QString& service, const QMap<QString, QString>& keyValues, int timeoutMs = 5000);

    /**
     * @brief Retrieve multiple values securely (synchronous)
     * @param service The service name
     * @param keys List of keys to retrieve
     * @param timeoutMs Timeout in milliseconds (default 5000)
     * @return Map of key-value pairs (values are empty if not found)
     */
    QMap<QString, QString> retrieveValuesSync(const QString& service, const QStringList& keys, int timeoutMs = 5000);

    /**
     * @brief Delete multiple values securely (synchronous)
     * @param service The service name
     * @param keys List of keys to delete
     * @param timeoutMs Timeout in milliseconds (default 5000)
     * @return true if all operations succeeded
     */
    bool deleteValuesSync(const QString& service, const QStringList& keys, int timeoutMs = 5000);

    /**
     * @brief Check if secure storage is available
     * @return true if QKeychain is available, false otherwise
     */
    static bool isSecureStorageAvailable();

private:
#ifdef QT_KEYCHAIN_LIB
    void handleStoreFinished(QKeychain::Job* job, std::function<void(bool)> callback);
    void handleReadFinished(QKeychain::Job* job, std::function<void(const QString&)> callback);
    void handleDeleteFinished(QKeychain::Job* job, std::function<void(bool)> callback);
#endif

    // Fallback methods using QSettings (less secure)
    void storeValueFallback(const QString& service, const QString& key, const QString& value,
                           std::function<void(bool)> callback);
    void retrieveValueFallback(const QString& service, const QString& key,
                              std::function<void(const QString&)> callback);
    void deleteValueFallback(const QString& service, const QString& key,
                            std::function<void(bool)> callback);

    // Simple obfuscation methods (NOT secure encryption)
    static QString obfuscateValue(const QString& value);
    static QString deobfuscateValue(const QString& obfuscatedValue);
};