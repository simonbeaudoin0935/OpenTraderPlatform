#include "DBClient.h"

#include <QMap>
#include <QSettings>
#include <QtConcurrent>

#include <databento/enums.hpp>
#include <databento/live.hpp>

#include "Assume.h"
#include "DBRecordTranslator.h"
#include "Logging.h"
#include "SecureStorage.h"
#include "Settings.h"

#define LOGGING_CATEGORY DBClientLog

Q_LOGGING_CATEGORY(DBClientLog, "DBClient")

namespace
{
    constexpr auto k_service = "Databento";
    constexpr auto k_keyName = "api_key";
} // namespace

DBClient* DBClient::m_instance = nullptr;

// ── Singleton lifecycle ────────────────────────────────────────────────────

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

DBClient::DBClient() : QObject(nullptr)
{
    // Load dataset from settings, defaulting to XNAS.ITCH
    if (appStateSettings != nullptr)
    {
        m_dataset = appStateSettings->value(k_settingsKeyDataset, k_defaultDataset).toString();
    }
    else
    {
        m_dataset = k_defaultDataset;
    }
}

DBClient::~DBClient()
{
    DEBUG << "DBClient destructor — cleaning up";

    if (m_liveClient)
    {
        INFO << "Stopping live client in destructor";
        m_liveClient.reset();
    }
    m_historicalClient.reset();

    DEBUG << "DBClient destroyed";
}

// ── API Key management ─────────────────────────────────────────────────────

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

// ── Live streaming ─────────────────────────────────────────────────────────

void DBClient::connectLive()
{
    OBJ_ASSUME_TRUE(hasApiKey());

    if (m_connectionState != ConnectionState::Disconnected)
    {
        WARNING << "connectLive() called while already" << static_cast<int>(m_connectionState);
        return;
    }

    setConnectionState(ConnectionState::Connecting);

    const std::string key = m_apiKey.toStdString();
    const std::string dataset = m_dataset.toStdString();

    INFO << "Building LiveThreaded client for dataset:" << m_dataset;

    m_liveClient = std::make_unique<databento::LiveThreaded>(
        databento::LiveThreaded::Builder().SetKey(key).SetDataset(dataset).BuildThreaded());

    m_liveClient->Start([this](databento::Metadata&& metadata) { onMetadataReceived(std::move(metadata)); },
                        [this](const databento::Record& record) { return onRecordReceived(record); },
                        [this](const std::exception& ex) { return onException(ex); });

    INFO << "Live session starting...";
}

void DBClient::disconnectLive()
{
    if (!m_liveClient)
    {
        DEBUG << "disconnectLive() called but no live client exists";
        return;
    }

    INFO << "Disconnecting live client";
    m_liveClient.reset();
    m_subscribedSymbols.clear();
    {
        QMutexLocker lock(&m_symbolMapMutex);
        m_symbolMap = databento::PitSymbolMap{};
    }
    setConnectionState(ConnectionState::Disconnected);
}

void DBClient::subscribeLive(const QString& p_symbol)
{
    if (m_connectionState != ConnectionState::Connected)
    {
        WARNING << "subscribeLive() called while not connected, state:" << static_cast<int>(m_connectionState);
        return;
    }

    if (m_subscribedSymbols.contains(p_symbol))
    {
        DEBUG << "Symbol already subscribed:" << p_symbol;
        return;
    }

    OBJ_ASSUME_DIFF(m_liveClient.get(), nullptr);

    const std::string sym = p_symbol.toStdString();

    m_liveClient->Subscribe({sym}, databento::Schema::Mbp10, databento::SType::RawSymbol);
    m_liveClient->Subscribe({sym}, databento::Schema::Trades, databento::SType::RawSymbol);

    m_subscribedSymbols.insert(p_symbol);
    INFO << "Subscribed to" << p_symbol << "(Mbp10 + Trades)";
}

// ── Historical data ────────────────────────────────────────────────────────

void DBClient::fetchHistoricalBars(const QString& p_symbol, const QDateTime& p_start, const QDateTime& p_end)
{
    OBJ_ASSUME_TRUE(hasApiKey());

    if (!m_historicalClient)
    {
        const std::string key = m_apiKey.toStdString();
        m_historicalClient =
            std::make_unique<databento::Historical>(databento::Historical::Builder().SetKey(key).Build());
    }

    const QString symbol = p_symbol;
    const QString dataset = m_dataset;
    const std::string stdDataset = dataset.toStdString();
    const std::string stdSymbol = symbol.toStdString();

    // Convert QDateTime to ISO 8601 strings for the string-based DateTimeRange overload
    const std::string startStr = p_start.toUTC().toString(Qt::ISODate).toStdString();
    const std::string endStr = p_end.toUTC().toString(Qt::ISODate).toStdString();

    // Capture a raw pointer for the Historical client (owned by this)
    databento::Historical* hist = m_historicalClient.get();

    INFO << "Fetching historical bars for" << symbol << "from" << p_start.toString() << "to" << p_end.toString();

    Q_UNUSED(QtConcurrent::run(
        [this, hist, stdDataset, stdSymbol, startStr, endStr, symbol]()
        {
            QVector<Bar> bars;

            try
            {
                hist->TimeseriesGetRange(stdDataset,
                                         databento::DateTimeRange<std::string>{startStr, endStr},
                                         {stdSymbol},
                                         databento::Schema::Ohlcv1M,
                                         [&bars, &symbol](const databento::Record& record) -> databento::KeepGoing
                                         {
                                             if (record.Holds<databento::OhlcvMsg>())
                                             {
                                                 const auto& msg = record.Get<databento::OhlcvMsg>();
                                                 bars.append(DBRecordTranslator::toBar(symbol, msg));
                                             }
                                             return databento::KeepGoing::Continue;
                                         });

                sDEBUG << "Historical fetch complete for" << symbol << ":" << bars.size() << "bars";
            }
            catch (const std::exception& ex)
            {
                sWARNING << "Historical fetch failed for" << symbol << ":" << ex.what();
            }

            emit historicalBarsReceived(symbol, bars);
        }));
}

// ── State queries ──────────────────────────────────────────────────────────

bool DBClient::isConnected() const
{
    return m_connectionState == ConnectionState::Connected;
}

DBClient::ConnectionState DBClient::getConnectionState() const
{
    return m_connectionState;
}

QString DBClient::getDataset() const
{
    return m_dataset;
}

void DBClient::setDataset(const QString& p_dataset)
{
    if (m_dataset == p_dataset)
        return;

    m_dataset = p_dataset;

    if (appStateSettings != nullptr)
    {
        appStateSettings->setValue(k_settingsKeyDataset, m_dataset);
    }

    INFO << "Dataset changed to:" << m_dataset;
}

// ── Live callbacks (run on Databento's internal thread) ────────────────────

void DBClient::onMetadataReceived(databento::Metadata&& p_metadata)
{
    INFO << "Metadata received — dataset:" << QString::fromStdString(p_metadata.dataset)
         << "symbols:" << p_metadata.symbols.size();

    // Build symbol map for today
    const auto now = std::chrono::system_clock::now();
    const auto today = date::floor<date::days>(now);
    const auto ymd = date::year_month_day{today};

    {
        QMutexLocker lock(&m_symbolMapMutex);
        m_symbolMap = p_metadata.CreateSymbolMapForDate(ymd);
    }

    setConnectionState(ConnectionState::Connected);
}

databento::KeepGoing DBClient::onRecordReceived(const databento::Record& p_record)
{
    // Update symbol map on SymbolMapping records
    {
        QMutexLocker lock(&m_symbolMapMutex);
        m_symbolMap.OnRecord(p_record);
    }

    const QString symbol = resolveSymbol(p_record);
    if (symbol.isEmpty())
        return databento::KeepGoing::Continue;

    if (p_record.Holds<databento::Mbp10Msg>())
    {
        const auto& msg = p_record.Get<databento::Mbp10Msg>();
        Level2 level2 = DBRecordTranslator::toLevel2(symbol, msg);
        emit newLevel2(symbol, level2);
    }
    else if (p_record.Holds<databento::Mbp1Msg>())
    {
        const auto& msg = p_record.Get<databento::Mbp1Msg>();
        Level1 level1 = DBRecordTranslator::toLevel1(symbol, msg);
        emit newLevel1(symbol, level1);
    }
    else if (p_record.Holds<databento::TradeMsg>())
    {
        const auto& msg = p_record.Get<databento::TradeMsg>();
        Trade trade = DBRecordTranslator::toTrade(symbol, msg);
        emit newTrade(symbol, trade);
    }

    return databento::KeepGoing::Continue;
}

databento::LiveThreaded::ExceptionAction DBClient::onException(const std::exception& p_exception)
{
    CRITICAL << "Live session exception:" << p_exception.what();

    setConnectionState(ConnectionState::Reconnecting);
    INFO << "Attempting auto-reconnect...";

    return databento::LiveThreaded::ExceptionAction::Restart;
}

// ── Helpers ────────────────────────────────────────────────────────────────

void DBClient::setConnectionState(ConnectionState p_state)
{
    if (m_connectionState == p_state)
        return;

    m_connectionState = p_state;
    INFO << "Connection state changed to:" << static_cast<int>(p_state);
    emit liveConnectionStateChanged(p_state);
}

QString DBClient::resolveSymbol(const databento::Record& p_record) const
{
    QMutexLocker lock(&m_symbolMapMutex);

    const auto it = m_symbolMap.Find(p_record);
    if (it == m_symbolMap.Map().end())
        return {};

    return QString::fromStdString(it->second);
}
