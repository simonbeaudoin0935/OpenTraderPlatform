#pragma once

#include <QObject>
#include <QString>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(DBClientLog)

/**
 * @brief Databento API client
 *
 * Manages the Databento API key and connection state.
 * In a future phase, this class will provide market data streaming
 * and historical data access via the Databento API.
 *
 * Connection state is determined by whether a valid API key is stored
 * in SecureStorage under the "Databento" service.
 */
class DBClient : public QObject
{
    Q_OBJECT

  public:
    [[nodiscard]] static DBClient* getInstance();
    static void destroyInstance();
    /** @return true if the singleton has been created */
    [[nodiscard]] static bool isInstantiated()
    {
        return m_instance != nullptr;
    }

    /**
     * @brief Load the API key from SecureStorage and emit connectionStateChanged
     * Call once at startup to initialise connection state.
     */
    void loadApiKey();

    /**
     * @brief Persist a new API key and update connection state
     * @param p_apiKey The Databento API key to store (plaintext; obfuscated at rest)
     */
    void storeApiKey(const QString& p_apiKey);

    /** @return true if a non-empty API key is currently loaded */
    [[nodiscard]] bool hasApiKey() const;

    /**
     * @return The loaded API key (empty string if not set).
     *         For internal use only — do NOT log or display this value.
     */
    [[nodiscard]] QString getApiKey() const;

  signals:
    /**
     * @brief Emitted when the Databento connection state changes
     * Thread context: Emitted from Main/GUI thread
     * @param isConnected true if a valid API key is present
     */
    void connectionStateChanged(bool isConnected);

  private:
    explicit DBClient();
    ~DBClient() override;

    static DBClient* m_instance;

    QString m_apiKey; ///< In-memory copy of the loaded API key
};
