#include <QTest>
#include <QTableView>
#include <QStandardItemModel>
#include "FrontEnd/GUI/Widgets/PositionWidget.h"
#include "Misc/CONSTANTS.h"

class PositionWidgetTests : public QObject
{
    Q_OBJECT

  private slots:
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
