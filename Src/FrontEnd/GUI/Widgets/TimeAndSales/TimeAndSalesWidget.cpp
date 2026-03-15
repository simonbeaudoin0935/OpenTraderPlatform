#include "TimeAndSalesWidget.h"

#include "Assume.h"
#include "LTTng/LTTngTracepoints.h"

TimeAndSalesWidget::TimeAndSalesWidget(QWidget* p_parent) : QWidget(p_parent), m_headerLabel(nullptr), m_table(nullptr)
{
    setupUI();
    setupStyles();
}

void TimeAndSalesWidget::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_headerLabel = new QLabel("Time && Sales");
    Q_CHECK_PTR(m_headerLabel);
    m_headerLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_headerLabel);

    m_table = new QTableWidget(this);
    Q_CHECK_PTR(m_table);
    m_table->setColumnCount(NUM_COLS);
    m_table->setHorizontalHeaderLabels({"Time", "Price", "Size"});
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(18);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setShowGrid(false);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    layout->addWidget(m_table);
}

void TimeAndSalesWidget::setupStyles()
{
    m_headerLabel->setStyleSheet("QLabel {"
                                 "   background-color: #2D2D2D;"
                                 "   color: white;"
                                 "   font-weight: bold;"
                                 "   padding: 4px;"
                                 "   border-bottom: 1px solid #3D3D3D;"
                                 "}");

    m_table->setStyleSheet("QTableWidget {"
                           "   background-color: #242424;"
                           "   alternate-background-color: #1C1C1C;"
                           "   color: white;"
                           "   border: none;"
                           "   font-size: 11px;"
                           "   gridline-color: #3D3D3D;"
                           "}"
                           "QHeaderView::section {"
                           "   background-color: #2D2D2D;"
                           "   color: #a0a0a0;"
                           "   border: none;"
                           "   border-right: 1px solid #3D3D3D;"
                           "   font-size: 10px;"
                           "   padding: 2px;"
                           "}");
    m_table->setAlternatingRowColors(true);
}

void TimeAndSalesWidget::onNewTrade(const QString& p_symbol, const Trade& p_trade)
{
    L2T_TP(l2trader, gui_timesales_widget_update);

    OBJ_ASSUME_EQUAL(p_symbol, p_trade.m_symbol);

    // Insert at top (newest first)
    m_table->insertRow(0);

    // Time column
    auto* timeItem = new QTableWidgetItem(p_trade.m_timestamp.time().toString("hh:mm:ss"));
    timeItem->setTextAlignment(Qt::AlignCenter);

    // Price column
    auto* priceItem = new QTableWidgetItem(QString::number(p_trade.m_price, 'f', 2));
    priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Size column
    auto* sizeItem = new QTableWidgetItem(QString::number(p_trade.m_size));
    sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Color by trade side: green = buy aggressor (hit ask), red = sell aggressor (hit bid)
    QColor textColor;
    switch (p_trade.m_side)
    {
    case TradeSide::Ask:
        textColor = QColor(0, 200, 83); // Green — buyer aggressor
        break;
    case TradeSide::Bid:
        textColor = QColor(255, 82, 82); // Red — seller aggressor
        break;
    case TradeSide::None:
        textColor = QColor(180, 180, 180); // Gray — unknown
        break;
    }

    timeItem->setForeground(textColor);
    priceItem->setForeground(textColor);
    sizeItem->setForeground(textColor);

    m_table->setItem(0, COL_TIME, timeItem);
    m_table->setItem(0, COL_PRICE, priceItem);
    m_table->setItem(0, COL_SIZE, sizeItem);

    // Trim old rows to cap memory
    while (m_table->rowCount() > m_maxRows)
    {
        m_table->removeRow(m_table->rowCount() - 1);
    }
}

void TimeAndSalesWidget::clearData()
{
    m_table->setRowCount(0);
}

void TimeAndSalesWidget::setMaxRows(int p_maxRows)
{
    m_maxRows = p_maxRows;

    // Trim existing rows if needed
    while (m_table->rowCount() > m_maxRows)
    {
        m_table->removeRow(m_table->rowCount() - 1);
    }
}
