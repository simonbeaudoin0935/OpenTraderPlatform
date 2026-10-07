#include <QTest>
#include <QTableView>
#include <QStandardItemModel>
#include <QSplitter>
#include <QHeaderView>
#include "FrontEnd/GUI/Widgets/PositionWidget.h"
#include "FrontEnd/GUI/Widgets/OrderWidget.h"
#include "Misc/CONSTANTS.h"

class PositionWidgetTests : public QObject
{
    Q_OBJECT

  private slots:
    void compactProfitHeadersExplainTheirMeaning()
    {
        PositionWidget widget;
        auto* table = widget.findChild<QTableView*>();
        QVERIFY(table);
        auto* model = table->model();
        QCOMPARE(model->headerData(4, Qt::Horizontal).toString(), QString("U. P&L"));
        QCOMPARE(model->headerData(5, Qt::Horizontal).toString(), QString("R. P&L"));
        QVERIFY(model->headerData(4, Qt::Horizontal, Qt::ToolTipRole).toString().contains("Unrealized"));
        const auto realizedTooltip = model->headerData(5, Qt::Horizontal, Qt::ToolTipRole).toString();
        QVERIFY(realizedTooltip.contains("Realized"));
        QVERIFY(realizedTooltip.contains("gross"));
        QVERIFY(realizedTooltip.contains("before fees"));
        const QFontMetrics metrics(table->horizontalHeader()->font());
        for (int column: {4, 5})
        {
            QVERIFY(metrics.horizontalAdvance(model->headerData(column, Qt::Horizontal).toString()) <
                    table->columnWidth(column));
        }
    }

    void splitterCanResizePositionsAgainstOrders()
    {
        QSplitter splitter(Qt::Horizontal);
        splitter.setChildrenCollapsible(false);
        auto* positions = new PositionWidget(&splitter);
        auto* orders = new OrderWidget(&splitter);
        splitter.addWidget(positions);
        splitter.addWidget(orders);
        splitter.resize(1400, 300);
        splitter.show();
        QVERIFY(QTest::qWaitForWindowExposed(&splitter));
        splitter.setSizes({700, 700});
        QCoreApplication::processEvents();

        const int initialWidth = positions->width();
        auto* handle = splitter.handle(1);
        const QPoint start = handle->rect().center();
        QTest::mousePress(handle, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(handle, start - QPoint(180, 0));
        QTest::mouseRelease(handle, Qt::LeftButton, Qt::NoModifier, start - QPoint(180, 0));
        QCoreApplication::processEvents();
        QVERIFY(positions->width() < initialWidth - 100);
        QVERIFY(positions->width() >= positions->minimumWidth());
        QVERIFY(positions->maximumWidth() > initialWidth);
        QCOMPARE(positions->findChild<QTableView*>()->horizontalScrollBarPolicy(), Qt::ScrollBarAsNeeded);
    }

    void displaysClosedPeakAndUpdatesPendingRow()
    {
        PositionWidget widget;
        auto* table = widget.findChild<QTableView*>();
        QVERIFY(table);
        auto* model = qobject_cast<QStandardItemModel*>(table->model());
        QVERIFY(model);
        QCOMPARE(model->columnCount(), 9);
        QCOMPARE(model->headerData(7, Qt::Horizontal).toString(), QString("Peak Shares"));
        QCOMPARE(model->headerData(8, Qt::Horizontal).toString(), QString("Position ID"));

        const auto now = QDateTime::currentDateTime().toTimeZone(TradingHours::MARKET_TIMEZONE);
        QJsonObject json{{"PositionID", "closed"},
                         {"AccountID", "test"},
                         {"Symbol", "LPCN"},
                         {"Quantity", "0"},
                         {"OpenedDateTime", now.addSecs(-60).toString(Qt::ISODate)},
                         {"ClosedDateTime", now.toString(Qt::ISODate)},
                         {"Deleted", true}};
        widget.updatePosition("test", Position(json));
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->item(0, 1)->text(), QString("0"));
        QCOMPARE(model->item(0, 7)->text(), QString("Pending"));
        QCOMPARE(model->item(0, 8)->text(), QString("closed"));

        Position reconciled(json);
        reconciled.setPeakQuantity(1400);
        widget.updatePosition("test", reconciled);
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->item(0, 7)->text(), QString("1400"));
        QCOMPARE(model->item(0, 8)->text(), QString("closed"));

        json["Quantity"] = "400";
        json["Deleted"] = false;
        json.remove("ClosedDateTime");
        widget.updatePosition("test", Position(json));
        QCOMPARE(model->item(0, 1)->text(), QString("400"));
        QVERIFY(model->item(0, 7)->text().isEmpty());
        QCOMPARE(model->item(0, 8)->text(), QString("closed"));
    }
};

QTEST_MAIN(PositionWidgetTests)
#include "PositionWidgetTests.moc"
