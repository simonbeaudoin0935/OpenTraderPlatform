#ifndef APPFRONTEND_H
#define APPFRONTEND_H

#include <QObject>
#include <QJsonObject>

class AppFrontend : public QObject {
    Q_OBJECT
public:
    explicit AppFrontend(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~AppFrontend() = default;

public slots:
    virtual void onPriceUpdated(const QJsonObject& priceData) = 0;
    virtual void onPricesFetched() = 0;
};

#endif
