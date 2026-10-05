#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

#include "RiskTypes.h"

class RiskTab : public QWidget
{
    Q_OBJECT

  public:
    explicit RiskTab(QWidget* parent = nullptr);
    ~RiskTab() override = default;

    void setSelectedAccountId(const QString& p_accountId);
    void setRiskConfig(const RiskConfig& p_config);
    void setRiskStatus(const RiskStatusSnapshot& p_status);
    [[nodiscard]] RiskConfig currentConfig() const;

  signals:
    void riskConfigChanged(const QString& accountId);
    void resetRiskDayRequested(const QString& accountId);
    void unlockRiskRequested(const QString& accountId);

  private slots:
    void onAnyConfigChanged();

  private:
    void setupUi();
    void connectSignals();
    void setControlsEnabled(bool p_enabled);

    QString m_selectedAccountId;
    bool m_loadingUi = false;

    QLabel* m_accountLabel = nullptr;
    QCheckBox* m_enabledCheckBox = nullptr;
    QDoubleSpinBox* m_dailyDrawdownLimitSpinBox = nullptr;
    QComboBox* m_drawdownBasisComboBox = nullptr;
    QDoubleSpinBox* m_maxPlannedLossSpinBox = nullptr;
    QSpinBox* m_maxPositionSharesSpinBox = nullptr;
    QDoubleSpinBox* m_maxPositionNotionalSpinBox = nullptr;
    QSpinBox* m_maxDailyEntriesSpinBox = nullptr;
    QSpinBox* m_maxOpenPositionsSpinBox = nullptr;
    QCheckBox* m_cooldownEnabledCheckBox = nullptr;
    QDoubleSpinBox* m_cooldownLossTriggerSpinBox = nullptr;
    QSpinBox* m_cooldownDurationSpinBox = nullptr;
    QSpinBox* m_warningAmberSpinBox = nullptr;
    QSpinBox* m_warningRedSpinBox = nullptr;

    QLabel* m_runtimeSummaryLabel = nullptr;
    QLabel* m_runtimeDetailsLabel = nullptr;
    QPushButton* m_resetDayButton = nullptr;
    QPushButton* m_unlockButton = nullptr;
};
