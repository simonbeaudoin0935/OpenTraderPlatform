#pragma once

#include <QString>
#include <QtSql/QSqlDatabase>
#include <QMap>

class LiveBarsDB {
public:
    LiveBarsDB(const QString& dbPath);
    ~LiveBarsDB();

    bool isOpen() const;
    bool storeBar(const QString& stock, qint64 timestamp, double open, double high, double low, double close, qint64 volume);

private:
    QSqlDatabase db;
    QMap<QString, int> stockSequences;
};