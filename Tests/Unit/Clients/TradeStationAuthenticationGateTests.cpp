#include <QNetworkReply>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

#include "AuthenticatedNetworkAccessManager.h"

class TradeStationAuthenticationGateTests : public QObject
{
    Q_OBJECT

  private slots:
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
