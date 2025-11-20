#include <QtTest>

#include <QProcess>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QTimer>
#include <QEventLoop>

class TestRecorderIntegration : public QObject
{
    Q_OBJECT

private slots:
    void testRecorderNoCriticalLogs();
    void testRecordedDataDirOption();
};

/**
 * @brief Tests that the recorder process does not produce any critical (CRIT) log messages.
 *
 * This test function starts the L2Trader recorder process with predefined arguments,
 * allows it to run for 20 seconds, then terminates it. It subsequently checks the
 * most recent log file in the user's local state directory for any "CRIT" level messages.
 * If any are found, the test fails and outputs the offending lines.
 *
 * The recorder binary path is determined as follows:
 * - If "/usr/bin/l2trader-recorder" exists, that path is used (installed binary branch).
 * - Otherwise, a relative path from the application directory is used (development build branch).
 *
 * @note This test assumes the presence of specific configuration files and directories
 *       as hardcoded in the arguments. It uses QProcess for execution and QTimer/QEventLoop
 *       for timing control.
 */
void TestRecorderIntegration::testRecorderNoCriticalLogs()
{
    // Path to the recorder binary
    QString recorderPath;
    if (QFile::exists("/usr/bin/l2trader-recorder")) {
        qInfo() << "Using installed recorder binary.";
        recorderPath = "/usr/bin/l2trader-recorder";
    } else {
        qInfo() << "Using development recorder binary.";
        recorderPath = QCoreApplication::applicationDirPath() + "/../../L2Trader_Recorder/src/L2Trader_Recorder";
    }

    // Args
    QStringList args;
    args << "--criterias=/home/simon/Documents/L2Trader/Example_Config/selection_criteria.ini"
         << "--cache-root-dir=/tmp/l2trader_test_cache"
         << "--stock-csv=/home/simon/Documents/L2Trader/Example_Config/nasdaq_screener_mini.csv";
        
    QProcess process;
    process.start(recorderPath, args);

    QVERIFY(process.waitForStarted(5000));

    bool hasCrit = false;
    QString buffer;

     connect(&process, &QProcess::readyReadStandardOutput, [&]() {
        buffer += process.readAllStandardOutput();
        int pos;
        while ((pos = buffer.indexOf('\n')) != -1) {
            QString line = buffer.left(pos);
            buffer = buffer.mid(pos + 1);
            if (line.contains("CRIT")) {
                hasCrit = true;
                qCritical() << "Found CRIT message:" << line;
            }
            if (line.contains("WARN")) {
                qWarning() << "Found WARN message:" << line;
            }
        }
    });

    // Run for 20 seconds
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(20000);
    QEventLoop loop;
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    loop.exec();

    // Process any remaining buffer
    if (!buffer.isEmpty()) {
        if (buffer.contains("CRIT")) {
            hasCrit = true;
            qCritical() << "Found CRIT message:" << buffer;
        }
        if (buffer.contains("WARN")) {
            qWarning() << "Found WARN message:" << buffer;
        }
    }

    // Kill the process
    process.kill();
    process.waitForFinished(5000);

    QVERIFY(!hasCrit);
}

/**
 * @brief Tests that the --recorded-data-dir option works correctly.
 *
 * This test verifies that when the --recorded-data-dir option is specified,
 * the Recorder app creates the recording folders in that directory instead
 * of the default cache location.
 */
void TestRecorderIntegration::testRecordedDataDirOption()
{
    // Create a temporary directory for testing
    QString testDir = "/tmp/l2trader_recorded_data_test";
    QDir dir(testDir);
    if (dir.exists()) {
        dir.removeRecursively();
    }
    dir.mkpath(".");

    // Path to the recorder binary
    QString recorderPath;
    if (QFile::exists("/usr/bin/l2trader-recorder")) {
        qInfo() << "Using installed recorder binary.";
        recorderPath = "/usr/bin/l2trader-recorder";
    } else {
        qInfo() << "Using development recorder binary.";
        recorderPath = QCoreApplication::applicationDirPath() + "/../../L2Trader_Recorder/src/L2Trader_Recorder";
    }

    // Args with custom recorded data directory
    QStringList args;
    args << "--criterias=/home/simon/Documents/L2Trader/Example_Config/selection_criteria.ini"
         << "--recorded-data-dir=" + testDir
         << "--stock-csv=/home/simon/Documents/L2Trader/Example_Config/nasdaq_screener_mini.csv";
        
    QProcess process;
    process.start(recorderPath, args);

    QVERIFY(process.waitForStarted(5000));

    // Run for 5 seconds (just enough to create the directories)
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(5000);
    QEventLoop loop;
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    loop.exec();

    // Kill the process
    process.kill();
    process.waitForFinished(5000);

    // Verify that the recording folders were created in the custom directory
    QString recordedDataPath = testDir + "/RecordedLiveData";
    QString barsPath = recordedDataPath + "/Bars";
    QString marketDepthPath = recordedDataPath + "/MarketDepthQuotes";

    QVERIFY2(QDir(recordedDataPath).exists(), "RecordedLiveData directory should exist in custom location");
    QVERIFY2(QDir(barsPath).exists(), "Bars directory should exist in custom location");
    QVERIFY2(QDir(marketDepthPath).exists(), "MarketDepthQuotes directory should exist in custom location");

    // Check that database files were created
    QDir barsDir(barsPath);
    QStringList dbFiles = barsDir.entryList(QStringList() << "*.db", QDir::Files);
    QVERIFY2(!dbFiles.isEmpty(), "At least one database file should be created in Bars directory");

    // Clean up
    dir.removeRecursively();
}

QTEST_MAIN(TestRecorderIntegration)

#include "main.moc"