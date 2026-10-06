#include "Level2Widget.h"
#include "Level2TableView.h"
#include "Level2.h"
#include <QHeaderView>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <QSet>
#include <QList>
#include <QVector>
#include <algorithm>
#include "CONSTANTS.h"
#include "LTTng/LTTngTracepoints.h"

namespace
{

    [[nodiscard]] bool isPopulatedLevel(const Level2Row& p_level)
    {
        return p_level.m_price > 0.0 && (p_level.m_size > 0 || p_level.m_orderCount > 0);
    }

} // namespace

Level2Widget::Level2Widget(QWidget* parent)
    : QWidget(parent)
    , tableView(new Level2TableView(this))
    , model(new QStandardItemModel(this))
    , bidLabel(new QLabel("BID", this))
    , askLabel(new QLabel("ASK", this))
    , spreadLabel(new QLabel("SPREAD", this))
    , m_dataSourceLabel(new QLabel("--", this))
{
    setupUI();
    setupStyles();
    updateDataSourceIndicator();
}

Level2Widget::~Level2Widget()
{
    // Qt will handle deletion of child widgets
}

void Level2Widget::setupUI()
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
    tableView->setTopMargin(48); // header(24) + spread(24)

    // BID side
    tableView->setColumnWidth(0, ordersWidth); // Orders
    tableView->setColumnWidth(1, sizeWidth);   // Size
    tableView->setColumnWidth(2, priceWidth);  // Price

    // ASK side
    tableView->setColumnWidth(3, priceWidth);  // Price
    tableView->setColumnWidth(4, sizeWidth);   // Size
    tableView->setColumnWidth(5, ordersWidth); // Orders

    // Set fixed width for the table view and widget
    tableView->setFixedWidth(totalWidth);
    setFixedWidth(totalWidth);

    // Apply an initial estimated height so layout is coherent at startup.
    // The precise height is corrected in the singleShot timer below once
    // the viewport geometry is known.
    const int rowHeight = tableView->verticalHeader()->defaultSectionSize();
    setFixedHeight(96 + 10 * rowHeight);
    tableView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Add widgets to main layout
    mainLayout->addWidget(headerWidget, 0, Qt::AlignTop);
    mainLayout->addWidget(spreadWidget, 0, Qt::AlignTop);
    mainLayout->addWidget(tableView, 1);

    // After the first layout pass the viewport geometry is known.
    // Re-snap the fixed height to exactly the top of the viewport + 10 rows,
    // eliminating any partial-row gap at the bottom caused by frame borders,
    // horizontal header height, or viewport margin interactions.
    QTimer::singleShot(0,
                       this,
                       [headerWidget, spreadWidget, this]()
                       {
                           int width = tableView->viewport()->width() + tableView->verticalHeader()->width();
                           headerWidget->setGeometry(0, 0, width, 24);
                           spreadWidget->setGeometry(0, 24, width, 24);

                           // viewport()->pos() is relative to tableView; map to Level2Widget
                           int viewportTop = tableView->y() + tableView->viewport()->y();
                           int rowH = tableView->verticalHeader()->sectionSize(0);
                           if (rowH <= 0)
                               rowH = tableView->verticalHeader()->defaultSectionSize();
                           setFixedHeight(viewportTop + 10 * rowH);
                       });

    // Set up a connection to handle header widget resizing
    connect(tableView->horizontalHeader(),
            &QHeaderView::geometriesChanged,
            [headerWidget, spreadWidget, this]()
            {
                int width = tableView->viewport()->width() + tableView->verticalHeader()->width();
                headerWidget->setGeometry(0, 0, width, 24);
                spreadWidget->setGeometry(0, 24, width, 24);
            });
}

void Level2Widget::setupStyles()
{
    const QString commonStyle = QStringLiteral("QLabel {"
                                               "   background-color: %1;"
                                               "   padding: 4px;"
                                               "   border-bottom: 1px solid %2;"
                                               "}")
                                    .arg(QString::fromLatin1(GUIThemeConstants::SIDEBAR_BACKGROUND))
                                    .arg(QString::fromLatin1(GUIThemeConstants::BORDER));

    bidLabel->setStyleSheet(commonStyle + "QLabel {"
                                          "   color: #00FF00;"
                                          "}");

    askLabel->setStyleSheet(commonStyle + "QLabel {"
                                          "   color: #FF0000;"
                                          "}");

    spreadLabel->setStyleSheet(commonStyle + QStringLiteral("QLabel {"
                                                            "   color: %1;"
                                                            "   border-top: 1px solid %2;"
                                                            "}")
                                                 .arg(QString::fromLatin1(GUIThemeConstants::TEXT_PRIMARY))
                                                 .arg(QString::fromLatin1(GUIThemeConstants::BORDER)));

    tableView->setStyleSheet("QHeaderView::section {"
                             "   border-top: none;"
                             "}"
                             "QHeaderView::section:first {"
                             "   border-top: 1px solid #00FF00;"
                             "}"
                             "QHeaderView::section:last {"
                             "   border-top: 1px solid #FF0000;"
                             "}");
}

void Level2Widget::setMarketDepthItem(QStandardItem* item, const Level2Row& level, const QString& field, int priceLevel)
{
    if (!isPopulatedLevel(level))
    {
        item->setText({});
        item->setForeground(QColor("#808080"));
        return;
    }

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

void Level2Widget::updateData(const std::array<Level2Row, 10>& bids, const std::array<Level2Row, 10>& asks)
{
    LTTnG_TP(opentraderplatform, gui_level2_widget_update);

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
    QVector<Level2Row> displayedBids;
    QVector<Level2Row> displayedAsks;
    for (const auto& bid: bids)
    {
        if (!isPopulatedLevel(bid))
            continue;

        displayedBids.append(bid);
        bidPrices.insert(bid.m_price);
    }
    for (const auto& ask: asks)
    {
        if (!isPopulatedLevel(ask))
            continue;

        displayedAsks.append(ask);
        askPrices.insert(ask.m_price);
    }

    // Sort price levels (descending for bids, ascending for asks)
    QList<double> sortedBidPrices = bidPrices.values();
    QList<double> sortedAskPrices = askPrices.values();
    std::sort(sortedBidPrices.begin(), sortedBidPrices.end(), std::greater<double>());
    std::sort(sortedAskPrices.begin(), sortedAskPrices.end());

    const int rowCount = std::max(displayedBids.size(), displayedAsks.size());
    for (int i = 0; i < rowCount; ++i)
    {
        QList<QStandardItem*> rowItems;

        const Level2Row bid = i < displayedBids.size() ? displayedBids[i] : Level2Row{};
        double bidPrice = bid.m_price;
        int bidPriceLevel = sortedBidPrices.indexOf(bidPrice);

        rowItems << new QStandardItem();
        setMarketDepthItem(rowItems.last(), bid, "Orders", bidPriceLevel);
        rowItems << new QStandardItem();
        setMarketDepthItem(rowItems.last(), bid, "Size", bidPriceLevel);
        rowItems << new QStandardItem();
        setMarketDepthItem(rowItems.last(), bid, "Price", bidPriceLevel);

        const Level2Row ask = i < displayedAsks.size() ? displayedAsks[i] : Level2Row{};
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

void Level2Widget::clearData()
{
    m_displayMode = DisplayMode::NoData;
    updateDataSourceIndicator();

    model->removeRows(0, model->rowCount());
    spreadLabel->setText("SPREAD: --");
}

void Level2Widget::setExpectedDataMode(bool p_hasLevel2)
{
    if (p_hasLevel2)
    {
        m_displayMode = DisplayMode::Level2;
    }
    else
    {
        m_displayMode = DisplayMode::NoData;
    }
    updateDataSourceIndicator();
}

void Level2Widget::updateDataSourceIndicator()
{
    switch (m_displayMode)
    {
    case DisplayMode::Level2:
        m_dataSourceLabel->setText("L2");
        m_dataSourceLabel->setStyleSheet("QLabel { background-color: #006400; color: #00FF00; padding: 2px 4px; "
                                         "border-radius: 3px; font-weight: bold; font-size: 10px; }");
        break;
    case DisplayMode::NoData:
        m_dataSourceLabel->setText("--");
        m_dataSourceLabel->setStyleSheet("QLabel { background-color: #3a3a3a; color: #808080; padding: 2px 4px; "
                                         "border-radius: 3px; font-weight: bold; font-size: 10px; }");
        break;
    }
}
