#pragma once

#include <QLabel>
#include <QProgressBar>
#include <QWidget>

#include "RiskTypes.h"

class RiskStatusWidget : public QWidget
{
    Q_OBJECT

  public:
    explicit RiskStatusWidget(QWidget* parent = nullptr);

    void setSnapshot(const RiskStatusSnapshot& p_snapshot);

  private:
    void applyStateColor(const QString& p_colorHex);

    QProgressBar* m_headroomBar = nullptr;
    QLabel* m_primaryLabel = nullptr;
    QLabel* m_secondaryLabel = nullptr;
};
