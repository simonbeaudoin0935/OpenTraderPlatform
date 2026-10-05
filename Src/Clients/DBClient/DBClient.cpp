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
#include <cstdint>
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
    enum class ReplayFileKind
    {
        Unknown,
        Mbp10,
        Mbp1,
        Trades,
    };

    constexpr auto k_service = "Databento";
    constexpr auto k_keyName = "api_key";
    constexpr auto kReplayDataDirSettingsKey = "RecordsInfo/ReplayDataDir";
    constexpr auto kDefaultLiveDataset = "XNAS.ITCH";
    constexpr auto kBasicFallbackDataset = "XNAS.BASIC";
    constexpr auto kMiniFallbackDataset = "EQUS.MINI";
    constexpr int kReplayDownloadMaxAttempts = 3;
    constexpr unsigned long kReplayDownloadRetryBaseDelayMs = 750;

    // Cache "current market day on XNAS.ITCH requires live license" once observed,
    // so the remaining same-day batch requests can skip guaranteed 403s.
    std::atomic<qint64> g_currentDayXnasNoLiveLicenseJulian{0};

    [[nodiscard]] bool isLiveLicenseError(const QString& p_errorText)
    {
        return p_errorText.contains("license_not_found_unauthorized", Qt::CaseInsensitive) ||
               p_errorText.contains("required to access", Qt::CaseInsensitive);
    }

    [[nodiscard]] bool isTopOfBookOnlyDataset(const QString& p_dataset)
    {
        return p_dataset == kBasicFallbackDataset || p_dataset == kMiniFallbackDataset;
    }

    [[nodiscard]] bool isRecoverableLiveException(const QString& p_errorText)
    {
        return p_errorText.contains("Broken pipe", Qt::CaseInsensitive) ||
               p_errorText.contains("Error reading from socket", Qt::CaseInsensitive) ||
               p_errorText.contains("timed out", Qt::CaseInsensitive);
    }

    [[nodiscard]] bool isRecoverableReplayDownloadException(const QString& p_errorText)
    {
        return p_errorText.contains("Failed to read connection", Qt::CaseInsensitive) ||
               p_errorText.contains("Failed to write connection", Qt::CaseInsensitive) ||
               p_errorText.contains("timed out", Qt::CaseInsensitive) ||
               p_errorText.contains("Broken pipe", Qt::CaseInsensitive) ||
               p_errorText.contains("Connection reset", Qt::CaseInsensitive);
    }

    [[nodiscard]] std::optional<QDateTime> parseProviderUtcDateTime(QString p_value)
    {
        p_value = p_value.trimmed();
        if (p_value.isEmpty())
        {
            return std::nullopt;
        }

        p_value.replace(' ', 'T');

        static const QRegularExpression fractionalSecondsRe(R"((\.\d{3})\d+(Z|[+-]\d{2}:\d{2})$)");
        p_value.replace(fractionalSecondsRe, R"(\1\2)");

        QDateTime parsed = QDateTime::fromString(p_value, Qt::ISODateWithMs);
        if (!parsed.isValid())
        {
            parsed = QDateTime::fromString(p_value, Qt::ISODate);
        }

        if (!parsed.isValid())
        {
            return std::nullopt;
        }

        return parsed.toUTC();
    }

    [[nodiscard]] std::optional<QDateTime> extractHistoricalAvailableEnd(const QString& p_errorText)
    {
        if (!p_errorText.contains("data_end_after_available_end", Qt::CaseInsensitive))
        {
            return std::nullopt;
        }

        static const QRegularExpression payloadAvailableEndRe(QStringLiteral("\"available_end\":\"([^\"]+)\""));
        if (const QRegularExpressionMatch payloadMatch = payloadAvailableEndRe.match(p_errorText);
            payloadMatch.hasMatch())
        {
            if (const std::optional<QDateTime> parsed = parseProviderUtcDateTime(payloadMatch.captured(1));
                parsed.has_value())
            {
                return parsed;
            }
        }

        static const QRegularExpression messageAvailableEndRe(R"(available up to '([^']+)')");
        if (const QRegularExpressionMatch messageMatch = messageAvailableEndRe.match(p_errorText);
            messageMatch.hasMatch())
        {
            return parseProviderUtcDateTime(messageMatch.captured(1));
        }

        return std::nullopt;
    }

    [[nodiscard]] std::optional<QDateTime> extractAnyHistoricalAvailableEnd(const QString& p_errorText)
    {
        static const QRegularExpression payloadAvailableEndRe(QStringLiteral("\"available_end\":\"([^\"]+)\""));
        if (const QRegularExpressionMatch payloadMatch = payloadAvailableEndRe.match(p_errorText);
            payloadMatch.hasMatch())
        {
            if (const std::optional<QDateTime> parsed = parseProviderUtcDateTime(payloadMatch.captured(1));
                parsed.has_value())
            {
                return parsed;
            }
        }

        static const QRegularExpression messageAvailableEndRe(R"(available up to '([^']+)')");
        if (const QRegularExpressionMatch messageMatch = messageAvailableEndRe.match(p_errorText);
            messageMatch.hasMatch())
        {
            return parseProviderUtcDateTime(messageMatch.captured(1));
        }

        return std::nullopt;
    }

    [[nodiscard]] bool datasetSupportsMbp10(const QString& p_dataset)
    {
        return !isTopOfBookOnlyDataset(p_dataset);
    }

    [[nodiscard]] bool datasetSupportsStatus(const QString& p_dataset)
    {
        return !isTopOfBookOnlyDataset(p_dataset);
    }

    [[nodiscard]] bool hasUsableTopOfBook(const Level2& p_level2)
    {
        return p_level2.m_bids[0].m_price > 0.0 || p_level2.m_asks[0].m_price > 0.0;
    }

    [[nodiscard]] QDate currentMarketDate()
    {
        return QDateTime::currentDateTimeUtc().toTimeZone(TradingHours::MARKET_TIMEZONE).date();
    }

    [[nodiscard]] bool
    shouldRetryReplayDownloadWithMini(const QString& p_dataset, const QDate& p_date, const QString& p_errorText)
    {
        return p_dataset == kDefaultLiveDataset && p_date == currentMarketDate() &&
               (isLiveLicenseError(p_errorText) || extractHistoricalAvailableEnd(p_errorText).has_value());
    }

    [[nodiscard]] QString replayLevel2SchemaNameForDataset(const QString& p_dataset)
    {
        return datasetSupportsMbp10(p_dataset) ? "mbp10" : "mbp1";
    }

    [[nodiscard]] databento::Schema replayLevel2SchemaForDataset(const QString& p_dataset)
    {
        return datasetSupportsMbp10(p_dataset) ? databento::Schema::Mbp10 : databento::Schema::Mbp1;
    }

    [[nodiscard]] ReplayFileKind replayLevel2FileKindForDataset(const QString& p_dataset)
    {
        return datasetSupportsMbp10(p_dataset) ? ReplayFileKind::Mbp10 : ReplayFileKind::Mbp1;
    }

    [[nodiscard]] databento::UnixNanos toUnixNanos(const QDateTime& p_time)
    {
        return databento::UnixNanos{
            std::chrono::nanoseconds{static_cast<std::int64_t>(p_time.toUTC().toMSecsSinceEpoch()) * 1000000LL}};
    }

    [[nodiscard]] QStringList datasetCandidates(const QString& p_configuredDataset, const QString& p_resolvedDataset)
    {
        QStringList candidates;
        if (!p_resolvedDataset.isEmpty())
        {
            candidates.append(p_resolvedDataset);
        }

        candidates.append(p_configuredDataset);
        if (p_configuredDataset == kDefaultLiveDataset)
        {
            candidates.append(kBasicFallbackDataset);
        }
        candidates.append(kMiniFallbackDataset);

        candidates.removeDuplicates();
        return candidates;
    }

    [[nodiscard]] QString defaultReplayBaseDir()
    {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/ReplayData";
    }

    [[nodiscard]] qint64 replaySessionStopEpochMs(const QDate& p_replayDate)
    {
        const QDateTime lastBarOpen(p_replayDate,
                                    TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                    TradingHours::MARKET_TIMEZONE);
        return lastBarOpen.addSecs(60).toMSecsSinceEpoch();
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

    [[nodiscard]] QString replayFileKindLabel(const ReplayFileKind p_kind)
    {
        switch (p_kind)
        {
        case ReplayFileKind::Mbp10:
            return "Mbp10";
        case ReplayFileKind::Mbp1:
            return "Mbp1";
        case ReplayFileKind::Trades:
            return "Trades";
        case ReplayFileKind::Unknown:
            break;
        }

        return "replay";
    }

    [[nodiscard]] bool replayRecordMatchesExpectedKind(const databento::Record& p_record, const ReplayFileKind p_kind)
    {
        switch (p_kind)
        {
        case ReplayFileKind::Mbp10:
            return p_record.Holds<databento::Mbp10Msg>();
        case ReplayFileKind::Mbp1:
            return p_record.Holds<databento::Mbp1Msg>();
        case ReplayFileKind::Trades:
            return p_record.Holds<databento::TradeMsg>();
        case ReplayFileKind::Unknown:
            return true;
        }

        return false;
    }

    [[nodiscard]] std::optional<QString> replayDataFileInvalidReason(const QString& p_path,
                                                                     const ReplayFileKind p_expectedKind)
    {
        const QFileInfo fileInfo(p_path);
        if (!fileInfo.exists())
        {
            return "file does not exist";
        }

        if (!fileInfo.isFile())
        {
            return "path is not a regular file";
        }

        if (fileInfo.size() <= 0)
        {
            return "file is empty";
        }

        try
        {
            databento::DbnFileStore store(std::filesystem::path(p_path.toStdString()));
            const databento::Record* record = nullptr;
            while ((record = store.NextRecord()) != nullptr)
            {
                if (replayRecordMatchesExpectedKind(*record, p_expectedKind))
                {
                    return std::nullopt;
                }
            }
        }
        catch (const std::exception& ex)
        {
            return QString("failed to parse DBN payload: %1").arg(ex.what());
        }

        if (p_expectedKind == ReplayFileKind::Unknown)
        {
            return "file contains no replay records";
        }

        return QString("file contains no %1 records").arg(replayFileKindLabel(p_expectedKind));
    }

    [[nodiscard]] bool isValidReplayDataFile(const QString& p_path, const ReplayFileKind p_expectedKind)
    {
        return !replayDataFileInvalidReason(p_path, p_expectedKind).has_value();
    }

    void removeInvalidReplayDataFile(const QString& p_path, const ReplayFileKind p_expectedKind)
    {
        if (replayDataFileInvalidReason(p_path, p_expectedKind).has_value())
        {
            QFile::remove(p_path);
        }
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
        for (const QString& fileName: dbnFiles)
        {
            const QFileInfo fileInfo(dir.absoluteFilePath(fileName));
            if (fileInfo.size() <= 0)
            {
                continue;
            }

            ++info.fileCount;
            info.totalSizeBytes += fileInfo.size();
        }

        if (info.fileCount == 0)
        {
            return std::nullopt;
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
        QMetaObject::invokeMethod(
            this,
            [this, p_apiKey]() { storeApiKey(p_apiKey); },
            Qt::QueuedConnection);
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
        emit credentialStorageFailed(QString("Could not save the Databento API key in the %1. "
                                             "Unlock the selected storage backend in Credentials and try again. "
                                             "No fallback to another backend will be used.")
                                         .arg(SecureStorage::backendName()));
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

    m_liveQuoteBootstrapStates.clear();
    {
        QMutexLocker lock(&m_liveQuoteTimestampMutex);
        m_latestObservedLiveQuote.clear();
    }

    setConnectionState(ConnectionState::Connecting);

    const std::string key = m_apiKey.toStdString();
    const QString configuredDataset = m_dataset;
    const QStringList candidates = datasetCandidates(configuredDataset, m_resolvedLiveDataset);

    INFO << "Building LiveThreaded client for dataset candidates:" << candidates;

    // BuildThreaded() performs synchronous network authentication which can block
    // for seconds when credentials are invalid or the subscription doesn't cover
    // the dataset.  Run it on the thread pool so the DBClient event loop stays
    // responsive for replay startup and other queued work.
    [[maybe_unused]] auto future = QtConcurrent::run(
        [this, key, configuredDataset, candidates]()
        {
            QString error;
            for (qsizetype index = 0; index < candidates.size(); ++index)
            {
                const QString& candidate = candidates.at(index);
                try
                {
                    auto* client = new databento::LiveThreaded(databento::LiveThreaded::Builder()
                                                                   .SetKey(key)
                                                                   .SetDataset(candidate.toStdString())
                                                                   .BuildThreaded());

                    // Hand the built client back to the DBClient thread for Start()
                    QMetaObject::invokeMethod(
                        this,
                        [this, client, candidate, configuredDataset]()
                        {
                            if (MainApp::isInReplayMode() || MainApp::isInReviewMode())
                            {
                                INFO << "Discarding live client startup while data source mode is not live";
                                delete client;
                                setConnectionState(ConnectionState::Disconnected);
                                return;
                            }

                            setResolvedLiveDataset(candidate);
                            if (candidate != configuredDataset)
                            {
                                WARNING << "Falling back from" << configuredDataset << "to" << candidate
                                        << "for live market data; top-of-book mode is active";
                                emit liveGatewayError(QString("Falling back from %1 to %2.\n\n"
                                                              "Bars, trades, and top-of-book remain available, but "
                                                              "full depth and status data still require %1 "
                                                              "entitlement.")
                                                          .arg(configuredDataset, candidate),
                                                      false);
                            }

                            m_liveClient.reset(client);
                            m_subscribedSymbols.clear();
                            for (auto it = m_requestedLiveSubscriptions.cbegin();
                                 it != m_requestedLiveSubscriptions.cend();
                                 ++it)
                            {
                                applyLiveSubscription(it.key(), it.value(), true);
                            }

                            m_liveClient->Start(
                                [this](databento::Metadata&& metadata) { onMetadataReceived(std::move(metadata)); },
                                [this](const databento::Record& record) { return onRecordReceived(record); },
                                [this](const std::exception& ex) { return onException(ex); });

                            INFO << "Live session starting with dataset:" << candidate;
                        },
                        Qt::QueuedConnection);
                    return;
                }
                catch (const std::exception& ex)
                {
                    error = QString::fromStdString(ex.what());
                    if (isLiveLicenseError(error) && index + 1 < candidates.size())
                    {
                        WARNING << "Live connect to" << candidate << "failed, retrying with" << candidates.at(index + 1)
                                << ":" << error;
                        continue;
                    }

                    break;
                }
            }

            QMetaObject::invokeMethod(
                this,
                [this, error]()
                {
                    if (MainApp::isInReplayMode() || MainApp::isInReviewMode())
                    {
                        INFO << "Ignoring live connect failure while data source mode is not live:" << error;
                        m_liveClient.reset();
                        setConnectionState(ConnectionState::Disconnected);
                        return;
                    }

                    CRITICAL << "Failed to connect live:" << error;
                    emit liveGatewayError(error, true);
                    m_liveClient.reset();
                    setConnectionState(ConnectionState::Disconnected);
                },
                Qt::QueuedConnection);
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
    m_liveQuoteBootstrapStates.clear();
    {
        QMutexLocker lock(&m_liveQuoteTimestampMutex);
        m_latestObservedLiveQuote.clear();
    }
    {
        QMutexLocker lock(&m_symbolMapMutex);
        m_symbolMap = databento::PitSymbolMap{};
    }
    setConnectionState(ConnectionState::Disconnected);
}

void DBClient::subscribeLive(const QString& p_symbol, std::optional<QDateTime> p_liveBackfillStart)
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(
            this,
            [this, p_symbol, p_liveBackfillStart]() { subscribeLive(p_symbol, p_liveBackfillStart); },
            Qt::QueuedConnection);
        return;
    }

    m_requestedLiveSubscriptions.insert(p_symbol, p_liveBackfillStart);

    if (m_subscribedSymbols.contains(p_symbol))
    {
        DEBUG << "Symbol already subscribed:" << p_symbol;
        return;
    }

    if (!m_liveClient)
    {
        DEBUG << "Queued live subscription for" << p_symbol << "until the live client is ready";
        return;
    }

    if (m_connectionState == ConnectionState::Connecting || m_connectionState == ConnectionState::Reconnecting)
    {
        DEBUG << "Queued live subscription for" << p_symbol << "while the session is starting";
        return;
    }

    if (m_connectionState != ConnectionState::Connected)
    {
        WARNING << "Queued live subscription for" << p_symbol << "while connection state is"
                << static_cast<int>(m_connectionState);
        return;
    }

    applyLiveSubscription(p_symbol, p_liveBackfillStart, false);
}

void DBClient::applyLiveSubscription(const QString& p_symbol,
                                     std::optional<QDateTime> p_liveBackfillStart,
                                     const bool p_allowLiveBackfill)
{
    if (m_subscribedSymbols.contains(p_symbol))
    {
        DEBUG << "Symbol already subscribed:" << p_symbol;
        return;
    }

    OBJ_ASSUME_DIFF(m_liveClient.get(), nullptr);

    const std::string sym = p_symbol.toStdString();
    const QString liveDataset = effectiveLiveDataset();
    // Live stream backfill replays historical quote/trade events through the same
    // pipeline used for truly live updates. This can temporarily drive stale BBO,
    // Time&Sales, and sticky-limit pricing decisions. We already warm bars via
    // BarCache historical fetches, so keep subscriptions strictly live-forward.
    const bool useLiveBackfill = false;
    if (p_allowLiveBackfill && p_liveBackfillStart.has_value())
    {
        WARNING << "Ignoring requested live stream backfill for" << p_symbol << "from"
                << p_liveBackfillStart->toString(Qt::ISODate) << "- using live-forward subscription only";
    }

    if (datasetSupportsMbp10(liveDataset))
    {
        if (useLiveBackfill)
        {
            m_liveClient->Subscribe({sym},
                                    databento::Schema::Mbp10,
                                    databento::SType::RawSymbol,
                                    toUnixNanos(p_liveBackfillStart.value()));
        }
        else
        {
            m_liveClient->Subscribe({sym}, databento::Schema::Mbp10, databento::SType::RawSymbol);
        }
    }
    else
    {
        if (useLiveBackfill)
        {
            m_liveClient->Subscribe({sym},
                                    databento::Schema::Mbp1,
                                    databento::SType::RawSymbol,
                                    toUnixNanos(p_liveBackfillStart.value()));
        }
        else
        {
            m_liveClient->Subscribe({sym}, databento::Schema::Mbp1, databento::SType::RawSymbol);
        }
    }

    if (useLiveBackfill)
    {
        m_liveClient->Subscribe({sym},
                                databento::Schema::Trades,
                                databento::SType::RawSymbol,
                                toUnixNanos(p_liveBackfillStart.value()));
    }
    else
    {
        m_liveClient->Subscribe({sym}, databento::Schema::Trades, databento::SType::RawSymbol);
    }

    if (p_liveBackfillStart.has_value() && !useLiveBackfill)
    {
        DEBUG << "Live stream backfill is disabled for" << p_symbol;
    }

    if (datasetSupportsStatus(liveDataset))
    {
        m_liveClient->Subscribe({sym}, databento::Schema::Status, databento::SType::RawSymbol);
    }

    m_subscribedSymbols.insert(p_symbol);
    INFO << "Subscribed to" << p_symbol << "using dataset" << liveDataset
         << (datasetSupportsMbp10(liveDataset) ? "(Mbp10 + Trades" : "(Mbp1 + Trades")
         << (datasetSupportsStatus(liveDataset) ? " + Status)" : ")")
         << (useLiveBackfill
                 ? QStringLiteral("with live backfill from %1").arg(p_liveBackfillStart->toString(Qt::ISODate))
                 : QStringLiteral("without live backfill"));

    scheduleLiveQuoteBootstrap(p_symbol);
}

void DBClient::scheduleLiveQuoteBootstrap(const QString& p_symbol, const int p_delayMs)
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(
            this,
            [this, p_symbol, p_delayMs]() { scheduleLiveQuoteBootstrap(p_symbol, p_delayMs); },
            Qt::QueuedConnection);
        return;
    }

    if (p_symbol.isEmpty())
    {
        return;
    }

    auto it = m_liveQuoteBootstrapStates.find(p_symbol);
    if (it == m_liveQuoteBootstrapStates.end())
    {
        LiveQuoteBootstrapState freshState;
        freshState.windowSeconds = k_liveQuoteBootstrapInitialWindowSeconds;
        freshState.retryDelayMs = k_liveQuoteBootstrapInitialRetryDelayMs;
        it = m_liveQuoteBootstrapStates.insert(p_symbol, freshState);
    }

    LiveQuoteBootstrapState& state = it.value();
    if (state.completed || state.inFlight || state.timerPending)
    {
        return;
    }

    const int delayMs = std::max(0, p_delayMs);
    state.timerPending = true;
    QTimer::singleShot(delayMs,
                       this,
                       [this, p_symbol]()
                       {
                           auto stateIt = m_liveQuoteBootstrapStates.find(p_symbol);
                           if (stateIt == m_liveQuoteBootstrapStates.end())
                           {
                               return;
                           }

                           stateIt->timerPending = false;
                           if (stateIt->completed || stateIt->inFlight)
                           {
                               return;
                           }

                           startLiveQuoteBootstrapAttempt(p_symbol);
                       });
}

void DBClient::startLiveQuoteBootstrapAttempt(const QString& p_symbol)
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(
            this,
            [this, p_symbol]() { startLiveQuoteBootstrapAttempt(p_symbol); },
            Qt::QueuedConnection);
        return;
    }

    if (p_symbol.isEmpty())
    {
        return;
    }

    auto it = m_liveQuoteBootstrapStates.find(p_symbol);
    if (it == m_liveQuoteBootstrapStates.end())
    {
        return;
    }

    LiveQuoteBootstrapState& state = it.value();
    if (state.completed || state.inFlight)
    {
        return;
    }

    if (latestObservedLiveQuote(p_symbol).isValid())
    {
        state.completed = true;
        return;
    }

    if (MainApp::isInReplayMode() || MainApp::isInReviewMode())
    {
        return;
    }

    if (m_connectionState != ConnectionState::Connected || !m_subscribedSymbols.contains(p_symbol))
    {
        scheduleLiveQuoteBootstrap(p_symbol, k_liveQuoteBootstrapWaitForConnectedMs);
        return;
    }

    if (state.attempt >= k_liveQuoteBootstrapMaxAttempts)
    {
        state.completed = true;
        WARNING << "Live quote bootstrap exhausted retries for" << p_symbol;
        return;
    }

    const int attempt = state.attempt + 1;
    state.attempt = attempt;
    state.inFlight = true;

    const int windowSeconds = state.windowSeconds;
    const QDateTime endUtc = QDateTime::currentDateTimeUtc();
    const QDateTime startUtc = endUtc.addSecs(-windowSeconds);
    const QString dataset = effectiveLiveDataset();
    const bool useMbp10 = datasetSupportsMbp10(dataset);
    const databento::Schema schema = useMbp10 ? databento::Schema::Mbp10 : databento::Schema::Mbp1;

    const std::string key = m_apiKey.toStdString();
    const std::string stdSymbol = p_symbol.toStdString();
    INFO << "Live quote bootstrap attempt" << attempt << "for" << p_symbol << "window=" << windowSeconds
         << "seconds using dataset" << dataset << "schema" << (useMbp10 ? "Mbp10" : "Mbp1");

    Q_UNUSED(QtConcurrent::run(
        [this, key, dataset, schema, p_symbol, stdSymbol, startUtc, endUtc, attempt, windowSeconds]()
        {
            std::optional<Level2> seedQuote;
            QString errorText;

            try
            {
                auto historical = databento::Historical::Builder().SetKey(key).Build();
                QDateTime queryEndUtc = endUtc;
                QDateTime queryStartUtc = startUtc;

                while (true)
                {
                    try
                    {
                        const std::string queryStartIso = queryStartUtc.toString(Qt::ISODate).toStdString();
                        const std::string queryEndIso = queryEndUtc.toString(Qt::ISODate).toStdString();
                        historical.TimeseriesGetRange(
                            dataset.toStdString(),
                            databento::DateTimeRange<std::string>{queryStartIso, queryEndIso},
                            {stdSymbol},
                            schema,
                            [this, &seedQuote, &p_symbol](const databento::Record& p_record) -> databento::KeepGoing
                            {
                                m_totalDataReceivedBytes += static_cast<qsizetype>(p_record.Size());

                                if (p_record.Holds<databento::Mbp10Msg>())
                                {
                                    const auto& msg = p_record.Get<databento::Mbp10Msg>();
                                    Level2 level2 = DBRecordTranslator::toLevel2(p_symbol, msg);
                                    if (hasUsableTopOfBook(level2))
                                    {
                                        seedQuote = std::move(level2);
                                    }
                                }
                                else if (p_record.Holds<databento::Mbp1Msg>())
                                {
                                    const auto& msg = p_record.Get<databento::Mbp1Msg>();
                                    Level2 level2 = DBRecordTranslator::toLevel2(p_symbol, msg);
                                    if (hasUsableTopOfBook(level2))
                                    {
                                        seedQuote = std::move(level2);
                                    }
                                }

                                return databento::KeepGoing::Continue;
                            });
                        break;
                    }
                    catch (const std::exception& ex)
                    {
                        const QString candidateError = QString::fromStdString(ex.what());
                        const std::optional<QDateTime> availableEnd = extractAnyHistoricalAvailableEnd(candidateError);
                        if (availableEnd.has_value())
                        {
                            const QDateTime clippedEndUtc = availableEnd->toUTC();
                            if (clippedEndUtc.isValid() && clippedEndUtc < queryEndUtc)
                            {
                                INFO << "Live quote bootstrap clamped request range for" << p_symbol << "to provider"
                                     << "available_end" << clippedEndUtc.toString(Qt::ISODate);
                                queryEndUtc = clippedEndUtc;
                                queryStartUtc = queryEndUtc.addSecs(-windowSeconds);
                                continue;
                            }
                        }

                        errorText = candidateError;
                        break;
                    }
                }
            }
            catch (const std::exception& ex)
            {
                errorText = QString::fromStdString(ex.what());
            }

            QMetaObject::invokeMethod(
                this,
                [this, p_symbol, attempt, windowSeconds, seedQuote, errorText]()
                { finishLiveQuoteBootstrapAttempt(p_symbol, attempt, windowSeconds, seedQuote, errorText); },
                Qt::QueuedConnection);
        }));
}

void DBClient::finishLiveQuoteBootstrapAttempt(const QString& p_symbol,
                                               const int p_attempt,
                                               const int p_windowSeconds,
                                               std::optional<Level2> p_seedQuote,
                                               const QString& p_errorText)
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(
            this,
            [this, p_symbol, p_attempt, p_windowSeconds, p_seedQuote, p_errorText]()
            { finishLiveQuoteBootstrapAttempt(p_symbol, p_attempt, p_windowSeconds, p_seedQuote, p_errorText); },
            Qt::QueuedConnection);
        return;
    }

    auto it = m_liveQuoteBootstrapStates.find(p_symbol);
    if (it == m_liveQuoteBootstrapStates.end())
    {
        return;
    }

    LiveQuoteBootstrapState& state = it.value();
    state.inFlight = false;

    if (state.completed)
    {
        return;
    }

    const QDateTime latestLiveQuote = latestObservedLiveQuote(p_symbol);
    if (latestLiveQuote.isValid())
    {
        state.completed = true;
        INFO << "Live quote bootstrap no longer needed for" << p_symbol << "- live quote already observed at"
             << latestLiveQuote.toString(Qt::ISODate);
        return;
    }

    if (p_seedQuote.has_value())
    {
        state.completed = true;
        INFO << "Seeded startup quote for" << p_symbol << "from historical window" << p_windowSeconds
             << "seconds on attempt" << p_attempt << "at"
             << p_seedQuote->m_timeStamp.toString("yyyy-MM-dd hh:mm:ss.zzz");
        emit dataUsageUpdated(m_totalDataReceivedBytes.load());
        emit newLevel2(p_symbol, *p_seedQuote);
        return;
    }

    if (!p_errorText.isEmpty())
    {
        WARNING << "Live quote bootstrap attempt" << p_attempt << "failed for" << p_symbol << ":" << p_errorText;
    }
    else
    {
        INFO << "Live quote bootstrap attempt" << p_attempt << "found no quote records for" << p_symbol << "within"
             << p_windowSeconds << "seconds";
    }

    if (state.attempt >= k_liveQuoteBootstrapMaxAttempts || !m_subscribedSymbols.contains(p_symbol) ||
        m_connectionState == ConnectionState::Disconnected)
    {
        state.completed = true;
        WARNING << "Stopping live quote bootstrap for" << p_symbol << "after" << state.attempt << "attempts";
        return;
    }

    state.windowSeconds = std::min(state.windowSeconds * 2, k_liveQuoteBootstrapMaxWindowSeconds);
    const int previousRetryDelayMs = state.retryDelayMs;
    state.retryDelayMs = std::min(state.retryDelayMs * 2, k_liveQuoteBootstrapMaxRetryDelayMs);

    // If the query succeeded but returned no quotes, probe wider windows quickly first.
    const bool shouldProbeImmediately = p_errorText.isEmpty() && state.attempt <= 2;
    const int nextDelayMs = shouldProbeImmediately ? 0 : previousRetryDelayMs;

    INFO << "Scheduling live quote bootstrap retry for" << p_symbol << "in" << nextDelayMs << "ms with window"
         << state.windowSeconds << "seconds";
    scheduleLiveQuoteBootstrap(p_symbol, nextDelayMs);
}

void DBClient::recordObservedLiveQuote(const QString& p_symbol, const QDateTime& p_quoteTime)
{
    if (p_symbol.isEmpty() || !p_quoteTime.isValid())
    {
        return;
    }

    const QDateTime quoteUtc = p_quoteTime.toUTC();
    QMutexLocker lock(&m_liveQuoteTimestampMutex);
    auto it = m_latestObservedLiveQuote.find(p_symbol);
    if (it == m_latestObservedLiveQuote.end() || it.value() < quoteUtc)
    {
        m_latestObservedLiveQuote.insert(p_symbol, quoteUtc);
    }
}

QDateTime DBClient::latestObservedLiveQuote(const QString& p_symbol) const
{
    if (p_symbol.isEmpty())
    {
        return {};
    }

    QMutexLocker lock(&m_liveQuoteTimestampMutex);
    return m_latestObservedLiveQuote.value(p_symbol);
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
    const QString configuredDataset = m_dataset;
    const QStringList candidates = datasetCandidates(configuredDataset, m_resolvedLiveDataset);
    const std::string stdSymbol = symbol.toStdString();
    const TimeFrame tf = p_tf;

    // Resolve which Databento schema to actually fetch (may differ from requested tf for non-native)
    const TimeFrame sourceTf = BarUtils::aggregateSourceTimeFrame(tf);
    const databento::Schema schema = schemaForTimeFrame(tf);

    // Databento end is exclusive. Add one source-bar-width so the last bar is included.
    const qint64 stepSecs = static_cast<qint64>(BarUtils::secondsPerBar(sourceTf));
    QDateTime exclusiveEnd = p_end.addSecs(stepSecs);

    // Cap end time to current UTC to avoid requesting past dataset's available_end.
    QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    if (exclusiveEnd.toUTC() > nowUtc)
    {
        exclusiveEnd = nowUtc;
        DEBUG << "Capped historical end time to current UTC:" << exclusiveEnd.toUTC().toString(Qt::ISODate);
    }

    const QDateTime startUtc = p_start.toUTC();
    const QString startIso = startUtc.toString(Qt::ISODate);
    const QString endIso = exclusiveEnd.toUTC().toString(Qt::ISODate);
    const std::string startStr = startIso.toStdString();

    databento::Historical* hist = m_historicalClient.get();

    INFO << "Fetching historical bars for" << symbol << "tf=" << static_cast<int>(tf) << "from" << p_start.toString()
         << "to" << p_end.toString();

    Q_UNUSED(QtConcurrent::run(
        [this,
         hist,
         candidates,
         configuredDataset,
         stdSymbol,
         startStr,
         startUtc,
         endIso,
         symbol,
         tf,
         sourceTf,
         schema]()
        {
            QVector<Bar> bars;
            bool fetchSucceeded = false;

            for (qsizetype index = 0; index < candidates.size(); ++index)
            {
                const QString& candidate = candidates.at(index);
                QString candidateEndIso = endIso;
                bool tryNextDataset = false;
                bars.clear();

                while (true)
                {
                    try
                    {
                        hist->TimeseriesGetRange(
                            candidate.toStdString(),
                            databento::DateTimeRange<std::string>{startStr, candidateEndIso.toStdString()},
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

                        if (candidate != configuredDataset)
                        {
                            sWARNING << "Historical fetch for" << symbol << "fell back from" << configuredDataset
                                     << "to" << candidate << "; top-of-book live mode is active";
                            emit liveGatewayError(
                                QString("Falling back from %1 to %2.\n\n"
                                        "Historical bars remain available, and top-of-book data can be "
                                        "used after a symbol is opened, but live streaming, full depth, "
                                        "and status data still require %1 entitlement.")
                                    .arg(configuredDataset, candidate),
                                false);
                        }
                        QMetaObject::invokeMethod(
                            this,
                            [this, candidate]() { setResolvedLiveDataset(candidate); },
                            Qt::QueuedConnection);
                        sDEBUG << "Historical fetch complete for" << symbol << ":" << bars.size()
                               << "source bars at tf=" << static_cast<int>(sourceTf) << "using dataset" << candidate;
                        emit dataUsageUpdated(m_totalDataReceivedBytes.load());
                        fetchSucceeded = true;
                        break;
                    }
                    catch (const std::exception& ex)
                    {
                        const QString error = QString::fromStdString(ex.what());

                        if (const std::optional<QDateTime> availableEnd = extractHistoricalAvailableEnd(error);
                            availableEnd.has_value())
                        {
                            if (*availableEnd <= startUtc)
                            {
                                sWARNING << "Historical fetch for" << symbol << "on" << candidate
                                         << "reported available_end" << availableEnd->toString(Qt::ISODate)
                                         << "which is not after start" << startUtc.toString(Qt::ISODate) << ":"
                                         << error;
                            }
                            else
                            {
                                const QString adjustedEndIso = availableEnd->toString(Qt::ISODate);
                                if (adjustedEndIso != candidateEndIso)
                                {
                                    sINFO << "Historical fetch for" << symbol << "on" << candidate
                                          << "reported lagging available_end" << adjustedEndIso
                                          << "- retrying with adjusted end";
                                    candidateEndIso = adjustedEndIso;
                                    continue;
                                }
                            }
                        }

                        if (isLiveLicenseError(error) && index + 1 < candidates.size())
                        {
                            sWARNING << "Historical fetch failed for" << symbol << "on" << candidate
                                     << "- retrying with" << candidates.at(index + 1) << ":" << error;
                            tryNextDataset = true;
                            break;
                        }

                        sWARNING << "Historical fetch failed for" << symbol << ":" << error;
                        break;
                    }
                }

                if (fetchSucceeded)
                {
                    break;
                }

                if (!tryNextDataset)
                {
                    break;
                }
            }

            // Aggregate if the requested tf differs from the fetched source tf
            if (fetchSucceeded && tf != sourceTf && !bars.isEmpty())
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
    const bool isCurrentMarketDayRequest = (date == currentMarketDate());
    QString initialReplayDataset = dataset;
    if (dataset == kDefaultLiveDataset && isCurrentMarketDayRequest &&
        g_currentDayXnasNoLiveLicenseJulian.load(std::memory_order_relaxed) == date.toJulianDay())
    {
        initialReplayDataset = kMiniFallbackDataset;
        INFO << "Current-day replay download for" << symbol
             << "skipping XNAS.ITCH due to previously detected missing live license;"
             << "using EQUS.MINI directly";
    }
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
    const QString mbp1Path = getReplayFilePath(date, symbol, "mbp1");
    const QString tradesPath = getReplayFilePath(date, symbol, "trades");
    const QString mbp10TempPath = mbp10Path + ".part";
    const QString mbp1TempPath = mbp1Path + ".part";
    const QString tradesTempPath = tradesPath + ".part";

    INFO << "Downloading replay data for" << symbol << "on" << date.toString(Qt::ISODate);

    // Each concurrent download gets its own Historical client for thread safety
    Q_UNUSED(QtConcurrent::run(
        [this,
         key,
         initialReplayDataset,
         isCurrentMarketDayRequest,
         stdSymbol,
         startStr,
         endStr,
         mbp10Path,
         mbp1Path,
         tradesPath,
         mbp10TempPath,
         mbp1TempPath,
         tradesTempPath,
         symbol,
         date,
         requestId]()
        {
            const auto cleanupInvalidArtifacts = [&]()
            {
                removeInvalidReplayDataFile(mbp10Path, ReplayFileKind::Mbp10);
                removeInvalidReplayDataFile(mbp1Path, ReplayFileKind::Mbp1);
                removeInvalidReplayDataFile(tradesPath, ReplayFileKind::Trades);
                QFile::remove(mbp10TempPath);
                QFile::remove(mbp1TempPath);
                QFile::remove(tradesTempPath);
            };

            try
            {
                cleanupInvalidArtifacts();

                const auto downloadSchemaToFile = [&](const QString& p_dataset,
                                                      databento::Schema p_schema,
                                                      const QString& p_finalPath,
                                                      const QString& p_tempPath,
                                                      const ReplayFileKind p_expectedKind,
                                                      const char* p_schemaName)
                {
                    removeInvalidReplayDataFile(p_finalPath, p_expectedKind);
                    if (isValidReplayDataFile(p_finalPath, p_expectedKind))
                    {
                        sDEBUG << "Skipping" << p_schemaName << "for" << symbol << "(already exists)";
                        return;
                    }

                    QFile::remove(p_tempPath);

                    bool downloaded = false;
                    for (int attempt = 1; attempt <= kReplayDownloadMaxAttempts; ++attempt)
                    {
                        try
                        {
                            sDEBUG << "Downloading" << p_schemaName << "for" << symbol << "using dataset" << p_dataset
                                   << "attempt" << attempt << "/" << kReplayDownloadMaxAttempts;

                            auto hist = databento::Historical::Builder().SetKey(key).Build();
                            hist.TimeseriesGetRangeToFile(p_dataset.toStdString(),
                                                          databento::DateTimeRange<std::string>{startStr, endStr},
                                                          {stdSymbol},
                                                          p_schema,
                                                          std::filesystem::path(p_tempPath.toStdString()));
                            downloaded = true;
                            break;
                        }
                        catch (const std::exception& ex)
                        {
                            const QString errorMessage = QString::fromStdString(ex.what());
                            const bool canRetry = attempt < kReplayDownloadMaxAttempts &&
                                                  isRecoverableReplayDownloadException(errorMessage);
                            if (!canRetry)
                            {
                                throw;
                            }

                            const unsigned long delayMs =
                                kReplayDownloadRetryBaseDelayMs * static_cast<unsigned long>(attempt);
                            sWARNING << "Transient replay download failure for" << p_schemaName << symbol << "attempt"
                                     << attempt << "/" << kReplayDownloadMaxAttempts << "- retrying in" << delayMs
                                     << "ms:" << errorMessage;
                            QThread::msleep(delayMs);
                        }
                    }

                    if (!downloaded)
                    {
                        throw std::runtime_error(QString("Failed to download %1 for %2 after %3 attempts")
                                                     .arg(p_schemaName)
                                                     .arg(symbol)
                                                     .arg(kReplayDownloadMaxAttempts)
                                                     .toStdString());
                    }

                    const std::optional<QString> invalidReason =
                        replayDataFileInvalidReason(p_tempPath, p_expectedKind);
                    if (invalidReason.has_value())
                    {
                        QFile::remove(p_tempPath);
                        throw std::runtime_error(QString("Replay download produced an unusable %1 file for %2: %3")
                                                     .arg(p_schemaName, symbol, *invalidReason)
                                                     .toStdString());
                    }

                    QFile::remove(p_finalPath);
                    if (!QFile::rename(p_tempPath, p_finalPath))
                    {
                        QFile::remove(p_tempPath);
                        throw std::runtime_error(QString("Failed to finalize %1 replay file for %2 at %3")
                                                     .arg(p_schemaName, symbol, p_finalPath)
                                                     .toStdString());
                    }
                };

                auto downloadReplayPayloads = [&](const QString& p_dataset)
                {
                    const QString level2SchemaName = replayLevel2SchemaNameForDataset(p_dataset);
                    const QString level2Path = getReplayFilePath(date, symbol, level2SchemaName);
                    const QString level2TempPath = level2Path + ".part";
                    const ReplayFileKind level2FileKind = replayLevel2FileKindForDataset(p_dataset);

                    downloadSchemaToFile(p_dataset,
                                         replayLevel2SchemaForDataset(p_dataset),
                                         level2Path,
                                         level2TempPath,
                                         level2FileKind,
                                         level2SchemaName == "mbp10" ? "Mbp10" : "Mbp1");
                    downloadSchemaToFile(p_dataset,
                                         databento::Schema::Trades,
                                         tradesPath,
                                         tradesTempPath,
                                         ReplayFileKind::Trades,
                                         "Trades");
                    return level2SchemaName;
                };

                QString downloadedDataset = initialReplayDataset;
                QString downloadedLevel2Schema;
                try
                {
                    downloadedLevel2Schema = downloadReplayPayloads(downloadedDataset);
                }
                catch (const std::exception& ex)
                {
                    const QString errorMessage = QString::fromStdString(ex.what());
                    if (downloadedDataset == kDefaultLiveDataset && isCurrentMarketDayRequest &&
                        isLiveLicenseError(errorMessage))
                    {
                        g_currentDayXnasNoLiveLicenseJulian.store(date.toJulianDay(), std::memory_order_relaxed);
                    }
                    if (!shouldRetryReplayDownloadWithMini(downloadedDataset, date, errorMessage))
                    {
                        throw;
                    }

                    downloadedDataset = kMiniFallbackDataset;
                    sWARNING << "Current-day replay download for" << symbol << "on" << date.toString(Qt::ISODate)
                             << "failed on XNAS.ITCH - retrying with EQUS.MINI top-of-book replay:" << errorMessage;
                    downloadedLevel2Schema = downloadReplayPayloads(downloadedDataset);
                }

                sINFO << "Replay download complete for" << symbol << "on" << date.toString(Qt::ISODate) << "using"
                      << downloadedDataset << downloadedLevel2Schema;
                emit replayDownloadFinished(symbol, date, true, {});
                if (!requestId.isEmpty())
                {
                    emit replayDownloadFinishedForRequest(requestId, symbol, date, true, {});
                }
            }
            catch (const std::exception& ex)
            {
                cleanupInvalidArtifacts();
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
    const bool hasMbp10 = isValidReplayDataFile(getReplayFilePath(p_date, p_symbol, "mbp10"), ReplayFileKind::Mbp10);
    const bool hasMbp1 = isValidReplayDataFile(getReplayFilePath(p_date, p_symbol, "mbp1"), ReplayFileKind::Mbp1);
    return (hasMbp10 || hasMbp1) &&
           isValidReplayDataFile(getReplayFilePath(p_date, p_symbol, "trades"), ReplayFileKind::Trades);
}

QFuture<std::optional<std::shared_ptr<QVector<Bar>>>>
DBClient::loadReplayOneMinuteBarsFromTrades(const QString& p_symbol, const QDate& p_date)
{
    const QString symbol = p_symbol;
    const QDate date = p_date;

    return QtConcurrent::run(
        [symbol, date]() -> std::optional<std::shared_ptr<QVector<Bar>>>
        {
            if (symbol.isEmpty() || !date.isValid())
            {
                sWARNING << "Refusing replay bar reconstruction with invalid inputs:" << symbol << date;
                return std::nullopt;
            }

            const QString tradesPath = getReplayFilePath(date, symbol, "trades");
            if (!QFile::exists(tradesPath))
            {
                sDEBUG << "No replay trades file available for local bar reconstruction:" << tradesPath;
                return std::nullopt;
            }

            try
            {
                databento::DbnFileStore tradeStore(std::filesystem::path(tradesPath.toStdString()));

                auto bars = std::make_shared<QVector<Bar>>();
                bars->reserve(BarUtils::barsPerDay(TimeFrame::ONE_MINUTE));

                for (int index = 0; index < BarUtils::barsPerDay(TimeFrame::ONE_MINUTE); ++index)
                {
                    bars->append(Bar::nullBar(QDateTime(date,
                                                        BarUtils::indexToBarTime(TimeFrame::ONE_MINUTE, index),
                                                        TradingHours::MARKET_TIMEZONE)));
                }

                int tradeCount = 0;
                const databento::Record* record = nullptr;
                while ((record = tradeStore.NextRecord()) != nullptr)
                {
                    if (!record->Holds<databento::TradeMsg>())
                    {
                        continue;
                    }

                    const Trade trade = DBRecordTranslator::toTrade(symbol, record->Get<databento::TradeMsg>());
                    const QDateTime tradeTs = trade.m_timestamp;

                    if (tradeTs.date() != date)
                    {
                        continue;
                    }

                    if (tradeTs.time() < TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION ||
                        tradeTs.time() > TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
                    {
                        continue;
                    }

                    const int barIndex = BarUtils::barIndex(TimeFrame::ONE_MINUTE, tradeTs.time());
                    const QDateTime barOpenTime(date,
                                                BarUtils::indexToBarTime(TimeFrame::ONE_MINUTE, barIndex),
                                                TradingHours::MARKET_TIMEZONE);
                    const float tradePrice = static_cast<float>(trade.m_price);
                    const quint64 tradeSize = static_cast<quint64>(trade.m_size);

                    Bar& slot = (*bars)[barIndex];
                    if (slot.getBarStatus() == Bar::BarStatus::Null)
                    {
                        slot = Bar(barOpenTime,
                                   tradePrice,
                                   tradePrice,
                                   tradePrice,
                                   tradePrice,
                                   tradeSize,
                                   Bar::BarStatus::Closed);
                    }
                    else
                    {
                        slot = Bar(barOpenTime,
                                   slot.getOpen(),
                                   std::max(slot.getHigh(), tradePrice),
                                   std::min(slot.getLow(), tradePrice),
                                   tradePrice,
                                   slot.getTotalVolume() + tradeSize,
                                   Bar::BarStatus::Closed);
                    }

                    ++tradeCount;
                }

                sDEBUG << "Reconstructed" << bars->size() << "full-day 1m bars from replay trades for" << symbol << "on"
                       << date.toString(Qt::ISODate) << "using" << tradeCount << "trades";
                return bars;
            }
            catch (const std::exception& ex)
            {
                sWARNING << "Failed to reconstruct local replay bars for" << symbol << "on"
                         << date.toString(Qt::ISODate) << ":" << ex.what();
                return std::nullopt;
            }
        });
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

QVector<QDate> DBClient::listAvailableReplayDatesForSymbol(const QString& p_symbol)
{
    if (p_symbol.isEmpty())
    {
        return {};
    }

    const QVector<ReplayDayInfo> days = listAvailableReplayDates();
    QVector<QDate> matchingDates;
    matchingDates.reserve(days.size());
    for (const ReplayDayInfo& day: days)
    {
        if (hasReplayData(day.date, p_symbol))
        {
            matchingDates.append(day.date);
        }
    }

    return matchingDates;
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
    static const QRegularExpression mbp1Re("^(.+)_mbp1\\.dbn\\.zst$");
    static const QRegularExpression tradesRe("^(.+)_trades\\.dbn\\.zst$");

    QMap<QString, ReplaySymbolInfo> symbolMap;
    for (const QString& fileName: files)
    {
        const QFileInfo fileInfo(dir.absoluteFilePath(fileName));
        if (fileInfo.size() <= 0)
        {
            continue;
        }

        const QRegularExpressionMatch mbp10Match = mbp10Re.match(fileName);
        if (mbp10Match.hasMatch())
        {
            const QString symbol = mbp10Match.captured(1);
            ReplaySymbolInfo& info = symbolMap[symbol];
            info.symbol = symbol;
            info.hasMbp10 = true;
            info.mbp10SizeBytes = fileInfo.size();
            continue;
        }

        const QRegularExpressionMatch mbp1Match = mbp1Re.match(fileName);
        if (mbp1Match.hasMatch())
        {
            const QString symbol = mbp1Match.captured(1);
            ReplaySymbolInfo& info = symbolMap[symbol];
            info.symbol = symbol;
            info.hasMbp1 = true;
            info.mbp1SizeBytes = fileInfo.size();
            continue;
        }

        const QRegularExpressionMatch tradesMatch = tradesRe.match(fileName);
        if (tradesMatch.hasMatch())
        {
            const QString symbol = tradesMatch.captured(1);
            ReplaySymbolInfo& info = symbolMap[symbol];
            info.symbol = symbol;
            info.hasTrades = true;
            info.tradesSizeBytes = fileInfo.size();
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

QString DBClient::effectiveLiveDataset() const
{
    return m_resolvedLiveDataset.isEmpty() ? m_dataset : m_resolvedLiveDataset;
}

void DBClient::setResolvedLiveDataset(const QString& p_dataset)
{
    OBJ_ASSUME_FALSE(p_dataset.isEmpty());
    m_resolvedLiveDataset = p_dataset;
}

void DBClient::setDataset(const QString& p_dataset)
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(
            this,
            [this, p_dataset]() { setDataset(p_dataset); },
            Qt::QueuedConnection);
        return;
    }

    if (m_dataset == p_dataset)
        return;

    m_dataset = p_dataset;
    m_resolvedLiveDataset.clear();

    if (appStateSettings != nullptr)
    {
        appStateSettings->setValue(k_settingsKeyDataset, m_dataset);
    }

    INFO << "Dataset changed to:" << m_dataset;
}

// ── Live callbacks (run on Databento's internal thread) ────────────────────

void DBClient::onMetadataReceived(databento::Metadata&& p_metadata)
{
    const QString liveDataset = QString::fromStdString(p_metadata.dataset);
    setResolvedLiveDataset(liveDataset);
    m_subscribedSymbols.clear();
    for (auto it = m_requestedLiveSubscriptions.cbegin(); it != m_requestedLiveSubscriptions.cend(); ++it)
    {
        m_subscribedSymbols.insert(it.key());
    }
    INFO << "Metadata received — dataset:" << liveDataset << "symbols:" << p_metadata.symbols.size();

    // Build symbol map for today
    const auto now = std::chrono::system_clock::now();
    const auto today = date::floor<date::days>(now);
    const auto ymd = date::year_month_day{today};

    {
        QMutexLocker lock(&m_symbolMapMutex);
        try
        {
            m_symbolMap = p_metadata.CreateSymbolMapForDate(ymd);
        }
        catch (const std::exception& ex)
        {
            WARNING << "Live metadata did not provide an InstrumentId-based symbol map for dataset" << liveDataset
                    << "- waiting for SymbolMapping records instead:" << ex.what();
            m_symbolMap = databento::PitSymbolMap{};
        }
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

    // Live records must never flow while replay/review is active.
    // Replay data is emitted by DBClient's replay engine, not by LiveThreaded callbacks.
    if (MainApp::isInReplayMode() || MainApp::isInReviewMode())
    {
        return databento::KeepGoing::Continue;
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
        if (hasUsableTopOfBook(level2))
        {
            recordObservedLiveQuote(symbol, level2.m_timeStamp);
        }
        emit newLevel2(symbol, level2);
    }
    else if (p_record.Holds<databento::Mbp1Msg>())
    {
        const auto& msg = p_record.Get<databento::Mbp1Msg>();
        Level2 level2 = DBRecordTranslator::toLevel2(symbol, msg);
        if (hasUsableTopOfBook(level2))
        {
            recordObservedLiveQuote(symbol, level2.m_timeStamp);
        }
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
    const QString errorText = QString::fromStdString(p_exception.what());
    CRITICAL << "Live session exception:" << errorText;

    if (!isRecoverableLiveException(errorText))
    {
        setConnectionState(ConnectionState::Disconnected);
        emit liveGatewayError(errorText, true);
        return databento::LiveThreaded::ExceptionAction::Stop;
    }

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

void DBClient::advanceLevel2(SymbolStream& p_stream)
{
    if (p_stream.bufferedLevel2.valid)
    {
        p_stream.nextLevel2 = p_stream.bufferedLevel2;
        p_stream.bufferedLevel2.valid = false;
        return;
    }

    p_stream.nextLevel2.valid = false;
    if (p_stream.level2Store == nullptr)
        return;

    const databento::Record* record = nullptr;
    PeekedRecord latestPreStartSnapshot;
    while ((record = p_stream.level2Store->NextRecord()) != nullptr)
    {
        std::optional<PeekedRecord> convertedRecord;
        if (p_stream.level2Schema == "mbp10" && record->Holds<databento::Mbp10Msg>())
        {
            const auto& msg = record->Get<databento::Mbp10Msg>();
            const qint64 epochMs = toEpochMs(msg.hd.ts_event);
            const Level2 level2 = DBRecordTranslator::toLevel2(p_stream.symbol, msg);
            convertedRecord = PeekedRecord{epochMs, level2, true};
        }
        else if (p_stream.level2Schema == "mbp1" && record->Holds<databento::Mbp1Msg>())
        {
            const auto& msg = record->Get<databento::Mbp1Msg>();
            const qint64 epochMs = toEpochMs(msg.hd.ts_event);
            const Level2 level2 = DBRecordTranslator::toLevel2(p_stream.symbol, msg);
            convertedRecord = PeekedRecord{epochMs, level2, true};
        }

        if (!convertedRecord.has_value())
        {
            continue;
        }

        if (convertedRecord->epochMs < m_startEpochMs)
        {
            latestPreStartSnapshot = *convertedRecord;
            continue;
        }

        if (latestPreStartSnapshot.valid)
        {
            p_stream.nextLevel2 = latestPreStartSnapshot;
            p_stream.bufferedLevel2 = *convertedRecord;
            DEBUG << "Bootstrapped replay Level2 for" << p_stream.symbol << "from"
                  << QDateTime::fromMSecsSinceEpoch(latestPreStartSnapshot.epochMs, TradingHours::MARKET_TIMEZONE)
                         .toString("yyyy-MM-dd hh:mm:ss.zzz")
                  << "while preserving first post-start update at"
                  << QDateTime::fromMSecsSinceEpoch(convertedRecord->epochMs, TradingHours::MARKET_TIMEZONE)
                         .toString("yyyy-MM-dd hh:mm:ss.zzz")
                  << "using" << p_stream.level2Schema;
            return;
        }

        p_stream.nextLevel2 = *convertedRecord;
        return;
    }

    if (latestPreStartSnapshot.valid)
    {
        p_stream.nextLevel2 = latestPreStartSnapshot;
        DEBUG << "Bootstrapped replay Level2 for" << p_stream.symbol << "from latest pre-start snapshot at"
              << QDateTime::fromMSecsSinceEpoch(latestPreStartSnapshot.epochMs, TradingHours::MARKET_TIMEZONE)
                     .toString("yyyy-MM-dd hh:mm:ss.zzz")
              << "using" << p_stream.level2Schema;
        return;
    }

    p_stream.level2Store.reset();
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

    const auto openLevel2Stream = [&](const QString& p_schema)
    {
        const QString level2Path = getReplayFilePath(p_date, p_symbol, p_schema);
        if (!QFile::exists(level2Path))
        {
            DEBUG << "No" << p_schema << "file found:" << level2Path;
            return false;
        }

        try
        {
            stream.level2Store =
                std::make_unique<databento::DbnFileStore>(std::filesystem::path(level2Path.toStdString()));
            stream.level2Schema = p_schema;
            advanceLevel2(stream);
            INFO << "Opened" << p_schema << "stream:" << level2Path;
            opened = true;
            return true;
        }
        catch (const std::exception& ex)
        {
            WARNING << "Failed to open" << p_schema << "file:" << ex.what();
            stream.level2Store.reset();
            stream.level2Schema.clear();
            return false;
        }
    };

    if (!openLevel2Stream("mbp10"))
    {
        openLevel2Stream("mbp1");
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
        if (s.nextLevel2.valid || s.nextTrade.valid)
            return true;
    }
    return false;
}

qint64 DBClient::earliestReplayEpoch() const
{
    qint64 earliest = std::numeric_limits<qint64>::max();
    for (const auto& s: m_replayStreams)
    {
        if (s.nextLevel2.valid)
            earliest = qMin(earliest, s.nextLevel2.epochMs);
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

    if (p_symbol.isEmpty())
    {
        WARNING << "addReplaySymbol called with an empty symbol";
        return false;
    }

    if (!isReplayActive())
    {
        WARNING << "addReplaySymbol called but replay is not active";
        return false;
    }

    INFO << "Adding symbol to active replay:" << p_symbol;
    const std::size_t streamCountBeforeOpen = m_replayStreams.size();
    const bool hadNoStreams = streamCountBeforeOpen == 0;
    const qint64 replayEpochSnapshot =
        MainApp::currentAppReplayTime.isValid() ? MainApp::currentAppReplayTime.toMSecsSinceEpoch() : m_startEpochMs;
    const bool opened = openReplayStreamsForSymbol(p_symbol, m_replayDate);
    if (!opened)
    {
        return false;
    }

    const bool appendedStream = m_replayStreams.size() > streamCountBeforeOpen;
    if (!hadNoStreams && appendedStream)
    {
        SymbolStream& stream = m_replayStreams.back();
        const qint64 catchupEpochMs = qMax(m_startEpochMs, replayEpochSnapshot);
        const QDateTime catchupTime = QDateTime::fromMSecsSinceEpoch(catchupEpochMs, TradingHours::MARKET_TIMEZONE);
        INFO << "[ReplayCatchup] Fast-forwarding newly added replay symbol" << p_symbol << "to"
             << catchupTime.toString("yyyy-MM-dd hh:mm:ss.zzz");

        std::uint64_t skippedLevel2 = 0;
        std::uint64_t skippedTrades = 0;
        while (stream.nextLevel2.valid && stream.nextLevel2.epochMs < catchupEpochMs)
        {
            ++skippedLevel2;
            advanceLevel2(stream);
        }
        while (stream.nextTrade.valid && stream.nextTrade.epochMs < catchupEpochMs)
        {
            ++skippedTrades;
            advanceTrade(stream);
        }

        const QString nextLevel2Time =
            stream.nextLevel2.valid
                ? QDateTime::fromMSecsSinceEpoch(stream.nextLevel2.epochMs, TradingHours::MARKET_TIMEZONE)
                      .toString("yyyy-MM-dd hh:mm:ss.zzz")
                : QStringLiteral("<none>");
        const QString nextTradeTime =
            stream.nextTrade.valid
                ? QDateTime::fromMSecsSinceEpoch(stream.nextTrade.epochMs, TradingHours::MARKET_TIMEZONE)
                      .toString("yyyy-MM-dd hh:mm:ss.zzz")
                : QStringLiteral("<none>");
        INFO << "[ReplayCatchup] Completed fast-forward for" << p_symbol << "- skipped level2:" << skippedLevel2
             << "skipped trades:" << skippedTrades << "next level2:" << nextLevel2Time
             << "next trade:" << nextTradeTime;
    }
    else if (!hadNoStreams && !appendedStream)
    {
        DEBUG << "[ReplayCatchup] Replay symbol" << p_symbol << "already active; skipped fast-forward";
    }

    if (hadNoStreams && m_playbackState == PlaybackState::Paused)
    {
        if (!MainApp::currentAppReplayTime.isValid())
        {
            MainApp::currentAppReplayTime =
                QDateTime::fromMSecsSinceEpoch(m_startEpochMs, TradingHours::MARKET_TIMEZONE);
        }
        const qint64 pausedReplayEpoch = MainApp::currentAppReplayTime.toMSecsSinceEpoch();

        INFO << "Initialized paused replay after loading first symbol:" << p_symbol << "at"
             << MainApp::currentAppReplayTime.toString("yyyy-MM-dd hh:mm:ss.zzz");

        while (hasAnyReplayRecord() && earliestReplayEpoch() <= pausedReplayEpoch)
        {
            emitNextReplayRecord();
        }

        emit replayPaused();
    }

    return true;
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

    startReplaySession(p_symbol, p_date, p_startTime, p_speed, false);
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

    startReplaySession(p_symbol, p_date, p_startTime, p_speed, true);
}

void DBClient::startReplaySession(const QString& p_symbol,
                                  QDate p_date,
                                  QTime p_startTime,
                                  PlaybackSpeed p_speed,
                                  const bool p_startPaused)
{
    if (m_playbackState != PlaybackState::Stopped)
    {
        WARNING << "Cannot start replay — already active";
        return;
    }

    INFO << "Starting replay" << (p_startPaused ? "(paused)" : "(playing)") << "for"
         << (p_symbol.isEmpty() ? QString("<no symbol>") : p_symbol) << "on" << p_date.toString(Qt::ISODate) << "at"
         << p_startTime.toString("hh:mm:ss") << "speed:" << static_cast<int>(p_speed);

    m_replayTimer.stop();
    m_playbackSpeed = p_speed;
    m_replayDate = p_date;
    m_startEpochMs = QDateTime(p_date, p_startTime, TradingHours::MARKET_TIMEZONE).toMSecsSinceEpoch();
    m_wallClockAnchorMs = 0;
    m_replayEpochAnchorMs = m_startEpochMs;
    m_pauseWallClockMs = 0;
    closeReplayStreams();
    MainApp::currentAppReplayTime = QDateTime(p_date, p_startTime, TradingHours::MARKET_TIMEZONE);

    const auto startReplayWithoutInitialSymbol = [this, p_date, p_startTime]()
    {
        WARNING << "Starting replay without an initial symbol; session will remain paused until a symbol is selected";
        m_playbackState = PlaybackState::Paused;
        m_wallClockAnchorMs = 0;
        m_pauseWallClockMs = QDateTime::currentMSecsSinceEpoch();
        MainApp::currentAppReplayTime = QDateTime(p_date, p_startTime, TradingHours::MARKET_TIMEZONE);
        emit replayPaused();
    };

    if (p_symbol.isEmpty())
    {
        startReplayWithoutInitialSymbol();
        return;
    }

    if (!openReplayStreamsForSymbol(p_symbol, p_date))
    {
        CRITICAL << "Failed to load replay data for initial symbol" << p_symbol << "on" << p_date.toString(Qt::ISODate)
                 << "- keeping replay paused so strategy symbols can attach";
        emit replayDataLoadFailed(
            QString("No replay data found for %1 on %2").arg(p_symbol, p_date.toString(Qt::ISODate)));
        startReplayWithoutInitialSymbol();
        return;
    }

    QDateTime initialTime = QDateTime::fromMSecsSinceEpoch(m_startEpochMs, TradingHours::MARKET_TIMEZONE);
    MainApp::currentAppReplayTime = initialTime;
    INFO << "Initialized replay time to:" << initialTime.toString("yyyy-MM-dd hh:mm:ss.zzz");

    m_replayEpochAnchorMs = m_startEpochMs;
    m_wallClockAnchorMs = p_startPaused ? 0 : QDateTime::currentMSecsSinceEpoch();

    if (p_startPaused)
    {
        while (hasAnyReplayRecord() && earliestReplayEpoch() <= m_startEpochMs)
        {
            emitNextReplayRecord();
        }

        m_playbackState = PlaybackState::Paused;
        emit replayPaused();
        INFO << "Replay started in paused state at configured start time";
        return;
    }

    m_playbackState = PlaybackState::Playing;
    emit replayStarted();
    scheduleNextReplayTick();
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

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 currentReplayMs = MainApp::currentAppReplayTime.isValid()
                                       ? MainApp::currentAppReplayTime.toMSecsSinceEpoch()
                                       : (m_replayEpochAnchorMs > 0 ? m_replayEpochAnchorMs : m_startEpochMs);
    // Always re-anchor replay time on resume. This keeps replay progression
    // continuous when speed is changed while paused.
    m_replayEpochAnchorMs = currentReplayMs;
    m_wallClockAnchorMs = now;
    m_pauseWallClockMs = 0;

    m_playbackState = PlaybackState::Playing;

    scheduleNextReplayTick();

    emit replayResumed();
}

void DBClient::setReplaySpeed(PlaybackSpeed p_speed)
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(
            this,
            [this, p_speed]() { setReplaySpeed(p_speed); },
            Qt::QueuedConnection);
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

    syncReplayTimeToWallClock();

    const qint64 startWallMs = QDateTime::currentMSecsSinceEpoch();
    int eventsEmitted = 0;

    while (m_playbackState == PlaybackState::Playing)
    {
        const qint64 dueEpoch = currentReplayEpochFromWallClock();
        const qint64 nextEpoch = earliestReplayEpoch();
        if (nextEpoch == std::numeric_limits<qint64>::max() || nextEpoch > dueEpoch)
            break;

        emitNextReplayRecord();
        ++eventsEmitted;

        if (!hasAnyReplayRecord())
        {
            INFO << "Replay reached end of data (batch loop)";
            m_playbackState = PlaybackState::Stopped;
            emit replayEndReached();
            emit replayStopped();
            LTTnG_TP(opentraderplatform, replay_tick, eventsEmitted, static_cast<long>(m_replayEpochAnchorMs));
            return;
        }

        if (QDateTime::currentMSecsSinceEpoch() - startWallMs >= ReplayConstants::MAX_SPEED_BATCH_BUDGET_MS)
            break;

        const qint64 upcomingEpoch = earliestReplayEpoch();
        if (calculateWallClockDelay(upcomingEpoch) > 0)
            break;
    }

    LTTnG_TP(opentraderplatform, replay_tick, eventsEmitted, static_cast<long>(m_replayEpochAnchorMs));

    if (m_playbackState == PlaybackState::Playing)
        scheduleNextReplayTick();
}

void DBClient::emitNextReplayRecord()
{
    // Find the stream with the globally earliest next record
    SymbolStream* bestStream = nullptr;
    bool useLevel2 = false;
    qint64 bestEpoch = std::numeric_limits<qint64>::max();

    for (auto& stream: m_replayStreams)
    {
        if (stream.nextLevel2.valid && stream.nextLevel2.epochMs < bestEpoch)
        {
            bestEpoch = stream.nextLevel2.epochMs;
            bestStream = &stream;
            useLevel2 = true;
        }
        if (stream.nextTrade.valid && stream.nextTrade.epochMs < bestEpoch)
        {
            bestEpoch = stream.nextTrade.epochMs;
            bestStream = &stream;
            useLevel2 = false;
        }
    }

    if (!bestStream)
        return;

    if (useLevel2)
    {
        updateReplayTime(bestStream->nextLevel2.epochMs);
        emit newLevel2(bestStream->symbol, std::get<Level2>(bestStream->nextLevel2.data));
        advanceLevel2(*bestStream);
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
        // Allow timeline progression with no loaded symbols/records.
        // This keeps replay usable for strategy/event workflows where the displayed chart symbol
        // has no local data for the selected day.
        if (m_replayStreams.empty() && m_playbackState == PlaybackState::Playing)
        {
            const qint64 stopEpoch = replaySessionStopEpochMs(m_replayDate);
            const qint64 currentEpoch = currentReplayEpochFromWallClock();

            if (currentEpoch >= stopEpoch)
            {
                INFO << "Replay reached end of session time with no active replay streams";
                m_playbackState = PlaybackState::Stopped;
                emit replayEndReached();
                emit replayStopped();
                return;
            }

            const qint64 remainingMs = stopEpoch - currentEpoch;
            const qint64 delay = qMax<qint64>(1, qMin<qint64>(remainingMs, ReplayConstants::CLOCK_UPDATE_INTERVAL_MS));
            m_replayTimer.start(static_cast<int>(delay));
            return;
        }

        INFO << "Replay reached end of data";
        m_playbackState = PlaybackState::Stopped;
        emit replayEndReached();
        emit replayStopped();
        return;
    }

    const qint64 nextEpoch = earliestReplayEpoch();
    const qint64 nextEventDelay = calculateWallClockDelay(nextEpoch);
    const qint64 delay = (m_playbackSpeed == PlaybackSpeed::AsFastAsPossible)
                             ? nextEventDelay
                             : qMin(nextEventDelay, qint64(ReplayConstants::CLOCK_UPDATE_INTERVAL_MS));
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

qint64 DBClient::currentReplayEpochFromWallClock() const
{
    if (m_playbackSpeed == PlaybackSpeed::AsFastAsPossible)
    {
        const qint64 nextEpoch = earliestReplayEpoch();
        return nextEpoch == std::numeric_limits<qint64>::max() ? m_replayEpochAnchorMs : nextEpoch;
    }

    const int speedValue = static_cast<int>(m_playbackSpeed);
    if (speedValue <= 0 || m_wallClockAnchorMs == 0)
    {
        return m_replayEpochAnchorMs;
    }

    const qint64 wallClockElapsedMs = QDateTime::currentMSecsSinceEpoch() - m_wallClockAnchorMs;
    return m_replayEpochAnchorMs + ((wallClockElapsedMs * speedValue) / 100);
}

void DBClient::syncReplayTimeToWallClock()
{
    updateReplayTime(currentReplayEpochFromWallClock());
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
