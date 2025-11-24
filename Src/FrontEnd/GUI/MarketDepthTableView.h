#pragma once

#include <QTableView>

class MarketDepthTableView : public QTableView {
    Q_OBJECT
public:
    explicit MarketDepthTableView(QWidget* parent = nullptr);
    void setTopMargin(int margin);
}; 