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
    
private slots:
    void onReceivedNewRawDataForStock(QString symbol, const QByteArray& rawData);

private:
    bool storeBarRawData(const QString& stock, qint64 epochMs, const QByteArray& rawData);
    
    QStringList stockTickers;
    QSqlDatabase db;
    QMap<QString, int> stockSequences;
    QMap<QString, StreamBars*> streamBars;
};