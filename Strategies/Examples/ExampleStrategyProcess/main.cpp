#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "OpenTraderPlatform/StrategySDK/ExternalStrategyRuntime.h"
#include "OpenTraderPlatform/StrategySDK/StrategyDescription.h"
#include "OpenTraderPlatform/StrategySDK/StrategyProcessMain.h"

namespace
{
    namespace Protocol = opentraderplatform::strategy::v1;

    constexpr std::string_view kStrategyName = "ExampleStrategyProcess";
    constexpr std::string_view kStrategyVersion = "1.0.0";

    [[nodiscard]] OpenTraderPlatform::StrategySDK::StrategyDescription describeStrategy()
    {
        return {
            .name = std::string(kStrategyName),
            .version = std::string(kStrategyVersion),
            .parameterSchema = {},
        };
    }

    class ExampleStrategyProcess final : public OpenTraderPlatform::StrategySDK::ExternalStrategyHandler
    {
      public:
        void bindRuntime(OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime* const p_runtime) override
        {
            m_runtime = p_runtime;
        }

        void onStart(const Protocol::StrategyConfiguration& p_configuration) override
        {
            std::ostringstream message;
            message << "[ExampleStrategyProcess] Started strategy '" << p_configuration.name() << "'.";
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

        OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime* m_runtime = nullptr;
        int m_processedBars = 0;
    };
} // namespace

int main(int argc, char** argv)
{
    ExampleStrategyProcess strategy;
    return OpenTraderPlatform::StrategySDK::runStrategyProcessMain(argc, argv, describeStrategy(), strategy);
}
