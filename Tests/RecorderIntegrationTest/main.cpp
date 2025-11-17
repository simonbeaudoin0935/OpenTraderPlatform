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

    // Wait for it to start
    QVERIFY(process.waitForStarted(5000));

    // Run for 20 seconds
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(20000);
    QEventLoop loop;
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    loop.exec();

    // Kill the process
    process.kill();
    process.waitForFinished(5000);

    // Find the latest log file
    QString logDir = QDir::homePath() + "/.local/state/Recorder/logs";
    QDir dir(logDir);
    QStringList filters;
    filters << "Recorder_*.log.ansi";
    QStringList logFiles = dir.entryList(filters, QDir::Files, QDir::Time);

    QVERIFY(!logFiles.isEmpty());

    QString latestLog = logDir + "/" + logFiles.last();
    qInfo() << "Opening log file:" << latestLog;
    QFile logFile(latestLog);
    QVERIFY(logFile.open(QIODevice::ReadOnly | QIODevice::Text));

    QTextStream in(&logFile);
    QString logContent = in.readAll();

    // Check for CRIT messages
    if (logContent.contains("CRIT")) {
        // Extract and log the CRIT lines
        QStringList lines = logContent.split('\n');
        for (const QString& line : lines) {
            if (line.contains("CRIT")) {
                qCritical() << "Found CRIT message:" << line;
            }
        }
        QFAIL("Critical messages found in recorder log");
    }

    // Check for WARN messages
    if (logContent.contains("WARN")) {
        // Extract and log the WARN lines
        QStringList lines = logContent.split('\n');
        for (const QString& line : lines) {
            if (line.contains("WARN")) {
                qWarning() << "Found WARN message:" << line;
            }
        }
    }
    if (logContent.contains("CRIT")) {
        // Extract and log the CRIT lines
        QStringList lines = logContent.split('\n');
        for (const QString& line : lines) {
            if (line.contains("CRIT")) {
                qCritical() << "Found CRIT message:" << line;
            }
        }
        QFAIL("Critical messages found in recorder log");
    }
}

QTEST_MAIN(TestRecorderIntegration)

#include "main.moc"