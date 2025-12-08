#pragma once

#include <QWidget>
#include <QStandardItemModel>

#include "Balance.h"

class QTableView;
class QLabel;

class BalanceWindow : public QWidget {
    Q_OBJECT
public:
    explicit BalanceWindow(QWidget* parent = nullptr);
    ~BalanceWindow();

public slots:
    void updateBalance(const Balance& balance);

private:
    void setupUI();
    void setupStyles();
    void updateBalanceData(const Balance& balance);

    QTableView* tableView;
    QStandardItemModel* model;
    QLabel* headerLabel;
}; 
