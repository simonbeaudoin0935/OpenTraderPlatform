#include "Level2TableView.h"

Level2TableView::Level2TableView(QWidget* parent) : QTableView(parent) {}

void Level2TableView::setTopMargin(int margin)
{
    setViewportMargins(0, margin, 0, 0);
}