#pragma once

#include <QTableView>

class Level2TableView : public QTableView
{
    Q_OBJECT
  public:
    explicit Level2TableView(QWidget* parent = nullptr);

    /// Set top margin for the table view
    /// Used to adjust spacing from header
    /// @param margin Top margin in pixels
    void setTopMargin(int margin);
};