#include "MarketDepthTable.h"
#include "MarketDepthTableView.h"
#include <QHeaderView>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

MarketDepthTable::MarketDepthTable(QWidget* parent)
    : QWidget(parent)
    , tableView(new MarketDepthTableView(this))
    , model(new QStandardItemModel(this))
    , bidLabel(new QLabel("BID", this))
    , askLabel(new QLabel("ASK", this))
{
    setupUI();
    setupStyles();
}

MarketDepthTable::~MarketDepthTable() {
    // Qt will handle deletion of child widgets
}

void MarketDepthTable::setupUI() {
    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Setup header widget with BID/ASK labels
    QWidget* headerWidget = new QWidget(this);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setSpacing(0);
    headerLayout->setContentsMargins(0, 0, 0, 0);

    // Configure labels
    bidLabel->setAlignment(Qt::AlignCenter);
    askLabel->setAlignment(Qt::AlignCenter);
    bidLabel->setFixedHeight(24);
    askLabel->setFixedHeight(24);

    headerLayout->addWidget(bidLabel, 1);
    headerLayout->addWidget(askLabel, 1);

    // Set up columns: BID and ASK sides with their respective columns
    QStringList headers;
    headers << "Price" << "Size" << "Orders" << "Name"    // BID columns
            << "Price" << "Size" << "Orders" << "Name";   // ASK columns
    model->setHorizontalHeaderLabels(headers);

    // Configure table view
    tableView->setModel(model);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableView->verticalHeader()->setVisible(false);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->setAlternatingRowColors(true);
    tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tableView->setTopMargin(24);

    // Set column widths
    int priceWidth = 80;
    int sizeWidth = 80;
    int ordersWidth = 60;
    int nameWidth = 80;
    
    // BID side
    tableView->setColumnWidth(0, priceWidth);  // Price
    tableView->setColumnWidth(1, sizeWidth);   // Size
    tableView->setColumnWidth(2, ordersWidth); // Orders
    tableView->setColumnWidth(3, nameWidth);   // Name
    
    // ASK side
    tableView->setColumnWidth(4, priceWidth);  // Price
    tableView->setColumnWidth(5, sizeWidth);   // Size
    tableView->setColumnWidth(6, ordersWidth); // Orders
    tableView->setColumnWidth(7, nameWidth);   // Name

    // Calculate and set the fixed width for the entire widget
    int totalWidth = (priceWidth + sizeWidth + ordersWidth + nameWidth) * 2; // *2 for both BID and ASK sides
    setFixedWidth(totalWidth);
    tableView->setFixedWidth(totalWidth);

    // Add widgets to main layout
    mainLayout->addWidget(headerWidget);
    mainLayout->addWidget(tableView);

    // Set up a connection to handle header widget resizing
    connect(tableView->horizontalHeader(), &QHeaderView::geometriesChanged,
            [headerWidget, this]() {
                headerWidget->setGeometry(0, 0,
                                        tableView->viewport()->width() +
                                        tableView->verticalHeader()->width(),
                                        24);
            });
}

void MarketDepthTable::setupStyles() {
    // Style the header labels
    bidLabel->setStyleSheet(
        "QLabel {"
        "   background-color: #2D2D2D;"
        "   color: #00FF00;"
        "   padding: 4px;"
        "   border-bottom: 1px solid #3D3D3D;"
        "}"
    );

    askLabel->setStyleSheet(
        "QLabel {"
        "   background-color: #2D2D2D;"
        "   color: #FF0000;"
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
        "QHeaderView::section:first {"
        "   border-top: 1px solid #00FF00;"  // Green line for BID section
        "}"
        "QHeaderView::section:last {"
        "   border-top: 1px solid #FF0000;"  // Red line for ASK section
        "}"
    );
}

void MarketDepthTable::setMarketDepthItem(QStandardItem* item, const MarketDepthLevel& level, const QString& field) {
    QString text;
    if (field == "Price") {
        text = level.getPrice();
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    } else if (field == "Size") {
        text = level.getSize();
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    } else if (field == "Orders") {
        text = QString::number(level.getOrderCount());
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    } else if (field == "Name") {
        text = level.getName();
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    
    item->setText(text);
    
    // Set color based on whether it's a bid or ask (determined by column position)
    int column = item->column();
    if (column < 4) {  // BID side
        item->setForeground(QColor("#00FF00"));  // Green for bids
    } else {  // ASK side
        item->setForeground(QColor("#FF0000"));  // Red for asks
    }
}

void MarketDepthTable::updateData(const QVector<MarketDepthLevel>& bids, const QVector<MarketDepthLevel>& asks) {
    // Clear existing data
    model->removeRows(0, model->rowCount());

    // Find the maximum number of rows needed
    int maxRows = qMax(bids.size(), asks.size());

    // Add data row by row
    for (int i = 0; i < maxRows; ++i) {
        QList<QStandardItem*> rowItems;
        
        // Add bid columns if available
        if (i < bids.size()) {
            const MarketDepthLevel& bid = bids[i];
            rowItems << new QStandardItem(); setMarketDepthItem(rowItems.last(), bid, "Price");
            rowItems << new QStandardItem(); setMarketDepthItem(rowItems.last(), bid, "Size");
            rowItems << new QStandardItem(); setMarketDepthItem(rowItems.last(), bid, "Orders");
            rowItems << new QStandardItem(); setMarketDepthItem(rowItems.last(), bid, "Name");
        } else {
            // Add empty cells for bid
            for (int j = 0; j < 4; ++j) {
                auto item = new QStandardItem("");
                item->setForeground(QColor("#00FF00"));
                rowItems << item;
            }
        }

        // Add ask columns if available
        if (i < asks.size()) {
            const MarketDepthLevel& ask = asks[i];
            rowItems << new QStandardItem(); setMarketDepthItem(rowItems.last(), ask, "Price");
            rowItems << new QStandardItem(); setMarketDepthItem(rowItems.last(), ask, "Size");
            rowItems << new QStandardItem(); setMarketDepthItem(rowItems.last(), ask, "Orders");
            rowItems << new QStandardItem(); setMarketDepthItem(rowItems.last(), ask, "Name");
        } else {
            // Add empty cells for ask
            for (int j = 0; j < 4; ++j) {
                auto item = new QStandardItem("");
                item->setForeground(QColor("#FF0000"));
                rowItems << item;
            }
        }

        model->appendRow(rowItems);
    }
} 