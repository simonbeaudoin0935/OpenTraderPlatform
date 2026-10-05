#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include <QMap>

#include "Position.h"

class QTableView;
class QLabel;
class QPushButton;
class QWidget;
class QToolButton;
class QMenu;
class QAction;

class PositionWidget : public QWidget
{
    Q_OBJECT
  public:
    explicit PositionWidget(QWidget* parent = nullptr);
    ~PositionWidget();

  public slots:
    /// Update or add a position to the display
    /// Creates a new row or updates existing row based on position ID
    /// @param account Account ID for the position
    /// @param position Position details (symbol, quantity, P&L, etc.)
    void updatePosition(const QString& account, const Position& position);

    /// Remove a position from the display
    /// @param account Account ID for the position
    /// @param positionID Unique position identifier
    void onPositionDeleted(const QString& account, const QString& positionID);

    /// Clear all positions from the table
    void clearAllPositions();

    void setReviewModeEnabled(bool p_enabled);

  signals:
    /// Emitted when user clicks on a symbol in the table
    /// Thread context: Emitted from Main/GUI thread
    /// @param symbol Stock symbol that was clicked
    void symbolClicked(const QString& symbol);

    /// Emitted when user requests to close all open positions visible in the widget's account scope.
    /// Thread context: Emitted from Main/GUI thread
    void closeAllPositionsRequested();

    /// Emitted when user requests passive close-all behavior (resting ask/bid-side limit).
    /// Thread context: Emitted from Main/GUI thread
    void closeAllPositionsPassiveRequested();

    /// Emitted when user requests to close a specific position row.
    /// Thread context: Emitted from Main/GUI thread
    /// @param positionId Unique position identifier for the selected row
    void closePositionRequested(const QString& positionId);

  private:
    void setupUI();
    void setupStyles();
    void rebuildTable();
    [[nodiscard]] bool shouldShowPosition(const Position& p_position) const;
    void onCurrentDayOnlyToggled(bool p_checked);
    QList<QStandardItem*> createRowItems(const Position& position);
    void onSymbolClicked(const QModelIndex& index);
    void onCustomContextMenuRequested(const QPoint& p_pos);

    QTableView* tableView;
    QStandardItemModel* model;
    QWidget* m_headerWidget;
    QLabel* headerLabel;
    QPushButton* m_closeAllPositionsButton;
    QPushButton* m_closeAllPositionsPassiveButton;
    QToolButton* m_settingsButton;
    QMenu* m_settingsMenu;
    QAction* m_currentDayOnlyAction;
    bool m_reviewModeEnabled = false;
    bool m_showCurrentDayOnly = false;
    QMap<QString, Position> m_positionsById;

    // Map to keep track of positions by their ID for updates
    QMap<QString, int> positionRowMap; // Maps positionID to row index
};
