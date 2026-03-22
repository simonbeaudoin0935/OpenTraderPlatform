#include "DBClient.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QtConcurrent>
#include <filesystem>
#include <algorithm>
#include <limits>

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
#include "ThreadNames.h"

#define LOGGING_CATEGORY DBClientLog

Q_LOGGING_CATEGORY(DBClientLog, "DBClient")

namespace
{
    constexpr auto k_service = "Databento";
    constexpr auto k_keyName = "api_key";
    constexpr auto kReplayDataDirSettingsKey = "RecordsInfo/ReplayDataDir";

    [[nodiscard]] QString defaultReplayBaseDir()
    {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/ReplayData";
    }

    [[nodiscard]] QString configuredReplayBaseDir()
    {
        if (appStateSettings == nullptr)
        {
            return QString();
        }

        QString savedDir = appStateSettings->value(kReplayDataDirSettingsKey).toString().trimmed();
        if (savedDir.isEmpty())
        {
            return QString();
        }

        const QString oldBase = getCacheLocation() + "/ReplayData";
        const QString newBase = getDataLocation() + "/ReplayData";
        if (savedDir == oldBase && QDir(newBase).exists())
        {
            qInfo() << "DBClient: migrating ReplayDataDir from" << savedDir << "to" << newBase;
            savedDir = newBase;
            appStateSettings->setValue(kReplayDataDirSettingsKey, savedDir);
        }

        if (!QDir(savedDir).exists())
        {
            qWarning() << "DBClient: saved ReplayDataDir" << savedDir << "does not exist, reverting to default";
            appStateSettings->remove(kReplayDataDirSettingsKey);
            return QString();
        }

        return savedDir;
    }

    [[nodiscard]] std::optional<DBClient::ReplayDayInfo> makeReplayDayInfo(const QDate& p_date)
    {
        if (!p_date.isValid())
        {
            return std::nullopt;
        }

        const QDir dir(DBClient::getReplayDataDir(p_date));
        if (!dir.exists())
        {
            return std::nullopt;
        }

        const QStringList dbnFiles = dir.entryList({"*.dbn.zst"}, QDir::Files, QDir::Name);
        if (dbnFiles.isEmpty())
        {
            return std::nullopt;
        }

        DBClient::ReplayDayInfo info;
        info.date = p_date;
        info.fileCount = dbnFiles.size();
        for (const QString& fileName: dbnFiles)
        {
            info.totalSizeBytes += QFileInfo(dir.absoluteFilePath(fileName)).size();
        }

        return info;
    }
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

    // Set kernel thread name when thread starts
    connect(&m_thread, &QThread::started, this, &DBClient::onThreadStarted, Qt::DirectConnection);
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

    // BuildThreaded() performs synchronous network authentication which can block
    // for seconds when credentials are invalid or the subscription doesn't cover
    // the dataset.  Run it on the thread pool so the DBClient event loop stays
    // responsive for replay startup and other queued work.
    [[maybe_unused]] auto future = QtConcurrent::run(
        [this, key, dataset]()
        {
            try
            {
                auto* client = new databento::LiveThreaded(
                    databento::LiveThreaded::Builder().SetKey(key).SetDataset(dataset).BuildThreaded());

                // Hand the built client back to the DBClient thread for Start()
                QMetaObject::invokeMethod(
                    this,
                    [this, client]()
                    {
                        m_liveClient.reset(client);

                        m_liveClient->Start(
                            [this](databento::Metadata&& metadata) { onMetadataReceived(std::move(metadata)); },
                            [this](const databento::Record& record) { return onRecordReceived(record); },
                            [this](const std::exception& ex) { return onException(ex); });

                        INFO << "Live session starting...";
                    },
                    Qt::QueuedConnection);
            }
            catch (const std::exception& ex)
            {
                QString error = QString::fromStdString(ex.what());
                QMetaObject::invokeMethod(
                    this,
                    [this, error]()
                    {
                        CRITICAL << "Failed to connect live:" << error;
                        m_liveClient.reset();
                        setConnectionState(ConnectionState::Disconnected);
                    },
                    Qt::QueuedConnection);
            }
        });
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
    downloadReplayData(p_symbol, p_date, {});
}

void DBClient::downloadReplayData(const QString& p_symbol, const QDate& p_date, const QString& p_requestId)
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(
            this,
            [this, p_symbol, p_date, p_requestId]() { downloadReplayData(p_symbol, p_date, p_requestId); },
            Qt::QueuedConnection);
        return;
    }

    OBJ_ASSUME_TRUE(hasApiKey());
    OBJ_ASSUME_TRUE(p_date.isValid());
    OBJ_ASSUME_FALSE(p_symbol.isEmpty());

    const std::string key = m_apiKey.toStdString();
    const QString symbol = p_symbol;
    const QDate date = p_date;
    const QString requestId = p_requestId.trimmed();
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
        [this, key, stdDataset, stdSymbol, startStr, endStr, mbp10Path, tradesPath, symbol, date, requestId]()
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
                if (!requestId.isEmpty())
                {
                    emit replayDownloadFinishedForRequest(requestId, symbol, date, true, {});
                }
            }
            catch (const std::exception& ex)
            {
                const QString errorMessage = QString::fromStdString(ex.what());
                sWARNING << "Replay download failed for" << symbol << ":" << errorMessage;
                emit replayDownloadFinished(symbol, date, false, errorMessage);
                if (!requestId.isEmpty())
                {
                    emit replayDownloadFinishedForRequest(requestId, symbol, date, false, errorMessage);
                }
            }
        }));
}

QString DBClient::getReplayBaseDir()
{
    if (!m_replayBaseDir.isEmpty())
    {
        return m_replayBaseDir;
    }

    const QString configuredDir = configuredReplayBaseDir();
    if (!configuredDir.isEmpty())
    {
        return configuredDir;
    }

    return defaultReplayBaseDir();
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

QVector<DBClient::ReplayDayInfo> DBClient::listAvailableReplayDates()
{
    const QDir base(getReplayBaseDir());
    if (!base.exists())
    {
        return {};
    }

    const QStringList dateDirs = base.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
    QVector<ReplayDayInfo> days;
    for (const QString& dirName: dateDirs)
    {
        const auto info = makeReplayDayInfo(QDate::fromString(dirName, Qt::ISODate));
        if (info.has_value())
        {
            days.append(info.value());
        }
    }

    return days;
}

std::optional<DBClient::ReplayDayInfo> DBClient::getReplayDateInfo(const QDate& p_date)
{
    return makeReplayDayInfo(p_date);
}

QVector<DBClient::ReplaySymbolInfo> DBClient::listAvailableReplaySymbols(const QDate& p_date)
{
    if (!p_date.isValid())
    {
        return {};
    }

    const QDir dir(getReplayDataDir(p_date));
    if (!dir.exists())
    {
        return {};
    }

    const QStringList files = dir.entryList({"*.dbn.zst"}, QDir::Files, QDir::Name);
    static const QRegularExpression mbp10Re("^(.+)_mbp10\\.dbn\\.zst$");
    static const QRegularExpression tradesRe("^(.+)_trades\\.dbn\\.zst$");

    QMap<QString, ReplaySymbolInfo> symbolMap;
    for (const QString& fileName: files)
    {
        const QRegularExpressionMatch mbp10Match = mbp10Re.match(fileName);
        if (mbp10Match.hasMatch())
        {
            const QString symbol = mbp10Match.captured(1);
            ReplaySymbolInfo& info = symbolMap[symbol];
            info.symbol = symbol;
            info.hasMbp10 = true;
            info.mbp10SizeBytes = QFileInfo(dir.absoluteFilePath(fileName)).size();
            continue;
        }

        const QRegularExpressionMatch tradesMatch = tradesRe.match(fileName);
        if (tradesMatch.hasMatch())
        {
            const QString symbol = tradesMatch.captured(1);
            ReplaySymbolInfo& info = symbolMap[symbol];
            info.symbol = symbol;
            info.hasTrades = true;
            info.tradesSizeBytes = QFileInfo(dir.absoluteFilePath(fileName)).size();
        }
    }

    QVector<ReplaySymbolInfo> symbols;
    symbols.reserve(symbolMap.size());
    for (auto it = symbolMap.cbegin(); it != symbolMap.cend(); ++it)
    {
        symbols.append(it.value());
    }

    return symbols;
}

QDate DBClient::getReplayDate()
{
    return getInstance()->m_replayDate;
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

void DBClient::advanceMbp10(SymbolStream& p_stream)
{
    p_stream.nextMbp10.valid = false;
    if (p_stream.mbp10Store == nullptr)
        return;

    const databento::Record* record = nullptr;
    while ((record = p_stream.mbp10Store->NextRecord()) != nullptr)
    {
        if (record->Holds<databento::Mbp10Msg>())
        {
            const auto& msg = record->Get<databento::Mbp10Msg>();
            const qint64 epochMs = toEpochMs(msg.hd.ts_event);
            if (epochMs >= m_startEpochMs)
            {
                p_stream.nextMbp10 = {epochMs, DBRecordTranslator::toLevel2(p_stream.symbol, msg), true};
                return;
            }
        }
    }
    p_stream.mbp10Store.reset();
}

void DBClient::advanceTrade(SymbolStream& p_stream)
{
    p_stream.nextTrade.valid = false;
    if (p_stream.tradesStore == nullptr)
        return;

    const databento::Record* record = nullptr;
    while ((record = p_stream.tradesStore->NextRecord()) != nullptr)
    {
        if (record->Holds<databento::TradeMsg>())
        {
            const auto& msg = record->Get<databento::TradeMsg>();
            const qint64 epochMs = toEpochMs(msg.hd.ts_event);
            if (epochMs >= m_startEpochMs)
            {
                p_stream.nextTrade = {epochMs, DBRecordTranslator::toTrade(p_stream.symbol, msg), true};
                return;
            }
        }
    }
    p_stream.tradesStore.reset();
}

bool DBClient::openReplayStreamsForSymbol(const QString& p_symbol, QDate p_date)
{
    // Don't add a symbol twice
    for (const auto& s: m_replayStreams)
    {
        if (s.symbol == p_symbol)
        {
            DEBUG << "Replay stream for" << p_symbol << "already open";
            return true;
        }
    }

    SymbolStream stream;
    stream.symbol = p_symbol;
    bool opened = false;

    const QString mbp10Path = getReplayFilePath(p_date, p_symbol, "mbp10");
    if (QFile::exists(mbp10Path))
    {
        try
        {
            stream.mbp10Store =
                std::make_unique<databento::DbnFileStore>(std::filesystem::path(mbp10Path.toStdString()));
            advanceMbp10(stream);
            INFO << "Opened Mbp10 stream:" << mbp10Path;
            opened = true;
        }
        catch (const std::exception& ex)
        {
            WARNING << "Failed to open Mbp10 file:" << ex.what();
            stream.mbp10Store.reset();
        }
    }
    else
    {
        DEBUG << "No Mbp10 file found:" << mbp10Path;
    }

    const QString tradesPath = getReplayFilePath(p_date, p_symbol, "trades");
    if (QFile::exists(tradesPath))
    {
        try
        {
            stream.tradesStore =
                std::make_unique<databento::DbnFileStore>(std::filesystem::path(tradesPath.toStdString()));
            advanceTrade(stream);
            INFO << "Opened Trades stream:" << tradesPath;
            opened = true;
        }
        catch (const std::exception& ex)
        {
            WARNING << "Failed to open Trades file:" << ex.what();
            stream.tradesStore.reset();
        }
    }
    else
    {
        DEBUG << "No Trades file found:" << tradesPath;
    }

    if (opened)
        m_replayStreams.push_back(std::move(stream));

    return opened;
}

void DBClient::closeReplayStreams()
{
    m_replayStreams.clear();
}

bool DBClient::hasAnyReplayRecord() const
{
    for (const auto& s: m_replayStreams)
    {
        if (s.nextMbp10.valid || s.nextTrade.valid)
            return true;
    }
    return false;
}

qint64 DBClient::earliestReplayEpoch() const
{
    qint64 earliest = std::numeric_limits<qint64>::max();
    for (const auto& s: m_replayStreams)
    {
        if (s.nextMbp10.valid)
            earliest = qMin(earliest, s.nextMbp10.epochMs);
        if (s.nextTrade.valid)
            earliest = qMin(earliest, s.nextTrade.epochMs);
    }
    return earliest;
}

bool DBClient::addReplaySymbol(const QString& p_symbol)
{
    if (QThread::currentThread() != thread())
    {
        bool result = false;
        QMetaObject::invokeMethod(
            this,
            [this, p_symbol, &result]() { result = addReplaySymbol(p_symbol); },
            Qt::BlockingQueuedConnection);
        return result;
    }

    if (!isReplayActive())
    {
        WARNING << "addReplaySymbol called but replay is not active";
        return false;
    }

    INFO << "Adding symbol to active replay:" << p_symbol;
    return openReplayStreamsForSymbol(p_symbol, m_replayDate);
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
    m_replayDate = p_date;
    m_startEpochMs = QDateTime(p_date, p_startTime, TradingHours::MARKET_TIMEZONE).toMSecsSinceEpoch();
    closeReplayStreams();

    if (!openReplayStreamsForSymbol(p_symbol, p_date))
    {
        CRITICAL << "Failed to load replay data";
        emit replayDataLoadFailed(
            QString("No replay data found for %1 on %2").arg(p_symbol, p_date.toString(Qt::ISODate)));
        return;
    }

    // Use earliest available record timestamp as anchor
    qint64 initialEpoch = earliestReplayEpoch();
    if (initialEpoch == std::numeric_limits<qint64>::max())
        initialEpoch = m_startEpochMs;

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
    m_replayDate = p_date;
    m_startEpochMs = QDateTime(p_date, p_startTime, TradingHours::MARKET_TIMEZONE).toMSecsSinceEpoch();
    closeReplayStreams();

    if (!openReplayStreamsForSymbol(p_symbol, p_date))
    {
        CRITICAL << "Failed to load replay data";
        emit replayDataLoadFailed(
            QString("No replay data found for %1 on %2").arg(p_symbol, p_date.toString(Qt::ISODate)));
        return;
    }

    qint64 initialEpoch = earliestReplayEpoch();
    if (initialEpoch == std::numeric_limits<qint64>::max())
        initialEpoch = m_startEpochMs;

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

        if (!hasAnyReplayRecord())
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

        qint64 nextEpoch = earliestReplayEpoch();
        if (calculateWallClockDelay(nextEpoch) > 0)
            break;
    }

    L2T_TP(l2trader, replay_tick, eventsEmitted, static_cast<long>(m_replayEpochAnchorMs));

    if (m_playbackState == PlaybackState::Playing)
        scheduleNextReplayTick();
}

void DBClient::emitNextReplayRecord()
{
    // Find the stream with the globally earliest next record
    SymbolStream* bestStream = nullptr;
    bool useMbp10 = false;
    qint64 bestEpoch = std::numeric_limits<qint64>::max();

    for (auto& stream: m_replayStreams)
    {
        if (stream.nextMbp10.valid && stream.nextMbp10.epochMs < bestEpoch)
        {
            bestEpoch = stream.nextMbp10.epochMs;
            bestStream = &stream;
            useMbp10 = true;
        }
        if (stream.nextTrade.valid && stream.nextTrade.epochMs < bestEpoch)
        {
            bestEpoch = stream.nextTrade.epochMs;
            bestStream = &stream;
            useMbp10 = false;
        }
    }

    if (!bestStream)
        return;

    if (useMbp10)
    {
        updateReplayTime(bestStream->nextMbp10.epochMs);
        emit newLevel2(bestStream->symbol, std::get<Level2>(bestStream->nextMbp10.data));
        advanceMbp10(*bestStream);
    }
    else
    {
        updateReplayTime(bestStream->nextTrade.epochMs);
        emit newTrade(bestStream->symbol, std::get<Trade>(bestStream->nextTrade.data));
        advanceTrade(*bestStream);
    }
}

void DBClient::scheduleNextReplayTick()
{
    if (!hasAnyReplayRecord())
    {
        INFO << "Replay reached end of data";
        m_playbackState = PlaybackState::Stopped;
        emit replayEndReached();
        emit replayStopped();
        return;
    }

    const qint64 nextEpoch = earliestReplayEpoch();
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

void DBClient::onThreadStarted()
{
    // Set kernel thread name for visibility in trace tools (ps, top, LTTng, TraceCompass)
    ThreadNames::setCurrentThreadName("DBClient");
}
