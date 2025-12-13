#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include <QMap>

#include "Order.h"

class QTableView;
class QLabel;

class OrderWindow : public QWidget {
    Q_OBJECT
public:
    explicit OrderWindow(QWidget* p_parent = nullptr);
    ~OrderWindow();

public slots:
    void updateOrder(const QString& account, const Order& order);

signals:
    void symbolClicked(const QString& symbol);
    void cancelOrderRequested(const QString& orderId);

private:
    void setupUI();
    void setupStyles();
    void updateOrderRow(const QString& account, const Order& order);
    QList<QStandardItem*> createRowItems(const Order& order);
    void onSymbolClicked(const QModelIndex& index);

    QTableView* m_tableView;
    QStandardItemModel* m_model;
    QLabel* m_headerLabel;

    // Map to keep track of orders by their ID for updates
    QMap<QString, int> m_orderRowMap;  // Maps orderID to row index
};
