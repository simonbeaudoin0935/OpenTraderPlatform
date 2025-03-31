#ifndef GUIFRONTEND_H
#define GUIFRONTEND_H

#include "../Core/AppFrontend.h"
#include <QMainWindow>
#include <QTimer>
#include <QPushButton>

#include "../Clients/FMPClient/FMPClient.h"
#include "../Clients/TradeStationClient/TradeStationClient.h"

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
    void onTradeStationClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onTradeStationAccountsReceived(QVector<AccountsResult> results) override;
    void onMemoryUsageUpdate(qint64 newDataUsage) override; // TODO deal with qint64 vs qsizetype


private slots:
    void onUpdateTimerTimeout();
    void onQuoteShortReceived(const FMPClient::QuoteShortResult quoteResult);
    void onTradeStationLoginClicked();
    void onTradeStationAuthStateChanged(bool isAuthenticated, QString reason);

private:
    static QString bytesToString(qint64 bytes);
    Ui::GuiFrontend* ui;  // Pointer to the UI object
    QPushButton* tradeStationLoginButton;  // Login button in status bar

    QTimer updateTimer;

    qsizetype FMPClientDataUsage = 0;
    qsizetype TradeStationClientDataUsage = 0;
    qint64 memoryUsage = 0;
};

#endif // GUIFRONTEND_H
