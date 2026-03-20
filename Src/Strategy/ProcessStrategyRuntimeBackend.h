#pragma once

#include <QByteArray>
#include <QProcess>
#include <QSocketNotifier>
#include <QString>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>

#include "StrategyConfig.h"
#include "StrategyRuntimeBackend.h"

class MainAlgo;
namespace l2trader::strategy::v1
{
    class HostToStrategyEnvelope;
}

class ProcessStrategyRuntimeBackend final : public IStrategyRuntimeBackend
{
  public:
    [[nodiscard]] static std::expected<std::unique_ptr<ProcessStrategyRuntimeBackend>, QString>
    create(MainAlgo* p_mainAlgo, const QString& p_strategyID, const StrategyConfig& p_config);

    ~ProcessStrategyRuntimeBackend() override;

    [[nodiscard]] StrategySDK* sdk() const override;
    [[nodiscard]] StrategyLogger* logger() const override;
    [[nodiscard]] Qt::HANDLE threadHandle() const override;
    void setThreadHandle(Qt::HANDLE p_handle) override;
    [[nodiscard]] bool isThreadRunning() const override;
    [[nodiscard]] QString start(const std::function<void()>& p_onStarted) override;
    void trackMonitoredSymbol(const QString& p_symbol) override;
    void publishBar(const QString& p_symbol, const Bar& p_bar) override;
    void publishLevel2(const QString& p_symbol, const Level2& p_level2) override;
    void publishTrade(const QString& p_symbol, const Trade& p_trade) override;
    void publishOrder(const Order& p_order) override;
    void publishPosition(const Position& p_position) override;
    void publishBalance(double p_balance) override;
    void invokeOnStop() override;
    void shutdownExecutionThread() override;
    void destroyRuntime() override;

  private:
    ProcessStrategyRuntimeBackend(MainAlgo* p_mainAlgo,
                                  QString p_strategyID,
                                  const StrategyConfig& p_config,
                                  StrategySDK* p_sdk,
                                  std::unique_ptr<StrategyLogger>&& p_logger);

    void setupProcessObservers();
    void captureProcessStream(QProcess::ProcessChannel p_channel);
    void drainInboundSocket();
    void handleInboundPayload(std::span<const std::uint8_t> p_payload);
    void reportFailure(const QString& p_errorMessage);
    void scheduleSocketCleanup(const QString& p_errorMessage);

    [[nodiscard]] QString openListeningSocket();
    [[nodiscard]] QString ensureProcessStarted();
    [[nodiscard]] QString acceptClientConnection();
    [[nodiscard]] QString completeHandshake();
    [[nodiscard]] QString
    sendHandshakeAck(bool p_accepted, const QString& p_rejectionReason, const QString& p_correlationID);
    [[nodiscard]] QString sendStartCommand();
    [[nodiscard]] QString sendStopLikeCommand(bool p_shutdown, const QString& p_reason);
    [[nodiscard]] bool sendHostEnvelope(const l2trader::strategy::v1::HostToStrategyEnvelope& p_envelope,
                                        const QString& p_context);
    void sendErrorMessage(const QString& p_code, const QString& p_message, const QString& p_correlationID);
    void cleanupSocketResources();
    void resetStartAttemptState();
    void markShutdownRequested();

    MainAlgo* m_mainAlgo = nullptr;
    QString m_strategyID;
    StrategyConfig m_config;
    StrategySDK* m_sdk = nullptr;
    std::unique_ptr<StrategyLogger> m_logger;
    QProcess m_process;
    QString m_socketPath;
    int m_serverFd = -1;
    int m_clientFd = -1;
    std::unique_ptr<QSocketNotifier> m_socketNotifier;
    QByteArray m_receiveBuffer;
    std::uint64_t m_outboundSequence = 1;
    bool m_runtimeDestroyed = false;
    bool m_shutdownRequested = false;
    bool m_failureReported = false;
};
