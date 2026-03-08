#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLabel>

#include "OrderWidget.h"
#include "Assume.h"
#include "CONSTANTS.h"

OrderWidget::OrderWidget(QWidget* p_parent)
    : QWidget(p_parent)
    , m_tableView(new QTableView(this))
    , m_model(new QStandardItemModel(this))
    , m_headerLabel(new QLabel("ORDERS", this))
{
    setupUI();
    setupStyles();
}

OrderWidget::~OrderWidget()
{
    // Qt will handle deletion of child widgets
}

void OrderWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Setup header
    m_headerLabel->setFixedHeight(24);
    m_headerLabel->setAlignment(Qt::AlignCenter);

    // Setup model columns
    QStringList headers;
    headers << "Status" << "Symbol" << "Action" << "Qty" << "Type" << "Limit" << "Stop" << "Date" << "Time"
            << "Latency" << "Order ID";
    m_model->setHorizontalHeaderLabels(headers);

    // Configure table view
    m_tableView->setModel(m_model);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->setSelectionMode(QAbstractItemView::NoSelection);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Connect click signal
    auto c = connect(m_tableView, &QTableView::clicked, this, &OrderWidget::onSymbolClicked, Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c);

    // Set column widths
    m_tableView->setColumnWidth(0, 100);  // Status
    m_tableView->setColumnWidth(1, 70);   // Symbol
    m_tableView->setColumnWidth(2, 53);   // Action
    m_tableView->setColumnWidth(3, 50);   // Quantity
    m_tableView->setColumnWidth(4, 70);   // Type
    m_tableView->setColumnWidth(5, 60);   // Limit
    m_tableView->setColumnWidth(6, 60);   // Stop
    m_tableView->setColumnWidth(7, 80);   // Date
    m_tableView->setColumnWidth(8, 80);   // Time
    m_tableView->setColumnWidth(9, 60);   // Latency
    m_tableView->setColumnWidth(10, 100); // Order ID

    // Add widgets to layout
    mainLayout->addWidget(m_headerLabel);
    mainLayout->addWidget(m_tableView);

    // Set a reasonable maximum width to fit on screen, but allow the table to scroll horizontally
    // Calculate total width for all columns
    int totalWidth = 0;
    for (int i = 0; i < headers.size(); ++i)
    {
        totalWidth += m_tableView->columnWidth(i);
    }

    // Set maximum width to accommodate content, but widget can be smaller and scroll
    setMaximumWidth(totalWidth + 20); // Add padding for scrollbar
    setMinimumWidth(400);             // Ensure minimum usable width
}

void OrderWidget::setupStyles()
{
    // Style the header label
    m_headerLabel->setStyleSheet("QLabel {"
                                 "   background-color: #2D2D2D;"
                                 "   color: #FFFFFF;"
                                 "   padding: 4px;"
                                 "   border-bottom: 1px solid #3D3D3D;"
                                 "}");

    // Style the table
    m_tableView->setStyleSheet("QTableView {"
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
                               "}");
}

void OrderWidget::updateOrder(const QString& account, const Order& order)
{
    Q_UNUSED(account);
    QString orderId = order.getOrderID();

    qDebug() << "OrderWidget::updateOrder called for order ID:" << orderId << "Symbol:" << order.getSymbol()
             << "Quantity:" << order.getQuantity() << "TradeAction:" << order.getTradeAction()
             << "OrderType:" << static_cast<int>(order.getOrderType().type) << "LimitPrice:"
             << (order.getLimitPrice().has_value() ? QString::number(order.getLimitPrice().value()) : "not set")
             << "StopPrice:"
             << (order.getStopPrice().has_value() ? QString::number(order.getStopPrice().value()) : "not set");

    // Store or update the order object
    m_orders.insert(orderId, order);

    if (m_orderRowMap.contains(orderId))
    {
        // Update existing order
        // Note: Row indices are maintained across updates. Orders are not removed from the display,
        // only updated in place. If order removal is needed in the future, consider implementing
        // a cleanup mechanism that rebuilds the map after row removal.
        updateOrderRow(account, order);
    }
    else
    {
        // Add new order at the top (latest first)
        // Shift all existing row indices down by 1
        for (auto& rowIndex: m_orderRowMap)
        {
            rowIndex++;
        }

        QList<QStandardItem*> rowItems = createRowItems(order);
        m_model->insertRow(0, rowItems);
        m_orderRowMap[orderId] = 0;
    }
}

void OrderWidget::updateOrderRow(const QString& account, const Order& order)
{
    Q_UNUSED(account);

    QString orderId = order.getOrderID();
    if (!m_orderRowMap.contains(orderId))
    {
        qWarning() << "OrderWidget::updateOrderRow: Order ID not found in map:" << orderId;
        return;
    }

    int row = m_orderRowMap[orderId];
    if (row < 0 || row >= m_model->rowCount())
    {
        qWarning() << "OrderWidget::updateOrderRow: Invalid row index:" << row;
        return;
    }

    QList<QStandardItem*> items = createRowItems(order);

    for (int col = 0; col < items.size(); ++col)
    {
        m_model->setItem(row, col, items[col]);
    }
}

QList<QStandardItem*> OrderWidget::createRowItems(const Order& order)
{
    QList<QStandardItem*> items;
    [[maybe_unused]] bool isReceivedOrder = (order.getOrderStatus() == Order::Status::ACK);

    // Status
    auto statusItem = new QStandardItem(order.getStatusDescription());
    Q_CHECK_PTR(statusItem);
    statusItem->setTextAlignment(Qt::AlignCenter);

    // Color code status based on OrderStatus enum
    switch (order.getOrderStatus())
    {
    case Order::Status::FLL:                          // Filled
        statusItem->setBackground(QColor("#D4EDDA")); // Light green
        statusItem->setForeground(QColor("#155724")); // Dark green text
        break;
    case Order::Status::FLP:                          // Partial Fill (UROut)
    case Order::Status::FPR:                          // Partial Fill (Alive)
        statusItem->setBackground(QColor("#F8F9FA")); // Light gray
        statusItem->setForeground(QColor("#383D41")); // Dark gray text
        break;
    case Order::Status::REJ:                          // Rejected
    case Order::Status::RJC:                          // Cancel Request Rejected
        statusItem->setBackground(QColor("#F8D7DA")); // Light red
        statusItem->setForeground(QColor("#721C24")); // Dark red text
        break;
    case Order::Status::CAN:                          // Canceled
    case Order::Status::TSC:                          // Trade Server Canceled
    case Order::Status::EXP:                          // Expired
    case Order::Status::BRO:                          // Broken
        statusItem->setBackground(QColor("#F8D7DA")); // Light red
        statusItem->setForeground(QColor("#721C24")); // Dark red text
        break;
    case Order::Status::OPN:                          // Sent
    case Order::Status::UCN:                          // Cancel Sent
    case Order::Status::RSN:                          // Replace Sent
        statusItem->setBackground(QColor("#FFF3CD")); // Light yellow
        statusItem->setForeground(QColor("#856404")); // Dark yellow text
        break;
    case Order::Status::DON: // Queued
        // Button styling for queued orders
        statusItem->setBackground(QColor("#007BFF"));
        statusItem->setForeground(QColor("#FFFFFF"));
        statusItem->setText("Queued ❌"); // Add X emoji to make it look like a cancel button
        break;
    case Order::Status::ACK: // Received
        // Button styling for received orders
        statusItem->setBackground(QColor("#007BFF"));
        statusItem->setForeground(QColor("#FFFFFF"));
        statusItem->setText("Received ❌"); // Add X emoji to make it look like a cancel button
        break;
    case Order::Status::LAT: // Too Late to Cancel
    case Order::Status::OUT: // UROut
    case Order::Status::UCH: // Replaced
    case Order::Status::CND: // Condition Met
    case Order::Status::OSO: // OSO Order
    case Order::Status::SUS: // Suspended
    default:
        // Default color for other statuses
        statusItem->setBackground(QColor("#F8F9FA")); // Light gray
        statusItem->setForeground(QColor("#383D41")); // Dark gray text
        break;
    }

    // Set tooltip with detailed status information
    QString tooltipText = order.getStatusDescription();
    if (order.getOrderStatus() == Order::Status::OUT)
    {
        tooltipText = "Successfully Cancelled (UROut)\n\nThis order was cancelled successfully.";
    }
    else if (order.getRejectReason().has_value() && !order.getRejectReason().value().isEmpty())
    {
        tooltipText += "\n\nReject Reason: " + order.getRejectReason().value();
    }
    statusItem->setToolTip(tooltipText);

    items << statusItem;

    // Symbol
    auto symbolItem = new QStandardItem(order.getSymbol());
    Q_CHECK_PTR(symbolItem);
    symbolItem->setTextAlignment(Qt::AlignCenter);
    items << symbolItem;

    // Trade Action — shorten verbose action names for display
    QString displayAction = order.getTradeAction();
    if (displayAction.compare("SELLSHORT", Qt::CaseInsensitive) == 0)
        displayAction = "SHORT";
    else if (displayAction.compare("BUYTOCOVER", Qt::CaseInsensitive) == 0)
        displayAction = "COVER";
    auto actionItem = new QStandardItem(displayAction);
    Q_CHECK_PTR(actionItem);
    actionItem->setTextAlignment(Qt::AlignCenter);
    // Color code buy/sell
    if (order.getTradeAction().contains("Buy", Qt::CaseInsensitive))
    {
        actionItem->setForeground(QColor(Qt::green));
    }
    else if (order.getTradeAction().contains("Sell", Qt::CaseInsensitive))
    {
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
    switch (order.getOrderType().type)
    {
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
    if (order.getOrderType().type == OrderType::Type::Limit || order.getOrderType().type == OrderType::Type::StopLimit)
    {
        auto limitPriceOpt = order.getLimitPrice();
        if (limitPriceOpt.has_value() && limitPriceOpt.value() > 0)
        {
            limitPriceStr = QString::number(limitPriceOpt.value(), 'f', 2);
        }
        else if (limitPriceOpt.has_value())
        {
            limitPriceStr = QString::number(limitPriceOpt.value(), 'f',
                                            2); // Show even if 0
        }
    }
    auto limitItem = new QStandardItem(limitPriceStr);
    Q_CHECK_PTR(limitItem);
    limitItem->setTextAlignment(Qt::AlignCenter);
    items << limitItem;

    // Stop Price
    QString stopPriceStr = "-";
    if (order.getOrderType().type == OrderType::Type::StopMarket ||
        order.getOrderType().type == OrderType::Type::StopLimit)
    {
        auto stopPriceOpt = order.getStopPrice();
        if (stopPriceOpt.has_value() && stopPriceOpt.value() > 0)
        {
            stopPriceStr = QString::number(stopPriceOpt.value(), 'f', 2);
        }
        else if (stopPriceOpt.has_value())
        {
            stopPriceStr = QString::number(stopPriceOpt.value(), 'f',
                                           2); // Show even if 0
        }
    }
    auto stopItem = new QStandardItem(stopPriceStr);
    Q_CHECK_PTR(stopItem);
    stopItem->setTextAlignment(Qt::AlignCenter);
    items << stopItem;

    // Date and Time (split into two columns)
    QDateTime orderDT = order.getOpenedDateTime();

    OBJ_ASSUME_EQUAL(orderDT.timeZone(), TradingHours::MARKET_TIMEZONE);

    QString dateStr = orderDT.toString("MM/dd/yyyy");
    QString timeStr = orderDT.toString("hh:mm:ss");

    auto dateItem = new QStandardItem(dateStr);
    Q_CHECK_PTR(dateItem);
    dateItem->setTextAlignment(Qt::AlignCenter);
    items << dateItem;

    auto timeItem = new QStandardItem(timeStr);
    Q_CHECK_PTR(timeItem);
    timeItem->setTextAlignment(Qt::AlignCenter);
    items << timeItem;

    // Helper lambda to format latency
    auto formatLatency = [](qint64 latencyMs) -> QString
    {
        if (latencyMs < 0)
        {
            return "Invalid";
        }
        else if (latencyMs < 1000)
        {
            // Less than 1 second - show milliseconds
            return QString("%1 ms").arg(latencyMs);
        }
        else if (latencyMs < 60000)
        {
            // Less than 1 minute - show seconds with decimal
            double latencySec = latencyMs / 1000.0;
            return QString("%1 s").arg(latencySec, 0, 'f', 2);
        }
        else
        {
            // 1 minute or more - show minutes and seconds
            int minutes = latencyMs / 60000;
            int seconds = (latencyMs % 60000) / 1000;
            return QString("%1m %2s").arg(minutes).arg(seconds);
        }
    };

    // Ack/Fill Latency - display the stored latency value directly
    QString latencyStr = "-";
    QString latencyTooltipStr;

    if (order.getLatencyMs().has_value())
    {
        qint64 latencyMs = order.getLatencyMs().value();
        latencyStr = formatLatency(latencyMs);
        latencyTooltipStr = QString("Latency: %1 ms").arg(latencyMs);
    }

    auto latencyItem = new QStandardItem(latencyStr);
    Q_CHECK_PTR(latencyItem);
    latencyItem->setTextAlignment(Qt::AlignCenter);
    if (!latencyTooltipStr.isEmpty())
    {
        latencyItem->setToolTip(latencyTooltipStr);
    }
    items << latencyItem;

    // Order ID
    auto orderIdItem = new QStandardItem(order.getOrderID());
    Q_CHECK_PTR(orderIdItem);
    orderIdItem->setTextAlignment(Qt::AlignCenter);
    items << orderIdItem;

    return items;
}

void OrderWidget::onSymbolClicked(const QModelIndex& index)
{
    // Get the order ID from the last column of the clicked row
    QStandardItem* orderIdItem = m_model->item(index.row(), 10); // Order ID at column 10
    if (orderIdItem == nullptr)
    {
        return;
    }

    QString orderId = orderIdItem->text();

    // Check if this row has "Received" or "Queued" status (first column)
    QStandardItem* statusItem = m_model->item(index.row(), 0); // Status column
    bool isCancelableOrder = (statusItem != nullptr && (statusItem->text().contains("Received", Qt::CaseInsensitive) ||
                                                        statusItem->text().contains("Queued", Qt::CaseInsensitive)));

    if (isCancelableOrder)
    {
        // This is a cancelable order - emit cancel signal
        emit cancelOrderRequested(orderId);
        return;
    }

    // Otherwise, handle symbol clicks (only on Symbol column)
    if (index.column() == 1)
    { // Symbol column
        QStandardItem* symbolItem = m_model->item(index.row(), 1);
        if (symbolItem != nullptr)
        {
            QString symbol = symbolItem->text();
            emit symbolClicked(symbol);
        }
    }
}

QStringList OrderWidget::getAllOrderIds() const
{
    return m_orderRowMap.keys();
}

QStringList OrderWidget::getCancellableOrderIds() const
{
    QStringList cancellableIds;

    // Only include orders that are in a cancellable state
    for (auto it = m_orders.constBegin(); it != m_orders.constEnd(); ++it)
    {
        const Order& order = it.value();
        Order::Status status = order.getOrderStatus();

        // Only cancel orders that are queued, received, or sent
        // Don't cancel filled, cancelled, rejected, expired, etc.
        if (status == Order::Status::DON || // Queued
            status == Order::Status::ACK || // Received
            status == Order::Status::OPN || // Sent
            status == Order::Status::FPR || // Partial Fill (Alive)
            status == Order::Status::CND || // Condition Met
            status == Order::Status::OSO || // OSO Order
            status == Order::Status::SUS)
        { // Suspended
            cancellableIds.append(it.key());
        }
    }

    return cancellableIds;
}


void OrderWidget::clearAllOrders()
{
    m_model->removeRows(0, m_model->rowCount());
    m_orderRowMap.clear();
    m_orders.clear();
    qDebug() << "OrderWidget cleared all orders";
}
