#include "ProcessStrategyRuntimeBackend.h"

#include <algorithm>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QMetaObject>
#include <QProcessEnvironment>
#include <QTimer>
#include <QUuid>

#include <array>
#include <cerrno>
#include <cstring>
#include <utility>
#include <vector>

#include <fcntl.h>

#ifdef FATAL
#undef FATAL
#endif

#include <google/protobuf/message_lite.h>
#include <google/protobuf/struct.pb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "L2Trader/StrategyProtocol/GeneratedProtocol.h"
#include "L2Trader/StrategyProtocol/ProtocolVersion.h"
#include "L2Trader/StrategySDK/MessageFraming.h"
#include "../Algo/MainAlgo.h"
#include "StrategyConfig.h"
#include "StrategyLogger.h"
#include "StrategyManager.h"
#include "StrategySDK.h"

Q_DECLARE_LOGGING_CATEGORY(StrategyManagerLog)

namespace
{
    namespace Protocol = l2trader::strategy::v1;

    constexpr int kStartupTimeoutMs = 5000;
    constexpr int kShutdownTimeoutMs = 5000;
    constexpr int kTerminateTimeoutMs = 2000;
    constexpr std::size_t kSocketReadChunkSize = 4096;

    [[nodiscard]] bool waitForFd(const int p_fd, const short p_events, const int p_timeoutMs)
    {
        pollfd descriptor{};
        descriptor.fd = p_fd;
        descriptor.events = p_events;

        while (true)
        {
            const int result = ::poll(&descriptor, 1, p_timeoutMs);
            if (result > 0)
            {
                return (descriptor.revents & p_events) != 0;
            }

            if (result == 0)
            {
                return false;
            }

            if (errno != EINTR)
            {
                return false;
            }
        }
    }

    [[nodiscard]] bool writeAll(const int p_fd, const std::span<const std::uint8_t> p_buffer)
    {
        std::size_t bytesWritten = 0;
        while (bytesWritten < p_buffer.size())
        {
            const ssize_t result = ::write(p_fd, p_buffer.data() + bytesWritten, p_buffer.size() - bytesWritten);
            if (result < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                return false;
            }

            if (result == 0)
            {
                return false;
            }

            bytesWritten += static_cast<std::size_t>(result);
        }

        return true;
    }

    [[nodiscard]] bool readAllWithTimeout(const int p_fd, const std::span<std::uint8_t> p_buffer, const int p_timeoutMs)
    {
        std::size_t bytesRead = 0;
        while (bytesRead < p_buffer.size())
        {
            if (!waitForFd(p_fd, POLLIN, p_timeoutMs))
            {
                return false;
            }

            const ssize_t result = ::read(p_fd, p_buffer.data() + bytesRead, p_buffer.size() - bytesRead);
            if (result < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                return false;
            }

            if (result == 0)
            {
                return false;
            }

            bytesRead += static_cast<std::size_t>(result);
        }

        return true;
    }

    [[nodiscard]] bool
    readFrameWithTimeout(const int p_fd, std::vector<std::uint8_t>* const p_payload, const int p_timeoutMs)
    {
        if (p_payload == nullptr)
        {
            return false;
        }

        std::array<std::uint8_t, L2Trader::StrategySDK::kFramePrefixSize> prefix{};
        if (!readAllWithTimeout(p_fd, prefix, p_timeoutMs))
        {
            return false;
        }

        const std::uint32_t payloadSize = L2Trader::StrategySDK::decodeFrameSize(prefix);
        p_payload->assign(payloadSize, std::uint8_t{0});
        if (payloadSize == 0)
        {
            return true;
        }

        return readAllWithTimeout(p_fd, *p_payload, p_timeoutMs);
    }

    [[nodiscard]] bool writeMessage(const int p_fd, const google::protobuf::MessageLite& p_message)
    {
        const std::vector<std::uint8_t> frame = L2Trader::StrategySDK::serializeFramedMessage(p_message);
        if (frame.empty())
        {
            return false;
        }

        return writeAll(p_fd, frame);
    }

    [[nodiscard]] QtMsgType toQtMessageType(const Protocol::LogLevel p_level)
    {
        switch (p_level)
        {
        case Protocol::LOG_LEVEL_DEBUG:
            return QtDebugMsg;
        case Protocol::LOG_LEVEL_WARNING:
            return QtWarningMsg;
        case Protocol::LOG_LEVEL_ERROR:
            return QtCriticalMsg;
        case Protocol::LOG_LEVEL_INFO:
        case Protocol::LOG_LEVEL_UNSPECIFIED:
        default:
            return QtInfoMsg;
        }
    }

    void populateProtobufValue(const QJsonValue& p_jsonValue, google::protobuf::Value* const p_value)
    {
        if (p_value == nullptr)
        {
            return;
        }

        switch (p_jsonValue.type())
        {
        case QJsonValue::Null:
        case QJsonValue::Undefined:
            p_value->set_null_value(google::protobuf::NullValue::NULL_VALUE);
            return;

        case QJsonValue::Bool:
            p_value->set_bool_value(p_jsonValue.toBool());
            return;

        case QJsonValue::Double:
            p_value->set_number_value(p_jsonValue.toDouble());
            return;

        case QJsonValue::String:
            p_value->set_string_value(p_jsonValue.toString().toStdString());
            return;

        case QJsonValue::Array:
        {
            auto* const listValue = p_value->mutable_list_value();
            for (const QJsonValue& entry: p_jsonValue.toArray())
            {
                populateProtobufValue(entry, listValue->add_values());
            }
            return;
        }

        case QJsonValue::Object:
        {
            auto* const structValue = p_value->mutable_struct_value();
            const QJsonObject jsonObject = p_jsonValue.toObject();
            for (auto it = jsonObject.begin(); it != jsonObject.end(); ++it)
            {
                populateProtobufValue(it.value(), &(*structValue->mutable_fields())[it.key().toStdString()]);
            }
            return;
        }
        }
    }

    void populateCustomParams(const std::map<QString, QJsonValue>& p_customParams,
                              google::protobuf::Struct* const p_struct)
    {
        if (p_struct == nullptr)
        {
            return;
        }

        for (const auto& [key, value]: p_customParams)
        {
            populateProtobufValue(value, &(*p_struct->mutable_fields())[key.toStdString()]);
        }
    }
} // namespace

std::expected<std::unique_ptr<ProcessStrategyRuntimeBackend>, QString>
ProcessStrategyRuntimeBackend::create(MainAlgo* p_mainAlgo, const QString& p_strategyID, const StrategyConfig& p_config)
{
    const QFileInfo executableInfo(p_config.executablePath);
    if (p_config.executablePath.isEmpty())
    {
        return std::unexpected("Strategy executable path is empty");
    }

    if (!executableInfo.exists())
    {
        return std::unexpected("Strategy executable does not exist: " + p_config.executablePath);
    }

    if (!executableInfo.isExecutable())
    {
        return std::unexpected("Strategy executable is not executable: " + p_config.executablePath);
    }

    auto logger = std::make_unique<StrategyLogger>(p_config.name);
    auto* sdk = new StrategySDK(p_mainAlgo, p_strategyID, p_config, logger.get());

    return std::unique_ptr<ProcessStrategyRuntimeBackend>(
        new ProcessStrategyRuntimeBackend(p_mainAlgo, p_strategyID, p_config, sdk, std::move(logger)));
}

ProcessStrategyRuntimeBackend::ProcessStrategyRuntimeBackend(MainAlgo* p_mainAlgo,
                                                             QString p_strategyID,
                                                             const StrategyConfig& p_config,
                                                             StrategySDK* p_sdk,
                                                             std::unique_ptr<StrategyLogger>&& p_logger)
    : m_mainAlgo(p_mainAlgo)
    , m_strategyID(std::move(p_strategyID))
    , m_config(p_config)
    , m_sdk(p_sdk)
    , m_logger(std::move(p_logger))
{
    m_process.setObjectName(QString("StrategyProcess_%1_%2").arg(m_config.name).arg(m_strategyID.left(8)));
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    setupProcessObservers();
}

ProcessStrategyRuntimeBackend::~ProcessStrategyRuntimeBackend()
{
    destroyRuntime();
}

StrategyBase* ProcessStrategyRuntimeBackend::strategy() const
{
    return nullptr;
}

StrategySDK* ProcessStrategyRuntimeBackend::sdk() const
{
    return m_sdk;
}

StrategyCallbackAdapter* ProcessStrategyRuntimeBackend::adapter() const
{
    return nullptr;
}

StrategyLogger* ProcessStrategyRuntimeBackend::logger() const
{
    return m_logger.get();
}

QThread* ProcessStrategyRuntimeBackend::executionThread()
{
    return nullptr;
}

Qt::HANDLE ProcessStrategyRuntimeBackend::threadHandle() const
{
    if (m_process.processId() <= 0)
    {
        return nullptr;
    }

    return reinterpret_cast<Qt::HANDLE>(static_cast<uintptr_t>(m_process.processId()));
}

void ProcessStrategyRuntimeBackend::setThreadHandle(Qt::HANDLE p_handle)
{
    Q_UNUSED(p_handle);
}

bool ProcessStrategyRuntimeBackend::isThreadRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

QString ProcessStrategyRuntimeBackend::start(const std::function<void()>& p_onStarted)
{
    if (m_process.state() != QProcess::NotRunning)
    {
        return "Strategy process backend is already running";
    }

    resetStartAttemptState();

    QString error = openListeningSocket();
    if (!error.isEmpty())
    {
        return error;
    }

    error = ensureProcessStarted();
    if (!error.isEmpty())
    {
        resetStartAttemptState();
        return error;
    }

    error = acceptClientConnection();
    if (!error.isEmpty())
    {
        markShutdownRequested();
        m_process.kill();
        m_process.waitForFinished(kTerminateTimeoutMs);
        resetStartAttemptState();
        return error;
    }

    error = completeHandshake();
    if (!error.isEmpty())
    {
        markShutdownRequested();
        const QString shutdownError = sendStopLikeCommand(true, "Handshake failed");
        if (!shutdownError.isEmpty())
        {
            qWarning(StrategyManagerLog) << "Failed to send handshake-failure shutdown command:" << shutdownError;
        }
        m_process.kill();
        m_process.waitForFinished(kTerminateTimeoutMs);
        resetStartAttemptState();
        return error;
    }

    if (p_onStarted)
    {
        p_onStarted();
    }

    qInfo(StrategyManagerLog) << "Started external strategy process:" << m_config.name << "ID:" << m_strategyID
                              << "PID:" << m_process.processId();
    return "";
}

void ProcessStrategyRuntimeBackend::invokeOnStop()
{
    if (m_process.state() == QProcess::NotRunning)
    {
        return;
    }

    markShutdownRequested();
    const QString error = sendStopLikeCommand(false, "Strategy unload requested");
    if (!error.isEmpty())
    {
        qWarning(StrategyManagerLog) << "Failed to send stop command to strategy process:" << m_strategyID << error;
    }
}

void ProcessStrategyRuntimeBackend::shutdownExecutionThread()
{
    if (m_process.state() == QProcess::NotRunning)
    {
        return;
    }

    if (!m_shutdownRequested)
    {
        markShutdownRequested();
        const QString error = sendStopLikeCommand(true, "Strategy shutdown requested");
        if (!error.isEmpty())
        {
            qWarning(StrategyManagerLog) << "Failed to send shutdown command to strategy process:" << m_strategyID
                                         << error;
        }
    }

    if (m_process.waitForFinished(kShutdownTimeoutMs))
    {
        return;
    }

    qWarning(StrategyManagerLog) << "Strategy process did not exit within timeout, terminating:" << m_strategyID;
    m_process.terminate();
    if (!m_process.waitForFinished(kTerminateTimeoutMs))
    {
        qWarning(StrategyManagerLog) << "Strategy process ignored terminate, killing:" << m_strategyID;
        m_process.kill();
        m_process.waitForFinished(kTerminateTimeoutMs);
    }
}

void ProcessStrategyRuntimeBackend::destroyRuntime()
{
    if (m_runtimeDestroyed)
    {
        return;
    }
    m_runtimeDestroyed = true;

    if (m_process.state() != QProcess::NotRunning && !m_shutdownRequested)
    {
        markShutdownRequested();
        const QString error = sendStopLikeCommand(true, "Strategy runtime backend destroyed");
        if (!error.isEmpty())
        {
            qWarning(StrategyManagerLog) << "Failed to send final shutdown command to strategy process:" << m_strategyID
                                         << error;
        }
    }

    shutdownExecutionThread();
    cleanupSocketResources();

    delete m_sdk;
    m_sdk = nullptr;
}

void ProcessStrategyRuntimeBackend::setupProcessObservers()
{
    QObject::connect(&m_process,
                     &QProcess::readyReadStandardOutput,
                     &m_process,
                     [this]() { captureProcessStream(QProcess::StandardOutput); });

    QObject::connect(&m_process,
                     &QProcess::readyReadStandardError,
                     &m_process,
                     [this]() { captureProcessStream(QProcess::StandardError); });

    QObject::connect(&m_process,
                     &QProcess::errorOccurred,
                     &m_process,
                     [this](const QProcess::ProcessError p_error)
                     {
                         if (m_shutdownRequested || m_runtimeDestroyed || p_error == QProcess::UnknownError)
                         {
                             return;
                         }

                         reportFailure("Strategy process error: " + m_process.errorString());
                     });

    QObject::connect(&m_process,
                     qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                     &m_process,
                     [this](const int p_exitCode, const QProcess::ExitStatus p_exitStatus)
                     {
                         captureProcessStream(QProcess::StandardOutput);
                         captureProcessStream(QProcess::StandardError);
                         cleanupSocketResources();

                         if (m_shutdownRequested || m_runtimeDestroyed)
                         {
                             qInfo(StrategyManagerLog)
                                 << "Strategy process exited cleanly:" << m_strategyID << "code:" << p_exitCode;
                             return;
                         }

                         const QString errorMessage =
                             p_exitStatus == QProcess::CrashExit
                                 ? QString("Strategy process crashed with exit code %1").arg(p_exitCode)
                                 : QString("Strategy process exited unexpectedly with exit code %1").arg(p_exitCode);
                         reportFailure(errorMessage);
                     });
}

void ProcessStrategyRuntimeBackend::captureProcessStream(const QProcess::ProcessChannel p_channel)
{
    const QByteArray output =
        p_channel == QProcess::StandardOutput ? m_process.readAllStandardOutput() : m_process.readAllStandardError();
    if (output.isEmpty() || m_logger == nullptr)
    {
        return;
    }

    const QtMsgType level = p_channel == QProcess::StandardError ? QtWarningMsg : QtInfoMsg;
    const QString prefix = p_channel == QProcess::StandardError ? "[stderr] " : "[stdout] ";
    const QString text = QString::fromUtf8(output);
    for (const QString& line: text.split('\n', Qt::SkipEmptyParts))
    {
        m_logger->log(level, prefix + line.trimmed());
    }
}

void ProcessStrategyRuntimeBackend::drainInboundSocket()
{
    if (m_clientFd < 0)
    {
        return;
    }

    std::array<char, kSocketReadChunkSize> buffer{};
    while (true)
    {
        const ssize_t result = ::read(m_clientFd, buffer.data(), buffer.size());
        if (result > 0)
        {
            m_receiveBuffer.append(buffer.data(), static_cast<qsizetype>(result));
            continue;
        }

        if (result == 0)
        {
            scheduleSocketCleanup("Strategy process socket disconnected unexpectedly");
            return;
        }

        if (errno == EINTR)
        {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            break;
        }

        scheduleSocketCleanup(
            QString("Failed to read strategy socket: %1").arg(QString::fromUtf8(std::strerror(errno))));
        return;
    }

    while (m_receiveBuffer.size() >= static_cast<qsizetype>(L2Trader::StrategySDK::kFramePrefixSize))
    {
        const auto* const rawData = reinterpret_cast<const std::uint8_t*>(m_receiveBuffer.constData());
        std::array<std::uint8_t, L2Trader::StrategySDK::kFramePrefixSize> prefix{};
        std::copy_n(rawData, prefix.size(), prefix.begin());

        const std::uint32_t payloadSize = L2Trader::StrategySDK::decodeFrameSize(prefix);
        const qsizetype totalFrameSize = static_cast<qsizetype>(L2Trader::StrategySDK::kFramePrefixSize + payloadSize);
        if (m_receiveBuffer.size() < totalFrameSize)
        {
            return;
        }

        handleInboundPayload(
            std::span<const std::uint8_t>(rawData + L2Trader::StrategySDK::kFramePrefixSize, payloadSize));
        m_receiveBuffer.remove(0, totalFrameSize);
    }
}

void ProcessStrategyRuntimeBackend::handleInboundPayload(const std::span<const std::uint8_t> p_payload)
{
    Protocol::StrategyToHostEnvelope envelope;
    if (!L2Trader::StrategySDK::parseMessagePayload(p_payload, &envelope))
    {
        reportFailure("Failed to parse inbound strategy payload");
        return;
    }

    switch (envelope.payload_case())
    {
    case Protocol::StrategyToHostEnvelope::kStrategyLog:
        if (m_logger)
        {
            m_logger->log(toQtMessageType(envelope.strategy_log().level()),
                          QString::fromStdString(envelope.strategy_log().message()));
        }
        break;

    case Protocol::StrategyToHostEnvelope::kHeartbeat:
        qDebug(StrategyManagerLog) << "Received heartbeat reply from strategy:" << m_strategyID
                                   << "sequence:" << envelope.heartbeat().monotonic_sequence();
        break;

    case Protocol::StrategyToHostEnvelope::kError:
        if (m_logger)
        {
            m_logger->log(QtCriticalMsg,
                          QString("[protocol-error] %1: %2")
                              .arg(QString::fromStdString(envelope.error().code()),
                                   QString::fromStdString(envelope.error().message())));
        }
        break;

    default:
        qDebug(StrategyManagerLog) << "Ignoring unsupported strategy-to-host envelope payload:"
                                   << envelope.payload_case() << "for strategy" << m_strategyID;
        break;
    }
}

void ProcessStrategyRuntimeBackend::reportFailure(const QString& p_errorMessage)
{
    if (m_shutdownRequested || m_failureReported)
    {
        return;
    }
    m_failureReported = true;

    qCritical(StrategyManagerLog) << "External strategy process failed:" << m_strategyID << p_errorMessage;
    if (m_logger)
    {
        m_logger->log(QtCriticalMsg, p_errorMessage);
    }

    if (m_mainAlgo == nullptr || m_mainAlgo->getStrategyManager() == nullptr)
    {
        return;
    }

    StrategyManager* const strategyManager = m_mainAlgo->getStrategyManager();
    QMetaObject::invokeMethod(
        strategyManager,
        [strategyManager, strategyID = m_strategyID, errorMessage = p_errorMessage]()
        { strategyManager->markStrategyFailed(strategyID, errorMessage); },
        Qt::QueuedConnection);
}

void ProcessStrategyRuntimeBackend::scheduleSocketCleanup(const QString& p_errorMessage)
{
    if (m_socketNotifier)
    {
        m_socketNotifier->setEnabled(false);
    }

    QTimer::singleShot(0,
                       &m_process,
                       [this, errorMessage = p_errorMessage]()
                       {
                           cleanupSocketResources();
                           if (!m_shutdownRequested && !errorMessage.isEmpty())
                           {
                               reportFailure(errorMessage);
                           }
                       });
}

QString ProcessStrategyRuntimeBackend::openListeningSocket()
{
    cleanupSocketResources();

    m_socketPath =
        QDir::tempPath() + QString("/l2t_%1.sock").arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(12));
    if (m_socketPath.size() >= static_cast<int>(sizeof(sockaddr_un::sun_path)))
    {
        return "Generated strategy socket path is too long: " + m_socketPath;
    }

    const int serverFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverFd < 0)
    {
        return QString("Failed to create strategy socket: %1").arg(QString::fromUtf8(std::strerror(errno)));
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const QByteArray encodedPath = QFile::encodeName(m_socketPath);
    std::strncpy(address.sun_path, encodedPath.constData(), sizeof(address.sun_path) - 1);

    ::unlink(address.sun_path);
    if (::bind(serverFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        const QString error =
            QString("Failed to bind strategy socket: %1").arg(QString::fromUtf8(std::strerror(errno)));
        ::close(serverFd);
        ::unlink(address.sun_path);
        return error;
    }

    if (::listen(serverFd, 1) != 0)
    {
        const QString error =
            QString("Failed to listen on strategy socket: %1").arg(QString::fromUtf8(std::strerror(errno)));
        ::close(serverFd);
        ::unlink(address.sun_path);
        return error;
    }

    m_serverFd = serverFd;
    return "";
}

QString ProcessStrategyRuntimeBackend::ensureProcessStarted()
{
    m_process.setProgram(m_config.executablePath);
    m_process.setArguments({});
    m_process.setWorkingDirectory(QFileInfo(m_config.executablePath).absolutePath());

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert("L2TRADER_STRATEGY_ID", m_strategyID);
    environment.insert("L2TRADER_STRATEGY_SOCKET", m_socketPath);
    m_process.setProcessEnvironment(environment);

    m_process.start();
    if (!m_process.waitForStarted(kStartupTimeoutMs))
    {
        return "Failed to start strategy process: " + m_process.errorString();
    }

    return "";
}

QString ProcessStrategyRuntimeBackend::acceptClientConnection()
{
    if (m_serverFd < 0)
    {
        return "Strategy socket listener is not open";
    }

    if (!waitForFd(m_serverFd, POLLIN, kStartupTimeoutMs))
    {
        return "Timed out waiting for strategy process to connect";
    }

    const int clientFd = ::accept(m_serverFd, nullptr, nullptr);
    if (clientFd < 0)
    {
        return QString("Failed to accept strategy connection: %1").arg(QString::fromUtf8(std::strerror(errno)));
    }

    ::close(m_serverFd);
    m_serverFd = -1;
    m_clientFd = clientFd;
    return "";
}

QString ProcessStrategyRuntimeBackend::completeHandshake()
{
    if (m_clientFd < 0)
    {
        return "Strategy socket client is not connected";
    }

    std::vector<std::uint8_t> payload;
    if (!readFrameWithTimeout(m_clientFd, &payload, kStartupTimeoutMs))
    {
        return "Timed out waiting for strategy handshake";
    }

    Protocol::StrategyToHostEnvelope envelope;
    if (!L2Trader::StrategySDK::parseMessagePayload(payload, &envelope))
    {
        return "Failed to parse strategy handshake payload";
    }

    if (envelope.payload_case() != Protocol::StrategyToHostEnvelope::kHandshakeHello)
    {
        return "Strategy sent a non-handshake payload before initialization completed";
    }

    const auto& handshake = envelope.handshake_hello();
    if (!L2Trader::StrategyProtocol::isCompatibleProtocol(handshake.protocol_version()))
    {
        const QString rejectionReason = QString("Unsupported strategy protocol version for %1").arg(m_config.name);
        const QString ackError =
            sendHandshakeAck(false, rejectionReason, QString::fromStdString(envelope.correlation_id()));
        if (!ackError.isEmpty())
        {
            qWarning(StrategyManagerLog) << "Failed to send handshake rejection:" << ackError;
        }
        return rejectionReason;
    }

    QString error = sendHandshakeAck(true, "", QString::fromStdString(envelope.correlation_id()));
    if (!error.isEmpty())
    {
        return error;
    }

    const int socketFlags = ::fcntl(m_clientFd, F_GETFL, 0);
    if (socketFlags < 0 || ::fcntl(m_clientFd, F_SETFL, socketFlags | O_NONBLOCK) != 0)
    {
        return QString("Failed to configure strategy socket as non-blocking: %1")
            .arg(QString::fromUtf8(std::strerror(errno)));
    }

    m_socketNotifier = std::make_unique<QSocketNotifier>(m_clientFd, QSocketNotifier::Read);
    QObject::connect(m_socketNotifier.get(),
                     &QSocketNotifier::activated,
                     &m_process,
                     [this]()
                     {
                         if (m_socketNotifier)
                         {
                             drainInboundSocket();
                         }
                     });

    error = sendStartCommand();
    if (!error.isEmpty())
    {
        return error;
    }

    qInfo(StrategyManagerLog) << "Accepted strategy process handshake:" << m_strategyID
                              << "SDK:" << QString::fromStdString(handshake.sdk_name())
                              << QString::fromStdString(handshake.sdk_version())
                              << "strategy:" << QString::fromStdString(handshake.strategy_name())
                              << QString::fromStdString(handshake.strategy_version());
    return "";
}

QString ProcessStrategyRuntimeBackend::sendHandshakeAck(const bool p_accepted,
                                                        const QString& p_rejectionReason,
                                                        const QString& p_correlationID)
{
    if (m_clientFd < 0)
    {
        return "Strategy socket is not connected";
    }

    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);
    envelope.set_correlation_id(p_correlationID.toStdString());

    auto* const handshakeAck = envelope.mutable_handshake_ack();
    L2Trader::StrategyProtocol::populateProtocolVersion(handshakeAck->mutable_protocol_version());
    handshakeAck->mutable_endpoint()->set_strategy_id(m_strategyID.toStdString());
    handshakeAck->mutable_endpoint()->set_socket_path(m_socketPath.toStdString());
    handshakeAck->set_accepted(p_accepted);
    handshakeAck->set_rejection_reason(p_rejectionReason.toStdString());

    if (!writeMessage(m_clientFd, envelope))
    {
        return "Failed to send handshake acknowledgement to strategy process";
    }

    return "";
}

QString ProcessStrategyRuntimeBackend::sendStartCommand()
{
    if (m_clientFd < 0)
    {
        return "Strategy socket is not connected";
    }

    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);

    auto* const startCommand = envelope.mutable_start();
    auto* const configuration = startCommand->mutable_configuration();
    configuration->set_name(m_config.name.toStdString());
    for (const QString& symbol: m_config.symbols)
    {
        configuration->add_symbols(symbol.toStdString());
    }
    configuration->set_position_size(static_cast<std::uint32_t>(m_config.positionSize));
    configuration->set_risk_limit(m_config.riskLimit);
    configuration->set_executable_path(m_config.executablePath.toStdString());
    populateCustomParams(m_config.customParams, configuration->mutable_custom_params());

    if (!writeMessage(m_clientFd, envelope))
    {
        return "Failed to send start command to strategy process";
    }

    return "";
}

QString ProcessStrategyRuntimeBackend::sendStopLikeCommand(const bool p_shutdown, const QString& p_reason)
{
    if (m_clientFd < 0)
    {
        return "";
    }

    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);

    if (p_shutdown)
    {
        envelope.mutable_shutdown()->set_reason(p_reason.toStdString());
    }
    else
    {
        envelope.mutable_stop()->set_reason(p_reason.toStdString());
    }

    if (!writeMessage(m_clientFd, envelope))
    {
        return QString("Failed to send %1 command to strategy process").arg(p_shutdown ? "shutdown" : "stop");
    }

    return "";
}

void ProcessStrategyRuntimeBackend::cleanupSocketResources()
{
    m_socketNotifier.reset();

    if (m_clientFd >= 0)
    {
        ::close(m_clientFd);
        m_clientFd = -1;
    }

    if (m_serverFd >= 0)
    {
        ::close(m_serverFd);
        m_serverFd = -1;
    }

    if (!m_socketPath.isEmpty())
    {
        const QByteArray encodedPath = QFile::encodeName(m_socketPath);
        ::unlink(encodedPath.constData());
        m_socketPath.clear();
    }

    m_receiveBuffer.clear();
}

void ProcessStrategyRuntimeBackend::resetStartAttemptState()
{
    cleanupSocketResources();
    m_outboundSequence = 1;
    m_shutdownRequested = false;
    m_failureReported = false;
}

void ProcessStrategyRuntimeBackend::markShutdownRequested()
{
    m_shutdownRequested = true;
}
