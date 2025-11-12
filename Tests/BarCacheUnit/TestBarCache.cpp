#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <QSettings>
#include <QDateTime>
#include <QTimeZone>

#include "TestBarCache.h"
#include "TSClient.h"
#include "BarCache.h"
#include "../../src/Misc/Logging.h"

static TSClient* client;
static QVector<QString> companies;

// will be called to create a global test data table.
void TestBarCache::initTestCase_data()
{
    companies.push_back("AVTE");
    companies.push_back("ELTX");
    companies.push_back("GNLX");
    companies.push_back("MAZE");
    companies.push_back("FDMT");
    companies.push_back("SEER");
    companies.push_back("QNRX");
    companies.push_back("ELAB");
    companies.push_back("REVB");
    companies.push_back("GLTO");
    companies.push_back("ADTX");
    companies.push_back("SXTP");
    companies.push_back("CELZ");
    companies.push_back("TCRT");
    companies.push_back("HCWB");
    companies.push_back("CYCN");
    companies.push_back("BCDA");
    companies.push_back("APLM");
    companies.push_back("QLGN");
}

// will be called before the first test function is executed.
void TestBarCache::initTestCase() {

    qInfo() << "Start of test suite";

    QThread::currentThread()->setObjectName("UnitTestThread");


    // manually expire the token to force a refresh on startup
    {
        AuthToken savedAuthToken = AuthToken::loadFromSettings();

        QString tokenStr = savedAuthToken.toString();
        QString obfuscated = tokenStr.size() > 10 ? tokenStr.left(10) + "***" : tokenStr + "***";
        qDebug() << "Token details:" << obfuscated;

        // This test suite neet a valid token to be present in the settings.
        QVERIFY(savedAuthToken.isValid());

        // The first thing we are going to test is that the client is able to
        // start with an expired token and refresh it.
        // If the present token is not expired, we will artificially set it to expired
        if(!savedAuthToken.isExpired()) {
            qInfo() << "Token is not expired, setting it to expired";

            AuthToken expiredToken = AuthToken(savedAuthToken.getAccessToken(),
                                            savedAuthToken.getRefreshToken(),
                                            savedAuthToken.getIdToken(),
                                            savedAuthToken.getTokenType(),
                                            savedAuthToken.getScope(),
                                            savedAuthToken.getExpiresIn(),
                                            QDateTime::currentDateTime().addSecs(-3600));

            QVERIFY(AuthToken::storeToSettings(expiredToken));
        } else {
            qInfo() << "Token is expired, no need to set it to expired";
        }
    }


    client = TSClient::getInstancePtr(); // ***** First time to do a getInstance, this will call the constructor

    bool triggered;

    QSignalSpy authStateSpy(client, &TSClient::authStateChanged); // Create signal spies to monitor authentication signals

    QVERIFY(client->isCleanedUp());
    QVERIFY(!client->isAuthenticated());
    QVERIFY(!client->isAuthInProgress());

    // ****** This will kickstart the refresh logic
    client->start();

    // Wait 1ms, just to make sure the client thread has started and
    QTest::qWait(1);

    // Verify the refresh has started
    QVERIFY(!client->isAuthenticated());
    QVERIFY(client->isAuthInProgress()); // The client will immediately initiate a refresh


    // Wait for authStateChanged signal to be fired
    // 2 second is generous for one refresh request round trip
    // When debugging, 2 seconds wasnt enough. Bumbed it to 4.
    // This is because the start of the suite is slow as the debugger sets up
    // the maaaany runtime thangs the app links to
    triggered = authStateSpy.wait(4000);

    QVERIFY(triggered);

    // Logically, only one signal must be emited
    QCOMPARE(authStateSpy.count(), 1);

    // Authentication completed
    bool success   = authStateSpy.first().at(0).toBool();
    QString reason = authStateSpy.first().at(1).toString();

    qInfo() << "Refresh reply : " << reason;
    authStateSpy.removeFirst();

    QVERIFY(success);

    QVERIFY(client->isAuthenticated());
    QVERIFY(!client->isAuthInProgress());

    // Wait just a little bit, for some reason this test thread outruns the housekeeping done in the TSClient that cleans the serviced
    // replies. Otherwise, the following isCleanedUp() triggers because the refreshTokenReply is not flushed from the map<>
    QTest::qWait(100);

    // Verify no resources were leaked
    QVERIFY(client->isCleanedUp());
}

// Will be called before each test function is executed.
void TestBarCache::init()
{

}

// Will be called after every test function.
void TestBarCache::cleanup() {

}

void TestBarCache::testGetBars()
{
    BarCache cache("AAPL");
    cache.clearDatabase();

    QDateTime initDate;
    QDateTime fromDate;
    QDateTime toDate;

    {
        //QDateTime now = QDateTime::currentDateTime();

        // Get the date for the previous day
        QDate previousDay(2025, 4, 11);

        // Create a QDateTime for previous day at 4:00 PM in New York time zone
        QTimeZone newYorkTimeZone = QTimeZone::fromName("America/New_York");

        // Define 4:00 PM (16:00)
        const QTime noon(12, 0, 0);


        initDate = QDateTime(previousDay, noon, newYorkTimeZone);
        fromDate = initDate;
        toDate = fromDate;

        qInfo() << "Init Date : " << initDate;
    }


    QVector<Bar> results;

    qsizetype nbars;

    qInfo() << "";
    qInfo() << "Warm the cache with 4 bars";
    {
        nbars = 4;
        toDate = fromDate.addSecs(60 * (nbars-1));
        results = cache.getBars(fromDate, toDate);

        QCOMPARE(cache.getLastHitType(), BarCache::HitType::Miss);
        QCOMPARE(cache.getLastNumberFetchedBars(), nbars);
        QCOMPARE_GE(results.size(), nbars);
    }

    qInfo() << "";
    qInfo() << "Test asking for the same 4 bars hits the cache";
    {
        results = cache.getBars(fromDate, toDate);

        QCOMPARE(cache.getLastHitType(), BarCache::HitType::Hit);
        QCOMPARE(cache.getLastNumberFetchedBars(), 0);
        QCOMPARE_GE(results.size(), nbars);
    }

    qInfo() << "";
    qInfo() << "Testing asking for 10 bars past triggers a miss";
    {
        nbars = 10;
        fromDate = toDate.addSecs(60);
        toDate = fromDate.addSecs(60 * (nbars-1));

        results = cache.getBars(fromDate, toDate);

        QCOMPARE(cache.getLastHitType(), BarCache::HitType::Miss);
        QCOMPARE(cache.getLastNumberFetchedBars(), nbars);
        QCOMPARE_GE(results.size(), nbars);
    }

    qInfo() << "";
    qInfo() << "Testing asking for the 14 is a hit";
    {
        results = cache.getBars(initDate, toDate);

        QCOMPARE(cache.getLastHitType(), BarCache::HitType::Hit);
        QCOMPARE(cache.getLastNumberFetchedBars(), 0);
        QCOMPARE_GE(results.size(), 14);
    }

    qInfo() << "";
    qInfo() << "Testing asking for the 1 bar past is a miss and only that one is fetched";
    {
        nbars = 1;
        results = cache.getBars(initDate, toDate.addSecs(60 * nbars));

        QCOMPARE(cache.getLastHitType(), BarCache::HitType::PartialHit);
        QCOMPARE_GE(results.size(), 15);
        QCOMPARE(cache.getLastNumberFetchedBars(), nbars);
        QCOMPARE(cache.getDuplicateStoreCount(), 0);
    }

    qInfo() << "";
    qInfo() << "Testing asking for the 1 bar before is a miss and only that one is fetched";
    {
        nbars = 1;
        results = cache.getBars(initDate.addSecs(-60 * nbars), toDate.addSecs(60 * nbars));

        QCOMPARE(cache.getLastHitType(), BarCache::HitType::PartialHit);
        QCOMPARE_GE(results.size(), 16);
        QCOMPARE(cache.getLastNumberFetchedBars(), nbars);
        QCOMPARE(cache.getDuplicateStoreCount(), 0);
    }
}

// Testing asking a range where we know there are only void bars
void TestBarCache::testGetBarsOnlyHoles()
{
    BarCache cache("TIVC");
    cache.clearDatabase();

    QTimeZone newYorkTimeZone = QTimeZone::fromName("America/New_York");

    //  Test against wednesday April 2 shortly after 17h where there is a bar holes from 17h-17h39
    QDate date(2025, 04, 02);
    const QTime _17PM10(17, 10, 0);
    const QTime _17PM20(17, 19, 0);

    QDateTime fromDate = QDateTime(date, _17PM10, newYorkTimeZone);
    QDateTime toDate   = QDateTime(date, _17PM20, newYorkTimeZone);


    qInfo() << "";
    qInfo() << "Testing asking for 10 bars not existing in the API database correctly return 10 void bars";
    {
        QVector<Bar> result = cache.getBars(fromDate, toDate);

        // We know that these 10 bars dont exist in the API database.
        QCOMPARE(cache.getLastNumberFetchedBars(), 0);
        QCOMPARE(cache.getLastHitType(), BarCache::HitType::Miss);
        QCOMPARE(cache.getNumberOfBars(), (unsigned) 10);

        for (auto &bar : result) {
            QVERIFY(bar.getBarStatus() == Bar::BarStatus::Void);
        }
    }

    qInfo() << "";
    qInfo() << "Testing asking for the same 10 void bars return a HIT";
    {
        QVector<Bar> result = cache.getBars(fromDate, toDate);

        // We know that these 10 bars dont exist in the API database.
        QCOMPARE(cache.getLastNumberFetchedBars(), 0);
        QCOMPARE(cache.getLastHitType(), BarCache::HitType::Hit);
        QCOMPARE(cache.getNumberOfBars(), (unsigned) 10);

        for (auto &bar : result) {
            QVERIFY(bar.getBarStatus() == Bar::BarStatus::Void);
        }
    }

    qInfo() << "";
    qInfo() << "Testing asking for 10 bars past the 10 we just asked is a partial HIT";
    {
        toDate = toDate.addSecs(10 * 60);

        QVector<Bar> result = cache.getBars(fromDate, toDate);

        // We know that these 10 bars dont exist in the API database.
        QCOMPARE(cache.getLastNumberFetchedBars(), 0);
        QCOMPARE(cache.getLastHitType(), BarCache::HitType::PartialHit);
        QCOMPARE(cache.getNumberOfBars(), (unsigned) 20);

        for (auto &bar : result) {
            QVERIFY(bar.getBarStatus() == Bar::BarStatus::Void);
        }
    }

    qInfo() << "";
    qInfo() << "Testing asking for 5 bars before the 20 we just asked is a partial HIT";
    {
        fromDate = fromDate.addSecs(-5 * 60);

        QVector<Bar> result = cache.getBars(fromDate, toDate);

        // We know that these 10 bars dont exist in the API database.
        QCOMPARE(cache.getLastNumberFetchedBars(), 0);
        QCOMPARE(cache.getLastHitType(), BarCache::HitType::PartialHit);
        QCOMPARE(cache.getNumberOfBars(), (unsigned) 25);

        for (auto &bar : result) {
            QVERIFY(bar.getBarStatus() == Bar::BarStatus::Void);
        }
    }
}

void TestBarCache::testGetBarsWithHoles()
{
    QSKIP("yoo");

    BarCache cache("TIVC");

    //  Test against wednesday April 2
    QDate date(2025, 04, 02);

    QTimeZone newYorkTimeZone = QTimeZone::fromName("America/New_York");

    const QTime _4PM(16, 0, 0);
    const QTime _8PM(19, 59, 0);

    QDateTime fromDate = QDateTime(date, _4PM, newYorkTimeZone);
    QDateTime toDate   = QDateTime(date, _8PM, newYorkTimeZone);


    QVector<Bar> result = cache.getBars(fromDate, toDate);

    // We know there are only 26 bars in the after hour session of that date
    QCOMPARE(cache.getLastNumberFetchedBars(), 26);
    QCOMPARE(cache.getLastHitType(), BarCache::HitType::Miss);

    // We need to have received 4 hours of 60 bars each
    QCOMPARE(result.size(), 4 * 60);
}

void TestBarCache::testBarStreaming()
{
    QSKIP("yo");

    QVector<BarCache*> caches;
    for (auto &company: companies){
        BarCache* cache = new BarCache(company, true);
        cache->clearDatabase();
        caches.push_back(cache);
    }

    //    BarCache cache("AAPL", true);

    QTest::qWait(120000);

    for (auto &cache: caches) {
        qInfo() <<  "Number of bars for : " << cache->getSymbol() << " : " << cache->getNumberOfBars();
    }


    for (auto &cache: caches) {
        delete cache;
    }
}
