#include "StrategyBase.h"
#include "StrategySDK.h"
#include "Assume.h"

void StrategyBase::log(const QString& message, int level) const
{
    ASSUME_TRUE(m_sdk != nullptr);
    m_sdk->log(message, static_cast<LogLevel>(level));
}
