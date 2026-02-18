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
    tableView->setColumnWidth(1, 80); // Value

    // Add widgets to layout
    mainLayout->addWidget(headerLabel);
    mainLayout->addWidget(tableView);

    // Initialize rows with field names
    QStringList fieldNames;
    fieldNames << "Buying Power" << "Cash Balance" << "Equity" << "Market Value" << "Today's P/L" << "Commission"
               << "Uncl. Dep.";

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

    // Resize Field column to contents
    tableView->resizeColumnToContents(0);

    // Set fixed width based on total column widths
    int totalWidth = tableView->columnWidth(0) + tableView->columnWidth(1);
    setFixedWidth(totalWidth);
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

    // Buying Power
    model->item(row++, 1)->setText(QString("%1 $").arg(QLocale().toString(balance.getBuyingPower(), 'f', 2)));

    // Cash Balance
    model->item(row++, 1)->setText(QString("%1 $").arg(QLocale().toString(balance.getCashBalance(), 'f', 2)));

    // Equity
    auto equityItem = model->item(row++, 1);
    equityItem->setText(QString("%1 $").arg(QLocale().toString(balance.getEquity(), 'f', 2)));

    // Market Value
    auto marketValueItem = model->item(row++, 1);
    marketValueItem->setText(QString("%1 $").arg(QLocale().toString(balance.getMarketValue(), 'f', 2)));

    // Today's P/L
    double todaysPL = balance.getTodaysProfitLoss();
    auto plItem = model->item(row++, 1);
    plItem->setText(QString("%1 $").arg(QLocale().toString(todaysPL, 'f', 2)));
    plItem->setForeground(todaysPL >= 0 ? QColor(Qt::green) : QColor(Qt::red));

    // Commission
    model->item(row++, 1)->setText(QString("%1 $").arg(QLocale().toString(balance.getComission(), 'f', 2)));

    // Uncl. Dep.
    model->item(row++, 1)->setText(QString("%1 $").arg(QLocale().toString(balance.getUnclearedDeposit(), 'f', 2)));
}
