#pragma once

#include <QNetworkReply>
#include <QQueue>
#include <QFile>
#include <QTextStream>

class MockStreamNetworkReply : public QNetworkReply {
    Q_OBJECT
public:
    MockStreamNetworkReply(const QString &mockDataFile, QObject* parent = nullptr);

    void setData(const QByteArray& data);

    qint64 bytesAvailable() const override;

    void abort() override;

protected:
    qint64 readData(char* data, qint64 maxSize) override;

private:
    QQueue<QByteArray*> buffer;
    QFile file;
    QTextStream textSteam;
};
