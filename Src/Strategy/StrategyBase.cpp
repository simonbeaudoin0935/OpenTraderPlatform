#include "StrategyBase.h"
#include "StrategySDK.h"

void StrategyBase::log(const QString& message, int level) const
{
    if (m_sdk)
    {
        m_sdk->log(message, static_cast<LogLevel>(level));
    }
}
