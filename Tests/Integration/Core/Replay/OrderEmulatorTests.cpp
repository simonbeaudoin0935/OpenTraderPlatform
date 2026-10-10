#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

#include "MainApp.h"
#include "OrderEmulator.h"

namespace
{
    const QDateTime kRegularTime(QDate(2026, 10, 7), QTime(10, 0), QTimeZone("America/New_York"));

    Level2 depth(double p_bid, double p_ask)
    {
        Level2 result;
        result.m_symbol = "TEST";
        result.m_timeStamp = kRegularTime;
        result.m_bids[0].m_price = p_bid;
        result.m_bids[0].m_size = 1000;
        result.m_asks[0].m_price = p_ask;
        result.m_asks[0].m_size = 1000;
        return result;
    }

    PlaceOrderRequest order(TradeAction p_action, OrderType::Type p_type = OrderType::Type::Market, int p_quantity = 10)
    {
        PlaceOrderRequest result;
        result.setAccountID(OrderEmulator::getSimulatedAccountID());
        result.setSymbol("TEST");
        result.setTradeAction(p_action);
        result.setOrderType(p_type);
        result.setQuantity(p_quantity);
        result.setRoute("replay");
        return result;
    }

    QJsonObject signalObject(const QSignalSpy& p_spy, int p_index)
    {
        return QJsonDocument::fromJson(p_spy.at(p_index).at(0).toByteArray()).object();
    }

    void waitForSignals(const QSignalSpy& p_spy, int p_count)
    {
        QTRY_VERIFY_WITH_TIMEOUT(p_spy.count() >= p_count, 1000);
    }

    void setRegularReplayTime()
    {
        MainApp::currentAppReplayTime = kRegularTime;
    }
} // namespace

class OrderEmulatorTests : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase()
    {
        setRegularReplayTime();
    }

    void invalidOrderIsRejectedWithReason()
    {
        OrderEmulator emulator;
        QSignalSpy statusSpy(&emulator, &OrderEmulator::orderStatusUpdate);

        PlaceOrderRequest request = order(TradeAction::Buy);
        request.setAccountID("WRONG");
        const QString orderId = emulator.placeOrder(request);

        waitForSignals(statusSpy, 1);
        const QJsonObject rejected = signalObject(statusSpy, 0);
        QCOMPARE(rejected.value("OrderID").toString(), orderId);
        QCOMPARE(rejected.value("Status").toString(), QStringLiteral("REJ"));
        QVERIFY(rejected.value("StatusDescription").toString().contains("Invalid account ID"));
        QCOMPARE(emulator.getOrders().size(), 1);
        QCOMPARE(emulator.getOrders().first().getOrderStatus(), Order::Status::REJ);
    }

    void marketableLimitOrderEmitsOpenAndFillAtTopOfBook()
    {
        OrderEmulator emulator;
        emulator.setReplaySpeed(-1);
        emulator.updateMarketDepth("TEST", depth(99.90, 100.10));
        QSignalSpy statusSpy(&emulator, &OrderEmulator::orderStatusUpdate);
        QSignalSpy positionSpy(&emulator, &OrderEmulator::positionUpdate);

        PlaceOrderRequest request = order(TradeAction::Buy, OrderType::Type::Limit);
        request.setLimitPrice(101.00);
        const QString orderId = emulator.placeOrder(request);

        waitForSignals(statusSpy, 2);
        waitForSignals(positionSpy, 1);
        QCOMPARE(signalObject(statusSpy, 0).value("OrderID").toString(), orderId);
        QCOMPARE(signalObject(statusSpy, 0).value("Status").toString(), QStringLiteral("OPN"));
        const QJsonObject filled = signalObject(statusSpy, 1);
        QCOMPARE(filled.value("Status").toString(), QStringLiteral("FLL"));
        QCOMPARE(filled.value("FilledPrice").toString(), QStringLiteral("100.1000"));
        QCOMPARE(emulator.getOrders().size(), 1);
        QCOMPARE(emulator.getOrders().first().getOrderStatus(), Order::Status::FLL);
        QCOMPARE(emulator.getPositions().size(), 1);
        QCOMPARE(emulator.getPositions().first().getQuantity().toInt(), 10);
    }

    void limitOrderWaitsForMarketabilityThenFillsWithPriceImprovement()
    {
        OrderEmulator emulator;
        emulator.setReplaySpeed(-1);
        emulator.updateMarketDepth("TEST", depth(99.90, 100.10));
        QSignalSpy statusSpy(&emulator, &OrderEmulator::orderStatusUpdate);

        PlaceOrderRequest request = order(TradeAction::Buy, OrderType::Type::Limit);
        request.setLimitPrice(99.00);
        const QString orderId = emulator.placeOrder(request);

        waitForSignals(statusSpy, 1);
        QCOMPARE(signalObject(statusSpy, 0).value("Status").toString(), QStringLiteral("OPN"));
        QTest::qWait(20);
        QCOMPARE(statusSpy.count(), 1);
        QCOMPARE(emulator.getOrders().size(), 1);
        QCOMPARE(emulator.getOrders().first().getOrderID(), orderId);

        emulator.updateMarketDepth("TEST", depth(98.90, 99.00));
        waitForSignals(statusSpy, 2);
        QCOMPARE(signalObject(statusSpy, 1).value("FilledPrice").toString(), QStringLiteral("99.0000"));

        emulator.clear();
        emulator.updateMarketDepth("TEST", depth(99.90, 100.10));
        QSignalSpy improvedSpy(&emulator, &OrderEmulator::orderStatusUpdate);
        request.setLimitPrice(101.00);
        const QString improvedOrderId = emulator.placeOrder(request);
        QVERIFY(!improvedOrderId.isEmpty());
        waitForSignals(improvedSpy, 2);
        QCOMPARE(signalObject(improvedSpy, 1).value("FilledPrice").toString(), QStringLiteral("100.1000"));
    }

    void openLimitOrderCanBeCancelled()
    {
        OrderEmulator emulator;
        emulator.setReplaySpeed(-1);
        emulator.updateMarketDepth("TEST", depth(99.90, 100.10));
        QSignalSpy statusSpy(&emulator, &OrderEmulator::orderStatusUpdate);

        PlaceOrderRequest request = order(TradeAction::Buy, OrderType::Type::Limit);
        request.setLimitPrice(99.00);
        const QString orderId = emulator.placeOrder(request);
        waitForSignals(statusSpy, 1);

        emulator.cancelOrder(orderId, "cancel-request");
        waitForSignals(statusSpy, 2);
        const QJsonObject cancelled = signalObject(statusSpy, 1);
        QCOMPARE(cancelled.value("Status").toString(), QStringLiteral("CAN"));
        QCOMPARE(emulator.getOrders().size(), 1);
        QCOMPARE(emulator.getOrders().first().getOrderStatus(), Order::Status::CAN);
    }

    void stopMarketOrderFillsOnlyAfterTrigger()
    {
        OrderEmulator emulator;
        emulator.setReplaySpeed(-1);
        emulator.updateMarketDepth("TEST", depth(99.90, 100.10));
        QSignalSpy statusSpy(&emulator, &OrderEmulator::orderStatusUpdate);

        PlaceOrderRequest request = order(TradeAction::Buy, OrderType::Type::StopMarket);
        request.setStopPrice(101.00);
        const QString stopOrderId = emulator.placeOrder(request);
        QVERIFY(!stopOrderId.isEmpty());
        waitForSignals(statusSpy, 1);
        QTest::qWait(20);
        QCOMPARE(statusSpy.count(), 1);

        emulator.updateMarketDepth("TEST", depth(100.80, 101.00));
        waitForSignals(statusSpy, 2);
        QCOMPARE(signalObject(statusSpy, 1).value("Status").toString(), QStringLiteral("FLL"));
        QCOMPARE(signalObject(statusSpy, 1).value("FilledPrice").toString(), QStringLiteral("101.0000"));
    }

    void closingPositionUpdatesRealizedProfitLossAndBalance()
    {
        OrderEmulator emulator;
        emulator.setReplaySpeed(-1);
        emulator.updateMarketDepth("TEST", depth(99.90, 100.10));
        QSignalSpy statusSpy(&emulator, &OrderEmulator::orderStatusUpdate);
        QSignalSpy positionSpy(&emulator, &OrderEmulator::positionUpdate);

        PlaceOrderRequest buyRequest = order(TradeAction::Buy, OrderType::Type::Limit);
        buyRequest.setLimitPrice(101.00);
        const QString firstOrderId = emulator.placeOrder(buyRequest);
        QVERIFY(!firstOrderId.isEmpty());
        waitForSignals(statusSpy, 2);
        emulator.updateMarketDepth("TEST", depth(104.90, 105.10));

        PlaceOrderRequest sellRequest = order(TradeAction::Sell, OrderType::Type::Limit);
        sellRequest.setLimitPrice(104.00);
        const QString sellOrderId = emulator.placeOrder(sellRequest);
        QVERIFY(!sellOrderId.isEmpty());
        waitForSignals(statusSpy, 4);
        waitForSignals(positionSpy, 2);
        QCOMPARE(emulator.getPositions().size(), 0);
        QCOMPARE(emulator.getRealizedProfitLoss(), 48.0);
        QCOMPARE(emulator.getClosedPositionsPnL(), 48.0);
        QCOMPARE(emulator.getBalance(), 100048.0);
        const QJsonObject closed = signalObject(positionSpy, positionSpy.count() - 1);
        QCOMPARE(closed.value("Quantity").toString(), QStringLiteral("0"));
        QCOMPARE(closed.value("TodaysProfitLoss").toString(), QStringLiteral("48.00"));
    }

    void clearResetsStateAndOrderIdSequence()
    {
        OrderEmulator emulator;
        emulator.setReplaySpeed(-1);
        emulator.updateMarketDepth("TEST", depth(99.90, 100.10));
        QSignalSpy statusSpy(&emulator, &OrderEmulator::orderStatusUpdate);

        PlaceOrderRequest request = order(TradeAction::Buy, OrderType::Type::Limit);
        request.setLimitPrice(101.00);
        const QString firstOrderId = emulator.placeOrder(request);
        QVERIFY(!firstOrderId.isEmpty());
        waitForSignals(statusSpy, 2);
        QVERIFY(!emulator.getOrders().isEmpty());

        emulator.clear();
        QCOMPARE(emulator.getOrders().size(), 0);
        QCOMPARE(emulator.getPositions().size(), 0);
        QCOMPARE(emulator.getBalance(), 100000.0);
        QCOMPARE(emulator.getRealizedProfitLoss(), 0.0);
        QCOMPARE(emulator.getClosedPositionsPnL(), 0.0);

        emulator.updateMarketDepth("TEST", depth(99.90, 100.10));
        const QString newOrderId = emulator.placeOrder(request);
        QCOMPARE(newOrderId, QStringLiteral("1000"));
    }
};

QTEST_GUILESS_MAIN(OrderEmulatorTests)
#include "OrderEmulatorTests.moc"
