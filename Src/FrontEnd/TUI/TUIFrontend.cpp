#include "TUIFrontend.h"
#include "MainAlgo.h"
#include "PlaceOrder.h"
#include "Settings.h"
#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QTimeZone>
#include <cmath>
#include <locale.h>

TUIFrontend::TUIFrontend(MainAlgo* p_mainAlgo, QObject* parent) : FrontEnd(parent), mainAlgo(p_mainAlgo)
{
    // Connect internal signal to slot for handling auth state changes
    connect(this,
            &FrontEnd::tradeStationAuthStateChanged,
            this,
            &TUIFrontend::onTradeStationAuthStateChanged,
            Qt::DirectConnection);
}

TUIFrontend::~TUIFrontend()
{
    cleanup();
}

void TUIFrontend::initialize()
{
    if (m_initialized)
    {
        return;
    }

    // Set locale for proper character encoding
    setlocale(LC_ALL, "");

    // Initialize ncurses
    initscr();             // Initialize the screen
    cbreak();              // Disable line buffering
    noecho();              // Don't echo input characters
    nonl();                // Disable newline translation to prevent scrolling issues
    keypad(stdscr, TRUE);  // Enable special keys
    nodelay(stdscr, TRUE); // Non-blocking input
    curs_set(0);           // Hide cursor

    // Prevent terminal scrolling
    scrollok(stdscr, FALSE); // Disable scrolling on stdscr
    idlok(stdscr, FALSE);    // Disable hardware scrolling

    // Enable colors if supported
    if (has_colors())
    {
        start_color();
        use_default_colors();

        // Define color pairs
        init_pair(1, COLOR_WHITE, COLOR_BLUE); // Header
        init_pair(2, COLOR_GREEN, -1);         // Positive P/L
        init_pair(3, COLOR_RED, -1);           // Negative P/L
        init_pair(4, COLOR_YELLOW, -1);        // Status bar
        init_pair(5, COLOR_CYAN, -1);          // Help text
    }

    setupWindows();

    // Setup input notifier for keyboard input
    m_inputNotifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
    connect(m_inputNotifier, &QSocketNotifier::activated, this, &TUIFrontend::handleInput);

    m_initialized = true;
    refreshDisplay();
}

void TUIFrontend::setupWindows()
{
    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);

    // Calculate window sizes
    // Layout: Orders (top half) | Positions + LastPrice (bottom half) | Status | Help
    int orderHeight = maxY / 2 - 2;
    int lastPriceHeight = 6;                             // Fixed height for last price window
    int positionHeight = maxY / 2 - lastPriceHeight - 3; // Leave room for status bar and help
    int statusHeight = 1;
    int helpHeight = 2;

    // Create windows
    m_orderWin = newwin(orderHeight, maxX, 0, 0);
    m_positionWin = newwin(positionHeight, maxX, orderHeight, 0);
    m_lastPriceWin = newwin(lastPriceHeight, maxX, orderHeight + positionHeight, 0);
    m_statusWin = newwin(statusHeight, maxX, orderHeight + positionHeight + lastPriceHeight, 0);
    m_helpWin = newwin(helpHeight, maxX, orderHeight + positionHeight + lastPriceHeight + statusHeight, 0);

    // Disable scrolling for all windows to prevent terminal scroll issues
    scrollok(m_orderWin, FALSE);
    scrollok(m_positionWin, FALSE);
    scrollok(m_lastPriceWin, FALSE);
    scrollok(m_statusWin, FALSE);
    scrollok(m_helpWin, FALSE);
}

void TUIFrontend::cleanup()
{
    if (m_initialized)
    {
        if (m_orderWin)
            delwin(m_orderWin);
        if (m_positionWin)
            delwin(m_positionWin);
        if (m_lastPriceWin)
            delwin(m_lastPriceWin);
        if (m_statusWin)
            delwin(m_statusWin);
        if (m_helpWin)
            delwin(m_helpWin);

        endwin();
        m_initialized = false;
    }
}

void TUIFrontend::refreshDisplay()
{
    if (!m_initialized)
    {
        return;
    }

    displayOrders();
    displayPositions();
    displayLastPrice();
    displayStatusBar();
    displayHelp();

    doupdate();
}

void TUIFrontend::displayOrders()
{
    if (!m_orderWin)
    {
        return;
    }

    werase(m_orderWin);
    box(m_orderWin, 0, 0);

    // Display header
    wattron(m_orderWin, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(m_orderWin, 0, 2, " ORDERS (%lld) ", static_cast<long long>(m_orders.size()));
    wattroff(m_orderWin, COLOR_PAIR(1) | A_BOLD);

    // Display column headers
    mvwprintw(m_orderWin,
              1,
              2,
              "%-12s %-8s %-6s %-6s %-8s %-8s %-20s %-15s",
              "Order ID",
              "Symbol",
              "Action",
              "Qty",
              "Type",
              "Price",
              "DateTime",
              "Status");

    // Display orders (most recent first)
    int row = 2;
    int maxRows = getmaxy(m_orderWin) - 3;

    QList<QString> orderIds = m_orders.keys();
    for (int i = orderIds.size() - 1; i >= 0 && row < maxRows; --i, ++row)
    {
        auto it = m_orders.constFind(orderIds[i]);
        if (it == m_orders.constEnd())
        {
            continue;
        }
        const Order& order = it.value();

        QString priceStr;
        if (order.getLimitPrice().has_value())
        {
            priceStr = QString::number(order.getLimitPrice().value(), 'f', 2);
        }
        else if (order.getStopPrice().has_value())
        {
            priceStr = QString::number(order.getStopPrice().value(), 'f', 2);
        }
        else
        {
            priceStr = "Market";
        }

        QString statusStr;
        switch (order.getOrderStatus())
        {
        case Order::Status::ACK:
            statusStr = "Acknowledged";
            break;
        case Order::Status::DON:
            statusStr = "Done";
            break;
        case Order::Status::FLL:
            statusStr = "Filled";
            break;
        case Order::Status::FPR:
            statusStr = "Part Filled";
            break;
        case Order::Status::OPN:
            statusStr = "Open";
            break;
        case Order::Status::OUT:
            statusStr = "Sent";
            break;
        case Order::Status::REJ:
            statusStr = "Rejected";
            break;
        case Order::Status::UCN:
            statusStr = "Canceling";
            break;
        case Order::Status::CAN:
            statusStr = "Canceled";
            break;
        default:
            statusStr = "Unknown";
            break;
        }

        mvwprintw(m_orderWin,
                  row,
                  2,
                  "%-12s %-8s %-6s %-6s %-8s %-8s %-20s %-15s",
                  order.getOrderID().left(12).toStdString().c_str(),
                  order.getSymbol().toStdString().c_str(),
                  order.getTradeAction().toStdString().c_str(),
                  order.getQuantity().toStdString().c_str(),
                  OrderType::toString(order.getOrderType().type).left(8).toStdString().c_str(),
                  priceStr.left(8).toStdString().c_str(),
                  order.getOpenedDateTime().toString("MM/dd hh:mm:ss").toStdString().c_str(),
                  statusStr.left(15).toStdString().c_str());
    }

    wnoutrefresh(m_orderWin);
}

void TUIFrontend::displayPositions()
{
    if (!m_positionWin)
    {
        return;
    }

    werase(m_positionWin);
    box(m_positionWin, 0, 0);

    // Display header
    wattron(m_positionWin, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(m_positionWin, 0, 2, " POSITIONS (%lld) ", static_cast<long long>(m_positions.size()));
    wattroff(m_positionWin, COLOR_PAIR(1) | A_BOLD);

    // Display column headers
    mvwprintw(m_positionWin,
              1,
              2,
              "%-8s %-10s %-12s %-12s %-12s %-10s %-12s",
              "Symbol",
              "Quantity",
              "Avg Price",
              "Last",
              "P/L",
              "P/L %",
              "Market Val");

    // Display positions
    int row = 2;
    int maxRows = getmaxy(m_positionWin) - 3;

    QList<QString> posIds = m_positions.keys();
    for (int i = 0; i < posIds.size() && row < maxRows; ++i, ++row)
    {
        auto it = m_positions.constFind(posIds[i]);
        if (it == m_positions.constEnd())
        {
            continue;
        }
        const Position& pos = it.value();

        double avgPrice = pos.getAveragePrice().toDouble();
        double last = pos.getLast().toDouble();
        int quantity = pos.getQuantity().toInt();
        double pl = (last - avgPrice) * quantity;
        // Use epsilon for floating point comparison to avoid division by zero
        constexpr double epsilon = 1e-9;
        double plPercent = (std::abs(avgPrice) > epsilon) ? ((last - avgPrice) / avgPrice * 100.0) : 0.0;
        double marketValue = last * quantity;

        // Color based on P/L
        if (pl > 0)
        {
            wattron(m_positionWin, COLOR_PAIR(2)); // Green for profit
        }
        else if (pl < 0)
        {
            wattron(m_positionWin, COLOR_PAIR(3)); // Red for loss
        }

        mvwprintw(m_positionWin,
                  row,
                  2,
                  "%-8s %-10s %-12s %-12s %-12s %-10s %-12s",
                  pos.getSymbol().toStdString().c_str(),
                  QString::number(quantity).toStdString().c_str(),
                  QString::number(avgPrice, 'f', 2).toStdString().c_str(),
                  QString::number(last, 'f', 2).toStdString().c_str(),
                  QString::number(pl, 'f', 2).toStdString().c_str(),
                  QString("%1%").arg(plPercent, 0, 'f', 2).toStdString().c_str(),
                  QString::number(marketValue, 'f', 2).toStdString().c_str());

        if (pl != 0)
        {
            wattroff(m_positionWin, COLOR_PAIR(pl > 0 ? 2 : 3));
        }
    }

    wnoutrefresh(m_positionWin);
}

void TUIFrontend::displayLastPrice()
{
    if (!m_lastPriceWin)
    {
        return;
    }

    werase(m_lastPriceWin);
    box(m_lastPriceWin, 0, 0);

    // Display header with symbol
    wattron(m_lastPriceWin, COLOR_PAIR(1) | A_BOLD);
    if (m_currentSymbol.isEmpty())
    {
        mvwprintw(m_lastPriceWin, 0, 2, " LAST PRICE (No Stock Selected) ");
    }
    else
    {
        mvwprintw(m_lastPriceWin, 0, 2, " LAST PRICE [%s] ", m_currentSymbol.toStdString().c_str());
    }
    wattroff(m_lastPriceWin, COLOR_PAIR(1) | A_BOLD);

    if (!m_hasLastBar || m_currentSymbol.isEmpty())
    {
        mvwprintw(m_lastPriceWin, 2, 4, "Waiting for live data...");
        wnoutrefresh(m_lastPriceWin);
        return;
    }

    // Get bar data - cast to double to avoid float-to-double conversion warnings
    double open = static_cast<double>(m_lastBar.getOpen());
    double high = static_cast<double>(m_lastBar.getHigh());
    double low = static_cast<double>(m_lastBar.getLow());
    double close = static_cast<double>(m_lastBar.getClose());
    quint64 volume = m_lastBar.getTotalVolume();
    QDateTime timestamp = m_lastBar.getTimestamp();
    Bar::BarStatus status = m_lastBar.getBarStatus();

    // Calculate price change
    double change = close - open;
    double changePercent = (std::abs(open) > 1e-9) ? (change / open * 100.0) : 0.0;

    // Display close price prominently
    int colorPair = (change >= 0) ? 2 : 3; // Green for positive, red for negative
    wattron(m_lastPriceWin, COLOR_PAIR(colorPair) | A_BOLD);
    mvwprintw(m_lastPriceWin, 1, 4, "CLOSE: $%.2f  (%+.2f / %+.2f%%)", close, change, changePercent);
    wattroff(m_lastPriceWin, COLOR_PAIR(colorPair) | A_BOLD);

    // Display OHLV on second line
    mvwprintw(m_lastPriceWin,
              2,
              4,
              "O: $%.2f  H: $%.2f  L: $%.2f  Vol: %llu",
              open,
              high,
              low,
              static_cast<unsigned long long>(volume));

    // Display timestamp and status on third line
    QString statusStr;
    switch (status)
    {
    case Bar::BarStatus::Open:
        statusStr = "LIVE";
        break;
    case Bar::BarStatus::Closed:
        statusStr = "CLOSED";
        break;
    default:
        statusStr = "---";
        break;
    }

    QString timeStr = timestamp.toString("hh:mm:ss");
    mvwprintw(m_lastPriceWin,
              3,
              4,
              "Time: %s  Status: %s",
              timeStr.toStdString().c_str(),
              statusStr.toStdString().c_str());

    wnoutrefresh(m_lastPriceWin);
}

void TUIFrontend::displayStatusBar()
{
    if (!m_statusWin)
    {
        return;
    }

    werase(m_statusWin);
    wattron(m_statusWin, COLOR_PAIR(4));

    mvwprintw(m_statusWin,
              0,
              2,
              "Data: %lld KB | Memory: %lld KB | Streams: %d",
              static_cast<long long>(m_dataUsage / 1024),
              static_cast<long long>(m_memoryUsage / 1024),
              m_streamCount);

    wattroff(m_statusWin, COLOR_PAIR(4));
    wnoutrefresh(m_statusWin);
}

void TUIFrontend::displayHelp()
{
    if (!m_helpWin)
    {
        return;
    }

    werase(m_helpWin);
    wattron(m_helpWin, COLOR_PAIR(5));

    mvwprintw(m_helpWin, 0, 2, "Keyboard Shortcuts:");
    mvwprintw(m_helpWin, 1, 2, "q/Q: Quit | r/R: Refresh | ?: Help");

    wattroff(m_helpWin, COLOR_PAIR(5));
    wnoutrefresh(m_helpWin);
}

void TUIFrontend::handleInput()
{
    int ch = getch();

    if (ch == ERR)
    {
        return; // No input available
    }

    switch (ch)
    {
    case 'q':
    case 'Q':
        handleQuitShortcut();
        break;
    case 'r':
    case 'R':
        handleRefreshShortcut();
        break;
    case '?':
        // Could implement a detailed help screen here
        refreshDisplay();
        break;
    case KEY_RESIZE:
        // Handle terminal resize
        endwin();
        refresh();
        setupWindows();
        refreshDisplay();
        break;
    default:
        break;
    }
}

void TUIFrontend::handleQuitShortcut()
{
    cleanup();
    QCoreApplication::quit();
}

void TUIFrontend::handleRefreshShortcut()
{
    refreshDisplay();
}

void TUIFrontend::onTSClientDataUsageUpdate(qsizetype newDataUsage)
{
    m_dataUsage = newDataUsage;
    if (m_initialized)
    {
        displayStatusBar();
        doupdate();
    }
}

void TUIFrontend::onMemoryUsageUpdate(qsizetype newDataUsage)
{
    m_memoryUsage = newDataUsage;
    if (m_initialized)
    {
        displayStatusBar();
        doupdate();
    }
}

void TUIFrontend::onStreamCountUpdate(int count)
{
    m_streamCount = count;
    if (m_initialized)
    {
        displayStatusBar();
        doupdate();
    }
}

void TUIFrontend::onTradeStationAccountsReceived(QVector<Account> results)
{
    Q_UNUSED(results);
    // Could display account info in status bar or separate window
}

void TUIFrontend::onNewPositionReceived(QString account, Position position)
{
    Q_UNUSED(account);
    m_positions.insert(position.getPositionID(), position);
    if (m_initialized)
    {
        refreshDisplay();
    }
}

void TUIFrontend::onPositionDeleted(QString account, QString positionID)
{
    Q_UNUSED(account);
    m_positions.remove(positionID);
    if (m_initialized)
    {
        refreshDisplay();
    }
}

void TUIFrontend::onNewOrderReceived(QString account, Order order)
{
    Q_UNUSED(account);
    m_orders.insert(order.getOrderID(), order);
    if (m_initialized)
    {
        refreshDisplay();
    }
}

void TUIFrontend::onBalanceUpdated(Balance balance)
{
    Q_UNUSED(balance);
    // Could display balance info in status bar
}

void TUIFrontend::onCurrentHighlightedStockBarReceived(QString symbol, Bar bar)
{
    if (symbol != m_currentSymbol)
    {
        return; // Ignore bars for symbols we're not tracking
    }

    m_lastBar = bar;
    m_hasLastBar = true;

    // On receiving the first bar, request all bars from the beginning of the day
    // This mimics the GUI behavior to trigger the same code path for testing
    if (!m_hasReceivedFirstBar)
    {
        m_hasReceivedFirstBar = true;
        qInfo() << "TUI received first bar for" << symbol << "- requesting day's historical bars";
        requestMissingBarsForDay(bar);
    }

    if (m_initialized)
    {
        displayLastPrice();
        doupdate();
    }
}

void TUIFrontend::onCurrentHighlightedReceivedNewMarketDepthQuote(QString symbol,
                                                                  MarketDepthQuote quote,
                                                                  double bidAskImbalance,
                                                                  double bidDWP,
                                                                  double askDWP)
{
    Q_UNUSED(symbol);
    Q_UNUSED(quote);
    Q_UNUSED(bidAskImbalance);
    Q_UNUSED(bidDWP);
    Q_UNUSED(askDWP);
    // Not applicable for minimal TUI
}

void TUIFrontend::saveLastDisplayedStock(const QString& symbol)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("UI/LastDisplayedStock", symbol);
    appStateSettings->sync();
    qInfo() << "Saved last displayed stock:" << symbol;
}

void TUIFrontend::restoreLastDisplayedStock()
{
    Q_CHECK_PTR(appStateSettings);
    QString lastSymbol = appStateSettings->value("UI/LastDisplayedStock").toString().toUpper();

    if (lastSymbol.isEmpty())
    {
        qInfo() << "No previously displayed stock to restore";
        return;
    }

    if (!isValidStockSymbol(lastSymbol))
    {
        qWarning() << "Previously saved stock symbol is invalid:" << lastSymbol;
        return;
    }

    qInfo() << "Restoring last displayed stock:" << lastSymbol;

    // Display the stock
    displayStock(lastSymbol);
}

void TUIFrontend::displayStock(const QString& symbol)
{
    if (symbol == m_currentSymbol)
    {
        qWarning() << "Symbol" << symbol << "is already the currently displayed symbol";
        return;
    }

    // Update our tracked symbol
    m_currentSymbol = symbol;
    m_hasLastBar = false;
    m_hasReceivedFirstBar = false;
    m_fetchedDayBars.reset();

    // Save the symbol for restoration on next startup
    saveLastDisplayedStock(symbol);

    // Notify MainAlgo to start streaming bars for this symbol
    QMetaObject::invokeMethod(mainAlgo, "onSelectDisplayedStock", Qt::QueuedConnection, Q_ARG(QString, symbol));

    // Update the display
    if (m_initialized)
    {
        displayLastPrice();
        doupdate();
    }
}

bool TUIFrontend::isValidStockSymbol(const QString& symbol) const
{
    // Check that symbol is not empty
    if (symbol.isEmpty())
    {
        return false;
    }

    // Check for leading or trailing whitespace
    if (symbol != symbol.trimmed())
    {
        return false;
    }

    // Check length (1-10 characters)
    if (symbol.length() < 1 || symbol.length() > 10)
    {
        return false;
    }

    // Check for valid characters (letters, numbers, dots, hyphens, slashes)
    for (const QChar& c: symbol)
    {
        if (!c.isLetterOrNumber() && c != '.' && c != '-' && c != '/')
        {
            return false;
        }
    }

    return true;
}

void TUIFrontend::onTradeStationAuthStateChanged(bool isAuthenticated, const QString& reason)
{
    Q_UNUSED(reason);

    if (isAuthenticated)
    {
        // Restore the last displayed stock now that we're authenticated
        // Only do this once on the first successful authentication
        if (!m_hasRestoredLastStock)
        {
            m_hasRestoredLastStock = true;
            restoreLastDisplayedStock();
        }
    }
}

void TUIFrontend::requestMissingBarsForDay(const Bar& firstBar)
{
    // Trading hours constants (America/New_York timezone)
    static constexpr int TRADING_START_HOUR = 6;

    // Request bars from the beginning of the trading day (6:01 AM) to the first received bar
    QDateTime first =
        QDateTime(firstBar.getTimeStamp().date(), QTime(TRADING_START_HOUR, 1, 0), QTimeZone("America/New_York"));
    QDateTime last = firstBar.getTimeStamp();

    qInfo() << "TUI requesting missing bars from" << first.toString(Qt::ISODate) << "to" << last.toString(Qt::ISODate);

    BarCache::GetBarsResult_t result =
        MainAlgo::getInstance()->requestMissingBarsDisplayedStock(first.date(), first.time(), last.time());

    if (std::holds_alternative<std::shared_ptr<QVector<Bar>>>(result))
    {
        // The barCache had the bars ready immediately
        m_fetchedDayBars = std::get<std::shared_ptr<QVector<Bar>>>(result);
        if (m_fetchedDayBars)
        {
            qInfo() << "TUI received" << m_fetchedDayBars->size() << "historical bars immediately";
        }
        else
        {
            qWarning() << "TUI received null bar data immediately";
        }
    }
    else
    {
        auto future = std::get<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(result);
        future.then(this,
                    [this](std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error> bars)
                    {
                        if (bars.has_value())
                        {
                            m_fetchedDayBars = bars.value();
                            if (m_fetchedDayBars)
                            {
                                qInfo() << "TUI received" << m_fetchedDayBars->size()
                                        << "historical bars asynchronously";
                            }
                            else
                            {
                                qWarning() << "TUI received null bar data asynchronously";
                            }
                        }
                        else
                        {
                            qCritical() << "TUI failed to get missing bars - Error:" << static_cast<int>(bars.error());
                        }
                    });
    }
}
