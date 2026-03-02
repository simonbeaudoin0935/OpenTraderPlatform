#pragma once

#include "FrontEnd.h"
#include "Bar.h"
#include "BarCache.h"
#include <QMap>
#include <QSocketNotifier>
#include <QVector>
#include <memory>
#include <ncurses.h>

// ncurses defines a 'timeout' macro that conflicts with Qt's QTimer::timeout
// Undefine it here to prevent conflicts
#undef timeout

// Forward declaration
class MainAlgo;

class TUIFrontend : public FrontEnd
{
    Q_OBJECT
  public:
    explicit TUIFrontend(MainAlgo* p_mainAlgo, QObject* parent = nullptr);
    ~TUIFrontend() override;

    // Initialize ncurses and display
    void initialize();

  public slots:

    void onTSClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onDBClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onMemoryUsageUpdate(qsizetype newDataUsage) override;

    void onTradeStationAccountsReceived(QVector<Account> results) override;
    void onNewPositionReceived(QString account, Position position) override;
    void onPositionDeleted(QString account, QString positionID) override;
    void onNewOrderReceived(QString account, Order order) override;
    void onBalanceUpdated(Balance balance) override;
    void onCurrentHighlightedStockBarReceived(QString symbol, Bar bar) override;
    void onCurrentHighlightedReceivedNewLevel2(QString symbol,
                                               Level2 level2,
                                               double bidAskImbalance,
                                               double bidDWP,
                                               double askDWP) override;

    // Replay mode notifications
    void onReplayModeEntered() override;
    void onReplayModeExited() override;
    void onReplayTimeUpdated(QDateTime currentTime) override;

  private slots:
    void handleInput();
    void onTradeStationAuthStateChanged(bool isAuthenticated, TSClient::AuthStateReason reason, const QString& message);

  private:
    void setupWindows();
    void refreshDisplay();
    void displayOrders();
    void displayPositions();
    void displayLastPrice();
    void displayStatusBar();
    void displayHelp();
    void cleanup();

    // Keyboard shortcut handlers
    void handleQuitShortcut();
    void handleRefreshShortcut();

    // Stock selection management
    void saveLastDisplayedStock(const QString& symbol);
    void restoreLastDisplayedStock();
    void displayStock(const QString& symbol);
    [[nodiscard]] bool isValidStockSymbol(const QString& symbol) const;

    // Bar fetching
    void requestMissingBarsForDay(const Bar& firstBar);

    MainAlgo* mainAlgo;

    // ncurses windows
    WINDOW* m_orderWin = nullptr;
    WINDOW* m_positionWin = nullptr;
    WINDOW* m_lastPriceWin = nullptr;
    WINDOW* m_statusWin = nullptr;
    WINDOW* m_helpWin = nullptr;

    // Data storage
    QHash<QString, Order> m_orders; // QHash used because Order lacks default constructor
    QHash<QString, Position> m_positions;

    // Current displayed stock info
    QString m_currentSymbol;
    Bar m_lastBar;
    bool m_hasLastBar = false;
    bool m_hasReceivedFirstBar = false;

    // Storage for fetched historical bars (triggered on first bar reception)
    std::shared_ptr<QVector<Bar>> m_fetchedDayBars;

    // Status info
    qsizetype m_dataUsage = 0;
    qsizetype m_memoryUsage = 0;

    // Input handling
    QSocketNotifier* m_inputNotifier = nullptr;
    bool m_initialized = false;
    bool m_hasRestoredLastStock = false;
};
