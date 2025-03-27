#include "terminalfrontend.h"
#include <QDebug>

TerminalFrontend::TerminalFrontend(QObject* parent) : AppFrontend(parent) {

}

void TerminalFrontend::onPriceUpdated(const QJsonObject& priceData) {
    //qDebug() << "Main Thread - Price Updated:" << priceData;
}

void TerminalFrontend::onPricesFetched() {
    //qCritical() << Q_FUNC_INFO << "TODO act on prices fetched";
}
