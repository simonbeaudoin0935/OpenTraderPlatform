#pragma once

#include <QString>
#include <QtSql/QSqlDatabase>
#include <QMap>
#include <QStringList>

#include "StreamBars.h"

class LiveBarsDB : public QObject {
    Q_OBJECT
    
public:
    LiveBarsDB(const QString& dbPath, QStringList& stockTickers);
    ~LiveBarsDB();

    bool isOpen() const;

    void startRecording();
    
    QMap<QString, QMap<Stream::StreamError, int>> getErrorCounters() const { return streamErrorCounters; }
    
private slots:
    void onReceivedNewRawDataForStock(QString symbol, const QByteArray& rawData);
    void onStreamErrorOccurred(Stream::StreamError error, QString errorMessage);

private:
    bool storeBarRawData(const QString& stock, qint64 epochMs, const QByteArray& rawData);
    
    QStringList stockTickers;
    QSqlDatabase db;
    QMap<QString, int> stockSequences;
    QMap<QString, StreamBars*> streamBars;
    QMap<QString, QMap<Stream::StreamError, int>> streamErrorCounters;
};