#pragma once

#include <QObject>

#include "Bar.h"

class StockRunUpDetector : public QObject
{
    Q_OBJECT
public:
    explicit StockRunUpDetector(QObject *parent = nullptr);

public slots:
    void start(QVector<QString> &watchlist);
signals:

private:
    QVector<QString> watchlist;

    QMap<QString, QVector<Bar>> aftermarketBars;

};
