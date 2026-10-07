#include <QTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>
#include <cmath>
#include "Core/PositionPnL.h"

namespace
{
    Position position(const QString& p_opened = "2026-10-07T07:18:46-04:00",
                      const QString& p_closed = "2026-10-07T07:22:49-04:00",
                      const QString& p_side = "Long")
    {
        return Position(QJsonObject{{"PositionID", "287803480"},
                                    {"AccountID", "test"},
                                    {"AssetType", "STOCK"},
                                    {"Symbol", "LPCN"},
                                    {"Quantity", "0"},
                                    {"LongShort", p_side},
                                    {"OpenedDateTime", p_opened},
                                    {"ClosedDateTime", p_closed},
                                    {"TodaysProfitLoss", "99.24"}});
    }

    Order order(const QString& p_id,
                const QString& p_action,
                double p_quantity,
                double p_price,
                const QString& p_time,
                const QString& p_status = "FLL")
    {
        return Order(QJsonObject{{"OrderID", p_id},
                                 {"AccountID", "test"},
                                 {"Symbol", "LPCN"},
                                 {"Quantity", QString::number(p_quantity)},
                                 {"TradeAction", p_action},
                                 {"FilledPrice", p_price},
                                 {"OpenedDateTime", p_time},
                                 {"ClosedDateTime", p_time},
                                 {"Status", p_status}});
    }
} // namespace

class PositionPnLTests : public QObject
{
    Q_OBJECT

  private slots:
    void reproducesFirstWinningTradeAndLateFill()
    {
        const QVector<double>
            prices{3.3799, 3.36, 3.3789, 3.37, 3.44, 3.359997, 3.36, 3.349, 3.35, 3.34, 3.33, 3.3389, 3.34, 3.51};
        QVector<Order> orders;
        for (qsizetype i = 0; i < prices.size(); ++i)
        {
            orders.append(order(QString::number(i), "Buy", 100, prices[i], "2026-10-07T07:18:45-04:00"));
        }
        QVERIFY(!PositionPnL::calculateClosedPosition(position(), orders).grossProfit.has_value());
        orders.append(order("exit", "Sell", 1400, 3.6371428572, "2026-10-07T07:22:48-04:00"));
        const auto result = PositionPnL::calculateClosedPosition(position(), orders);
        QVERIFY2(result.grossProfit.has_value(), qPrintable(result.reason));
        QVERIFY(std::abs(result.grossProfit.value() - 371.33030008) < 0.000001);
        QCOMPARE(QString::number(result.grossProfit.value(), 'f', 2), QString("371.33"));
        QCOMPARE(result.peakQuantity.value(), 1400.0);
        std::reverse(orders.begin(), orders.end());
        QCOMPARE(PositionPnL::calculateClosedPosition(position(), orders).grossProfit.value(),
                 result.grossProfit.value());
    }

    void isolatesSubsequentTradesAndAccounts()
    {
        QVector<Order> orders{order("1", "Buy", 100, 3.0, "2026-10-07T07:18:45-04:00"),
                              order("2", "Sell", 100, 4.0, "2026-10-07T07:22:48-04:00"),
                              order("3", "Buy", 100, 5.0, "2026-10-07T07:30:00-04:00"),
                              order("4", "Sell", 100, 2.0, "2026-10-07T07:31:00-04:00")};
        Order other = order("other", "Sell", 900, 8.0, "2026-10-07T07:22:48-04:00");
        other.m_accountID = "another-account";
        orders.append(other);
        QCOMPARE(PositionPnL::calculateClosedPosition(position(), orders).grossProfit.value(), 100.0);
        QCOMPARE(
            PositionPnL::calculateClosedPosition(position("2026-10-07T07:30:00-04:00", "2026-10-07T07:31:01-04:00"),
                                                 orders)
                .grossProfit.value(),
            -300.0);
    }

    void handlesScaleOutAndReentryWithinCycle()
    {
        const QVector<Order> orders{order("1", "Buy", 100, 10.0, "2026-10-07T07:18:45-04:00"),
                                    order("2", "Sell", 50, 12.0, "2026-10-07T07:19:00-04:00"),
                                    order("3", "Buy", 50, 11.0, "2026-10-07T07:20:00-04:00"),
                                    order("4", "Sell", 100, 13.0, "2026-10-07T07:22:48-04:00")};
        QCOMPARE(PositionPnL::calculateClosedPosition(position(), orders).grossProfit.value(), 350.0);
        QCOMPARE(PositionPnL::calculateClosedPosition(position(), orders).peakQuantity.value(), 100.0);
    }

    void handlesShortsAndOvernightPositions()
    {
        const QVector<Order> orders{order("1", "Sell Short", 100, 10.0, "2026-10-06T15:00:00-04:00"),
                                    order("2", "Buy to Cover", 100, 8.0, "2026-10-07T07:22:48-04:00")};
        QCOMPARE(PositionPnL::calculateClosedPosition(
                     position("2026-10-06T15:00:01-04:00", "2026-10-07T07:22:49-04:00", "Short"),
                     orders)
                     .grossProfit.value(),
                 200.0);
        QCOMPARE(PositionPnL::calculateClosedPosition(
                     position("2026-10-06T15:00:01-04:00", "2026-10-07T07:22:49-04:00", "Short"),
                     orders)
                     .peakQuantity.value(),
                 100.0);
    }

    void tracksNewPeakAfterScaleOutWithoutCountingTotalPurchases()
    {
        const QVector<Order> orders{order("1", "Buy", 100, 10.0, "2026-10-07T07:18:45-04:00"),
                                    order("2", "Sell", 50, 12.0, "2026-10-07T07:19:00-04:00"),
                                    order("3", "Buy", 200, 11.0, "2026-10-07T07:20:00-04:00"),
                                    order("4", "Sell", 250, 13.0, "2026-10-07T07:22:48-04:00")};
        const auto result = PositionPnL::calculateClosedPosition(position(), orders);
        QVERIFY(result.peakQuantity.has_value());
        QCOMPARE(result.peakQuantity.value(), 250.0);
        QCOMPARE(result.grossProfit.value(), 650.0);
    }

    void usesExecutedRatherThanOrderedQuantity()
    {
        Order partial = order("1", "Buy", 1000, 3.0, "2026-10-07T07:18:45-04:00", "OUT");
        partial.m_executedQuantity = 100;
        const Order exit = order("2", "Sell", 100, 4.0, "2026-10-07T07:22:48-04:00");
        QCOMPARE(PositionPnL::calculateClosedPosition(position(), {partial, exit}).grossProfit.value(), 100.0);
        QCOMPARE(PositionPnL::calculateClosedPosition(position(), {partial, exit}).peakQuantity.value(), 100.0);
        partial.m_executedQuantity.reset();
        QVERIFY(!PositionPnL::calculateClosedPosition(position(), {partial, exit}).grossProfit.has_value());
    }

    void rejectsMissingEntriesAndIncompleteExits()
    {
        const Order buy = order("1", "Buy", 1400, 3.0, "2026-10-07T07:18:45-04:00");
        const Order partialExit = order("2", "Sell", 1000, 4.0, "2026-10-07T07:22:48-04:00");
        QVERIFY(!PositionPnL::calculateClosedPosition(position(), {buy, partialExit}).grossProfit.has_value());
        QVERIFY(!PositionPnL::calculateClosedPosition(position(), {buy, partialExit}).peakQuantity.has_value());
        QVERIFY(!PositionPnL::calculateClosedPosition(position(), {partialExit}).grossProfit.has_value());
        const Order wrongEntry = order("1", "Buy", 100, 3.0, "2026-10-07T07:18:45-04:00");
        QVERIFY(!PositionPnL::calculateClosedPosition(position(), {wrongEntry, partialExit}).grossProfit.has_value());
    }

    void persistsSeparateRealizedField()
    {
        Position value = position();
        QVERIFY(!value.getRealizedProfitLoss().has_value());
        value.setRealizedProfitLoss(371.33030008);
        value.setPeakQuantity(1400);
        Position restored(QJsonDocument::fromJson(value.toJsonString().toUtf8()).object());
        QCOMPARE(restored.getRealizedProfitLoss().value(), 371.33030008);
        QCOMPARE(restored.getPeakQuantity().value(), 1400.0);
        QCOMPARE(restored.getTodaysProfitLoss(), QString("99.24"));
        restored.setRealizedProfitLoss(std::nullopt);
        restored.setPeakQuantity(std::nullopt);
        QVERIFY(!QJsonDocument::fromJson(restored.toJsonString().toUtf8()).object().contains("PeakQuantity"));
        QVERIFY(!QJsonDocument::fromJson(restored.toJsonString().toUtf8()).object().contains("RealizedProfitLoss"));
    }

    void waitsForBrokerConfirmationInsteadOfUsingSyntheticPrices()
    {
        const Order buy = order("1", "Buy", 100, 3.0, "2026-10-07T07:18:45-04:00");
        Order exit = order("2", "Sell", 100, 3.62, "2026-10-07T07:22:48-04:00");
        exit.m_fillIsSynthetic = true;
        QVERIFY(!PositionPnL::calculateClosedPosition(position(), {buy, exit}).grossProfit.has_value());
        exit.m_fillIsSynthetic = false;
        exit.m_filledPrice = 3.6371428572;
        QVERIFY(std::abs(PositionPnL::calculateClosedPosition(position(), {buy, exit}).grossProfit.value() -
                         63.71428572) < 0.000001);
    }

    void parsesExecutedQuantityFromBrokerPayload()
    {
        Order parsed(QJsonObject{{"OrderID", "partial"},
                                 {"Status", "OUT"},
                                 {"Legs",
                                  QJsonArray{QJsonObject{{"ExecQuantity", "400"},
                                                         {"QuantityOrdered", "1400"},
                                                         {"BuyOrSell", "Sell"},
                                                         {"OpenOrClose", "Close"}}}}});
        QCOMPARE(parsed.m_executedQuantity.value(), 400.0);
    }

    void interpretsLegacyLifecycleTimestampsInMarketTimezone()
    {
        const Position legacy = position("2026-10-07T07:18:46", "2026-10-07T07:22:49");
        QCOMPARE(legacy.getOpenedDateTime(), position().getOpenedDateTime());
        QCOMPARE(legacy.getClosedDateTime(), position().getClosedDateTime());
    }
};

QTEST_GUILESS_MAIN(PositionPnLTests)
#include "PositionPnLTests.moc"
