#include "MarketDepthTable.h"
#include "MarketDepthTableView.h"
#include "Level2.h"
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
    , m_dataSourceLabel(new QLabel("--", this))
{
    setupUI();
    setupStyles();
    updateDataSourceIndicator();
}

MarketDepthTable::~MarketDepthTable()
{
    // Qt will handle deletion of child widgets
}

void MarketDepthTable::setupUI()
{
    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Calculate widths first as they're used in multiple places
    int priceWidth = 56;
    int sizeWidth = 56;   // Restored to original value
    int ordersWidth = 51; // Increased from 46 (another 10% increase)
    int totalWidth = (priceWidth + sizeWidth + ordersWidth) * 2;

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

    // Configure data source indicator (small, between BID and ASK)
    m_dataSourceLabel->setAlignment(Qt::AlignCenter);
    m_dataSourceLabel->setFixedHeight(24);
    m_dataSourceLabel->setFixedWidth(30);
    m_dataSourceLabel->setToolTip("Data source: L2 = Level 2 (full depth), L1 = Level 1 (best bid/ask only)");

    headerLayout->addWidget(bidLabel, 1);
    headerLayout->addWidget(m_dataSourceLabel, 0);
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
    dwpLabelContainer->setToolTip(
        "Depth-Weighted Price : Estimate a volume-weighted average price within the order book to identify a 'fair value' beyond the NBBO");
    dwpLabelContainer->setStyleSheet("QWidget {"
                                     "   background-color: #2D2D2D;"
                                     "   border-bottom: 1px solid #3D3D3D;"
                                     "}");

    dwpLayout->addWidget(bidDWPLabel, 1);
    dwpLayout->addWidget(dwpLabelContainer, 1);
    dwpLayout->addWidget(askDWPLabel, 1);

    // Set up columns: BID and ASK sides with their respective columns
    QStringList headers;
    headers << "Orders" << "Size" << "Price"  // BID columns
            << "Price" << "Size" << "Orders"; // ASK columns
    model->setHorizontalHeaderLabels(headers);

    // Configure table view
    tableView->setModel(model);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableView->verticalHeader()->setVisible(false);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->setAlternatingRowColors(true);
    tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tableView->setTopMargin(48); // Increased to accommodate spread row

    // BID side
    tableView->setColumnWidth(0,
                              ordersWidth); // Orders
    tableView->setColumnWidth(1,
                              sizeWidth); // Size
    tableView->setColumnWidth(2,
                              priceWidth); // Price

    // ASK side
    tableView->setColumnWidth(3,
                              priceWidth); // Price
    tableView->setColumnWidth(4,
                              sizeWidth); // Size
    tableView->setColumnWidth(5,
                              ordersWidth); // Orders

    // Set fixed width for the table view and widget
    tableView->setFixedWidth(totalWidth);
    setFixedWidth(totalWidth);

    // Add widgets to main layout with zero spacing
    mainLayout->addWidget(headerWidget, 0, Qt::AlignTop);
    mainLayout->addWidget(spreadWidget, 0, Qt::AlignTop);
    mainLayout->addWidget(dwpWidget, 0, Qt::AlignTop);
    mainLayout->addWidget(tableView, 1);

    // Ensure initial geometry is correct
    QTimer::singleShot(0,
                       this,
                       [headerWidget, spreadWidget, dwpWidget, this]()
                       {
                           int width = tableView->viewport()->width() + tableView->verticalHeader()->width();
                           headerWidget->setGeometry(0, 0, width, 24);
                           spreadWidget->setGeometry(0, 24, width, 24);
                           dwpWidget->setGeometry(0, 48, width, 24);
                       });

    // Set up a connection to handle header widget resizing
    connect(tableView->horizontalHeader(),
            &QHeaderView::geometriesChanged,
            [headerWidget, spreadWidget, dwpWidget, this]()
            {
                int width = tableView->viewport()->width() + tableView->verticalHeader()->width();
                headerWidget->setGeometry(0, 0, width, 24);
                spreadWidget->setGeometry(0, 24, width, 24);
                dwpWidget->setGeometry(0, 48, width, 24);
            });
}

void MarketDepthTable::setupStyles()
{
    // Style the header labels
    QString commonStyle = "QLabel {"
                          "   background-color: #2D2D2D;"
                          "   padding: 4px;"
                          "   border-bottom: 1px solid #3D3D3D;"
                          "}";

    bidLabel->setStyleSheet(commonStyle + "QLabel {"
                                          "   color: #00FF00;"
                                          "}");

    askLabel->setStyleSheet(commonStyle + "QLabel {"
                                          "   color: #FF0000;"
                                          "}");

    spreadLabel->setStyleSheet(commonStyle + "QLabel {"
                                             "   color: #FFFFFF;"
                                             "   border-top: 1px solid #3D3D3D;" // Add top border
                                             "}");

    // Style the DWP labels
    QString dwpStyle = "QLabel {"
                       "   background-color: #2D2D2D;"
                       "   padding: 4px;"
                       "   border-bottom: 1px solid #3D3D3D;"
                       "}";

    bidDWPLabel->setStyleSheet(dwpStyle + "QLabel {"
                                          "   color: #00FF00;"
                                          "}");

    dwpLabel->setStyleSheet(dwpStyle + "QLabel {"
                                       "   color: #FFFFFF;"
                                       "}");

    askDWPLabel->setStyleSheet(dwpStyle + "QLabel {"
                                          "   color: #FF0000;"
                                          "}");

    // Style the table
    tableView->setStyleSheet("QTableView {"
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
                             "   border-top: 1px solid #00FF00;" // Green line for BID section
                             "}"
                             "QHeaderView::section:last {"
                             "   border-top: 1px solid #FF0000;" // Red line for ASK section
                             "}");
}

void MarketDepthTable::setMarketDepthItem(QStandardItem* item,
                                          const Level2Row& level,
                                          const QString& field,
                                          int priceLevel)
{
    QString text;
    if (field == "Price")
    {
        text = QString::number(level.m_price, 'f', 2);
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    else if (field == "Size")
    {
        text = QString::number(level.m_size);
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    else if (field == "Orders")
    {
        text = QString::number(level.m_orderCount);
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    item->setText(text);

    // Set color based on whether it's a bid or ask and the price level
    int column = item->column();
    if (column < 3)
    { // BID side
        switch (priceLevel)
        {
        case 0:
            item->setForeground(QColor("#00FF00")); // Bright green
            break;
        case 1:
            item->setForeground(QColor("#FFFF00")); // Yellow
            break;
        case 2:
            item->setForeground(QColor("#00FFFF")); // Cyan
            break;
        case 3:
            item->setForeground(QColor("#FFA500")); // Orange
            break;
        case 4:
            item->setForeground(QColor("#98FB98")); // Pale green
            break;
        case 5:
            item->setForeground(QColor("#87CEEB")); // Sky blue
            break;
        default:
            item->setForeground(QColor("#009900")); // Dark green for remaining levels
        }
    }
    else
    { // ASK side
        switch (priceLevel)
        {
        case 0:
            item->setForeground(QColor("#00FF00")); // Bright green
            break;
        case 1:
            item->setForeground(QColor("#FFFF00")); // Yellow
            break;
        case 2:
            item->setForeground(QColor("#FF69B4")); // Hot pink
            break;
        case 3:
            item->setForeground(QColor("#FFA500")); // Orange
            break;
        case 4:
            item->setForeground(QColor("#DDA0DD")); // Plum
            break;
        case 5:
            item->setForeground(QColor("#F08080")); // Light coral
            break;
        default:
            item->setForeground(QColor("#FF0000")); // Red for remaining levels
        }
    }
}

void MarketDepthTable::updateData(const std::array<Level2Row, 10>& bids, const std::array<Level2Row, 10>& asks)
{
    // Update display mode to Level 2
    m_displayMode = DisplayMode::Level2;
    updateDataSourceIndicator();

    // Clear existing data
    model->removeRows(0, model->rowCount());

    // Calculate spread if we have data
    if (bids[0].m_price > 0.0 && asks[0].m_price > 0.0)
    {
        double bestBid = bids[0].m_price;
        double bestAsk = asks[0].m_price;
        double spread = bestAsk - bestBid;
        spreadLabel->setText(QString("SPREAD: %1").arg(spread, 0, 'f', 2));
    }
    else
    {
        spreadLabel->setText("SPREAD: N/A");
    }

    // Find unique price levels for bids and asks
    QSet<double> bidPrices, askPrices;
    for (const auto& bid: bids)
    {
        bidPrices.insert(bid.m_price);
    }
    for (const auto& ask: asks)
    {
        askPrices.insert(ask.m_price);
    }

    // Sort price levels (descending for bids, ascending for asks)
    QList<double> sortedBidPrices = bidPrices.values();
    QList<double> sortedAskPrices = askPrices.values();
    std::sort(sortedBidPrices.begin(), sortedBidPrices.end(), std::greater<double>());
    std::sort(sortedAskPrices.begin(), sortedAskPrices.end());

    // Add data row by row - always 10 rows
    for (int i = 0; i < 10; ++i)
    {
        QList<QStandardItem*> rowItems;

        const Level2Row& bid = bids[i];
        double bidPrice = bid.m_price;
        int bidPriceLevel = sortedBidPrices.indexOf(bidPrice);

        rowItems << new QStandardItem();
        setMarketDepthItem(rowItems.last(), bid, "Orders", bidPriceLevel);
        rowItems << new QStandardItem();
        setMarketDepthItem(rowItems.last(), bid, "Size", bidPriceLevel);
        rowItems << new QStandardItem();
        setMarketDepthItem(rowItems.last(), bid, "Price", bidPriceLevel);

        const Level2Row& ask = asks[i];
        double askPrice = ask.m_price;
        int askPriceLevel = sortedAskPrices.indexOf(askPrice);

        rowItems << new QStandardItem();
        setMarketDepthItem(rowItems.last(), ask, "Price", askPriceLevel);
        rowItems << new QStandardItem();
        setMarketDepthItem(rowItems.last(), ask, "Size", askPriceLevel);
        rowItems << new QStandardItem();
        setMarketDepthItem(rowItems.last(), ask, "Orders", askPriceLevel);

        model->appendRow(rowItems);
    }
}

void MarketDepthTable::updateDWP(double bidDWP, double askDWP)
{
    bidDWPLabel->setText(QString::number(bidDWP, 'f', 2));
    askDWPLabel->setText(QString::number(askDWP, 'f', 2));
}

void MarketDepthTable::updateLevel1Data(const Level1& level1)
{
    // Update display mode
    m_displayMode = DisplayMode::Level1;
    updateDataSourceIndicator();

    // Clear existing data
    model->removeRows(0, model->rowCount());

    // Get best bid/ask from Level1
    double bestBid = level1.m_bid.m_price;
    double bestAsk = level1.m_ask.m_price;
    int bidSize = level1.m_bid.m_size;
    int askSize = level1.m_ask.m_size;

    // Calculate spread
    if (bestBid > 0 && bestAsk > 0)
    {
        double spread = bestAsk - bestBid;
        spreadLabel->setText(QString("SPREAD: %1").arg(spread, 0, 'f', 2));
    }
    else
    {
        spreadLabel->setText("SPREAD: N/A");
    }

    // Clear DWP (not available for L1)
    bidDWPLabel->setText("--");
    askDWPLabel->setText("--");

    // Add single row with best bid/ask
    QList<QStandardItem*> rowItems;

    // BID side: Orders, Size, Price
    auto* bidOrdersItem = new QStandardItem("--");
    bidOrdersItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    bidOrdersItem->setForeground(QColor("#00FF00"));
    rowItems << bidOrdersItem;

    auto* bidSizeItem = new QStandardItem(QString::number(bidSize));
    bidSizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    bidSizeItem->setForeground(QColor("#00FF00"));
    rowItems << bidSizeItem;

    auto* bidPriceItem = new QStandardItem(QString::number(bestBid, 'f', 2));
    bidPriceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    bidPriceItem->setForeground(QColor("#00FF00"));
    rowItems << bidPriceItem;

    // ASK side: Price, Size, Orders
    auto* askPriceItem = new QStandardItem(QString::number(bestAsk, 'f', 2));
    askPriceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    askPriceItem->setForeground(QColor("#00FF00"));
    rowItems << askPriceItem;

    auto* askSizeItem = new QStandardItem(QString::number(askSize));
    askSizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    askSizeItem->setForeground(QColor("#00FF00"));
    rowItems << askSizeItem;

    auto* askOrdersItem = new QStandardItem("--");
    askOrdersItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    askOrdersItem->setForeground(QColor("#00FF00"));
    rowItems << askOrdersItem;

    model->appendRow(rowItems);
}

void MarketDepthTable::clearData()
{
    m_displayMode = DisplayMode::NoData;
    updateDataSourceIndicator();

    model->removeRows(0, model->rowCount());
    spreadLabel->setText("SPREAD: --");
    bidDWPLabel->setText("--");
    askDWPLabel->setText("--");
}

void MarketDepthTable::setExpectedDataMode(bool p_hasLevel2, bool p_hasLevel1)
{
    if (p_hasLevel2)
    {
        m_displayMode = DisplayMode::Level2;
    }
    else if (p_hasLevel1)
    {
        m_displayMode = DisplayMode::Level1;
    }
    else
    {
        m_displayMode = DisplayMode::NoData;
    }
    updateDataSourceIndicator();
}

void MarketDepthTable::updateDataSourceIndicator()
{
    switch (m_displayMode)
    {
    case DisplayMode::Level2:
        m_dataSourceLabel->setText("L2");
        m_dataSourceLabel->setStyleSheet("QLabel { background-color: #006400; color: #00FF00; padding: 2px 4px; "
                                         "border-radius: 3px; font-weight: bold; font-size: 10px; }");
        break;
    case DisplayMode::Level1:
        m_dataSourceLabel->setText("L1");
        m_dataSourceLabel->setStyleSheet("QLabel { background-color: #8B8000; color: #FFFF00; padding: 2px 4px; "
                                         "border-radius: 3px; font-weight: bold; font-size: 10px; }");
        break;
    case DisplayMode::NoData:
        m_dataSourceLabel->setText("--");
        m_dataSourceLabel->setStyleSheet("QLabel { background-color: #3a3a3a; color: #808080; padding: 2px 4px; "
                                         "border-radius: 3px; font-weight: bold; font-size: 10px; }");
        break;
    }
}
