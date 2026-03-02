#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include <QMap>

#include "Position.h"

class QTableView;
class QLabel;

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

  signals:
    /// Emitted when user clicks on a symbol in the table
    /// @param symbol Stock symbol that was clicked
    void symbolClicked(const QString& symbol);

  private:
    void setupUI();
    void setupStyles();
    void updatePositionRow(const QString& account, const Position& position);
    QList<QStandardItem*> createRowItems(const Position& position);
    void onSymbolClicked(const QModelIndex& index);

    QTableView* tableView;
    QStandardItemModel* model;
    QLabel* headerLabel;

    // Map to keep track of positions by their ID for updates
    QMap<QString, int> positionRowMap; // Maps positionID to row index
};
