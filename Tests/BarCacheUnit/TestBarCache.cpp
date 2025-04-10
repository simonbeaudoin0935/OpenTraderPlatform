#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <QSettings>
#include <QDateTime>

#include "TestBarCache.h"
#include "TSClient.h"
#include "BarCache.h"

static TSClient* client;

// will be called to create a global test data table.
void TestBarCache::initTestCase_data()
{


}

// will be called before the first test function is executed.
void TestBarCache::initTestCase() {
    qInfo() << "Start of test suite";

    QThread::currentThread()->setObjectName("UnitTestThread");

    client = TSClient::getInstancePtr(); // ***** First time to do a getInstance, this will call the constructor
}

// Will be called before each test function is executed.
void TestBarCache::init()
{

}

// Will be called after every test function.
void TestBarCache::cleanup() {

}

void TestBarCache::test()
{
    BarCache TestBarCache("AAPL", false);
}
