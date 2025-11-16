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
    bool storeBarJson(const QString& stock, qint64 timestamp, const QString& jsonData);

    void startRecording();
    
private slots:
    void onReceivedNewJson(QString symbol, const QJsonObject& jsonObj);

private:
    QStringList stockTickers;
    QSqlDatabase db;
    QMap<QString, int> stockSequences;
    QMap<QString, StreamBars*> streamBars;
};