#include <QNetworkReply>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

#include "AuthenticatedNetworkAccessManager.h"

namespace
{
    // Accepts connections and reads requests but never answers, like a wedged HTTP/2 connection
    void startSilentServer(QTcpServer& p_server)
    {
        QVERIFY(p_server.listen(QHostAddress::LocalHost, 0));
        QObject::connect(
            &p_server,
            &QTcpServer::newConnection,
            &p_server,
            [&p_server]()
            {
                QTcpSocket* socket = p_server.nextPendingConnection();
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket]() { socket->readAll(); });
            });
    }

    QNetworkRequest requestFor(const QTcpServer& p_server, bool p_streaming)
    {
        QNetworkRequest request(QUrl(QString("http://127.0.0.1:%1/test").arg(p_server.serverPort())));
        request.setAttribute(AuthenticatedNetworkAccessManager::StreamingRequestAttribute, p_streaming);
        return request;
    }
} // namespace

class TradeStationAuthenticationGateTests : public QObject
{
    Q_OBJECT

  private slots:
    void silentStreamReportsStallWithoutAborting()
    {
        QTcpServer server;
        startSilentServer(server);
        AuthenticatedNetworkAccessManager manager([](const QNetworkRequest&) { return true; });
        manager.setWatchdogTimeouts(100, 5000);
        QSignalSpy stalled(&manager, &AuthenticatedNetworkAccessManager::connectionStalled);

        QNetworkReply* reply = manager.get(requestFor(server, true));
        QVERIFY(stalled.wait(2000));
        QCOMPARE(stalled.count(), 1);
        QVERIFY(stalled.first().first().toString().startsWith("stream /test"));
        QVERIFY(reply->isRunning());
        delete reply;
    }

    void silentRestRequestReportsStallAndAborts()
    {
        QTcpServer server;
        startSilentServer(server);
        AuthenticatedNetworkAccessManager manager([](const QNetworkRequest&) { return true; });
        manager.setWatchdogTimeouts(5000, 100);
        QSignalSpy stalled(&manager, &AuthenticatedNetworkAccessManager::connectionStalled);

        QNetworkReply* reply = manager.get(requestFor(server, false));
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QVERIFY(finished.wait(2000));
        QCOMPARE(reply->error(), QNetworkReply::OperationCanceledError);
        QCOMPARE(stalled.count(), 1);
        delete reply;
    }

    void respondingRequestDoesNotReportStall()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        connect(&server,
                &QTcpServer::newConnection,
                &server,
                [&server]()
                {
                    QTcpSocket* socket = server.nextPendingConnection();
                    connect(socket,
                            &QTcpSocket::readyRead,
                            socket,
                            [socket]()
                            {
                                socket->readAll();
                                socket->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
                                socket->disconnectFromHost();
                            });
                });
        AuthenticatedNetworkAccessManager manager([](const QNetworkRequest&) { return true; });
        manager.setWatchdogTimeouts(200, 200);
        QSignalSpy stalled(&manager, &AuthenticatedNetworkAccessManager::connectionStalled);

        QNetworkReply* reply = manager.get(requestFor(server, true));
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QVERIFY(finished.wait(2000));
        QCOMPARE(reply->error(), QNetworkReply::NoError);
        QTest::qWait(400);
        QCOMPARE(stalled.count(), 0);
        delete reply;
    }

    void retiredManagerDeletesItselfAfterLastReply()
    {
        QTcpServer server;
        startSilentServer(server);
        auto* manager = new AuthenticatedNetworkAccessManager([](const QNetworkRequest&) { return true; });
        QSignalSpy destroyed(manager, &QObject::destroyed);

        QNetworkReply* reply = manager->get(requestFor(server, true));
        reply->setParent(nullptr);
        manager->retire();
        QVERIFY(manager->isRetired());
        QTest::qWait(50);
        QCOMPARE(destroyed.count(), 0);

        delete reply;
        QVERIFY(destroyed.wait(1000));
    }

    void blockedRequestsNeverReachNetwork()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        QSignalSpy connections(&server, &QTcpServer::newConnection);
        AuthenticatedNetworkAccessManager manager([](const QNetworkRequest&) { return false; });
        const QNetworkRequest request(QUrl(QString("http://127.0.0.1:%1/test").arg(server.serverPort())));
        const QList<QNetworkReply*> replies{manager.get(request),
                                            manager.post(request, QByteArray("test")),
                                            manager.deleteResource(request)};
        for (QNetworkReply* reply: replies)
        {
            QSignalSpy finished(reply, &QNetworkReply::finished);
            if (!reply->isFinished())
            {
                QVERIFY(finished.wait());
            }
            QCOMPARE(reply->error(), QNetworkReply::AuthenticationRequiredError);
            QVERIFY(reply->readAll().isEmpty());
            reply->deleteLater();
        }
        QTest::qWait(50);
        QCOMPARE(connections.count(), 0);
    }

    void successfulAuthenticationResumesRequests()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        int requestCount = 0;
        connect(&server,
                &QTcpServer::newConnection,
                &server,
                [&]()
                {
                    QTcpSocket* socket = server.nextPendingConnection();
                    connect(socket,
                            &QTcpSocket::readyRead,
                            socket,
                            [&, socket]()
                            {
                                socket->readAll();
                                ++requestCount;
                                socket->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
                                socket->disconnectFromHost();
                            });
                    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                });
        bool authenticated = false;
        AuthenticatedNetworkAccessManager manager([&](const QNetworkRequest&) { return authenticated; });
        const QNetworkRequest request(QUrl(QString("http://127.0.0.1:%1/test").arg(server.serverPort())));
        QNetworkReply* blocked = manager.get(request);
        QSignalSpy blockedFinished(blocked, &QNetworkReply::finished);
        QVERIFY(blockedFinished.wait());
        QCOMPARE(requestCount, 0);
        blocked->deleteLater();

        authenticated = true;
        QNetworkReply* allowed = manager.get(request);
        QSignalSpy allowedFinished(allowed, &QNetworkReply::finished);
        QVERIFY(allowedFinished.wait());
        QCOMPARE(allowed->error(), QNetworkReply::NoError);
        QCOMPARE(allowed->readAll(), QByteArray("ok"));
        QCOMPARE(requestCount, 1);
        allowed->deleteLater();

        authenticated = false;
        blocked = manager.get(request);
        QSignalSpy blockedAgain(blocked, &QNetworkReply::finished);
        QVERIFY(blockedAgain.wait());
        QCOMPARE(blocked->error(), QNetworkReply::AuthenticationRequiredError);
        QCOMPARE(requestCount, 1);
        blocked->deleteLater();
    }
};

QTEST_GUILESS_MAIN(TradeStationAuthenticationGateTests)
#include "TradeStationAuthenticationGateTests.moc"
