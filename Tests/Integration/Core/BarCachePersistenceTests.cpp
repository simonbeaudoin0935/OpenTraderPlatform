#include <QScopeGuard>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include "BarCache.h"
#include "BarUtils.h"
#include "DatabaseThread.h"
#include "MainApp.h"
#include "SecureStorage.h"
#include "Settings.h"

class BarCachePersistenceTests : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase()
    {
        QVERIFY(m_directory.isValid());
        QCoreApplication::setOrganizationName("OpenTraderPlatform");
        QCoreApplication::setApplicationName("BarCachePersistenceTests");
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_directory.path());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_directory.path());
        // A locked, isolated vault prevents this local fixture from reading host credentials.
        QVERIFY(SecureStorage::configureBackend(SecureStorage::Backend::YubiKey));
        m_settings = std::make_unique<QSettings>(m_directory.path() + "/state.ini", QSettings::IniFormat);
        appStateSettings = m_settings.get();
        m_oldCacheRoot = cacheRootDir;
        cacheRootDir = m_directory.path();
        MainApp::setTradingMode(TradingMode::Sim);
        TSClient::getInstance()->start();
        m_clientCreated = true;
        DatabaseThread::getInstance()->start();
        m_databaseCreated = true;
    }

    void persistsAndReloadsInclusiveRanges()
    {
        const QString symbol = "CACHE_RELOAD";
        const QDate day(2024, 1, 8);
        {
            BarCache cache(symbol, &m_owner);
            for (int index = 0; index < 960; ++index)
                cache.storeBar(TimeFrame::ONE_MINUTE, bar(day, index, 60, 100.0f));
            auto saved = DatabaseThread::getInstance()->getCachedBarsForDate(symbol, TimeFrame::ONE_MINUTE, day);
            QTRY_VERIFY_WITH_TIMEOUT(saved.isFinished(), 10000);
            QCOMPARE(saved.result()->size(), qsizetype(960));
            compareRange(cache,
                         TimeFrame::ONE_MINUTE,
                         day,
                         QTime(4, 1),
                         QTime(4, 3),
                         {bar(day, 1, 60, 100.0f), bar(day, 2, 60, 100.0f), bar(day, 3, 60, 100.0f)});
            QCOMPARE(cache.getLatestClosedBarTimestamp(TimeFrame::ONE_MINUTE, day),
                     std::optional<QDateTime>(bar(day, 959, 60, 100.0f).getTimeStamp()));
        }
        BarCache reloaded(symbol, &m_owner);
        compareRange(reloaded,
                     TimeFrame::ONE_MINUTE,
                     day,
                     QTime(4, 1),
                     QTime(4, 3),
                     {bar(day, 1, 60, 100.0f), bar(day, 2, 60, 100.0f), bar(day, 3, 60, 100.0f)});
        compareRange(reloaded, TimeFrame::ONE_MINUTE, day, QTime(19, 59), QTime(19, 59), {bar(day, 959, 60, 100.0f)});
        QCOMPARE(reloaded.getLatestClosedBarTimestamp(TimeFrame::ONE_MINUTE, day),
                 std::optional<QDateTime>(bar(day, 959, 60, 100.0f).getTimeStamp()));
    }

    void isolatesSymbolsDatesAndTimeFrames()
    {
        const QDate firstDay(2024, 1, 8);
        const QDate secondDay(2024, 1, 9);
        BarCache first("CACHE_FIRST", &m_owner);
        BarCache second("CACHE_SECOND", &m_owner);
        first.storeBar(TimeFrame::ONE_MINUTE, bar(firstDay, 0, 60, 10.0f));
        first.storeBar(TimeFrame::FIVE_MINUTES, bar(firstDay, 0, 300, 20.0f));
        first.storeBar(TimeFrame::ONE_MINUTE, bar(secondDay, 0, 60, 30.0f));
        second.storeBar(TimeFrame::ONE_MINUTE, bar(firstDay, 0, 60, 40.0f));
        compareRange(first, TimeFrame::ONE_MINUTE, firstDay, QTime(4, 0), QTime(4, 0), {bar(firstDay, 0, 60, 10.0f)});
        compareRange(first,
                     TimeFrame::FIVE_MINUTES,
                     firstDay,
                     QTime(4, 0),
                     QTime(4, 0),
                     {bar(firstDay, 0, 300, 20.0f)});
        compareRange(first, TimeFrame::ONE_MINUTE, secondDay, QTime(4, 0), QTime(4, 0), {bar(secondDay, 0, 60, 30.0f)});
        compareRange(second, TimeFrame::ONE_MINUTE, firstDay, QTime(4, 0), QTime(4, 0), {bar(firstDay, 0, 60, 40.0f)});
        for (const auto& symbol: {QString("CACHE_FIRST"), QString("CACHE_SECOND")})
        {
            auto rows = DatabaseThread::getInstance()->getCachedBarsForDate(symbol, TimeFrame::ONE_MINUTE, firstDay);
            QTRY_VERIFY_WITH_TIMEOUT(rows.isFinished(), 5000);
            QCOMPARE(rows.result()->size(), qsizetype(1));
            QVERIFY(rows.result()->first() == bar(firstDay, 0, 60, symbol == "CACHE_FIRST" ? 10.0f : 40.0f));
        }
        auto higher =
            DatabaseThread::getInstance()->getCachedBarsForDate("CACHE_FIRST", TimeFrame::FIVE_MINUTES, firstDay);
        QTRY_VERIFY_WITH_TIMEOUT(higher.isFinished(), 5000);
        QCOMPARE(higher.result()->size(), qsizetype(1));
        QVERIFY(higher.result()->first() == bar(firstDay, 0, 300, 20.0f));
        auto otherDay =
            DatabaseThread::getInstance()->getCachedBarsForDate("CACHE_FIRST", TimeFrame::ONE_MINUTE, secondDay);
        QTRY_VERIFY_WITH_TIMEOUT(otherDay.isFinished(), 5000);
        QCOMPARE(otherDay.result()->size(), qsizetype(1));
        QVERIFY(otherDay.result()->first() == bar(secondDay, 0, 60, 30.0f));
    }

    void duplicatesReplaceWithoutAddingRowsAndOpenBarsStayTransient()
    {
        const QDate day(2024, 1, 8);
        BarCache cache("CACHE_DUPLICATE", &m_owner);
        const Bar initial = bar(day, 0, 60, 10.0f);
        const Bar revised = bar(day, 0, 60, 20.0f);
        cache.storeBar(TimeFrame::ONE_MINUTE, initial);
        cache.storeBar(TimeFrame::ONE_MINUTE, initial);
        cache.storeBar(TimeFrame::ONE_MINUTE, revised);
        Bar forming = bar(day, 1, 60, 30.0f);
        forming.setBarStatus(Bar::BarStatus::Open);
        cache.storeBar(TimeFrame::ONE_MINUTE, forming);
        cache.storeBar(TimeFrame::ONE_MINUTE, Bar::nullBar(bar(day, 2, 60, 0.0f).getTimeStamp()));
        compareRange(cache, TimeFrame::ONE_MINUTE, day, QTime(4, 0), QTime(4, 1), {revised, forming});
        QCOMPARE(cache.getLatestClosedBarTimestamp(TimeFrame::ONE_MINUTE, day),
                 std::optional<QDateTime>(initial.getTimeStamp()));
        auto rows = DatabaseThread::getInstance()->getCachedBarsForDate("CACHE_DUPLICATE", TimeFrame::ONE_MINUTE, day);
        QTRY_VERIFY_WITH_TIMEOUT(rows.isFinished(), 5000);
        QCOMPARE(rows.result()->size(), qsizetype(1));
        QVERIFY(rows.result()->first() == revised);
        auto latest =
            DatabaseThread::getInstance()->getLatestClosedBarTimestamp("CACHE_DUPLICATE", TimeFrame::ONE_MINUTE, day);
        QTRY_VERIFY_WITH_TIMEOUT(latest.isFinished(), 5000);
        QCOMPARE(latest.result(), std::optional<QDateTime>(initial.getTimeStamp()));
        auto missing = DatabaseThread::getInstance()->getBarsFromDatabase("CACHE_DUPLICATE",
                                                                          TimeFrame::ONE_MINUTE,
                                                                          day,
                                                                          QTime(4, 0),
                                                                          QTime(4, 1));
        QTRY_VERIFY_WITH_TIMEOUT(missing.isFinished(), 5000);
        QVERIFY(!missing.result().has_value());
    }

    void cleanupTestCase()
    {
        if (m_databaseCreated)
        {
            // Drain queued cache closes before the database singleton's main-thread teardown.
            QVERIFY(QMetaObject::invokeMethod(DatabaseThread::getInstance(), []() {}, Qt::BlockingQueuedConnection));
            DatabaseThread::destroyInstance();
        }
        if (m_clientCreated)
            TSClient::destroyInstance();
        cacheRootDir = m_oldCacheRoot;
        appStateSettings = nullptr;
    }

  private:
    static Bar bar(const QDate& p_day, int p_index, int p_seconds, float p_price)
    {
        return Bar(QDateTime(p_day, QTime(4, 0).addSecs(p_index * p_seconds), TradingHours::MARKET_TIMEZONE),
                   p_price,
                   p_price + 2.0f,
                   p_price - 1.0f,
                   p_price + 1.0f,
                   static_cast<quint64>(100 + p_index));
    }

    void compareRange(BarCache& p_cache,
                      TimeFrame p_tf,
                      const QDate& p_day,
                      const QTime& p_first,
                      const QTime& p_last,
                      const QVector<Bar>& p_expected)
    {
        auto result = p_cache.getBars(p_tf, p_day, p_first, p_last);
        std::shared_ptr<QVector<Bar>> bars;
        if (std::holds_alternative<std::shared_ptr<QVector<Bar>>>(result))
            bars = std::get<std::shared_ptr<QVector<Bar>>>(result);
        else
        {
            auto future = std::get<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(result);
            QTRY_VERIFY_WITH_TIMEOUT(future.isFinished(), 5000);
            QVERIFY(future.result().has_value());
            bars = future.result().value();
        }
        QVERIFY(bars);
        QCOMPARE(bars->size(), p_expected.size());
        for (qsizetype i = 0; i < p_expected.size(); ++i)
            QVERIFY2(bars->at(i) == p_expected.at(i), qPrintable(QString("Bar mismatch at index %1").arg(i)));
    }

    QTemporaryDir m_directory;
    QObject m_owner;
    std::unique_ptr<QSettings> m_settings;
    QString m_oldCacheRoot;
    bool m_clientCreated = false;
    bool m_databaseCreated = false;
};

QTEST_GUILESS_MAIN(BarCachePersistenceTests)
#include "BarCachePersistenceTests.moc"
