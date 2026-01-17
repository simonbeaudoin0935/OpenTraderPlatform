#include "PositionWindow.h"
#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLabel>
#include <QtDebug>

PositionWindow::PositionWindow(QWidget* parent)
    : QWidget(parent)
    , tableView(new QTableView(this))
    , model(new QStandardItemModel(this))
    , headerLabel(new QLabel("POSITIONS", this))
{
    setupUI();
    setupStyles();
}

PositionWindow::~PositionWindow() {
    // Qt will handle deletion of child widgets
}

void PositionWindow::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Setup header
    headerLabel->setFixedHeight(24);
    headerLabel->setAlignment(Qt::AlignCenter);

    // Setup model columns
    QStringList headers;
    headers << "Symbol" << "Quantity" << "Avg Price" << "Last" << "P/L" << "P/L %" << "Market Value";
    model->setHorizontalHeaderLabels(headers);

    // Configure table view
    tableView->setModel(model);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableView->verticalHeader()->setVisible(false);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->setAlternatingRowColors(true);
    tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Connect click signal
    connect(tableView, &QTableView::clicked, this, &PositionWindow::onSymbolClicked);

    // Set column widths
    tableView->setColumnWidth(0, 70);  // Symbol
    tableView->setColumnWidth(1, 70);  // Quantity
    tableView->setColumnWidth(2, 70);  // Avg Price
    tableView->setColumnWidth(3, 70);  // Last
    tableView->setColumnWidth(4, 70);  // P/L
    tableView->setColumnWidth(5, 70);  // P/L %
    tableView->setColumnWidth(6, 90);  // Market Value

    // Add widgets to layout
    mainLayout->addWidget(headerLabel);
    mainLayout->addWidget(tableView);

    // Set fixed width based on total column widths
    int totalWidth = 0;
    for (int i = 0; i < headers.size(); ++i) {
        totalWidth += tableView->columnWidth(i);
    }
    setFixedWidth(totalWidth);
}

void PositionWindow::setupStyles() {
    // Style the header label
    headerLabel->setStyleSheet(
        "QLabel {"
        "   background-color: #2D2D2D;"
        "   color: #FFFFFF;"
        "   padding: 4px;"
        "   border-bottom: 1px solid #3D3D3D;"
        "}"
    );

    // Style the table
    tableView->setStyleSheet(
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

void PositionWindow::updatePosition(const QString& account, const Position& position) {
    QString positionId = position.getPositionID();
    QString symbol = position.getSymbol();
    
    Q_ASSERT(!positionId.isEmpty());
    Q_ASSERT(!symbol.isEmpty());
    
    if (positionRowMap.contains(positionId)) {
        // Update existing position with the same positionID
        updatePositionRow(account, position);
    } else if (symbolRowMap.contains(symbol)) {
        // Reuse existing row for this symbol (handles reopening closed positions)
        int row = symbolRowMap[symbol];
        Q_ASSERT(row >= 0 && row < model->rowCount());
        
        // Remove old positionID mapping if it exists
        QString oldPositionId = positionRowMap.key(row, QString());
        if (!oldPositionId.isEmpty()) {
            positionRowMap.remove(oldPositionId);
        }
        
        // Update mappings with new positionID
        positionRowMap[positionId] = row;
        
        // Update the row with new position data
        QList<QStandardItem*> items = createRowItems(position);
        for (int col = 0; col < items.size(); ++col) {
            model->setItem(row, col, items[col]);
        }
    } else {
        // Add new position (first time seeing this symbol)
        QList<QStandardItem*> rowItems = createRowItems(position);
        model->appendRow(rowItems);
        int newRow = model->rowCount() - 1;
        positionRowMap[positionId] = newRow;
        symbolRowMap[symbol] = newRow;
    }
}

void PositionWindow::updatePositionRow(const QString& account, const Position& position) {
    Q_UNUSED(account);

    QString positionId = position.getPositionID();
    Q_ASSERT(positionRowMap.contains(positionId));
    
    int row = positionRowMap[positionId];
    Q_ASSERT(row >= 0 && row < model->rowCount());
    
    QList<QStandardItem*> items = createRowItems(position);
    
    for (int col = 0; col < items.size(); ++col) {
        model->setItem(row, col, items[col]);
    }
    
    // Update symbolRowMap and handle potential symbol changes
    QString newSymbol = position.getSymbol();
    QString oldSymbol = symbolRowMap.key(row, QString());
    
    if (!oldSymbol.isEmpty() && oldSymbol != newSymbol) {
        // Symbol changed (unlikely but possible) - remove old mapping
        symbolRowMap.remove(oldSymbol);
    }
    
    symbolRowMap[newSymbol] = row;
}

QList<QStandardItem*> PositionWindow::createRowItems(const Position& position) {
    QList<QStandardItem*> items;

    // Symbol
    auto symbolItem = new QStandardItem(position.getSymbol());
    symbolItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    items << symbolItem;

    // Quantity
    auto quantityItem = new QStandardItem(position.getQuantity());
    quantityItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    items << quantityItem;

    // Average Price
    auto avgPriceItem = new QStandardItem(QString::number(position.getAveragePrice().toDouble(), 'f', 2));
    avgPriceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    items << avgPriceItem;

    // Last Price
    auto lastItem = new QStandardItem(QString::number(position.getLast().toDouble(), 'f', 2));
    lastItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    items << lastItem;

    // P/L
    double pl = position.getUnrealizedProfitLoss().toDouble();
    auto plItem = new QStandardItem(QString::number(pl, 'f', 2));
    plItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    plItem->setForeground(pl >= 0 ? QColor(Qt::green) : QColor(Qt::red));
    items << plItem;

    // P/L %
    double plPercent = position.getUnrealizedProfitLossPercent().toDouble();
    auto plPercentItem = new QStandardItem(QString::number(plPercent, 'f', 2) + "%");
    plPercentItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    plPercentItem->setForeground(plPercent >= 0 ? QColor(Qt::green) : QColor(Qt::red));
    items << plPercentItem;

    // Market Value
    auto marketValueItem = new QStandardItem(QString::number(position.getMarketValue().toDouble(), 'f', 2));
    marketValueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    items << marketValueItem;

    return items;
}

void PositionWindow::onPositionDeleted(const QString& account, const QString& positionID) {
    Q_UNUSED(account);

    if (positionRowMap.contains(positionID)) {
        int row = positionRowMap[positionID];
        Q_ASSERT(row >= 0 && row < model->rowCount());
        
        // Set quantity to 0 instead of removing the row
        auto quantityItem = new QStandardItem("0");
        quantityItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        model->setItem(row, 1, quantityItem);  // Column 1 is Quantity
        
        // Remove the positionID mapping (it won't be used anymore)
        // but keep the symbolRowMap entry so we can reuse this row if the position is reopened
        positionRowMap.remove(positionID);
    }
}

void PositionWindow::onSymbolClicked(const QModelIndex& index) {
    if (index.column() == 0) {  // Only handle clicks on the Symbol column
        QString symbol = model->item(index.row(), 0)->text();
        emit symbolClicked(symbol);
    }
}
