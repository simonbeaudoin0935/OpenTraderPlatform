#include "DBClient.h"

#include <QMap>

#include "Logging.h"
#include "SecureStorage.h"
#include "Assume.h"

#define LOGGING_CATEGORY DBClientLog

Q_LOGGING_CATEGORY(DBClientLog, "DBClient")

namespace
{
    constexpr auto k_service = "Databento";
    constexpr auto k_keyName = "api_key";
} // namespace

DBClient* DBClient::m_instance = nullptr;

DBClient* DBClient::getInstance()
{
    if (m_instance == nullptr)
    {
        sDEBUG << "Singleton instance created";
        m_instance = new DBClient();
    }
    return m_instance;
}

void DBClient::destroyInstance()
{
    ASSUME_TRUE(m_instance != nullptr);
    sDEBUG << "Destroying singleton instance";
    delete m_instance;
    m_instance = nullptr;
}

DBClient::DBClient() : QObject(nullptr) {}

DBClient::~DBClient()
{
    DEBUG << "DBClient destroyed";
}

void DBClient::loadApiKey()
{
    SecureStorage storage;
    const QMap<QString, QString> values = storage.retrieveValuesSync(k_service, {k_keyName});
    m_apiKey = values.value(k_keyName);

    if (m_apiKey.isEmpty())
    {
        INFO << "No Databento API key found in secure storage";
    }
    else
    {
        INFO << "Databento API key loaded from secure storage";
    }

    emit connectionStateChanged(!m_apiKey.isEmpty());
}

void DBClient::storeApiKey(const QString& p_apiKey)
{
    OBJ_ASSUME_FALSE(p_apiKey.isEmpty());

    SecureStorage storage;
    const bool success = storage.storeValuesSync(k_service, {{k_keyName, p_apiKey}});

    if (success)
    {
        m_apiKey = p_apiKey;
        INFO << "Databento API key saved to secure storage";
        emit connectionStateChanged(true);
    }
    else
    {
        WARNING << "Failed to save Databento API key to secure storage";
    }
}

bool DBClient::hasApiKey() const
{
    return !m_apiKey.isEmpty();
}

QString DBClient::getApiKey() const
{
    return m_apiKey;
}
