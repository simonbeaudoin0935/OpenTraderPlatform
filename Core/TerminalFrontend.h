#ifndef TERMINALFRONTEND_H
#define TERMINALFRONTEND_H

#include "AppFrontend.h"

class TerminalFrontend : public AppFrontend {
    Q_OBJECT
public:
    explicit TerminalFrontend(QObject* parent = nullptr);

public slots:

    // Usage uptade
    void onFMPClientDataUsageUpdate(qsizetype newDataUsage);
    void onTSClientDataUsageUpdate(qsizetype newDataUsage);
    void onMemoryUsageUpdate(qint64 newDataUsage);

    void onTradeStationAccountsReceived(QVector<AccountsResult> results);
    void onMarketDepthNotAvailable();

};

#endif
