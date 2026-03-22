#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include "L2Trader/StrategySDK/ConfigurationHelpers.h"
#include "L2Trader/StrategySDK/ExternalStrategyRuntime.h"
#include "L2Trader/StrategySDK/StrategyDescription.h"
#include "L2Trader/StrategySDK/StrategyProcessMain.h"

namespace
{
    using namespace std::chrono_literals;
    namespace Protocol = l2trader::strategy::v1;

    constexpr std::string_view kStrategyName = "OrderTestStrategyProcess";
    constexpr std::string_view kStrategyVersion = "1.0.0";

    [[nodiscard]] L2Trader::StrategySDK::StrategyDescription describeStrategy()
    {
        return {
            .name = std::string(kStrategyName),
            .version = std::string(kStrategyVersion),
            .parameterSchema =
                {
                    L2Trader::StrategySDK::stringField("symbol",
                                                       "Symbol",
                                                       "SPY",
                                                       "Ticker symbol used for the order test"),
                    L2Trader::StrategySDK::stringField("accountID",
                                                       "Account ID",
                                                       "SIM123456",
                                                       "Broker account used for order placement"),
                    L2Trader::StrategySDK::doubleField("limitPrice",
                                                       "Limit Price",
                                                       1.0,
                                                       "Limit price used for the staged limit order"),
                    L2Trader::StrategySDK::intField("timeoutSecs",
                                                    "Timeout (seconds)",
                                                    60,
                                                    "Fails the test if it does not complete in time"),
                },
        };
    }

    [[nodiscard]] std::string orderStatusToString(const Protocol::OrderStatus p_status)
    {
        switch (p_status)
        {
        case Protocol::ORDER_STATUS_PENDING:
            return "PENDING";
        case Protocol::ORDER_STATUS_ACKNOWLEDGED:
            return "ACKNOWLEDGED";
        case Protocol::ORDER_STATUS_OPEN:
            return "OPEN";
        case Protocol::ORDER_STATUS_FILLED:
            return "FILLED";
        case Protocol::ORDER_STATUS_CANCELLED:
            return "CANCELLED";
        case Protocol::ORDER_STATUS_REJECTED:
            return "REJECTED";
        case Protocol::ORDER_STATUS_UNSPECIFIED:
        default:
            return "UNSPECIFIED";
        }
    }

    class OrderTestStrategyProcess final : public L2Trader::StrategySDK::ExternalStrategyHandler
    {
      public:
        void bindRuntime(L2Trader::StrategySDK::ExternalStrategyRuntime* const p_runtime) override
        {
            m_runtime = p_runtime;
        }

        void onStart(const Protocol::StrategyConfiguration& p_configuration) override
        {
            m_symbol = L2Trader::StrategySDK::stringFieldOr(p_configuration, "symbol", "SPY");
            m_accountId = L2Trader::StrategySDK::stringFieldOr(p_configuration, "accountID", "SIM123456");
            m_limitPrice = L2Trader::StrategySDK::doubleFieldOr(p_configuration, "limitPrice", 1.0);
            const int timeoutSecs =
                static_cast<int>(L2Trader::StrategySDK::intFieldOr(p_configuration, "timeoutSecs", 60));

            sendLog("=== OrderTestStrategyProcess starting ===");
            sendLog("  account : " + m_accountId);
            sendLog("  symbol  : " + m_symbol);
            sendLog("  limitPx : " + std::to_string(m_limitPrice));
            sendLog("  timeout : " + std::to_string(timeoutSecs) + "s");
            sendLog("[Init] balance=$" + std::to_string(m_runtime->cashBalance()) +
                    "  openOrders=" + std::to_string(m_runtime->orders().size()) +
                    "  positions=" + std::to_string(m_runtime->positions().size()));

            const auto claimResult = m_runtime->claimSymbols({m_symbol});
            if (claimResult.grantedSymbols.empty())
            {
                fail("[Init] FAIL - symbol claim denied");
                return;
            }

            m_timeoutTimerId = m_runtime->startTimer(std::chrono::seconds(timeoutSecs),
                                                     [this]()
                                                     {
                                                         if (m_state == State::Complete)
                                                         {
                                                             return;
                                                         }

                                                         fail("[Timeout] Test timed out in state " +
                                                              std::to_string(static_cast<int>(m_state)));
                                                     });

            placeMarketOrder();
        }

        void onStop(std::string_view p_reason) override
        {
            stopWatchdog();
            sendLog("=== OrderTestStrategyProcess stopped ===");
            sendLog("[Stop] " + std::string(p_reason));
        }

        void onShutdown(std::string_view p_reason) override
        {
            stopWatchdog();
            sendLog("=== OrderTestStrategyProcess stopped ===");
            sendLog("[Shutdown] " + std::string(p_reason));
        }

        void onOrderUpdate(const Protocol::OrderUpdate& p_order) override
        {
            sendLog("[Order] id=" + p_order.order_id() + " status=" + orderStatusToString(p_order.status()));

            switch (m_state)
            {
            case State::WaitMarketFill:
                handleMarketOrderUpdate(p_order);
                break;
            case State::WaitLimitOpen:
                handleLimitOrderUpdate(p_order);
                break;
            case State::WaitCancel:
                handleCancelUpdate(p_order);
                break;
            case State::Init:
            case State::Complete:
                break;
            }
        }

        void onPositionUpdate(const Protocol::PositionUpdate& p_position) override
        {
            sendLog("[Position] symbol=" + p_position.symbol() + " qty=" + std::to_string(p_position.quantity()) +
                    " avgPx=" + std::to_string(p_position.average_price()));
        }

        void onBalanceUpdate(const Protocol::BalanceUpdate& p_balance) override
        {
            sendLog("[Balance] $" + std::to_string(p_balance.cash()), Protocol::LOG_LEVEL_DEBUG);
        }

        void onHostError(const Protocol::ErrorMessage& p_error) override
        {
            fail("[Error] " + p_error.code() + ": " + p_error.message());
        }

      private:
        enum class State
        {
            Init,
            WaitMarketFill,
            WaitLimitOpen,
            WaitCancel,
            Complete,
        };

        void placeMarketOrder()
        {
            sendLog("[Step 1] Placing market BUY 1 " + m_symbol + " ...");
            m_state = State::WaitMarketFill;
            m_marketOrderRequestId =
                m_runtime->placeOrder(m_symbol, m_accountId, Protocol::ORDER_SIDE_BUY, Protocol::ORDER_TYPE_MARKET, 1);
            if (m_marketOrderRequestId.empty())
            {
                fail("[Step 1] FAIL - unable to send market order intent");
            }
        }

        void placeLimitOrder()
        {
            sendLog("[Step 2] Placing limit BUY 1 " + m_symbol + " @ $" + std::to_string(m_limitPrice) + " ...");
            m_state = State::WaitLimitOpen;
            m_limitOrderRequestId = m_runtime->placeOrder(m_symbol,
                                                          m_accountId,
                                                          Protocol::ORDER_SIDE_BUY,
                                                          Protocol::ORDER_TYPE_LIMIT,
                                                          1,
                                                          m_limitPrice);
            if (m_limitOrderRequestId.empty())
            {
                fail("[Step 2] FAIL - unable to send limit order intent");
            }
        }

        void cancelLimitOrder()
        {
            if (m_limitOrderId.empty())
            {
                fail("[Step 3] FAIL - missing limit order id");
                return;
            }

            sendLog("[Step 3] Cancelling limit order " + m_limitOrderId + " ...");
            m_cancelSent = true;
            m_state = State::WaitCancel;
            m_cancelRequestId = m_runtime->cancelOrder(m_limitOrderId);
            if (m_cancelRequestId.empty())
            {
                fail("[Step 3] FAIL - unable to send cancel intent");
            }
        }

        void handleMarketOrderUpdate(const Protocol::OrderUpdate& p_order)
        {
            if (p_order.symbol() != m_symbol)
            {
                return;
            }

            if (m_marketOrderId.empty())
            {
                m_marketOrderId = p_order.order_id();
                m_marketOrderPlaced = true;
                sendLog("[Step 1] Market order acknowledged - OrderID=" + m_marketOrderId);
            }
            else if (p_order.order_id() != m_marketOrderId)
            {
                return;
            }

            if (p_order.status() == Protocol::ORDER_STATUS_FILLED)
            {
                m_marketFillReceived = true;
                sendLog(
                    "[Step 1] PASS - market order filled. SDK: orders=" + std::to_string(m_runtime->orders().size()) +
                    " positions=" + std::to_string(m_runtime->positions().size()));
                placeLimitOrder();
            }
            else if (p_order.status() == Protocol::ORDER_STATUS_REJECTED)
            {
                fail("[Step 1] FAIL - market order rejected");
            }
        }

        void handleLimitOrderUpdate(const Protocol::OrderUpdate& p_order)
        {
            if (p_order.symbol() != m_symbol)
            {
                return;
            }

            if (m_limitOrderId.empty())
            {
                m_limitOrderId = p_order.order_id();
                m_limitOrderPlaced = true;
                sendLog("[Step 2] Limit order acknowledged - OrderID=" + m_limitOrderId);
            }
            else if (p_order.order_id() != m_limitOrderId)
            {
                return;
            }

            if (p_order.status() == Protocol::ORDER_STATUS_OPEN ||
                p_order.status() == Protocol::ORDER_STATUS_ACKNOWLEDGED)
            {
                m_limitOPNReceived = true;
                sendLog("[Step 2] PASS - limit order is open");
                cancelLimitOrder();
            }
            else if (p_order.status() == Protocol::ORDER_STATUS_REJECTED)
            {
                fail("[Step 2] FAIL - limit order rejected");
            }
            else if (p_order.status() == Protocol::ORDER_STATUS_FILLED)
            {
                fail("[Step 2] FAIL - limit order filled before cancellation");
            }
        }

        void handleCancelUpdate(const Protocol::OrderUpdate& p_order)
        {
            if (p_order.order_id() != m_limitOrderId)
            {
                return;
            }

            if (p_order.status() == Protocol::ORDER_STATUS_CANCELLED)
            {
                m_cancelConfirmed = true;
                sendLog("[Step 3] PASS - limit order cancelled");
                complete();
            }
            else if (p_order.status() == Protocol::ORDER_STATUS_REJECTED)
            {
                fail("[Step 3] FAIL - limit order cancellation rejected");
            }
        }

        void complete()
        {
            stopWatchdog();
            m_state = State::Complete;
            printSummary();
        }

        void fail(const std::string& p_message)
        {
            sendLog(p_message, Protocol::LOG_LEVEL_ERROR);
            stopWatchdog();
            m_state = State::Complete;
            printSummary();
        }

        void printSummary()
        {
            sendLog("");
            sendLog("========================================");
            sendLog("         ORDER TEST RESULTS");
            sendLog("========================================");
            sendLog("  [1] Market order placed   : " + passFail(m_marketOrderPlaced));
            sendLog("  [2] Market order filled   : " + passFail(m_marketFillReceived));
            sendLog("  [3] Limit order placed    : " + passFail(m_limitOrderPlaced));
            sendLog("  [4] Limit order OPN       : " + passFail(m_limitOPNReceived));
            sendLog("  [5] Cancel request sent   : " + passFail(m_cancelSent));
            sendLog("  [6] Cancel confirmed      : " + passFail(m_cancelConfirmed));
            sendLog("========================================");

            const bool allPassed = m_marketOrderPlaced && m_marketFillReceived && m_limitOrderPlaced &&
                                   m_limitOPNReceived && m_cancelSent && m_cancelConfirmed;
            sendLog(allPassed ? "  OVERALL: ALL TESTS PASSED" : "  OVERALL: SOME TESTS FAILED");
            sendLog("========================================");
            sendLog("");
            sendLog("[Final State] orders=" + std::to_string(m_runtime->orders().size()) +
                    " positions=" + std::to_string(m_runtime->positions().size()) + " balance=$" +
                    std::to_string(m_runtime->cashBalance()));
        }

        [[nodiscard]] static std::string passFail(const bool p_value)
        {
            return p_value ? "PASS" : "FAIL";
        }

        void stopWatchdog()
        {
            if (m_runtime == nullptr || m_timeoutTimerId == 0)
            {
                return;
            }

            (void)m_runtime->cancelTimer(m_timeoutTimerId);
            m_timeoutTimerId = 0;
        }

        void sendLog(const std::string& p_message, const Protocol::LogLevel p_level = Protocol::LOG_LEVEL_INFO)
        {
            if (m_runtime == nullptr || m_runtime->log(p_message, p_level))
            {
                return;
            }

            std::cerr << "[OrderTestStrategyProcess] Failed to send log: " << p_message << std::endl;
        }

        L2Trader::StrategySDK::ExternalStrategyRuntime* m_runtime = nullptr;
        State m_state = State::Init;
        std::string m_symbol = "SPY";
        std::string m_accountId = "SIM123456";
        double m_limitPrice = 1.0;
        std::string m_marketOrderRequestId;
        std::string m_limitOrderRequestId;
        std::string m_cancelRequestId;
        std::string m_marketOrderId;
        std::string m_limitOrderId;
        bool m_marketOrderPlaced = false;
        bool m_marketFillReceived = false;
        bool m_limitOrderPlaced = false;
        bool m_limitOPNReceived = false;
        bool m_cancelSent = false;
        bool m_cancelConfirmed = false;
        L2Trader::StrategySDK::ExternalStrategyRuntime::TimerId m_timeoutTimerId = 0;
    };
} // namespace

int main(int argc, char** argv)
{
    OrderTestStrategyProcess strategy;
    return L2Trader::StrategySDK::runStrategyProcessMain(argc, argv, describeStrategy(), strategy);
}
