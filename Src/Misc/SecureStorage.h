#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QMap>
#include <QStringList>
#include <functional>

#include <qt6keychain/keychain.h>

/**
 * @brief Secure storage utility for sensitive data like API keys and credentials
 *
 * OS Keyring is the default; optional YubiKey vault storage unlocks once per session.
 * Backend selection applies after restart. Neither backend falls back to the other.
 */
class SecureStorage : public QObject
{
    Q_OBJECT

  public:
    explicit SecureStorage(QObject* parent = nullptr);
    ~SecureStorage();

    enum class Backend
    {
        OSKeyring,
        YubiKey
    };
    static Backend activeBackend();
    static Backend configuredBackend();
    [[nodiscard]] static bool configureBackend(Backend p_backend);
    [[nodiscard]] static bool unlockYubiKey(QString& p_error);
    [[nodiscard]] static bool resetYubiKey(QString& p_error);
    static QString backendName();

    /**
     * @brief Store a value securely (async)
     * @param service The service name (e.g., "FMP_API")
     * @param key The key name (e.g., "access_token")
     * @param value The value to store
     * @param callback Callback function called with success status
     */
    void storeValue(const QString& service,
                    const QString& key,
                    const QString& value,
                    std::function<void(bool)> callback = nullptr);

    /**
     * @brief Retrieve a value securely (async)
     * @param service The service name
     * @param key The key name
     * @param callback Callback function called with the retrieved value (empty if not found)
     */
    void retrieveValue(const QString& service, const QString& key, std::function<void(const QString&)> callback);

    /**
     * @brief Delete a stored value (async)
     * @param service The service name
     * @param key The key name
     * @param callback Callback function called with success status
     */
    void deleteValue(const QString& service, const QString& key, std::function<void(bool)> callback = nullptr);

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
     * @brief Check whether native keyring storage is supported
     * @return Backend support, not a guarantee that the keyring is unlocked
     */
    static bool isSecureStorageAvailable();

  private:
    enum class Operation
    {
        Read,
        Write,
        Delete
    };
    struct Result
    {
        bool finished = false;
        QKeychain::Error error = QKeychain::OtherError;
        QString value;
    };
    static QKeychain::Job*
    createJob(const QString& p_service, const QString& p_key, const QString& p_value, Operation p_operation);
    static Result readResult(QKeychain::Job* p_job, Operation p_operation);
    static Result
    runYubiKeyJob(const QString& p_service, const QString& p_key, const QString& p_value, Operation p_operation);
    static Result runJob(const QString& p_service,
                         const QString& p_key,
                         const QString& p_value,
                         Operation p_operation,
                         int p_timeoutMs);
    void startJob(const QString& p_service,
                  const QString& p_key,
                  const QString& p_value,
                  Operation p_operation,
                  std::function<void(Result)> p_callback);
};