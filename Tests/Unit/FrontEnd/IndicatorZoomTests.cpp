#include <QtTest>
#include <memory>

#include "Indicators/MacdIndicator.h"
#include "Indicators/RsiIndicator.h"
#include "Misc/CONSTANTS.h"

class IndicatorZoomTests : public QObject
{
    Q_OBJECT

  private:
    static QDateTime startTime()
    {
        return QDateTime(QDate(2026, 10, 5), QTime(9, 30), TradingHours::MARKET_TIMEZONE);
    }

    static void appendBar(QMap<int, Bar>& p_bars, const float p_close)
    {
        const int index = p_bars.size();
        p_bars.insert(index, Bar(startTime().addSecs(index * 60), p_close, p_close, p_close, p_close, 100));
    }

  private slots:
    void preservesManualRange_data()
    {
        QTest::addColumn<bool>("macd");
        QTest::addColumn<double>("zoomFactor");
        QTest::newRow("macd-zoom-in") << true << 0.5;
        QTest::newRow("macd-zoom-out") << true << 2.0;
        QTest::newRow("rsi-zoom-in") << false << 0.5;
        QTest::newRow("rsi-zoom-out") << false << 2.0;
    }

    void preservesManualRange()
    {
        QFETCH(bool, macd);
        QFETCH(double, zoomFactor);
        QCustomPlot plot;
        std::unique_ptr<ChartIndicator> indicator;
        if (macd)
        {
            indicator = std::make_unique<MacdIndicator>(&plot, plot.xAxis, plot.yAxis);
        }
        else
        {
            indicator = std::make_unique<RsiIndicator>(&plot, plot.xAxis, plot.yAxis);
        }
        indicator->setVisible(true);
        QMap<int, Bar> bars;
        appendBar(bars, 10.0f);
        appendBar(bars, 11.0f);
        appendBar(bars, 10.5f);
        const ChartIndicator::UpdateContext context{bars, startTime(), TimeFrame::ONE_MINUTE};
        indicator->rebuild(context);
        const QCPRange automaticRange = plot.yAxis->range();

        // Match the subplot wheel zoom: scale around the range center.
        plot.yAxis->scaleRange(zoomFactor, automaticRange.center());
        plot.yAxis->moveRange(automaticRange.size() * 0.1);
        const QCPRange manualRange = plot.yAxis->range();
        appendBar(bars, 100.0f);
        indicator->rebuild(context);
        QVERIFY(plot.yAxis->range() == manualRange);
        bars.last().setClose(200.0f);
        indicator->rebuild(context);
        QVERIFY(plot.yAxis->range() == manualRange);
        indicator->setVisible(false);
        indicator->rebuild(context);
        indicator->setVisible(true);
        indicator->rebuild(context);
        QVERIFY(plot.yAxis->range() == manualRange);
        appendBar(bars, 300.0f);
        indicator->rebuild(context);
        QVERIFY(plot.yAxis->range() == manualRange);

        indicator->clear();
        indicator->rebuild(context);
        QVERIFY(plot.yAxis->range() != manualRange);
        if (!macd)
        {
            QVERIFY(plot.yAxis->range() == QCPRange(0.0, 100.0));
        }
    }

    void macdAutomaticallyScalesUntilAdjusted()
    {
        QCustomPlot plot;
        MacdIndicator indicator(&plot, plot.xAxis, plot.yAxis);
        indicator.setVisible(true);
        QMap<int, Bar> bars;
        appendBar(bars, 10.0f);
        appendBar(bars, 11.0f);
        const ChartIndicator::UpdateContext context{bars, startTime(), TimeFrame::ONE_MINUTE};
        indicator.rebuild(context);
        const QCPRange initialRange = plot.yAxis->range();
        QCOMPARE(initialRange.center(), 0.0);
        appendBar(bars, 100.0f);
        indicator.rebuild(context);
        QVERIFY(plot.yAxis->range().size() > initialRange.size());
        QCOMPARE(plot.yAxis->range().center(), 0.0);
    }

    void macdZeroCenteredRange_data()
    {
        QTest::addColumn<float>("lastClose");
        QTest::newRow("rising") << 100.0f;
        QTest::newRow("falling") << 1.0f;
        QTest::newRow("flat") << 10.0f;
    }

    void macdZeroCenteredRange()
    {
        QFETCH(float, lastClose);
        QCustomPlot plot;
        MacdIndicator indicator(&plot, plot.xAxis, plot.yAxis);
        indicator.setVisible(true);
        QMap<int, Bar> bars;
        appendBar(bars, 10.0f);
        appendBar(bars, lastClose);
        const ChartIndicator::UpdateContext context{bars, startTime(), TimeFrame::ONE_MINUTE};
        indicator.rebuild(context);
        const QCPRange range = plot.yAxis->range();
        QVERIFY(range.upper > 0.0);
        QCOMPARE(range.lower, -range.upper);
    }
};

QTEST_MAIN(IndicatorZoomTests)
#include "IndicatorZoomTests.moc"
