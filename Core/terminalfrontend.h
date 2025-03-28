#ifndef TERMINALFRONTEND_H
#define TERMINALFRONTEND_H

#include "appfrontend.h"

class TerminalFrontend : public AppFrontend {
    Q_OBJECT
public:
    explicit TerminalFrontend(QObject* parent = nullptr);

public slots:
    void onFMPClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onMemoryUsageUpdate(qint64 newDataUsage) override;
};

#endif
