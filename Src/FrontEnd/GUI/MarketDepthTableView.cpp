#include "MarketDepthTableView.h"

MarketDepthTableView::MarketDepthTableView(QWidget* parent) : QTableView(parent) {}

void MarketDepthTableView::setTopMargin(int margin)
{
    setViewportMargins(0, margin, 0, 0);
}