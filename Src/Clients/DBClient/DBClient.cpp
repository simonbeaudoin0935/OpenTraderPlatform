#include "DBClient.h"

#include <QDir>
#include <QMap>
#include <QSettings>
#include <QStandardPaths>
#include <QtConcurrent>
#include <filesystem>
#include <algorithm>

#include <databento/enums.hpp>
#include <databento/live.hpp>

#include "Assume.h"
#include "BarUtils.h"
#include "CONSTANTS.h"
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
QString DBClient::m_replayBaseDir;

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

    try
    {
        m_liveClient = std::make_unique<databento::LiveThreaded>(
            databento::LiveThreaded::Builder().SetKey(key).SetDataset(dataset).BuildThreaded());

        m_liveClient->Start([this](databento::Metadata&& metadata) { onMetadataReceived(std::move(metadata)); },
                            [this](const databento::Record& record) { return onRecordReceived(record); },
                            [this](const std::exception& ex) { return onException(ex); });

        INFO << "Live session starting...";
    }
    catch (const std::exception& ex)
    {
        CRITICAL << "Failed to connect live:" << ex.what();
        m_liveClient.reset();
        setConnectionState(ConnectionState::Disconnected);
    }
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
    m_liveClient->Subscribe({sym}, databento::Schema::Status, databento::SType::RawSymbol);

    m_subscribedSymbols.insert(p_symbol);
    INFO << "Subscribed to" << p_symbol << "(Mbp10 + Trades + Status)";
}

// ── Historical data ────────────────────────────────────────────────────────

namespace
{
    /**
 * @brief Map a TimeFrame to the Databento schema used to fetch it.
 * Non-native timescales are fetched from their source schema then aggregated.
 */
    databento::Schema schemaForTimeFrame(TimeFrame tf)
    {
        switch (BarUtils::aggregateSourceTimeFrame(tf))
        {
        case TimeFrame::ONE_HOUR:
            return databento::Schema::Ohlcv1H;
        case TimeFrame::ONE_DAY:
            return databento::Schema::Ohlcv1D;
        default: // ONE_MINUTE (native and all 5m/15m/30m derivatives)
            return databento::Schema::Ohlcv1M;
        }
    }
} // anonymous namespace

void DBClient::fetchHistoricalBars(const QString& p_symbol,
                                   const QDateTime& p_start,
                                   const QDateTime& p_end,
                                   TimeFrame p_tf)
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
    const TimeFrame tf = p_tf;

    // Resolve which Databento schema to actually fetch (may differ from requested tf for non-native)
    const TimeFrame sourceTf = BarUtils::aggregateSourceTimeFrame(tf);
    const databento::Schema schema = schemaForTimeFrame(tf);

    // Databento end is exclusive. Add one source-bar-width so the last bar is included.
    const qint64 stepSecs = static_cast<qint64>(BarUtils::minutesPerBar(sourceTf)) * 60;
    QDateTime exclusiveEnd = p_end.addSecs(stepSecs);

    // Cap end time to current UTC to avoid requesting past dataset's available_end.
    QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    if (exclusiveEnd.toUTC() > nowUtc)
    {
        exclusiveEnd = nowUtc;
        DEBUG << "Capped historical end time to current UTC:" << exclusiveEnd.toUTC().toString(Qt::ISODate);
    }

    const std::string startStr = p_start.toUTC().toString(Qt::ISODate).toStdString();
    const std::string endStr = exclusiveEnd.toUTC().toString(Qt::ISODate).toStdString();

    databento::Historical* hist = m_historicalClient.get();

    INFO << "Fetching historical bars for" << symbol << "tf=" << static_cast<int>(tf) << "from" << p_start.toString()
         << "to" << p_end.toString();

    Q_UNUSED(QtConcurrent::run(
        [this, hist, stdDataset, stdSymbol, startStr, endStr, symbol, tf, sourceTf, schema]()
        {
            QVector<Bar> bars;

            try
            {
                hist->TimeseriesGetRange(stdDataset,
                                         databento::DateTimeRange<std::string>{startStr, endStr},
                                         {stdSymbol},
                                         schema,
                                         [&bars, &symbol, this](const databento::Record& record) -> databento::KeepGoing
                                         {
                                             m_totalDataReceivedBytes += static_cast<qsizetype>(record.Size());
                                             if (record.Holds<databento::OhlcvMsg>())
                                             {
                                                 const auto& msg = record.Get<databento::OhlcvMsg>();
                                                 bars.append(DBRecordTranslator::toBar(symbol, msg));
                                             }
                                             return databento::KeepGoing::Continue;
                                         });

                sDEBUG << "Historical fetch complete for" << symbol << ":" << bars.size()
                       << "source bars at tf=" << static_cast<int>(sourceTf);
                emit dataUsageUpdated(m_totalDataReceivedBytes.load());
            }
            catch (const std::exception& ex)
            {
                sWARNING << "Historical fetch failed for" << symbol << ":" << ex.what();
            }

            // Aggregate if the requested tf differs from the fetched source tf
            if (tf != sourceTf && !bars.isEmpty())
            {
                bars = BarUtils::aggregateBars(bars, tf);
                sDEBUG << "Aggregated to" << bars.size() << "bars at tf=" << static_cast<int>(tf);
            }

            emit historicalBarsReceived(symbol, bars);
        }));
}

// ── Replay data download ───────────────────────────────────────────────────

void DBClient::downloadReplayData(const QString& p_symbol, const QDate& p_date)
{
    OBJ_ASSUME_TRUE(hasApiKey());
    OBJ_ASSUME_TRUE(p_date.isValid());
    OBJ_ASSUME_FALSE(p_symbol.isEmpty());

    const std::string key = m_apiKey.toStdString();
    const QString symbol = p_symbol;
    const QDate date = p_date;
    const QString dataset = m_dataset;
    const std::string stdDataset = dataset.toStdString();
    const std::string stdSymbol = symbol.toStdString();

    // Build time range for full trading day (4:00 AM to 7:00 PM ET, exclusive end)
    const QDateTime start(date, QTime(4, 0, 0), TradingHours::MARKET_TIMEZONE);
    const QDateTime end(date, QTime(19, 0, 0), TradingHours::MARKET_TIMEZONE);
    const std::string startStr = start.toUTC().toString(Qt::ISODate).toStdString();
    const std::string endStr = end.toUTC().toString(Qt::ISODate).toStdString();

    // Ensure output directory exists
    const QString dir = getReplayDataDir(date);
    QDir().mkpath(dir);

    const QString mbp10Path = getReplayFilePath(date, symbol, "mbp10");
    const QString tradesPath = getReplayFilePath(date, symbol, "trades");

    INFO << "Downloading replay data for" << symbol << "on" << date.toString(Qt::ISODate);

    // Each concurrent download gets its own Historical client for thread safety
    Q_UNUSED(QtConcurrent::run(
        [this, key, stdDataset, stdSymbol, startStr, endStr, mbp10Path, tradesPath, symbol, date]()
        {
            try
            {
                auto hist = databento::Historical::Builder().SetKey(key).Build();

                // Skip individual files that already exist (resume support)
                if (!QFile::exists(mbp10Path))
                {
                    sDEBUG << "Downloading Mbp10 for" << symbol;
                    hist.TimeseriesGetRangeToFile(stdDataset,
                                                  databento::DateTimeRange<std::string>{startStr, endStr},
                                                  {stdSymbol},
                                                  databento::Schema::Mbp10,
                                                  std::filesystem::path(mbp10Path.toStdString()));
                }
                else
                {
                    sDEBUG << "Skipping Mbp10 for" << symbol << "(already exists)";
                }

                if (!QFile::exists(tradesPath))
                {
                    sDEBUG << "Downloading Trades for" << symbol;
                    hist.TimeseriesGetRangeToFile(stdDataset,
                                                  databento::DateTimeRange<std::string>{startStr, endStr},
                                                  {stdSymbol},
                                                  databento::Schema::Trades,
                                                  std::filesystem::path(tradesPath.toStdString()));
                }
                else
                {
                    sDEBUG << "Skipping Trades for" << symbol << "(already exists)";
                }

                sINFO << "Replay download complete for" << symbol << "on" << date.toString(Qt::ISODate);
                emit replayDownloadFinished(symbol, date, true, {});
            }
            catch (const std::exception& ex)
            {
                sWARNING << "Replay download failed for" << symbol << ":" << ex.what();
                emit replayDownloadFinished(symbol, date, false, QString::fromStdString(ex.what()));
            }
        }));
}

QString DBClient::getReplayBaseDir()
{
    if (!m_replayBaseDir.isEmpty())
        return m_replayBaseDir;
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/ReplayData";
}

void DBClient::setReplayBaseDir(const QString& p_dir)
{
    m_replayBaseDir = p_dir;
}

QString DBClient::getReplayDataDir(const QDate& p_date)
{
    return getReplayBaseDir() + "/" + p_date.toString(Qt::ISODate);
}

QString DBClient::getReplayFilePath(const QDate& p_date, const QString& p_symbol, const QString& p_schema)
{
    return getReplayDataDir(p_date) + "/" + p_symbol + "_" + p_schema + ".dbn.zst";
}

bool DBClient::hasReplayData(const QDate& p_date, const QString& p_symbol)
{
    return QFile::exists(getReplayFilePath(p_date, p_symbol, "mbp10")) &&
           QFile::exists(getReplayFilePath(p_date, p_symbol, "trades"));
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
    // Track data usage
    m_totalDataReceivedBytes += static_cast<qsizetype>(p_record.Size());
    if (++m_recordCounter % k_emitEveryNRecords == 0)
    {
        emit dataUsageUpdated(m_totalDataReceivedBytes.load());
    }

    // Handle gateway-level error messages (no symbol resolution needed)
    if (p_record.Holds<databento::ErrorMsg>())
    {
        const auto& msg = p_record.Get<databento::ErrorMsg>();
        const QString errorText = QString::fromUtf8(msg.Err());

        using databento::ErrorCode;
        const bool isFatal = msg.code == ErrorCode::AuthFailed || msg.code == ErrorCode::ApiKeyDeactivated ||
                             msg.code == ErrorCode::InvalidSubscription ||
                             msg.code == ErrorCode::ConnectionLimitExceeded;

        CRITICAL << "Databento gateway error:" << errorText << "(code:" << static_cast<int>(msg.code)
                 << "fatal:" << isFatal << ")";

        emit liveGatewayError(errorText, isFatal);
        return databento::KeepGoing::Continue;
    }

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
    else if (p_record.Holds<databento::TradeMsg>())
    {
        const auto& msg = p_record.Get<databento::TradeMsg>();
        Trade trade = DBRecordTranslator::toTrade(symbol, msg);
        emit newTrade(symbol, trade);
    }
    else if (p_record.Holds<databento::StatusMsg>())
    {
        const auto& msg = p_record.Get<databento::StatusMsg>();

        using databento::StatusAction;
        using databento::TriState;

        const bool isHalted = msg.action == StatusAction::Halt || msg.action == StatusAction::Pause ||
                              msg.action == StatusAction::Suspend ||
                              msg.action == StatusAction::NotAvailableForTrading || msg.is_trading == TriState::No;

        const bool isSsr = msg.is_short_sell_restricted == TriState::Yes;

        QString haltReason;
        if (isHalted)
        {
            haltReason = QString::fromUtf8(databento::ToString(msg.reason));
        }

        emit newStatus(symbol, isHalted, haltReason, isSsr);
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
