#include "MarketDepthTable.h"
#include "MarketDepthTableView.h"
#include <QHeaderView>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <QSet>
#include <QList>
#include <algorithm>

MarketDepthTable::MarketDepthTable(QWidget* parent)
    : QWidget(parent)
    , tableView(new MarketDepthTableView(this))
    , model(new QStandardItemModel(this))
    , bidLabel(new QLabel("BID", this))
    , askLabel(new QLabel("ASK", this))
    , spreadLabel(new QLabel("SPREAD", this))
    , bidAskImbalanceGauge(nullptr)
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

    // Calculate widths first as they're used in multiple places
    int priceWidth = 56;
    int sizeWidth = 56;  // Restored to original value
    int ordersWidth = 51;  // Increased from 46 (another 10% increase)
    int nameWidth = 45;  // Previously reduced from 56 (20% reduction)
    int totalWidth = (priceWidth + sizeWidth + ordersWidth + nameWidth) * 2;

    // Setup header widget with BID/ASK labels
    QWidget* headerWidget = new QWidget(this);
    headerWidget->setFixedWidth(totalWidth);
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

    // Create spread widget with its own layout
    QWidget* spreadWidget = new QWidget(this);
    spreadWidget->setFixedWidth(totalWidth);
    QHBoxLayout* spreadLayout = new QHBoxLayout(spreadWidget);
    spreadLayout->setSpacing(0);
    spreadLayout->setContentsMargins(0, 0, 0, 0);
    
    spreadLabel->setAlignment(Qt::AlignCenter);
    spreadLabel->setFixedHeight(24);
    spreadLayout->addWidget(spreadLabel);

    // Create bid-ask imbalance gauge widget
    bidAskImbalanceGauge = new QLabel(this);
    bidAskImbalanceGauge->setAlignment(Qt::AlignCenter);
    bidAskImbalanceGauge->setFixedHeight(24);
    
    // Create gauge widget with its own layout
    QWidget* gaugeWidget = new QWidget(this);
    gaugeWidget->setFixedWidth(totalWidth);
    QHBoxLayout* gaugeLayout = new QHBoxLayout(gaugeWidget);
    gaugeLayout->setSpacing(0);
    gaugeLayout->setContentsMargins(0, 0, 0, 0);
    gaugeLayout->addWidget(bidAskImbalanceGauge);

    // Set up columns: BID and ASK sides with their respective columns
    QStringList headers;
    headers << "Name" << "Orders" << "Size" << "Price"    // BID columns
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
    tableView->setTopMargin(48);  // Increased to accommodate spread row
    
    // BID side
    tableView->setColumnWidth(0, nameWidth);   // Name
    tableView->setColumnWidth(1, ordersWidth); // Orders
    tableView->setColumnWidth(2, sizeWidth);   // Size
    tableView->setColumnWidth(3, priceWidth);  // Price
    
    // ASK side
    tableView->setColumnWidth(4, priceWidth);  // Price
    tableView->setColumnWidth(5, sizeWidth);   // Size
    tableView->setColumnWidth(6, ordersWidth); // Orders
    tableView->setColumnWidth(7, nameWidth);   // Name

    // Set fixed width for the table view and widget
    tableView->setFixedWidth(totalWidth);
    setFixedWidth(totalWidth);

    // Add widgets to main layout with zero spacing
    mainLayout->addWidget(headerWidget, 0, Qt::AlignTop);
    mainLayout->addWidget(spreadWidget, 0, Qt::AlignTop);
    mainLayout->addWidget(gaugeWidget, 0, Qt::AlignTop);
    mainLayout->addWidget(tableView, 1);

    // Ensure initial geometry is correct
    QTimer::singleShot(0, this, [headerWidget, spreadWidget, gaugeWidget, this]() {
        int width = tableView->viewport()->width() +
                   tableView->verticalHeader()->width();
        headerWidget->setGeometry(0, 0, width, 24);
        spreadWidget->setGeometry(0, 24, width, 24);
        gaugeWidget->setGeometry(0, 48, width, 24);
    });

    // Set up a connection to handle header widget resizing
    connect(tableView->horizontalHeader(), &QHeaderView::geometriesChanged,
            [headerWidget, spreadWidget, gaugeWidget, this]() {
                int width = tableView->viewport()->width() +
                           tableView->verticalHeader()->width();
                headerWidget->setGeometry(0, 0, width, 24);
                spreadWidget->setGeometry(0, 24, width, 24);
                gaugeWidget->setGeometry(0, 48, width, 24);
            });
}

void MarketDepthTable::setupStyles() {
    // Style the header labels
    QString commonStyle = 
        "QLabel {"
        "   background-color: #2D2D2D;"
        "   padding: 4px;"
        "   border-bottom: 1px solid #3D3D3D;"
        "}";

    bidLabel->setStyleSheet(commonStyle +
        "QLabel {"
        "   color: #00FF00;"
        "}"
    );

    askLabel->setStyleSheet(commonStyle +
        "QLabel {"
        "   color: #FF0000;"
        "}"
    );

    spreadLabel->setStyleSheet(commonStyle +
        "QLabel {"
        "   color: #FFFFFF;"
        "   border-top: 1px solid #3D3D3D;"  // Add top border
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

void MarketDepthTable::setMarketDepthItem(QStandardItem* item, const MarketDepthLevel& level, const QString& field, int priceLevel) {
    QString text;
    if (field == "Price") {
        // Convert to double and format with exactly 2 decimal places
        double price = level.getPrice().toDouble();
        text = QString::number(price, 'f', 2);
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
    
    // Set color based on whether it's a bid or ask and the price level
    int column = item->column();
    if (column < 4) {  // BID side
        switch (priceLevel) {
            case 0:
                item->setForeground(QColor("#00FF00"));  // Bright green
                break;
            case 1:
                item->setForeground(QColor("#FFFF00"));  // Yellow
                break;
            case 2:
                item->setForeground(QColor("#00FFFF"));  // Cyan
                break;
            case 3:
                item->setForeground(QColor("#FFA500"));  // Orange
                break;
            case 4:
                item->setForeground(QColor("#98FB98"));  // Pale green
                break;
            case 5:
                item->setForeground(QColor("#87CEEB"));  // Sky blue
                break;
            default:
                item->setForeground(QColor("#009900"));  // Dark green for remaining levels
        }
    } else {  // ASK side
        switch (priceLevel) {
            case 0:
                item->setForeground(QColor("#00FF00"));  // Bright green
                break;
            case 1:
                item->setForeground(QColor("#FFFF00"));  // Yellow
                break;
            case 2:
                item->setForeground(QColor("#FF69B4"));  // Hot pink
                break;
            case 3:
                item->setForeground(QColor("#FFA500"));  // Orange
                break;
            case 4:
                item->setForeground(QColor("#DDA0DD"));  // Plum
                break;
            case 5:
                item->setForeground(QColor("#F08080"));  // Light coral
                break;
            default:
                item->setForeground(QColor("#FF0000"));  // Red for remaining levels
        }
    }
}

void MarketDepthTable::updateData(const QVector<MarketDepthLevel>& bids, const QVector<MarketDepthLevel>& asks, double bidAskImbalance) {
    // Update the gauge for bid-ask imbalance
    QString gaugeText = QString("Imbalance: %1").arg(bidAskImbalance, 0, 'f', 2);
    bidAskImbalanceGauge->setText(gaugeText);
    
    // Set color based on imbalance value
    if (bidAskImbalance > 0) {
        bidAskImbalanceGauge->setStyleSheet("QLabel { background-color: green; color: white; }");
    } else if (bidAskImbalance < 0) {
        bidAskImbalanceGauge->setStyleSheet("QLabel { background-color: red; color: white; }");
    } else {
        bidAskImbalanceGauge->setStyleSheet("QLabel { background-color: gray; color: white; }");
    }

    // Clear existing data
    model->removeRows(0, model->rowCount());

    // Calculate spread if we have both bids and asks
    if (!bids.isEmpty() && !asks.isEmpty()) {
        double bestBid = bids.first().getPrice().toDouble();
        double bestAsk = asks.first().getPrice().toDouble();
        double spread = bestAsk - bestBid;
        spreadLabel->setText(QString("SPREAD: %1").arg(spread, 0, 'f', 2));
    } else {
        spreadLabel->setText("SPREAD: N/A");
    }

    // Find unique price levels for bids and asks
    QSet<double> bidPrices, askPrices;
    for (const auto& bid : bids) {
        bidPrices.insert(bid.getPrice().toDouble());
    }
    for (const auto& ask : asks) {
        askPrices.insert(ask.getPrice().toDouble());
    }

    // Sort price levels (descending for bids, ascending for asks)
    QList<double> sortedBidPrices = bidPrices.values();
    QList<double> sortedAskPrices = askPrices.values();
    std::sort(sortedBidPrices.begin(), sortedBidPrices.end(), std::greater<double>());
    std::sort(sortedAskPrices.begin(), sortedAskPrices.end());

    // Find the maximum number of rows needed
    int maxRows = qMax(bids.size(), asks.size());

    // Add data row by row
    for (int i = 0; i < maxRows; ++i) {
        QList<QStandardItem*> rowItems;
        
        // Add bid columns if available
        if (i < bids.size()) {
            const MarketDepthLevel& bid = bids[i];
            double bidPrice = bid.getPrice().toDouble();
            int priceLevel = sortedBidPrices.indexOf(bidPrice);
            
            rowItems << new QStandardItem(); setMarketDepthItem(rowItems.last(), bid, "Name", priceLevel);
            rowItems << new QStandardItem(); setMarketDepthItem(rowItems.last(), bid, "Orders", priceLevel);
            rowItems << new QStandardItem(); setMarketDepthItem(rowItems.last(), bid, "Size", priceLevel);
            rowItems << new QStandardItem(); setMarketDepthItem(rowItems.last(), bid, "Price", priceLevel);
        } else {
            // Add empty cells for bid
            for (int j = 0; j < 4; ++j) {
                auto item = new QStandardItem("");
                item->setForeground(QColor("#009900"));  // Darker green for empty bid cells
                rowItems << item;
            }
        }

        // Add ask columns if available
        if (i < asks.size()) {
            const MarketDepthLevel& ask = asks[i];
            double askPrice = ask.getPrice().toDouble();
            int priceLevel = sortedAskPrices.indexOf(askPrice);
            
            rowItems << new QStandardItem(); setMarketDepthItem(rowItems.last(), ask, "Price", priceLevel);
            rowItems << new QStandardItem(); setMarketDepthItem(rowItems.last(), ask, "Size", priceLevel);
            rowItems << new QStandardItem(); setMarketDepthItem(rowItems.last(), ask, "Orders", priceLevel);
            rowItems << new QStandardItem(); setMarketDepthItem(rowItems.last(), ask, "Name", priceLevel);
        } else {
            // Add empty cells for ask
            for (int j = 0; j < 4; ++j) {
                auto item = new QStandardItem("");
                item->setForeground(QColor("#FF0000"));  // Red for empty ask cells
                rowItems << item;
            }
        }

        model->appendRow(rowItems);
    }
} 