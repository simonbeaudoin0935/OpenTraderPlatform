#include "PositionWidget.h"
#include <QHBoxLayout>
#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include "Assume.h"
#include "LTTng/LTTngTracepoints.h"

PositionWidget::PositionWidget(QWidget* parent)
    : QWidget(parent)
    , tableView(new QTableView(this))
    , model(new QStandardItemModel(this))
    , m_headerWidget(new QWidget(this))
    , headerLabel(new QLabel("POSITIONS", this))
    , m_closeAllPositionsButton(new QPushButton("Close All", this))
{
    setupUI();
    setupStyles();
}

PositionWidget::~PositionWidget()
{
    // Qt will handle deletion of child widgets
}

void PositionWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Setup header
    m_headerWidget->setFixedHeight(24);
    headerLabel->setAlignment(Qt::AlignCenter);
    m_closeAllPositionsButton->setFixedHeight(20);
    m_closeAllPositionsButton->setCursor(Qt::PointingHandCursor);
    m_closeAllPositionsButton->setToolTip("Close all open positions for the selected account");

    QHBoxLayout* headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(6, 0, 6, 0);
    headerLayout->setSpacing(6);
    headerLayout->addStretch();
    headerLayout->addWidget(headerLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_closeAllPositionsButton);

    auto closeAllConnection = connect(m_closeAllPositionsButton,
                                      &QPushButton::clicked,
                                      this,
                                      &PositionWidget::closeAllPositionsRequested,
                                      Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(closeAllConnection);

    // Setup model columns (Position ID at END like OrderWidget)
    QStringList headers;
    headers << "Symbol" << "Quantity" << "Avg Price" << "Last" << "Unrealized P/L" << "Realized P/L" << "Market Value"
            << "Position ID";
    model->setHorizontalHeaderLabels(headers);

    // Configure table view
    tableView->setModel(model);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableView->verticalHeader()->setVisible(false);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->setAlternatingRowColors(true);
    tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tableView->setContextMenuPolicy(Qt::CustomContextMenu);

    // Connect click signal
    auto symbolClickConnection =
        connect(tableView, &QTableView::clicked, this, &PositionWidget::onSymbolClicked, Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(symbolClickConnection);
    auto contextMenuConnection = connect(tableView,
                                         &QTableView::customContextMenuRequested,
                                         this,
                                         &PositionWidget::onCustomContextMenuRequested,
                                         Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(contextMenuConnection);

    // Set column widths
    tableView->setColumnWidth(0, 70); // Symbol
    tableView->setColumnWidth(1, 70); // Quantity
    tableView->setColumnWidth(2, 70); // Avg Price
    tableView->setColumnWidth(3, 70); // Last
    tableView->setColumnWidth(4, 90); // Unrealized P/L
    tableView->setColumnWidth(5, 90); // Realized P/L
    tableView->setColumnWidth(6, 90); // Market Value
    tableView->setColumnWidth(7, 90); // Position ID

    // Add widgets to layout
    mainLayout->addWidget(m_headerWidget);
    mainLayout->addWidget(tableView);

    // Set fixed width based on total column widths
    int totalWidth = 0;
    for (int i = 0; i < headers.size(); ++i)
    {
        totalWidth += tableView->columnWidth(i);
    }
    setFixedWidth(totalWidth);
}

void PositionWidget::setupStyles()
{
    m_headerWidget->setStyleSheet("QWidget {"
                                  "   background-color: #2D2D2D;"
                                  "   border-bottom: 1px solid #3D3D3D;"
                                  "}");
    headerLabel->setStyleSheet("QLabel { color: #FFFFFF; background: transparent; }");
    m_closeAllPositionsButton->setStyleSheet("QPushButton {"
                                             "   background-color: #7A1F1F;"
                                             "   color: #FFFFFF;"
                                             "   border: 1px solid #A63A3A;"
                                             "   border-radius: 3px;"
                                             "   padding: 0 8px;"
                                             "}"
                                             "QPushButton:hover {"
                                             "   background-color: #9B2C2C;"
                                             "}"
                                             "QPushButton:pressed {"
                                             "   background-color: #5F1919;"
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
                             "}");
}

void PositionWidget::setReviewModeEnabled(const bool p_enabled)
{
    m_reviewModeEnabled = p_enabled;
    m_closeAllPositionsButton->setEnabled(!p_enabled);
    m_closeAllPositionsButton->setText(p_enabled ? "Read-Only" : "Close All");
}

void PositionWidget::updatePosition(const QString& account, const Position& position)
{
    L2T_TP(l2trader, gui_position_widget_update);

    QString positionId = position.getPositionID();

    if (positionRowMap.contains(positionId))
    {
        // Update existing position
        updatePositionRow(account, position);
    }
    else
    {
        // Add new position - most recent at the top (row 0)
        QList<QStandardItem*> rowItems = createRowItems(position);
        // Shift all existing row indices down to make room at row 0
        for (auto& rowIdx: positionRowMap)
            rowIdx++;
        model->insertRow(0, rowItems);
        positionRowMap[positionId] = 0;
    }
}

void PositionWidget::updatePositionRow(const QString& account, const Position& position)
{
    Q_UNUSED(account);

    int row = positionRowMap[position.getPositionID()];
    QList<QStandardItem*> items = createRowItems(position);

    for (int col = 0; col < items.size(); ++col)
    {
        model->setItem(row, col, items[col]);
    }
}

QList<QStandardItem*> PositionWidget::createRowItems(const Position& position)
{
    QList<QStandardItem*> items;

    // Symbol
    auto symbolItem = new QStandardItem(position.getSymbol());
    symbolItem->setTextAlignment(Qt::AlignCenter);
    items << symbolItem;

    // Quantity
    auto quantityItem = new QStandardItem(position.getQuantity());
    quantityItem->setTextAlignment(Qt::AlignCenter);
    items << quantityItem;

    // Average Price
    auto avgPriceItem = new QStandardItem(QString::number(position.getAveragePrice().toDouble(), 'f', 2));
    avgPriceItem->setTextAlignment(Qt::AlignCenter);
    items << avgPriceItem;

    // Last Price
    auto lastItem = new QStandardItem(QString::number(position.getLast().toDouble(), 'f', 2));
    lastItem->setTextAlignment(Qt::AlignCenter);
    items << lastItem;

    // Unrealized P/L (only for open positions)
    int qty = position.getQuantity().toInt();
    double unrealizedPL = position.getUnrealizedProfitLoss().toDouble();
    auto unrealizedItem = new QStandardItem();
    unrealizedItem->setTextAlignment(Qt::AlignCenter);
    if (qty != 0)
    {
        unrealizedItem->setText(QString::number(unrealizedPL, 'f', 2));
        unrealizedItem->setForeground(unrealizedPL >= 0 ? QColor(Qt::green) : QColor(Qt::red));
    }
    // else: leave empty for closed positions
    items << unrealizedItem;

    // Realized P/L (only for closed positions, uses TodaysProfitLoss field)
    double realizedPL = position.getTodaysProfitLoss().toDouble();
    auto realizedItem = new QStandardItem();
    realizedItem->setTextAlignment(Qt::AlignCenter);
    if (qty == 0)
    {
        QString text = QString::number(realizedPL, 'f', 2);
        realizedItem->setText(text);
        realizedItem->setForeground(realizedPL >= 0 ? QColor(Qt::green) : QColor(Qt::red));
    }
    // else: leave empty for open positions
    items << realizedItem;

    // Market Value
    auto marketValueItem = new QStandardItem(QString::number(position.getMarketValue().toDouble(), 'f', 2));
    marketValueItem->setTextAlignment(Qt::AlignCenter);
    items << marketValueItem;

    // Position ID (at end, like OrderWidget)
    auto positionIDItem = new QStandardItem(position.getPositionID());
    positionIDItem->setTextAlignment(Qt::AlignCenter);
    items << positionIDItem;

    return items;
}

void PositionWidget::onPositionDeleted(const QString& account, const QString& positionID)
{
    Q_UNUSED(account);

    if (positionRowMap.contains(positionID))
    {
        int row = positionRowMap[positionID];
        // Set quantity to 0 instead of removing the row
        auto quantityItem = new QStandardItem("0");
        quantityItem->setTextAlignment(Qt::AlignCenter);
        model->setItem(row, 1,
                       quantityItem); // Column 1 is Quantity
    }
}

void PositionWidget::onSymbolClicked(const QModelIndex& index)
{
    if (index.column() == 0)
    { // Only handle clicks on the Symbol column
        QString symbol = model->item(index.row(), 0)->text();
        emit symbolClicked(symbol);
    }
}

void PositionWidget::onCustomContextMenuRequested(const QPoint& p_pos)
{
    if (m_reviewModeEnabled)
    {
        return;
    }

    const QModelIndex index = tableView->indexAt(p_pos);
    if (!index.isValid())
    {
        return;
    }

    QStandardItem* const quantityItem = model->item(index.row(), 1);
    QStandardItem* const positionIDItem = model->item(index.row(), 7);
    if (quantityItem == nullptr || positionIDItem == nullptr)
    {
        return;
    }

    bool quantityOk = false;
    const double quantity = quantityItem->text().toDouble(&quantityOk);
    if (!quantityOk || qFuzzyCompare(1.0 + qAbs(quantity), 1.0))
    {
        return;
    }

    const QString positionID = positionIDItem->text().trimmed();
    if (positionID.isEmpty())
    {
        return;
    }

    QMenu menu(this);
    QAction* const closeAction = menu.addAction("Close Position");
    Q_CHECK_PTR(closeAction);

    if (menu.exec(tableView->viewport()->mapToGlobal(p_pos)) == closeAction)
    {
        emit closePositionRequested(positionID);
    }
}


void PositionWidget::clearAllPositions()
{
    model->removeRows(0, model->rowCount());
    positionRowMap.clear();
    qDebug() << "PositionWidget cleared all positions";
}
