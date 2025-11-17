#pragma once

#include <QString>
#include <QtSql/QSqlDatabase>
#include <QMap>
#include <QStringList>

#include "StreamMarketDepthQuote.h"

class LiveMarketDepthQuoteDB : public QObject {
    Q_OBJECT
    
public:
    LiveMarketDepthQuoteDB(const QString& dbPath, QStringList& stockTickers);
    ~LiveMarketDepthQuoteDB();

    bool isOpen() const;

    void startRecording();
    
private slots:
    void onReceivedNewRawDataForStock(QString symbol, const QByteArray& rawData);

private:
    bool storeMarketDepthQuoteRawData(const QString& stock, qint64 epochMs, const QByteArray& rawData);
    
    QStringList stockTickers;
    QSqlDatabase db;
    QMap<QString, int> stockSequences;
    QMap<QString, StreamMarketDepthQuote*> streamMarketDepthQuotes;
};