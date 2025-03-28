#ifndef GUIFRONTEND_H
#define GUIFRONTEND_H

#include "../Core/appfrontend.h"
#include <QMainWindow>
#include <QTimer>

#include "../Clients/FMPClient/fmpclient.h"

// Forward declare the generated UI class
namespace Ui {
class GuiFrontend;
}

class GuiFrontend : public AppFrontend {
    Q_OBJECT
public:
    explicit GuiFrontend(QObject* parent = nullptr);
    ~GuiFrontend() override;

public slots:

    void onFMPClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onMemoryUsageUpdate(qint64 newDataUsage) override; // TODO deal with qint64 vs qsizetype

private slots:
    void onUpdateTimerTimeout();

    //TODO test
    void onQuoteShortReceived(const FMPClient::QuoteShortResult quoteResult);

private:
    Ui::GuiFrontend* ui;  // Pointer to the UI object

    QTimer updateTimer;

    qsizetype FMPDataUsage = 0;
    qint64 memoryUsage = 0;
};

#endif // GUIFRONTEND_H
