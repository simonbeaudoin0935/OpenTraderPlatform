#include "MarketDepthTableView.h"

MarketDepthTableView::MarketDepthTableView(QWidget* parent)
    : QTableView(parent)
{
}

void MarketDepthTableView::setTopMargin(int margin)
{
    Q_ASSERT(margin >= 0);
    setViewportMargins(0, margin, 0, 0);
} 