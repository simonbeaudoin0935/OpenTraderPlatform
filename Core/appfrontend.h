#ifndef APPFRONTEND_H
#define APPFRONTEND_H

#include <QObject>
#include <QJsonObject>

class AppFrontend : public QObject {
    Q_OBJECT
public:
    explicit AppFrontend(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~AppFrontend() = default;

signals:
    void tradeStationAuthStateChanged(bool isAuthenticated, QString reason);

    void fmpDataUsageUpdated(qsizetype newDataUsage);
    void tradeStationDataUsageUpdated(qsizetype newDataUsage);


public slots:

    virtual void onFMPClientDataUsageUpdate(qsizetype newDataUsage) = 0;
    virtual void onTradeStationClientDataUsageUpdate(qsizetype newDataUsage) = 0;
    virtual void onMemoryUsageUpdate(qint64 newDataUsage) = 0;
};

#endif
