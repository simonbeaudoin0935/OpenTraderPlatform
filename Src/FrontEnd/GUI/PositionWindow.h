#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include <QMap>

#include "Position.h"

class QTableView;
class QLabel;

class PositionWindow : public QWidget
{
    Q_OBJECT
  public:
    explicit PositionWindow(QWidget* parent = nullptr);
    ~PositionWindow();

  public slots:
    void updatePosition(const QString& account, const Position& position);
    void onPositionDeleted(const QString& account, const QString& positionID);

  signals:
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
