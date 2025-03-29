#ifndef GUIFRONTEND_H
#define GUIFRONTEND_H

#include "../Core/appfrontend.h"
#include <QMainWindow>
#include <QTimer>
#include <QPushButton>

#include "../Clients/FMPClient/fmpclient.h"
#include "../Clients/TradeStationClient/tradestationclient.h"

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
    void onQuoteShortReceived(const FMPClient::QuoteShortResult quoteResult);
    void onTradeStationLoginClicked();
    void onTradeStationAuthStateChanged(bool isAuthenticated);
    void onTradeStationAuthError(const QString& error);

private:
    static QString bytesToString(qint64 bytes);
    Ui::GuiFrontend* ui;  // Pointer to the UI object
    QPushButton* tradeStationLoginButton;  // Login button in status bar

    QTimer updateTimer;

    qsizetype FMPDataUsage = 0;
    qint64 memoryUsage = 0;
};

#endif // GUIFRONTEND_H
