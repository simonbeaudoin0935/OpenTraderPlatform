#include "BalanceWindow.h"
#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLabel>

BalanceWindow::BalanceWindow(QWidget* parent)
    : QWidget(parent)
    , tableView(new QTableView(this))
    , model(new QStandardItemModel(this))
    , headerLabel(new QLabel("BALANCE", this))
{
    setupUI();
    setupStyles();
}

BalanceWindow::~BalanceWindow()
{
    // Qt will handle deletion of child widgets
}

void BalanceWindow::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Setup header
    headerLabel->setFixedHeight(24);
    headerLabel->setAlignment(Qt::AlignCenter);

    // Setup model with 2 columns: Label and Value
    QStringList headers;
    headers << "Field" << "Value";
    model->setHorizontalHeaderLabels(headers);

    // Configure table view
    tableView->setModel(model);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableView->verticalHeader()->setVisible(false);
    tableView->setSelectionMode(QAbstractItemView::NoSelection);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->setAlternatingRowColors(true);
    tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Set column widths
    tableView->setColumnWidth(0, 120); // Field
    tableView->setColumnWidth(1, 100); // Value

    // Add widgets to layout
    mainLayout->addWidget(headerLabel);
    mainLayout->addWidget(tableView);

    // Set fixed width based on total column widths
    int totalWidth = 0;
    for (int i = 0; i < headers.size(); ++i)
    {
        totalWidth += tableView->columnWidth(i);
    }
    setFixedWidth(totalWidth);

    // Initialize rows with field names
    QStringList fieldNames;
    fieldNames << "Account ID" << "Buying Power" << "Cash Balance" << "Equity" << "Market Value" << "Today's P/L"
               << "Commission" << "Uncleared Deposit";

    for (const QString& fieldName: fieldNames)
    {
        QList<QStandardItem*> rowItems;

        auto labelItem = new QStandardItem(fieldName);
        labelItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        rowItems << labelItem;

        auto valueItem = new QStandardItem("--");
        valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rowItems << valueItem;

        model->appendRow(rowItems);
    }
}

void BalanceWindow::setupStyles()
{
    // Style the header label
    headerLabel->setStyleSheet("QLabel {"
                               "   background-color: #2D2D2D;"
                               "   color: #FFFFFF;"
                               "   padding: 4px;"
                               "   border-bottom: 1px solid #3D3D3D;"
                               "}");

    // Style the table
    tableView->setStyleSheet("QTableView {"
                             "   alternate-background-color: #1C1C1C;"
                             "   background-color: #242424;"
                             "   color: white;"
                             "   gridline-color: #3D3D3D;"
                             "}"
                             "QHeaderView::section {"
                             "   background-color: #2D2D2D;"
                             "   color: white;"
                             "   border: none;"
                             "   border-right: 1px solid #3D3D3D;"
                             "   padding: 4px;"
                             "}");
}

void BalanceWindow::updateBalance(const Balance& balance)
{
    updateBalanceData(balance);
}

void BalanceWindow::updateBalanceData(const Balance& balance)
{
    // Row indices match the order in setupUI
    int row = 0;

    // Account ID
    model->item(row++, 1)->setText(balance.getAccountID());

    // Buying Power
    model->item(row++, 1)->setText(QString("$%1").arg(balance.getBuyingPower(), 0, 'f', 2));

    // Cash Balance
    model->item(row++, 1)->setText(QString("$%1").arg(balance.getCashBalance(), 0, 'f', 2));

    // Equity
    auto equityItem = model->item(row++, 1);
    equityItem->setText(QString("$%1").arg(balance.getEquity(), 0, 'f', 2));

    // Market Value
    auto marketValueItem = model->item(row++, 1);
    marketValueItem->setText(QString("$%1").arg(balance.getMarketValue(), 0, 'f', 2));

    // Today's P/L
    double todaysPL = balance.getTodaysProfitLoss();
    auto plItem = model->item(row++, 1);
    plItem->setText(QString("$%1").arg(todaysPL, 0, 'f', 2));
    plItem->setForeground(todaysPL >= 0 ? QColor(Qt::green) : QColor(Qt::red));

    // Commission
    model->item(row++, 1)->setText(QString("$%1").arg(balance.getComission(), 0, 'f', 2));

    // Uncleared Deposit
    model->item(row++, 1)->setText(QString("$%1").arg(balance.getUnclearedDeposit(), 0, 'f', 2));
}
