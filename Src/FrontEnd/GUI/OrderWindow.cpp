#include "OrderWindow.h"
#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLabel>
#include "Misc/MarketHours.h"

OrderWindow::OrderWindow(QWidget* p_parent)
    : QWidget(p_parent)
    , m_tableView(new QTableView(this))
    , m_model(new QStandardItemModel(this))
    , m_headerLabel(new QLabel("ORDERS", this))
{
    setupUI();
    setupStyles();
}

OrderWindow::~OrderWindow() {
    // Qt will handle deletion of child widgets
}

void OrderWindow::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Setup header
    m_headerLabel->setFixedHeight(24);
    m_headerLabel->setAlignment(Qt::AlignCenter);

    // Setup model columns
    QStringList headers;
    headers << "Order ID" << "Symbol" << "Action" << "Qty" << "Type" << "Limit" << "Stop" << "DateTime" << "Status";
    m_model->setHorizontalHeaderLabels(headers);

    // Configure table view
    m_tableView->setModel(m_model);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Connect click signal
    auto c = connect(m_tableView, &QTableView::clicked, this, &OrderWindow::onSymbolClicked, Qt::UniqueConnection);
    Q_ASSERT(c);

    // Set column widths
    m_tableView->setColumnWidth(0, 100); // Order ID
    m_tableView->setColumnWidth(1, 70);  // Symbol
    m_tableView->setColumnWidth(2, 80);  // Action
    m_tableView->setColumnWidth(3, 50);  // Quantity
    m_tableView->setColumnWidth(4, 70);  // Type
    m_tableView->setColumnWidth(5, 60);  // Limit
    m_tableView->setColumnWidth(6, 60);  // Stop
    m_tableView->setColumnWidth(7, 100); // Status

    // Add widgets to layout
    mainLayout->addWidget(m_headerLabel);
    mainLayout->addWidget(m_tableView);

    // Set fixed width based on total column widths
    int totalWidth = 0;
    for (int i = 0; i < headers.size(); ++i) {
        totalWidth += m_tableView->columnWidth(i);
    }
    setFixedWidth(totalWidth);
}

void OrderWindow::setupStyles() {
    // Style the header label
    m_headerLabel->setStyleSheet(
        "QLabel {"
        "   background-color: #2D2D2D;"
        "   color: #FFFFFF;"
        "   padding: 4px;"
        "   border-bottom: 1px solid #3D3D3D;"
        "}"
    );

    // Style the table
    m_tableView->setStyleSheet(
        "QTableView {"
        "   alternate-background-color: #1C1C1C;"
        "   background-color: #242424;"
        "   color: white;"
        "   gridline-color: #3D3D3D;"
        "}"
        "QTableView::item:selected {"
        "   background-color: #2C539E;"
        "}"
        "QHeaderView::section {"
        "   background-color: #2D2D2D;"
        "   color: white;"
        "   border: none;"
        "   border-right: 1px solid #3D3D3D;"
        "   padding: 4px;"
        "}"
    );
}

void OrderWindow::updateOrder(const QString& account, const Order& order) {
    Q_UNUSED(account);
    QString orderId = order.getOrderID();
    
    qDebug() << "OrderWindow::updateOrder called for order ID:" << orderId 
                        << "Symbol:" << order.getSymbol()
                        << "Quantity:" << order.getQuantity()
                        << "TradeAction:" << order.getTradeAction()
                        << "OrderType:" << static_cast<int>(order.getOrderType().type)
                        << "LimitPrice:" << (order.getLimitPrice().has_value() ? QString::number(order.getLimitPrice().value()) : "not set")
                        << "StopPrice:" << (order.getStopPrice().has_value() ? QString::number(order.getStopPrice().value()) : "not set");
    
    if (m_orderRowMap.contains(orderId)) {
        // Update existing order
        // Note: Row indices are maintained across updates. Orders are not removed from the display,
        // only updated in place. If order removal is needed in the future, consider implementing
        // a cleanup mechanism that rebuilds the map after row removal.
        updateOrderRow(account, order);
    } else {
        // Add new order
        QList<QStandardItem*> rowItems = createRowItems(order);
        m_model->appendRow(rowItems);
        m_orderRowMap[orderId] = m_model->rowCount() - 1;
    }
}

void OrderWindow::updateOrderRow(const QString& account, const Order& order) {
    Q_UNUSED(account);

    QString orderId = order.getOrderID();
    if (!m_orderRowMap.contains(orderId)) {
        qWarning() << "OrderWindow::updateOrderRow: Order ID not found in map:" << orderId;
        return;
    }

    int row = m_orderRowMap[orderId];
    if (row < 0 || row >= m_model->rowCount()) {
        qWarning() << "OrderWindow::updateOrderRow: Invalid row index:" << row;
        return;
    }

    QList<QStandardItem*> items = createRowItems(order);
    
    for (int col = 0; col < items.size(); ++col) {
        m_model->setItem(row, col, items[col]);
    }
}

QList<QStandardItem*> OrderWindow::createRowItems(const Order& order) {
    QList<QStandardItem*> items;

    // Order ID
    auto orderIdItem = new QStandardItem(order.getOrderID());
    Q_CHECK_PTR(orderIdItem);
    orderIdItem->setTextAlignment(Qt::AlignCenter);
    items << orderIdItem;

    // Symbol
    auto symbolItem = new QStandardItem(order.getSymbol());
    Q_CHECK_PTR(symbolItem);
    symbolItem->setTextAlignment(Qt::AlignCenter);
    items << symbolItem;

    // Trade Action
    auto actionItem = new QStandardItem(order.getTradeAction());
    Q_CHECK_PTR(actionItem);
    actionItem->setTextAlignment(Qt::AlignCenter);
    // Color code buy/sell
    if (order.getTradeAction().contains("Buy", Qt::CaseInsensitive)) {
        actionItem->setForeground(QColor(Qt::green));
    } else if (order.getTradeAction().contains("Sell", Qt::CaseInsensitive)) {
        actionItem->setForeground(QColor(Qt::red));
    }
    items << actionItem;

    // Quantity
    auto quantityItem = new QStandardItem(order.getQuantity());
    Q_CHECK_PTR(quantityItem);
    quantityItem->setTextAlignment(Qt::AlignCenter);
    items << quantityItem;

    // Order Type
    QString orderTypeStr;
    switch (order.getOrderType().type) {
        case OrderType::Type::Market:
            orderTypeStr = "Market";
            break;
        case OrderType::Type::Limit:
            orderTypeStr = "Limit";
            break;
        case OrderType::Type::StopMarket:
            orderTypeStr = "StopMkt";
            break;
        case OrderType::Type::StopLimit:
            orderTypeStr = "StopLmt";
            break;
        default:
            orderTypeStr = "Unknown";
            break;
    }
    auto typeItem = new QStandardItem(orderTypeStr);
    Q_CHECK_PTR(typeItem);
    typeItem->setTextAlignment(Qt::AlignCenter);
    items << typeItem;

    // Limit Price
    QString limitPriceStr = "-";
    if (order.getOrderType().type == OrderType::Type::Limit || 
        order.getOrderType().type == OrderType::Type::StopLimit) {
        auto limitPriceOpt = order.getLimitPrice();
        if (limitPriceOpt.has_value() && limitPriceOpt.value() > 0) {
            limitPriceStr = QString::number(limitPriceOpt.value(), 'f', 2);
        } else if (limitPriceOpt.has_value()) {
            limitPriceStr = QString::number(limitPriceOpt.value(), 'f', 2);  // Show even if 0
        }
    }
    auto limitItem = new QStandardItem(limitPriceStr);
    Q_CHECK_PTR(limitItem);
    limitItem->setTextAlignment(Qt::AlignCenter);
    items << limitItem;

    // Stop Price
    QString stopPriceStr = "-";
    if (order.getOrderType().type == OrderType::Type::StopMarket || 
        order.getOrderType().type == OrderType::Type::StopLimit) {
        auto stopPriceOpt = order.getStopPrice();
        if (stopPriceOpt.has_value() && stopPriceOpt.value() > 0) {
            stopPriceStr = QString::number(stopPriceOpt.value(), 'f', 2);
        } else if (stopPriceOpt.has_value()) {
            stopPriceStr = QString::number(stopPriceOpt.value(), 'f', 2);  // Show even if 0
        }
    }
    auto stopItem = new QStandardItem(stopPriceStr);
    Q_CHECK_PTR(stopItem);
    stopItem->setTextAlignment(Qt::AlignCenter);
    items << stopItem;

    // DateTime
    QDateTime nyDateTime = MarketHours::toNewYorkTime(order.getOpenedDateTime());
    QString dateTimeStr = nyDateTime.toString("MM/dd/yyyy hh:mm:ss");
    QString timeOnlyStr = nyDateTime.toString("hh:mm:ss");
    auto dateTimeItem = new QStandardItem(timeOnlyStr);
    Q_CHECK_PTR(dateTimeItem);
    dateTimeItem->setTextAlignment(Qt::AlignCenter);
    dateTimeItem->setToolTip(dateTimeStr);  // Show full datetime on hover
    items << dateTimeItem;

    // Status
    auto statusItem = new QStandardItem(order.getStatusDescription());
    Q_CHECK_PTR(statusItem);
    statusItem->setTextAlignment(Qt::AlignCenter);
    
    // Color code status based on OrderStatus enum
    switch (order.getOrderStatus()) {
        case OrderStatus::FLL:  // Filled
            statusItem->setBackground(QColor("#D4EDDA"));  // Light green
            statusItem->setForeground(QColor("#155724"));  // Dark green text
            break;
        case OrderStatus::FLP:  // Partial Fill (UROut)
        case OrderStatus::FPR:  // Partial Fill (Alive)
            statusItem->setBackground(QColor("#F8F9FA"));  // Light gray
            statusItem->setForeground(QColor("#383D41"));  // Dark gray text
            break;
        case OrderStatus::REJ:  // Rejected
        case OrderStatus::RJC:  // Cancel Request Rejected
            statusItem->setBackground(QColor("#F8D7DA"));  // Light red
            statusItem->setForeground(QColor("#721C24"));  // Dark red text
            break;
        case OrderStatus::CAN:  // Canceled
        case OrderStatus::TSC:  // Trade Server Canceled
        case OrderStatus::EXP:  // Expired
        case OrderStatus::BRO:  // Broken
            statusItem->setBackground(QColor("#F8D7DA"));  // Light red
            statusItem->setForeground(QColor("#721C24"));  // Dark red text
            break;
        case OrderStatus::OPN:  // Sent
        case OrderStatus::DON:  // Queued
        case OrderStatus::UCN:  // Cancel Sent
        case OrderStatus::RSN:  // Replace Sent
            statusItem->setBackground(QColor("#FFF3CD"));  // Light yellow
            statusItem->setForeground(QColor("#856404"));  // Dark yellow text
            break;
        case OrderStatus::ACK:  // Received
            statusItem->setBackground(QColor("#D1ECF1"));  // Light blue
            statusItem->setForeground(QColor("#0C5460"));  // Dark blue text
            break;
        case OrderStatus::LAT:  // Too Late to Cancel
        case OrderStatus::OUT:  // UROut
        case OrderStatus::UCH:  // Replaced
        case OrderStatus::CND:  // Condition Met
        case OrderStatus::OSO:  // OSO Order
        case OrderStatus::SUS:  // Suspended
        default:
            // Default color for other statuses
            statusItem->setBackground(QColor("#F8F9FA"));  // Light gray
            statusItem->setForeground(QColor("#383D41"));  // Dark gray text
            break;
    }
    
    items << statusItem;

    return items;
}

void OrderWindow::onSymbolClicked(const QModelIndex& index) {
    if (index.column() == 1) {  // Only handle clicks on the Symbol column (now column 1)
        QStandardItem* item = m_model->item(index.row(), 1);
        if (item != nullptr) {
            QString symbol = item->text();
            emit symbolClicked(symbol);
        }
    }
}
