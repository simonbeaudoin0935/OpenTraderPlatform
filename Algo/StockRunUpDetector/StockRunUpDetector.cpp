#include "StockRunUpDetector.h"

StockRunUpDetector::StockRunUpDetector(QObject *parent)
    : QObject{parent}
{}

void StockRunUpDetector::start(QVector<QString> &watchlist)
{
    this->watchlist = watchlist;
}
