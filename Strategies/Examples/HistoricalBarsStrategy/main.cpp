#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QTimeZone>

#include "OpenTraderPlatform/StrategySDK/ConfigurationHelpers.h"
#include "OpenTraderPlatform/StrategySDK/ExternalStrategyRuntime.h"
#include "OpenTraderPlatform/StrategySDK/StrategyDescription.h"
#include "OpenTraderPlatform/StrategySDK/StrategyProcessMain.h"

namespace
{
    using namespace std::chrono_literals;
    namespace Protocol = opentraderplatform::strategy::v1;

    constexpr std::string_view kStrategyName = "HistoricalBarsStrategyProcess";
    constexpr std::string_view kStrategyVersion = "1.0.0";

    [[nodiscard]] OpenTraderPlatform::StrategySDK::StrategyDescription describeStrategy()
    {
        return {
            .name = std::string(kStrategyName),
            .version = std::string(kStrategyVersion),
            .parameterSchema =
                {
                    OpenTraderPlatform::StrategySDK::stringField("symbol",
                                                       "Symbol",
                                                       "AAPL",
                                                       "Ticker symbol used for historical bar requests"),
                },
        };
    }

    [[nodiscard]] const QTimeZone& getNewYorkTimeZone()
    {
        static const QTimeZone timezone("America/New_York");
        return timezone;
    }

    [[nodiscard]] QDate previousTradingDay(QDate p_date)
    {
        while (p_date.dayOfWeek() == Qt::Saturday || p_date.dayOfWeek() == Qt::Sunday)
        {
            p_date = p_date.addDays(-1);
        }

        return p_date;
    }

    [[nodiscard]] std::int64_t toUnixNanos(const QDateTime& p_datetime)
    {
        return static_cast<std::int64_t>(p_datetime.toMSecsSinceEpoch()) * 1000000LL;
    }

    class HistoricalBarsStrategyProcess final : public OpenTraderPlatform::StrategySDK::ExternalStrategyHandler
    {
      public:
        void bindRuntime(OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime* const p_runtime) override
        {
            m_runtime = p_runtime;
        }

        void onStart(const Protocol::StrategyConfiguration& p_configuration) override
        {
            m_symbol = OpenTraderPlatform::StrategySDK::stringFieldOr(p_configuration, "symbol", "AAPL");
            m_daysBack = 0;
            m_totalBarsFetched = 0;

            const QDate today = QDateTime::currentDateTimeUtc().toTimeZone(getNewYorkTimeZone()).date();
            m_currentDate = previousTradingDay(today.addDays(-1));

            sendLog("[HistoricalBarsStrategyProcess] Started for symbol " + m_symbol + ", first fetch date " +
                    m_currentDate.toString("yyyy-MM-dd").toStdString());

            fetchNextDay();
        }

        void onStop(std::string_view p_reason) override
        {
            stopTimer();
            sendLog("[HistoricalBarsStrategyProcess] Stop requested: " + std::string(p_reason));
            sendLog("[HistoricalBarsStrategyProcess] Summary: fetched " + std::to_string(m_daysBack) +
                    " day(s), total bars=" + std::to_string(m_totalBarsFetched));
        }

        void onShutdown(std::string_view p_reason) override
        {
            stopTimer();
            sendLog("[HistoricalBarsStrategyProcess] Shutdown requested: " + std::string(p_reason));
            sendLog("[HistoricalBarsStrategyProcess] Summary: fetched " + std::to_string(m_daysBack) +
                    " day(s), total bars=" + std::to_string(m_totalBarsFetched));
        }

        void onHostError(const Protocol::ErrorMessage& p_error) override
        {
            std::cerr << "[HistoricalBarsStrategyProcess] Host error (" << p_error.code() << "): " << p_error.message()
                      << std::endl;
        }

      private:
        void fetchNextDay()
        {
            if (m_runtime == nullptr)
            {
                return;
            }

            const QDateTime sessionDay(m_currentDate, QTime(0, 0), getNewYorkTimeZone());
            const QDateTime firstBar(m_currentDate, QTime(9, 31), getNewYorkTimeZone());
            const QDateTime lastBar(m_currentDate, QTime(16, 0), getNewYorkTimeZone());

            sendLog("[HistoricalBarsStrategyProcess] Fetching day " + std::to_string(m_daysBack + 1) + " for " +
                    m_symbol + " on " + m_currentDate.toString("yyyy-MM-dd").toStdString());

            const auto result = m_runtime->requestHistoricalBars(m_symbol,
                                                                 toUnixNanos(sessionDay),
                                                                 toUnixNanos(firstBar),
                                                                 toUnixNanos(lastBar),
                                                                 "1m");

            if (result.hasError())
            {
                const std::string errorCode = result.errorCode.value_or("unknown_error");
                const std::string errorMessage = result.errorMessage.value_or("no error message");
                sendLog("[HistoricalBarsStrategyProcess] Failed to fetch bars for " +
                            m_currentDate.toString("yyyy-MM-dd").toStdString() + " (" + errorCode +
                            "): " + errorMessage,
                        Protocol::LOG_LEVEL_WARNING);
            }
            else
            {
                m_totalBarsFetched += static_cast<int>(result.bars.size());
                sendLog("[HistoricalBarsStrategyProcess] Fetched " + std::to_string(result.bars.size()) + " bars for " +
                        m_symbol + " on " + m_currentDate.toString("yyyy-MM-dd").toStdString() +
                        " (total bars=" + std::to_string(m_totalBarsFetched) + ")");
            }

            ++m_daysBack;
            m_currentDate = previousTradingDay(m_currentDate.addDays(-1));

            if (m_runtime != nullptr)
            {
                m_fetchTimerId = m_runtime->startTimer(2s, [this]() { fetchNextDay(); });
            }
        }

        void stopTimer()
        {
            if (m_runtime == nullptr || m_fetchTimerId == 0)
            {
                return;
            }

            (void)m_runtime->cancelTimer(m_fetchTimerId);
            m_fetchTimerId = 0;
        }

        void sendLog(const std::string& p_message, const Protocol::LogLevel p_level = Protocol::LOG_LEVEL_INFO)
        {
            if (m_runtime == nullptr || m_runtime->log(p_message, p_level))
            {
                return;
            }

            std::cerr << "[HistoricalBarsStrategyProcess] Failed to send log: " << p_message << std::endl;
        }

        OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime* m_runtime = nullptr;
        OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime::TimerId m_fetchTimerId = 0;
        std::string m_symbol = "AAPL";
        QDate m_currentDate;
        int m_daysBack = 0;
        int m_totalBarsFetched = 0;
    };
} // namespace

int main(int argc, char** argv)
{
    HistoricalBarsStrategyProcess strategy;
    return OpenTraderPlatform::StrategySDK::runStrategyProcessMain(argc, argv, describeStrategy(), strategy);
}
