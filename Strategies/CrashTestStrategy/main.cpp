#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <string_view>

#include "L2Trader/StrategySDK/ExternalStrategyRuntime.h"
#include "L2Trader/StrategySDK/StrategyDescription.h"
#include "L2Trader/StrategySDK/StrategyProcessMain.h"

namespace
{
    using namespace std::chrono_literals;
    namespace Protocol = l2trader::strategy::v1;

    constexpr std::string_view kStrategyName = "CrashTestStrategyProcess";
    constexpr std::string_view kStrategyVersion = "1.0.0";

    [[nodiscard]] L2Trader::StrategySDK::StrategyDescription describeStrategy()
    {
        return {
            .name = std::string(kStrategyName),
            .version = std::string(kStrategyVersion),
            .parameterSchema = {},
        };
    }

    class CrashTestStrategyProcess final : public L2Trader::StrategySDK::ExternalStrategyHandler
    {
      public:
        void bindRuntime(L2Trader::StrategySDK::ExternalStrategyRuntime* const p_runtime) override
        {
            m_runtime = p_runtime;
        }

        void onStart(const Protocol::StrategyConfiguration&) override
        {
            sendLog("[CrashTestStrategyProcess] Started");
            sendLog("[CrashTestStrategyProcess] Strategy will crash in 5 seconds to validate process isolation.");
            armCrashTimer();
        }

        void onPause(std::string_view p_reason) override
        {
            disarmCrashTimer();
            sendLog("[CrashTestStrategyProcess] Paused: " + std::string(p_reason));
        }

        void onResume(std::string_view p_reason) override
        {
            sendLog("[CrashTestStrategyProcess] Resumed: " + std::string(p_reason));
            armCrashTimer();
        }

        void onStop(std::string_view p_reason) override
        {
            disarmCrashTimer();
            sendLog("[CrashTestStrategyProcess] Stop requested: " + std::string(p_reason));
        }

        void onShutdown(std::string_view p_reason) override
        {
            disarmCrashTimer();
            sendLog("[CrashTestStrategyProcess] Shutdown requested: " + std::string(p_reason));
        }

        void onHostError(const Protocol::ErrorMessage& p_error) override
        {
            std::cerr << "[CrashTestStrategyProcess] Host error (" << p_error.code() << "): " << p_error.message()
                      << std::endl;
        }

      private:
        void armCrashTimer()
        {
            if (m_runtime == nullptr)
            {
                return;
            }

            disarmCrashTimer();
            m_crashTimerId = m_runtime->startTimer(5s,
                                                   [this]()
                                                   {
                                                       sendLog("[CrashTestStrategyProcess] Triggering crash now!");
                                                       std::raise(SIGSEGV);
                                                   });
        }

        void disarmCrashTimer()
        {
            if (m_runtime == nullptr || m_crashTimerId == 0)
            {
                return;
            }

            (void)m_runtime->cancelTimer(m_crashTimerId);
            m_crashTimerId = 0;
        }

        void sendLog(const std::string& p_message)
        {
            if (m_runtime == nullptr || m_runtime->log(p_message))
            {
                return;
            }

            std::cerr << "[CrashTestStrategyProcess] Failed to send log: " << p_message << std::endl;
        }

        L2Trader::StrategySDK::ExternalStrategyRuntime* m_runtime = nullptr;
        L2Trader::StrategySDK::ExternalStrategyRuntime::TimerId m_crashTimerId = 0;
    };
} // namespace

int main(int argc, char** argv)
{
    CrashTestStrategyProcess strategy;
    return L2Trader::StrategySDK::runStrategyProcessMain(argc, argv, describeStrategy(), strategy);
}
