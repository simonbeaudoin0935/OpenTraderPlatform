#include "RiskTab.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "Misc/CONSTANTS.h"
#include "Misc/Logging/Logging.h"

RiskTab::RiskTab(QWidget* parent) : QWidget(parent)
{
    setupUi();
    connectSignals();
    setControlsEnabled(false);
}

void RiskTab::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    m_accountLabel = new QLabel("Account: (none)", this);
    m_accountLabel->setStyleSheet("QLabel { color: #cfd8e3; font-weight: bold; }");
    mainLayout->addWidget(m_accountLabel);

    auto* limitsGroup = new QGroupBox("Risk Limits", this);
    auto* limitsForm = new QFormLayout(limitsGroup);

    m_enabledCheckBox = new QCheckBox("Enable risk enforcement", limitsGroup);
    m_enabledCheckBox->setChecked(RiskManagementConstants::DEFAULT_ENABLED);
    limitsForm->addRow("Engine:", m_enabledCheckBox);

    m_dailyDrawdownLimitSpinBox = new QDoubleSpinBox(limitsGroup);
    m_dailyDrawdownLimitSpinBox->setDecimals(2);
    m_dailyDrawdownLimitSpinBox->setRange(RiskManagementConstants::MIN_DAILY_DRAWDOWN_LIMIT_USD,
                                          RiskManagementConstants::MAX_DAILY_DRAWDOWN_LIMIT_USD);
    m_dailyDrawdownLimitSpinBox->setSingleStep(RiskManagementConstants::DAILY_DRAWDOWN_LIMIT_STEP_USD);
    m_dailyDrawdownLimitSpinBox->setSuffix(" $");
    limitsForm->addRow("Daily Max Drawdown:", m_dailyDrawdownLimitSpinBox);

    m_drawdownBasisComboBox = new QComboBox(limitsGroup);
    m_drawdownBasisComboBox->addItem("Equity Peak Drawdown", static_cast<int>(RiskDrawdownBasis::EquityPeak));
    m_drawdownBasisComboBox->addItem("Today's PnL Drawdown", static_cast<int>(RiskDrawdownBasis::TodaysProfitLoss));
    m_drawdownBasisComboBox->addItem("Realized PnL Drawdown", static_cast<int>(RiskDrawdownBasis::RealizedProfitLoss));
    m_drawdownBasisComboBox->addItem("Today's PnL Loss From Baseline (Non-trailing)",
                                     static_cast<int>(RiskDrawdownBasis::TodaysProfitLossFromBaseline));
    constexpr int equityPeakIndex = 0;
    constexpr int todaysPnlIndex = 1;
    constexpr int realizedPnlIndex = 2;
    constexpr int todaysPnlBaselineIndex = 3;
    m_drawdownBasisComboBox->setItemData(equityPeakIndex,
                                         QStringLiteral("Trailing equity retracement.\n"
                                                        "Drawdown = max(0, session equity peak - current equity).\n"
                                                        "Remaining budget = daily limit - drawdown."),
                                         Qt::ToolTipRole);
    m_drawdownBasisComboBox->setItemData(todaysPnlIndex,
                                         QStringLiteral("Trailing intraday PnL retracement (realized + unrealized).\n"
                                                        "Drawdown = max(0, peak Today's PnL - current Today's PnL).\n"
                                                        "A pullback from a green peak still consumes drawdown budget."),
                                         Qt::ToolTipRole);
    m_drawdownBasisComboBox->setItemData(realizedPnlIndex,
                                         QStringLiteral("Trailing realized-PnL retracement (closed trades only).\n"
                                                        "Drawdown = max(0, peak realized PnL - current realized PnL).\n"
                                                        "Unrealized swings do not affect this mode."),
                                         Qt::ToolTipRole);
    m_drawdownBasisComboBox->setItemData(
        todaysPnlBaselineIndex,
        QStringLiteral("Non-trailing net-loss mode on Today's PnL.\n"
                       "Drawdown = max(0, baseline Today's PnL - current Today's PnL).\n"
                       "Pullbacks from green peaks do not consume budget unless PnL drops "
                       "below baseline."),
        Qt::ToolTipRole);
    m_drawdownBasisComboBox->setToolTip(QStringLiteral(
        "Each basis defines how drawdown budget is consumed.\nOpen the dropdown to see the formula for each mode."));
    limitsForm->addRow("Drawdown Basis:", m_drawdownBasisComboBox);

    m_maxPlannedLossSpinBox = new QDoubleSpinBox(limitsGroup);
    m_maxPlannedLossSpinBox->setDecimals(2);
    m_maxPlannedLossSpinBox->setRange(RiskManagementConstants::MIN_MAX_PLANNED_LOSS_PER_TRADE_USD,
                                      RiskManagementConstants::MAX_MAX_PLANNED_LOSS_PER_TRADE_USD);
    m_maxPlannedLossSpinBox->setSingleStep(RiskManagementConstants::MAX_PLANNED_LOSS_PER_TRADE_STEP_USD);
    m_maxPlannedLossSpinBox->setSuffix(" $");
    limitsForm->addRow("Max Planned Loss / Trade:", m_maxPlannedLossSpinBox);

    m_maxPositionSharesSpinBox = new QSpinBox(limitsGroup);
    m_maxPositionSharesSpinBox->setRange(RiskManagementConstants::MIN_MAX_POSITION_SHARES,
                                         RiskManagementConstants::MAX_MAX_POSITION_SHARES);
    m_maxPositionSharesSpinBox->setSingleStep(RiskManagementConstants::MAX_POSITION_SHARES_STEP);
    limitsForm->addRow("Max Position Shares:", m_maxPositionSharesSpinBox);

    m_maxPositionNotionalSpinBox = new QDoubleSpinBox(limitsGroup);
    m_maxPositionNotionalSpinBox->setDecimals(2);
    m_maxPositionNotionalSpinBox->setRange(RiskManagementConstants::MIN_MAX_POSITION_NOTIONAL_USD,
                                           RiskManagementConstants::MAX_MAX_POSITION_NOTIONAL_USD);
    m_maxPositionNotionalSpinBox->setSingleStep(RiskManagementConstants::MAX_POSITION_NOTIONAL_STEP_USD);
    m_maxPositionNotionalSpinBox->setSuffix(" $");
    limitsForm->addRow("Max Position Notional:", m_maxPositionNotionalSpinBox);

    m_maxDailyEntriesSpinBox = new QSpinBox(limitsGroup);
    m_maxDailyEntriesSpinBox->setRange(RiskManagementConstants::MIN_MAX_DAILY_ENTRY_TRADES,
                                       RiskManagementConstants::MAX_MAX_DAILY_ENTRY_TRADES);
    limitsForm->addRow("Max Daily Entry Trades:", m_maxDailyEntriesSpinBox);

    m_maxOpenPositionsSpinBox = new QSpinBox(limitsGroup);
    m_maxOpenPositionsSpinBox->setRange(RiskManagementConstants::MIN_MAX_OPEN_POSITIONS,
                                        RiskManagementConstants::MAX_MAX_OPEN_POSITIONS);
    limitsForm->addRow("Max Open Positions:", m_maxOpenPositionsSpinBox);

    m_cooldownEnabledCheckBox = new QCheckBox("Enable cooldown", limitsGroup);
    limitsForm->addRow("Cooldown:", m_cooldownEnabledCheckBox);

    m_cooldownLossTriggerSpinBox = new QDoubleSpinBox(limitsGroup);
    m_cooldownLossTriggerSpinBox->setDecimals(2);
    m_cooldownLossTriggerSpinBox->setRange(RiskManagementConstants::MIN_COOLDOWN_LOSS_TRIGGER_USD,
                                           RiskManagementConstants::MAX_COOLDOWN_LOSS_TRIGGER_USD);
    m_cooldownLossTriggerSpinBox->setSingleStep(RiskManagementConstants::COOLDOWN_LOSS_TRIGGER_STEP_USD);
    m_cooldownLossTriggerSpinBox->setSuffix(" $");
    limitsForm->addRow("Cooldown Loss Trigger:", m_cooldownLossTriggerSpinBox);

    m_cooldownDurationSpinBox = new QSpinBox(limitsGroup);
    m_cooldownDurationSpinBox->setRange(RiskManagementConstants::MIN_COOLDOWN_DURATION_SEC,
                                        RiskManagementConstants::MAX_COOLDOWN_DURATION_SEC);
    m_cooldownDurationSpinBox->setSingleStep(RiskManagementConstants::COOLDOWN_DURATION_STEP_SEC);
    m_cooldownDurationSpinBox->setSuffix(" s");
    limitsForm->addRow("Cooldown Duration:", m_cooldownDurationSpinBox);

    m_warningAmberSpinBox = new QSpinBox(limitsGroup);
    m_warningAmberSpinBox->setRange(RiskManagementConstants::MIN_WARNING_USED_PERCENT,
                                    RiskManagementConstants::MAX_WARNING_USED_PERCENT);
    m_warningAmberSpinBox->setSuffix(" %");
    limitsForm->addRow("Warning Amber (used):", m_warningAmberSpinBox);

    m_warningRedSpinBox = new QSpinBox(limitsGroup);
    m_warningRedSpinBox->setRange(RiskManagementConstants::MIN_WARNING_USED_PERCENT,
                                  RiskManagementConstants::MAX_WARNING_USED_PERCENT);
    m_warningRedSpinBox->setSuffix(" %");
    limitsForm->addRow("Warning Red (used):", m_warningRedSpinBox);

    mainLayout->addWidget(limitsGroup);

    auto* runtimeGroup = new QGroupBox("Runtime", this);
    auto* runtimeLayout = new QVBoxLayout(runtimeGroup);
    m_runtimeSummaryLabel = new QLabel("No data", runtimeGroup);
    m_runtimeSummaryLabel->setStyleSheet("QLabel { color: #d5e4f4; font-weight: bold; }");
    m_runtimeDetailsLabel = new QLabel("No data", runtimeGroup);
    m_runtimeDetailsLabel->setWordWrap(true);
    m_runtimeDetailsLabel->setStyleSheet("QLabel { color: #b8c7d6; }");
    runtimeLayout->addWidget(m_runtimeSummaryLabel);
    runtimeLayout->addWidget(m_runtimeDetailsLabel);

    auto* actionsLayout = new QHBoxLayout();
    m_resetDayButton = new QPushButton("Reset Day", runtimeGroup);
    m_unlockButton = new QPushButton("Unlock Trading", runtimeGroup);
    actionsLayout->addWidget(m_resetDayButton);
    actionsLayout->addWidget(m_unlockButton);
    actionsLayout->addStretch();
    runtimeLayout->addLayout(actionsLayout);

    mainLayout->addWidget(runtimeGroup);
    mainLayout->addStretch();
}

void RiskTab::connectSignals()
{
    auto connectControl = [this](QObject* p_sender, const char* p_signal)
    { connect(p_sender, p_signal, this, SLOT(onAnyConfigChanged())); };

    connectControl(m_enabledCheckBox, SIGNAL(toggled(bool)));
    connectControl(m_dailyDrawdownLimitSpinBox, SIGNAL(valueChanged(double)));
    connectControl(m_drawdownBasisComboBox, SIGNAL(currentIndexChanged(int)));
    connectControl(m_maxPlannedLossSpinBox, SIGNAL(valueChanged(double)));
    connectControl(m_maxPositionSharesSpinBox, SIGNAL(valueChanged(int)));
    connectControl(m_maxPositionNotionalSpinBox, SIGNAL(valueChanged(double)));
    connectControl(m_maxDailyEntriesSpinBox, SIGNAL(valueChanged(int)));
    connectControl(m_maxOpenPositionsSpinBox, SIGNAL(valueChanged(int)));
    connectControl(m_cooldownEnabledCheckBox, SIGNAL(toggled(bool)));
    connectControl(m_cooldownLossTriggerSpinBox, SIGNAL(valueChanged(double)));
    connectControl(m_cooldownDurationSpinBox, SIGNAL(valueChanged(int)));
    connectControl(m_warningAmberSpinBox, SIGNAL(valueChanged(int)));
    connectControl(m_warningRedSpinBox, SIGNAL(valueChanged(int)));

    connect(m_resetDayButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                if (m_selectedAccountId.isEmpty())
                {
                    return;
                }
                logInputEvent(u"RiskTab", u"reset-day", {inputDetail(u"accountId", m_selectedAccountId)});
                emit resetRiskDayRequested(m_selectedAccountId);
            });
    connect(m_unlockButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                if (m_selectedAccountId.isEmpty())
                {
                    return;
                }
                logInputEvent(u"RiskTab", u"unlock-trading", {inputDetail(u"accountId", m_selectedAccountId)});
                emit unlockRiskRequested(m_selectedAccountId);
            });
}

void RiskTab::setControlsEnabled(const bool p_enabled)
{
    m_enabledCheckBox->setEnabled(p_enabled);
    m_dailyDrawdownLimitSpinBox->setEnabled(p_enabled);
    m_drawdownBasisComboBox->setEnabled(p_enabled);
    m_maxPlannedLossSpinBox->setEnabled(p_enabled);
    m_maxPositionSharesSpinBox->setEnabled(p_enabled);
    m_maxPositionNotionalSpinBox->setEnabled(p_enabled);
    m_maxDailyEntriesSpinBox->setEnabled(p_enabled);
    m_maxOpenPositionsSpinBox->setEnabled(p_enabled);
    m_cooldownEnabledCheckBox->setEnabled(p_enabled);
    m_cooldownLossTriggerSpinBox->setEnabled(p_enabled);
    m_cooldownDurationSpinBox->setEnabled(p_enabled);
    m_warningAmberSpinBox->setEnabled(p_enabled);
    m_warningRedSpinBox->setEnabled(p_enabled);
    m_resetDayButton->setEnabled(p_enabled);
    m_unlockButton->setEnabled(p_enabled);
}

void RiskTab::setSelectedAccountId(const QString& p_accountId)
{
    m_selectedAccountId = p_accountId.trimmed().toUpper();
    if (m_selectedAccountId.isEmpty())
    {
        m_accountLabel->setText("Account: (none)");
        setControlsEnabled(false);
        m_runtimeSummaryLabel->setText("No account selected");
        m_runtimeDetailsLabel->setText("Select an account in the top toolbar.");
        return;
    }

    m_accountLabel->setText(QString("Account: %1").arg(m_selectedAccountId));
    setControlsEnabled(true);
}

void RiskTab::setRiskConfig(const RiskConfig& p_config)
{
    m_loadingUi = true;
    const QSignalBlocker b0(m_enabledCheckBox);
    const QSignalBlocker b1(m_dailyDrawdownLimitSpinBox);
    const QSignalBlocker b2(m_drawdownBasisComboBox);
    const QSignalBlocker b3(m_maxPlannedLossSpinBox);
    const QSignalBlocker b4(m_maxPositionSharesSpinBox);
    const QSignalBlocker b5(m_maxPositionNotionalSpinBox);
    const QSignalBlocker b6(m_maxDailyEntriesSpinBox);
    const QSignalBlocker b7(m_maxOpenPositionsSpinBox);
    const QSignalBlocker b8(m_cooldownEnabledCheckBox);
    const QSignalBlocker b9(m_cooldownLossTriggerSpinBox);
    const QSignalBlocker b10(m_cooldownDurationSpinBox);
    const QSignalBlocker b11(m_warningAmberSpinBox);
    const QSignalBlocker b12(m_warningRedSpinBox);

    m_enabledCheckBox->setChecked(p_config.enabled);
    m_dailyDrawdownLimitSpinBox->setValue(p_config.dailyDrawdownLimitUsd);
    const int basisIndex = m_drawdownBasisComboBox->findData(static_cast<int>(p_config.dailyDrawdownBasis));
    m_drawdownBasisComboBox->setCurrentIndex(basisIndex >= 0 ? basisIndex : 0);
    m_maxPlannedLossSpinBox->setValue(p_config.maxPlannedLossPerTradeUsd);
    m_maxPositionSharesSpinBox->setValue(p_config.maxPositionShares);
    m_maxPositionNotionalSpinBox->setValue(p_config.maxPositionNotionalUsd);
    m_maxDailyEntriesSpinBox->setValue(p_config.maxDailyEntryTrades);
    m_maxOpenPositionsSpinBox->setValue(p_config.maxOpenPositions);
    m_cooldownEnabledCheckBox->setChecked(p_config.cooldownEnabled);
    m_cooldownLossTriggerSpinBox->setValue(p_config.cooldownLossTriggerUsd);
    m_cooldownDurationSpinBox->setValue(p_config.cooldownDurationSec);
    m_warningAmberSpinBox->setValue(p_config.warningAmberUsedPercent);
    m_warningRedSpinBox->setValue(p_config.warningRedUsedPercent);
    m_loadingUi = false;
}

void RiskTab::setRiskStatus(const RiskStatusSnapshot& p_status)
{
    m_runtimeSummaryLabel->setText(p_status.summaryText.isEmpty() ? QString("No runtime state") : p_status.summaryText);
    m_runtimeDetailsLabel->setText(p_status.detailsText.isEmpty() ? QString("No runtime state") : p_status.detailsText);
}

RiskConfig RiskTab::currentConfig() const
{
    RiskConfig config;
    config.enabled = m_enabledCheckBox->isChecked();
    config.dailyDrawdownLimitUsd = m_dailyDrawdownLimitSpinBox->value();
    config.dailyDrawdownBasis = riskDrawdownBasisFromInt(m_drawdownBasisComboBox->currentData().toInt());
    config.maxPlannedLossPerTradeUsd = m_maxPlannedLossSpinBox->value();
    config.maxPositionShares = m_maxPositionSharesSpinBox->value();
    config.maxPositionNotionalUsd = m_maxPositionNotionalSpinBox->value();
    config.maxDailyEntryTrades = m_maxDailyEntriesSpinBox->value();
    config.maxOpenPositions = m_maxOpenPositionsSpinBox->value();
    config.cooldownEnabled = m_cooldownEnabledCheckBox->isChecked();
    config.cooldownLossTriggerUsd = m_cooldownLossTriggerSpinBox->value();
    config.cooldownDurationSec = m_cooldownDurationSpinBox->value();
    config.warningAmberUsedPercent = m_warningAmberSpinBox->value();
    config.warningRedUsedPercent = m_warningRedSpinBox->value();
    return config;
}

void RiskTab::onAnyConfigChanged()
{
    if (m_loadingUi || m_selectedAccountId.isEmpty())
    {
        return;
    }

    const RiskDrawdownBasis drawdownBasis = riskDrawdownBasisFromInt(m_drawdownBasisComboBox->currentData().toInt());
    logInputEvent(u"RiskTab",
                  u"config-changed",
                  {inputDetail(u"accountId", m_selectedAccountId),
                   inputDetail(u"drawdownLimitUsd", m_dailyDrawdownLimitSpinBox->value()),
                   inputDetail(u"drawdownBasis", riskDrawdownBasisToString(drawdownBasis)),
                   inputDetail(u"drawdownBasisId", static_cast<int>(drawdownBasis))});
    emit riskConfigChanged(m_selectedAccountId);
}
