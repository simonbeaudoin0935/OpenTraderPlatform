#include "DBClient.h"

#include <QCoreApplication>
#include <QDir>
#include <QMap>
#include <QSettings>
#include <QStandardPaths>
#include <QtConcurrent>
#include <filesystem>
#include <algorithm>

#include <databento/enums.hpp>
#include <databento/live.hpp>
#include <databento/dbn_file_store.hpp>

#include "Assume.h"
#include "BarUtils.h"
#include "CONSTANTS.h"
#include "DBRecordTranslator.h"
#include "Logging.h"
#include "MainApp.h"
#include "SecureStorage.h"
#include "Settings.h"
#include "LTTng/LTTngTracepoints.h"

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

    // Load API key eagerly so hasApiKey() is valid before thread starts.
    // GUIFrontend::loadApiKey() will re-load on the DBClient thread later,
    // emitting connectionStateChanged once the frontend is connected.
    SecureStorage storage;
    const QMap<QString, QString> values = storage.retrieveValuesSync(k_service, {k_keyName});
    m_apiKey = values.value(k_keyName);

    m_thread.setObjectName("DBClient");
    this->moveToThread(&m_thread);

    // Replay timer (single-shot — we re-arm after each tick)
    m_replayTimer.setSingleShot(true);
    m_replayTimer.moveToThread(&m_thread);
    connect(&m_replayTimer, &QTimer::timeout, this, &DBClient::onReplayTimerTick);
}

DBClient::~DBClient()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), QCoreApplication::instance()->thread());
    DEBUG << "DBClient destructor — cleaning up";

    // Destroy Databento clients on DBClient thread (they may have active callbacks)
    QMetaObject::invokeMethod(
        this,
        [this]()
        {
            if (m_liveClient)
            {
                INFO << "Stopping live client in destructor";
                m_liveClient.reset();
            }
            m_historicalClient.reset();
            closeReplayStreams();
            m_replayTimer.stop();
        },
        Qt::BlockingQueuedConnection);

    m_thread.quit();
    if (!m_thread.wait(5000))
    {
        WARNING << "DBClient thread did not finish within timeout, terminating";
        m_thread.terminate();
        m_thread.wait();
    }

    DEBUG << "DBClient destroyed";
}

// ── API Key management ─────────────────────────────────────────────────────

void DBClient::loadApiKey()
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(this, &DBClient::loadApiKey, Qt::QueuedConnection);
        return;
    }

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
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(this, [this, p_apiKey]() { storeApiKey(p_apiKey); }, Qt::QueuedConnection);
        return;
    }

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
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(this, &DBClient::connectLive, Qt::QueuedConnection);
        return;
    }

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
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(this, &DBClient::disconnectLive, Qt::QueuedConnection);
        return;
    }

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
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(this, [this, p_symbol]() { subscribeLive(p_symbol); }, Qt::QueuedConnection);
        return;
    }

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
        case TimeFrame::ONE_SECOND:
            return databento::Schema::Ohlcv1S;
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
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(
            this,
            [this, p_symbol, p_start, p_end, p_tf]() { fetchHistoricalBars(p_symbol, p_start, p_end, p_tf); },
            Qt::QueuedConnection);
        return;
    }

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
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(
            this,
            [this, p_symbol, p_date]() { downloadReplayData(p_symbol, p_date); },
            Qt::QueuedConnection);
        return;
    }

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
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(this, [this, p_dataset]() { setDataset(p_dataset); }, Qt::QueuedConnection);
        return;
    }

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

// ── Replay playback ────────────────────────────────────────────────────────

static qint64 toEpochMs(databento::UnixNanos ts)
{
    return static_cast<qint64>(std::chrono::duration_cast<std::chrono::milliseconds>(ts.time_since_epoch()).count());
}

void DBClient::advanceMbp10()
{
    m_nextMbp10.valid = false;
    if (m_mbp10Store == nullptr)
        return;

    const databento::Record* record = nullptr;
    while ((record = m_mbp10Store->NextRecord()) != nullptr)
    {
        if (record->Holds<databento::Mbp10Msg>())
        {
            const auto& msg = record->Get<databento::Mbp10Msg>();
            const qint64 epochMs = toEpochMs(msg.hd.ts_event);
            if (epochMs >= m_startEpochMs)
            {
                m_nextMbp10 = {epochMs, DBRecordTranslator::toLevel2(m_replaySymbol, msg), true};
                return;
            }
        }
    }
    m_mbp10Store.reset();
}

void DBClient::advanceTrade()
{
    m_nextTrade.valid = false;
    if (m_tradesStore == nullptr)
        return;

    const databento::Record* record = nullptr;
    while ((record = m_tradesStore->NextRecord()) != nullptr)
    {
        if (record->Holds<databento::TradeMsg>())
        {
            const auto& msg = record->Get<databento::TradeMsg>();
            const qint64 epochMs = toEpochMs(msg.hd.ts_event);
            if (epochMs >= m_startEpochMs)
            {
                m_nextTrade = {epochMs, DBRecordTranslator::toTrade(m_replaySymbol, msg), true};
                return;
            }
        }
    }
    m_tradesStore.reset();
}

bool DBClient::openReplayStreams(const QString& p_symbol, QDate p_date, QTime p_startTime)
{
    closeReplayStreams();

    m_replaySymbol = p_symbol;
    m_startEpochMs = QDateTime(p_date, p_startTime, TradingHours::MARKET_TIMEZONE).toMSecsSinceEpoch();

    const QString mbp10Path = getReplayFilePath(p_date, p_symbol, "mbp10");
    const QString tradesPath = getReplayFilePath(p_date, p_symbol, "trades");

    if (QFile::exists(mbp10Path))
    {
        try
        {
            m_mbp10Store = std::make_unique<databento::DbnFileStore>(std::filesystem::path(mbp10Path.toStdString()));
            advanceMbp10();
            INFO << "Opened Mbp10 stream:" << mbp10Path;
        }
        catch (const std::exception& ex)
        {
            WARNING << "Failed to open Mbp10 file:" << ex.what();
            m_mbp10Store.reset();
        }
    }
    else
    {
        DEBUG << "No Mbp10 file found:" << mbp10Path;
    }

    if (QFile::exists(tradesPath))
    {
        try
        {
            m_tradesStore = std::make_unique<databento::DbnFileStore>(std::filesystem::path(tradesPath.toStdString()));
            advanceTrade();
            INFO << "Opened Trades stream:" << tradesPath;
        }
        catch (const std::exception& ex)
        {
            WARNING << "Failed to open Trades file:" << ex.what();
            m_tradesStore.reset();
        }
    }
    else
    {
        DEBUG << "No Trades file found:" << tradesPath;
    }

    return m_nextMbp10.valid || m_nextTrade.valid;
}

void DBClient::closeReplayStreams()
{
    m_mbp10Store.reset();
    m_tradesStore.reset();
    m_nextMbp10.valid = false;
    m_nextTrade.valid = false;
}

void DBClient::startReplay(const QString& p_symbol, QDate p_date, QTime p_startTime, PlaybackSpeed p_speed)
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(
            this,
            [this, p_symbol, p_date, p_startTime, p_speed]() { startReplay(p_symbol, p_date, p_startTime, p_speed); },
            Qt::QueuedConnection);
        return;
    }

    if (m_playbackState != PlaybackState::Stopped)
    {
        WARNING << "Cannot start replay — already active";
        return;
    }

    m_replayTimer.stop();

    INFO << "Starting replay for" << p_symbol << "on" << p_date.toString(Qt::ISODate) << "at"
         << p_startTime.toString("hh:mm:ss") << "speed:" << static_cast<int>(p_speed);

    m_playbackSpeed = p_speed;

    if (!openReplayStreams(p_symbol, p_date, p_startTime))
    {
        CRITICAL << "Failed to load replay data";
        emit replayDataLoadFailed(
            QString("No replay data found for %1 on %2").arg(p_symbol, p_date.toString(Qt::ISODate)));
        return;
    }

    // Use earliest available record timestamp as anchor
    qint64 initialEpoch = m_startEpochMs;
    if (m_nextMbp10.valid)
        initialEpoch = m_nextMbp10.epochMs;
    if (m_nextTrade.valid && m_nextTrade.epochMs < initialEpoch)
        initialEpoch = m_nextTrade.epochMs;

    QDateTime initialTime = QDateTime::fromMSecsSinceEpoch(initialEpoch, TradingHours::MARKET_TIMEZONE);
    MainApp::currentAppReplayTime = initialTime;
    INFO << "Initialized replay time to:" << initialTime.toString("yyyy-MM-dd hh:mm:ss.zzz");

    m_replayEpochAnchorMs = initialEpoch;
    m_wallClockAnchorMs = QDateTime::currentMSecsSinceEpoch();

    m_playbackState = PlaybackState::Playing;
    emit replayStarted();

    emitNextReplayRecord();
    scheduleNextReplayTick();
}

void DBClient::startReplayPaused(const QString& p_symbol, QDate p_date, QTime p_startTime, PlaybackSpeed p_speed)
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(
            this,
            [this, p_symbol, p_date, p_startTime, p_speed]()
            { startReplayPaused(p_symbol, p_date, p_startTime, p_speed); },
            Qt::QueuedConnection);
        return;
    }

    if (m_playbackState != PlaybackState::Stopped)
    {
        WARNING << "Cannot start replay — already active";
        return;
    }

    INFO << "Starting replay (paused) for" << p_symbol << "on" << p_date.toString(Qt::ISODate) << "at"
         << p_startTime.toString("hh:mm:ss");

    m_playbackSpeed = p_speed;

    if (!openReplayStreams(p_symbol, p_date, p_startTime))
    {
        CRITICAL << "Failed to load replay data";
        emit replayDataLoadFailed(
            QString("No replay data found for %1 on %2").arg(p_symbol, p_date.toString(Qt::ISODate)));
        return;
    }

    qint64 initialEpoch = m_startEpochMs;
    if (m_nextMbp10.valid)
        initialEpoch = m_nextMbp10.epochMs;
    if (m_nextTrade.valid && m_nextTrade.epochMs < initialEpoch)
        initialEpoch = m_nextTrade.epochMs;

    QDateTime initialTime = QDateTime::fromMSecsSinceEpoch(initialEpoch, TradingHours::MARKET_TIMEZONE);
    MainApp::currentAppReplayTime = initialTime;

    m_replayEpochAnchorMs = initialEpoch;
    m_wallClockAnchorMs = 0; // Will be set on resume

    // Emit first record then pause
    m_playbackState = PlaybackState::Playing;
    emit replayStarted();

    emitNextReplayRecord();

    m_playbackState = PlaybackState::Paused;
    emit replayPaused();

    INFO << "Replay started in paused state after first record";
}

void DBClient::stopReplay()
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(this, &DBClient::stopReplay, Qt::QueuedConnection);
        return;
    }

    if (m_playbackState == PlaybackState::Stopped)
    {
        DEBUG << "stopReplay called but already stopped";
        return;
    }

    INFO << "Stopping replay";

    m_replayTimer.stop();
    m_playbackState = PlaybackState::Stopped;
    m_wallClockAnchorMs = 0;
    m_replayEpochAnchorMs = 0;
    m_pauseWallClockMs = 0;
    clearReplayTime();
    closeReplayStreams();

    emit replayStopped();
}

void DBClient::pauseReplay()
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(this, &DBClient::pauseReplay, Qt::QueuedConnection);
        return;
    }

    if (m_playbackState != PlaybackState::Playing)
    {
        WARNING << "Cannot pause — not currently playing";
        return;
    }

    DEBUG << "Pausing replay";

    m_replayTimer.stop();
    m_pauseWallClockMs = QDateTime::currentMSecsSinceEpoch();
    m_playbackState = PlaybackState::Paused;

    emit replayPaused();
}

void DBClient::resumeReplay()
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(this, &DBClient::resumeReplay, Qt::QueuedConnection);
        return;
    }

    if (m_playbackState != PlaybackState::Paused)
    {
        WARNING << "Cannot resume — not currently paused";
        return;
    }

    DEBUG << "Resuming replay";

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_wallClockAnchorMs == 0)
    {
        m_wallClockAnchorMs = now;
    }
    else if (m_pauseWallClockMs > 0)
    {
        m_wallClockAnchorMs += (now - m_pauseWallClockMs);
    }
    m_pauseWallClockMs = 0;

    m_playbackState = PlaybackState::Playing;

    scheduleNextReplayTick();

    emit replayResumed();
}

void DBClient::setReplaySpeed(PlaybackSpeed p_speed)
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(this, [this, p_speed]() { setReplaySpeed(p_speed); }, Qt::QueuedConnection);
        return;
    }

    if (m_playbackSpeed == p_speed)
        return;

    INFO << "Speed changed from" << static_cast<int>(m_playbackSpeed) << "to" << static_cast<int>(p_speed);
    m_playbackSpeed = p_speed;

    if (m_playbackState == PlaybackState::Playing)
    {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        qint64 currentReplayMs = MainApp::currentAppReplayTime.isValid()
                                     ? MainApp::currentAppReplayTime.toMSecsSinceEpoch()
                                     : m_replayEpochAnchorMs;
        m_wallClockAnchorMs = now;
        m_replayEpochAnchorMs = currentReplayMs;

        m_replayTimer.stop();
        scheduleNextReplayTick();
    }
}

void DBClient::onReplayTimerTick()
{
    if (m_playbackState != PlaybackState::Playing)
        return;

    const qint64 startWallMs = QDateTime::currentMSecsSinceEpoch();
    int eventsEmitted = 0;

    while (m_playbackState == PlaybackState::Playing)
    {
        emitNextReplayRecord();
        ++eventsEmitted;

        if (!m_nextMbp10.valid && !m_nextTrade.valid)
        {
            INFO << "Replay reached end of data (batch loop)";
            m_playbackState = PlaybackState::Stopped;
            emit replayEndReached();
            emit replayStopped();
            L2T_TP(l2trader, replay_tick, eventsEmitted, static_cast<long>(m_replayEpochAnchorMs));
            return;
        }

        if (QDateTime::currentMSecsSinceEpoch() - startWallMs >= ReplayConstants::MAX_SPEED_BATCH_BUDGET_MS)
            break;

        qint64 nextEpoch = m_nextMbp10.valid ? m_nextMbp10.epochMs : m_nextTrade.epochMs;
        if (m_nextTrade.valid && m_nextTrade.epochMs < nextEpoch)
            nextEpoch = m_nextTrade.epochMs;
        if (calculateWallClockDelay(nextEpoch) > 0)
            break;
    }

    L2T_TP(l2trader, replay_tick, eventsEmitted, static_cast<long>(m_replayEpochAnchorMs));

    if (m_playbackState == PlaybackState::Playing)
        scheduleNextReplayTick();
}

void DBClient::emitNextReplayRecord()
{
    const bool hasMbp10 = m_nextMbp10.valid;
    const bool hasTrade = m_nextTrade.valid;

    if (!hasMbp10 && !hasTrade)
        return;

    bool useMbp10 = hasMbp10 && (!hasTrade || m_nextMbp10.epochMs <= m_nextTrade.epochMs);

    if (useMbp10)
    {
        updateReplayTime(m_nextMbp10.epochMs);
        emit newLevel2(m_replaySymbol, std::get<Level2>(m_nextMbp10.data));
        advanceMbp10();
    }
    else
    {
        updateReplayTime(m_nextTrade.epochMs);
        emit newTrade(m_replaySymbol, std::get<Trade>(m_nextTrade.data));
        advanceTrade();
    }
}

void DBClient::scheduleNextReplayTick()
{
    const bool hasMbp10 = m_nextMbp10.valid;
    const bool hasTrade = m_nextTrade.valid;

    if (!hasMbp10 && !hasTrade)
    {
        INFO << "Replay reached end of data";
        m_playbackState = PlaybackState::Stopped;
        emit replayEndReached();
        emit replayStopped();
        return;
    }

    qint64 nextEpoch = hasMbp10 ? m_nextMbp10.epochMs : m_nextTrade.epochMs;
    if (hasTrade && m_nextTrade.epochMs < nextEpoch)
        nextEpoch = m_nextTrade.epochMs;

    const qint64 delay = calculateWallClockDelay(nextEpoch);
    m_replayTimer.start(static_cast<int>(delay));
}

qint64 DBClient::calculateWallClockDelay(qint64 p_replayEpochMs) const
{
    if (m_playbackSpeed == PlaybackSpeed::AsFastAsPossible)
        return 0;

    int speedValue = static_cast<int>(m_playbackSpeed);
    if (speedValue <= 0)
        return 0;

    qint64 replayOffsetMs = p_replayEpochMs - m_replayEpochAnchorMs;
    qint64 wallClockOffsetMs = (replayOffsetMs * 100) / speedValue;
    qint64 targetWallMs = m_wallClockAnchorMs + wallClockOffsetMs;
    qint64 delay = targetWallMs - QDateTime::currentMSecsSinceEpoch();

    return qBound(qint64(0), delay, qint64(60000));
}

void DBClient::updateReplayTime(qint64 p_epochMs)
{
    QDateTime currentTime = MainApp::currentAppReplayTime;
    qint64 currentMs = currentTime.isValid() ? currentTime.toMSecsSinceEpoch() : 0;

    if (p_epochMs > currentMs)
    {
        QDateTime newTime = QDateTime::fromMSecsSinceEpoch(p_epochMs, TradingHours::MARKET_TIMEZONE);
        MainApp::currentAppReplayTime = newTime;
        setCurrentReplayTime(newTime);
        emit replayTimeUpdated(newTime);
    }
}
