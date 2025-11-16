#pragma once

#include <QString>
#include <QtSql/QSqlDatabase>
#include <QMap>

class LiveBarsDB {
public:
    LiveBarsDB(const QString& dbPath);
    ~LiveBarsDB();

    bool isOpen() const;
    bool storeBarJson(const QString& stock, qint64 timestamp, const QString& jsonData);

private:
    QSqlDatabase db;
    QMap<QString, int> stockSequences;
};