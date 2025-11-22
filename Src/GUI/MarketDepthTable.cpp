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
#include <QPixmap>
#include <QPainter>
#include <QFont>

MarketDepthTable::MarketDepthTable(QWidget* parent)
    : QWidget(parent)
    , tableView(new MarketDepthTableView(this))
    , model(new QStandardItemModel(this))
    , bidLabel(new QLabel("BID", this))
    , askLabel(new QLabel("ASK", this))
    , spreadLabel(new QLabel("SPREAD", this))
    , dwpLabel(new QLabel("DWP", this))
    , bidDWPLabel(new QLabel("", this))
    , askDWPLabel(new QLabel("", this))
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

    // Create DWP widget with its own layout
    QWidget* dwpWidget = new QWidget(this);
    dwpWidget->setFixedWidth(totalWidth);
    QHBoxLayout* dwpLayout = new QHBoxLayout(dwpWidget);
    dwpLayout->setSpacing(0);
    dwpLayout->setContentsMargins(0, 0, 0, 0);
    
    // Configure DWP labels
    bidDWPLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    dwpLabel->setAlignment(Qt::AlignCenter);
    askDWPLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    
    bidDWPLabel->setFixedHeight(24);
    dwpLabel->setFixedHeight(24);
    askDWPLabel->setFixedHeight(24);

    // Add question mark icon to DWP label
    QPixmap questionIcon(16, 16);
    questionIcon.fill(Qt::transparent);
    QPainter painter(&questionIcon);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 12, QFont::Bold));
    painter.drawText(questionIcon.rect(), Qt::AlignCenter, "?");
    
    // Create a layout for the DWP label to properly align the icon and text
    QHBoxLayout* dwpLabelLayout = new QHBoxLayout();
    dwpLabelLayout->setSpacing(4);
    dwpLabelLayout->setContentsMargins(0, 0, 0, 0);
    dwpLabelLayout->setAlignment(Qt::AlignCenter);
    
    QLabel* iconLabel = new QLabel(this);
    iconLabel->setPixmap(questionIcon);
    iconLabel->setFixedSize(16, 16);
    iconLabel->setAlignment(Qt::AlignCenter);
    
    QLabel* textLabel = new QLabel("DWP", this);
    textLabel->setStyleSheet("color: white;");
    textLabel->setAlignment(Qt::AlignCenter);
    
    dwpLabelLayout->addStretch();
    dwpLabelLayout->addWidget(textLabel);
    dwpLabelLayout->addWidget(iconLabel);
    dwpLabelLayout->addStretch();
    
    QWidget* dwpLabelContainer = new QWidget(this);
    dwpLabelContainer->setLayout(dwpLabelLayout);
    dwpLabelContainer->setFixedHeight(24);
    dwpLabelContainer->setToolTip("Depth-Weighted Price : Estimate a volume-weighted average price within the order book to identify a 'fair value' beyond the NBBO");
    dwpLabelContainer->setStyleSheet(
        "QWidget {"
        "   background-color: #2D2D2D;"
        "   border-bottom: 1px solid #3D3D3D;"
        "}"
    );
    
    dwpLayout->addWidget(bidDWPLabel, 1);
    dwpLayout->addWidget(dwpLabelContainer, 1);
    dwpLayout->addWidget(askDWPLabel, 1);

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
    mainLayout->addWidget(dwpWidget, 0, Qt::AlignTop);
    mainLayout->addWidget(tableView, 1);

    // Ensure initial geometry is correct
    QTimer::singleShot(0, this, [headerWidget, spreadWidget, dwpWidget, this]() {
        int width = tableView->viewport()->width() +
                   tableView->verticalHeader()->width();
        headerWidget->setGeometry(0, 0, width, 24);
        spreadWidget->setGeometry(0, 24, width, 24);
        dwpWidget->setGeometry(0, 48, width, 24);
    });

    // Set up a connection to handle header widget resizing
    bool connection = connect(tableView->horizontalHeader(), &QHeaderView::geometriesChanged,
            [headerWidget, spreadWidget, dwpWidget, this]() {
                int width = tableView->viewport()->width() +
                           tableView->verticalHeader()->width();
                headerWidget->setGeometry(0, 0, width, 24);
                spreadWidget->setGeometry(0, 24, width, 24);
                dwpWidget->setGeometry(0, 48, width, 24);
            }, Qt::UniqueConnection);
    Q_ASSERT_X(connection, "MarketDepthTable", "Failed to create unique connection for horizontalHeader geometriesChanged");
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

    // Style the DWP labels
    QString dwpStyle = 
        "QLabel {"
        "   background-color: #2D2D2D;"
        "   padding: 4px;"
        "   border-bottom: 1px solid #3D3D3D;"
        "}";

    bidDWPLabel->setStyleSheet(dwpStyle +
        "QLabel {"
        "   color: #00FF00;"
        "}"
    );

    dwpLabel->setStyleSheet(dwpStyle +
        "QLabel {"
        "   color: #FFFFFF;"
        "}"
    );

    askDWPLabel->setStyleSheet(dwpStyle +
        "QLabel {"
        "   color: #FF0000;"
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

void MarketDepthTable::updateData(const QVector<MarketDepthLevel>& bids, const QVector<MarketDepthLevel>& asks) {
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

void MarketDepthTable::updateDWP(double bidDWP, double askDWP) {
    bidDWPLabel->setText(QString::number(bidDWP, 'f', 2));
    askDWPLabel->setText(QString::number(askDWP, 'f', 2));
} 
