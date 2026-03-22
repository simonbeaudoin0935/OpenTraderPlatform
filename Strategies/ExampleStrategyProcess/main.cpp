#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "L2Trader/StrategySDK/ExternalStrategyRuntime.h"

namespace
{
    namespace Protocol = l2trader::strategy::v1;

    constexpr std::string_view kStrategyName = "ExampleStrategyProcess";
    constexpr std::string_view kStrategyVersion = "1.0.0";

    class ExampleStrategyProcess final : public L2Trader::StrategySDK::ExternalStrategyHandler
    {
      public:
        [[nodiscard]] std::string strategyName() const override
        {
            return std::string(kStrategyName);
        }

        [[nodiscard]] std::string strategyVersion() const override
        {
            return std::string(kStrategyVersion);
        }

        void setRuntime(L2Trader::StrategySDK::ExternalStrategyRuntime* const p_runtime)
        {
            m_runtime = p_runtime;
        }

        void onStart(const Protocol::StrategyConfiguration& p_configuration) override
        {
            std::ostringstream message;
            message << "[ExampleStrategyProcess] Started strategy '" << p_configuration.name() << "' for "
                    << p_configuration.symbols_size() << " symbol(s).";
            sendLog(message.str());
        }

        void onBar(const Protocol::Bar& p_bar) override
        {
            ++m_processedBars;
            if (m_processedBars % 10 != 0)
            {
                return;
            }

            std::ostringstream message;
            message << "[ExampleStrategyProcess] Bar #" << m_processedBars << " close=" << p_bar.close();
            sendLog(message.str());
        }

        void onStop(std::string_view p_reason) override
        {
            const std::string message = "[ExampleStrategyProcess] Stop requested: " + std::string(p_reason);
            sendLog(message);
        }

        void onShutdown(std::string_view p_reason) override
        {
            const std::string message = "[ExampleStrategyProcess] Shutdown requested: " + std::string(p_reason);
            sendLog(message);
        }

        void onHostError(const Protocol::ErrorMessage& p_error) override
        {
            std::cerr << "[ExampleStrategyProcess] Host error (" << p_error.code() << "): " << p_error.message()
                      << std::endl;
        }

      private:
        void sendLog(const std::string& p_message)
        {
            if (m_runtime == nullptr || m_runtime->log(p_message))
            {
                return;
            }

            std::cerr << "[ExampleStrategyProcess] Failed to send log: " << p_message << std::endl;
        }

        L2Trader::StrategySDK::ExternalStrategyRuntime* m_runtime = nullptr;
        int m_processedBars = 0;
    };
} // namespace

int main()
{
    ExampleStrategyProcess strategy;
    L2Trader::StrategySDK::ExternalStrategyRuntime runtime(strategy);
    strategy.setRuntime(&runtime);
    return runtime.run();
}
