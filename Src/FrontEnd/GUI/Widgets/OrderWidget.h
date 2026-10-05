#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include <QMap>
#include <QHash>

#include "Order.h"

class QTableView;
class QLabel;
class QPushButton;
class QWidget;
class QToolButton;
class QMenu;
class QAction;

class OrderWidget : public QWidget
{
    Q_OBJECT
  public:
    explicit OrderWidget(QWidget* p_parent = nullptr);
    ~OrderWidget();

  public slots:
    /// Update or add an order to the display
    /// Creates a new row or updates existing row based on order ID
    /// @param account Account ID for the order
    /// @param order Order details (status, price, quantity, etc.)
    void updateOrder(const QString& account, const Order& order);

    /// Clear all orders from the table
    void clearAllOrders();

    void setReviewModeEnabled(bool p_enabled);

  public:
    /// Get list of all order IDs currently displayed
    /// @return List of order ID strings
    QStringList getAllOrderIds() const;

    /// Get list of order IDs that can be cancelled
    /// Filters out already-filled or rejected orders
    /// @return List of cancellable order ID strings
    QStringList getCancellableOrderIds() const;

  signals:
    /// Emitted when user clicks on a symbol in the table
    /// @param symbol Stock symbol that was clicked
    void symbolClicked(const QString& symbol);

    /// Emitted when user requests to cancel a specific order
    /// @param orderId ID of order to cancel
    void cancelOrderRequested(const QString& orderId);

    /// Emitted when user requests to cancel all orders
    void cancelAllOrdersRequested();

  private:
    void setupUI();
    void setupStyles();
    void rebuildTable();
    [[nodiscard]] bool shouldShowOrder(const Order& p_order) const;
    void onCurrentDayOnlyToggled(bool p_checked);
    QList<QStandardItem*> createRowItems(const Order& order);
    void onSymbolClicked(const QModelIndex& index);
    void onCustomContextMenuRequested(const QPoint& p_pos);

    QTableView* m_tableView;
    QStandardItemModel* m_model;
    QWidget* m_headerWidget;
    QLabel* m_headerLabel;
    QPushButton* m_cancelAllOrdersButton;
    QToolButton* m_settingsButton;
    QMenu* m_settingsMenu;
    QAction* m_currentDayOnlyAction;
    bool m_reviewModeEnabled = false;
    bool m_showCurrentDayOnly = false;

    // Map to keep track of orders by their ID for updates
    QMap<QString, int> m_orderRowMap; // Maps orderID to row index
    QHash<QString, Order>
        m_orders; // Store actual Order objects to check status (QHash used because Order lacks default constructor)
};
