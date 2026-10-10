#include "StreamReceiver.h"

#include <QLoggingCategory>

#include "CONSTANTS.h"

Q_LOGGING_CATEGORY(StreamRecoveryLog, "StreamRecovery")

StreamReceiver::StreamReceiver(QObject* parent) : QObject(parent), m_streamRetryTimer(this), m_recoveryTimer(this)
{
    m_streamRetryTimer.setSingleShot(true);
    m_streamRetryTimer.setInterval(StreamConstants::BROKERAGE_RETRY_DELAY_MS);
    m_recoveryTimer.setSingleShot(true);
    m_recoveryTimer.setInterval(StreamConstants::BROKERAGE_RECOVERY_TIMEOUT_MS);
    connect(&m_recoveryTimer,
            &QTimer::timeout,
            this,
            [this]()
            {
                qCCritical(StreamRecoveryLog)
                    << m_recoveryFeed << "stream recovery failed for account" << m_recoveryAccount
                    << "- no completed snapshot within" << StreamConstants::BROKERAGE_RECOVERY_TIMEOUT_MS
                    << "ms; reconnect attempts continue";
            });
}

void StreamReceiver::beginRecovery(const QString& p_feed, const QString& p_account)
{
    if (m_recovering)
    {
        return;
    }
    m_recovering = true;
    m_recoveryFeed = p_feed;
    m_recoveryAccount = p_account;
    m_recoveryTimer.start();
}

void StreamReceiver::completeRecovery()
{
    if (!m_recovering)
    {
        return;
    }
    m_recoveryTimer.stop();
    m_recovering = false;
    qCInfo(StreamRecoveryLog) << m_recoveryFeed << "stream recovered for account" << m_recoveryAccount
                              << "- snapshot complete";
}

void StreamReceiver::stopRecovery()
{
    m_streamStopped = true;
    m_streamRetryTimer.stop();
    m_recoveryTimer.stop();
    m_recovering = false;
}
