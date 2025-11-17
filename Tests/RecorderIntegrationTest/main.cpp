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

QTEST_MAIN(TestRecorderIntegration)

#include "main.moc"