#include "ChartToolbar.h"
#include <QHBoxLayout>
#include <QCheckBox>
#include <QFormLayout>
#include <QSignalBlocker>
#include <QColorDialog>

#include "CONSTANTS.h"
#include "Misc/Logging/Logging.h"

namespace
{
    constexpr int EMA_MIN_PERIOD = 1;
    constexpr int EMA_MAX_PERIOD = 5000;
    constexpr int RSI_MIN_PERIOD = 1;
    constexpr int RSI_MAX_PERIOD = 5000;
    constexpr int RSI_MIN_OVERSOLD = 0;
    constexpr int RSI_MAX_OVERSOLD = 99;
    constexpr int RSI_MIN_OVERBOUGHT = 1;
    constexpr int RSI_MAX_OVERBOUGHT = 100;

    QColor normalizeColorOrDefault(const QColor& candidate, const QColor& fallback)
    {
        return candidate.isValid() ? candidate : fallback;
    }

    QString emaSettingsTitle(const int slot)
    {
        return QString("EMA %1 Settings").arg(slot + 1);
    }
} // namespace

/**
 * @brief Constructs a ChartToolbar widget.
 */
ChartToolbar::ChartToolbar(QWidget* parent)
    : QWidget(parent)
    , m_vwapLineColor(normalizeColorOrDefault(QColor(ChartIndicatorConstants::DEFAULT_VWAP_COLOR), QColor(0, 220, 220)))
    , m_rsiLineColor(normalizeColorOrDefault(QColor(ChartIndicatorConstants::DEFAULT_RSI_COLOR), QColor(179, 136, 255)))
    , m_earlyPreMarketColor(normalizeColorOrDefault(QColor(ChartBackgroundConstants::DEFAULT_EARLY_PRE_MARKET_COLOR),
                                                    QColor(255, 165, 0, 90)))
    , m_preMarketColor(
          normalizeColorOrDefault(QColor(ChartBackgroundConstants::DEFAULT_PRE_MARKET_COLOR), QColor(255, 165, 0, 180)))
    , m_afterHoursColor(normalizeColorOrDefault(QColor(ChartBackgroundConstants::DEFAULT_AFTER_HOURS_COLOR),
                                                QColor(138, 43, 226, 180)))
{
    for (int slot = 0; slot < EMA_SLOT_COUNT; ++slot)
    {
        m_emaColors[slot] = defaultEmaColorForSlot(slot);
    }

    label = new QLabel("Timeframe:", this);
    label->setStyleSheet("font-weight: bold;");

    comboBox = new QComboBox(this);
    comboBox->setMinimumWidth(80);
    comboBox->setMaximumWidth(100);

    timeFrameSettingsButton = new QToolButton(this);
    timeFrameSettingsButton->setText("⚙");
    timeFrameSettingsButton->setToolTip("Timeframe Settings");
    timeFrameSettingsButton->setPopupMode(QToolButton::InstantPopup);
    timeFrameSettingsButton->setFixedSize(22, 22);

    timeFrameSettingsMenu = new QMenu(this);
    timeFrameSettingsButton->setMenu(timeFrameSettingsMenu);

    QWidgetAction* timeFrameSettingsAction = new QWidgetAction(timeFrameSettingsMenu);
    QWidget* timeFrameSettingsWidget = new QWidget(timeFrameSettingsMenu);
    QFormLayout* timeFrameSettingsLayout = new QFormLayout(timeFrameSettingsWidget);
    timeFrameSettingsLayout->setContentsMargins(8, 6, 8, 6);
    timeFrameSettingsLayout->setSpacing(6);

    autoCheckBox = new QCheckBox(timeFrameSettingsWidget);
    autoCheckBox->setToolTip("Automatically switch timeframe based on zoom level");

    timeFrameSettingsLayout->addRow("Auto:", autoCheckBox);
    timeFrameSettingsAction->setDefaultWidget(timeFrameSettingsWidget);
    timeFrameSettingsMenu->addAction(timeFrameSettingsAction);

    volumeCheckBox = new QCheckBox("Volume", this);
    volumeCheckBox->setChecked(true);

    volumeSettingsButton = new QToolButton(this);
    volumeSettingsButton->setText("⚙");
    volumeSettingsButton->setToolTip("Volume Settings");
    volumeSettingsButton->setPopupMode(QToolButton::InstantPopup);
    volumeSettingsButton->setFixedSize(22, 22);

    volumeSettingsMenu = new QMenu(this);
    volumeSettingsButton->setMenu(volumeSettingsMenu);

    QWidgetAction* volumeSettingsAction = new QWidgetAction(volumeSettingsMenu);
    QWidget* volumeSettingsWidget = new QWidget(volumeSettingsMenu);
    QFormLayout* volumeSettingsLayout = new QFormLayout(volumeSettingsWidget);
    volumeSettingsLayout->setContentsMargins(8, 6, 8, 6);
    volumeSettingsLayout->setSpacing(6);

    volumeAutoScaleCheckBox = new QCheckBox(volumeSettingsWidget);
    volumeAutoScaleCheckBox->setChecked(true);
    volumeAutoScaleCheckBox->setToolTip("Auto-scale volume Y-axis to visible bar range");

    volumeAutoScaleModeCombo = new QComboBox(volumeSettingsWidget);
    volumeAutoScaleModeCombo->addItem("Highest Bar", 0);
    volumeAutoScaleModeCombo->addItem("Second Highest", 1);
    volumeAutoScaleModeCombo->setCurrentIndex(1);
    volumeAutoScaleModeCombo->setToolTip("Choose how auto-scale range upper bound is selected");

    volumeSettingsLayout->addRow("Auto-scale:", volumeAutoScaleCheckBox);
    volumeSettingsLayout->addRow("Scale Mode:", volumeAutoScaleModeCombo);
    volumeSettingsAction->setDefaultWidget(volumeSettingsWidget);
    volumeSettingsMenu->addAction(volumeSettingsAction);

    ordersCheckBox = new QCheckBox("Orders", this);
    ordersCheckBox->setChecked(true);
    ordersCheckBox->setToolTip("Show/hide order markers and position lines on chart");

    bboCheckBox = new QCheckBox("BBO", this);
    bboCheckBox->setChecked(false);
    bboCheckBox->setToolTip("Show best bid and best ask guide lines on the chart");

    level2DepthCheckBox = new QCheckBox("Depth", this);
    level2DepthCheckBox->setChecked(false);
    level2DepthCheckBox->setToolTip("Show top-of-book depth levels at the current timeline");

    vwapCheckBox = new QCheckBox("VWAP", this);
    vwapCheckBox->setChecked(false);
    vwapCheckBox->setToolTip("Show/hide the session VWAP overlay line");

    vwapSettingsButton = new QToolButton(this);
    vwapSettingsButton->setText("⚙");
    vwapSettingsButton->setToolTip("VWAP Settings");
    vwapSettingsButton->setPopupMode(QToolButton::InstantPopup);
    vwapSettingsButton->setFixedSize(22, 22);

    vwapSettingsMenu = new QMenu(this);
    vwapSettingsButton->setMenu(vwapSettingsMenu);

    QWidgetAction* vwapSettingsAction = new QWidgetAction(vwapSettingsMenu);
    QWidget* vwapSettingsWidget = new QWidget(vwapSettingsMenu);
    QFormLayout* vwapSettingsLayout = new QFormLayout(vwapSettingsWidget);
    vwapSettingsLayout->setContentsMargins(8, 6, 8, 6);
    vwapSettingsLayout->setSpacing(6);

    vwapSourceCombo = new QComboBox(vwapSettingsWidget);
    vwapSourceCombo->addItem("Close", 0);
    vwapSourceCombo->addItem("HLC3", 1);
    vwapSourceCombo->setToolTip("Input price used by VWAP");

    vwapResetTimeEdit = new QTimeEdit(vwapSettingsWidget);
    vwapResetTimeEdit->setDisplayFormat("HH:mm");
    vwapResetTimeEdit->setTime(QTime(4, 0));
    vwapResetTimeEdit->setToolTip("Session reset anchor time (market timezone)");

    vwapColorButton = new QPushButton(vwapSettingsWidget);
    vwapColorButton->setToolTip("VWAP line color (supports alpha)");
    applyColorButtonStyle(vwapColorButton, m_vwapLineColor);

    vwapSettingsLayout->addRow("Source:", vwapSourceCombo);
    vwapSettingsLayout->addRow("Reset:", vwapResetTimeEdit);
    vwapSettingsLayout->addRow("Color:", vwapColorButton);
    vwapSettingsAction->setDefaultWidget(vwapSettingsWidget);
    vwapSettingsMenu->addAction(vwapSettingsAction);

    for (int slot = 0; slot < EMA_SLOT_COUNT; ++slot)
    {
        m_emaCheckBoxes[slot] = new QCheckBox(this);
        m_emaCheckBoxes[slot]->setChecked(false);
        m_emaCheckBoxes[slot]->setToolTip("Show/hide EMA overlay");

        m_emaSettingsButtons[slot] = new QToolButton(this);
        m_emaSettingsButtons[slot]->setText("⚙");
        m_emaSettingsButtons[slot]->setToolTip(emaSettingsTitle(slot));
        m_emaSettingsButtons[slot]->setPopupMode(QToolButton::InstantPopup);
        m_emaSettingsButtons[slot]->setFixedSize(22, 22);

        m_emaSettingsMenus[slot] = new QMenu(this);
        m_emaSettingsButtons[slot]->setMenu(m_emaSettingsMenus[slot]);

        QWidgetAction* emaSettingsAction = new QWidgetAction(m_emaSettingsMenus[slot]);
        QWidget* emaSettingsWidget = new QWidget(m_emaSettingsMenus[slot]);
        QFormLayout* emaSettingsLayout = new QFormLayout(emaSettingsWidget);
        emaSettingsLayout->setContentsMargins(8, 6, 8, 6);
        emaSettingsLayout->setSpacing(6);

        m_emaPeriodSpins[slot] = new QSpinBox(emaSettingsWidget);
        m_emaPeriodSpins[slot]->setRange(EMA_MIN_PERIOD, EMA_MAX_PERIOD);
        m_emaPeriodSpins[slot]->setValue(defaultEmaPeriodForSlot(slot));
        m_emaPeriodSpins[slot]->setToolTip("EMA lookback period");

        m_emaColorButtons[slot] = new QPushButton(emaSettingsWidget);
        m_emaColorButtons[slot]->setToolTip("EMA line color (supports alpha)");
        updateEmaColorButtonStyle(slot);

        emaSettingsLayout->addRow("Period:", m_emaPeriodSpins[slot]);
        emaSettingsLayout->addRow("Color:", m_emaColorButtons[slot]);
        emaSettingsAction->setDefaultWidget(emaSettingsWidget);
        m_emaSettingsMenus[slot]->addAction(emaSettingsAction);

        updateEmaLabel(slot);
    }

    macdCheckBox = new QCheckBox("MACD", this);
    macdCheckBox->setChecked(false);
    macdCheckBox->setToolTip("Show/hide the MACD indicator pane");

    strategyStatusCheckBox = new QCheckBox("Status", this);
    strategyStatusCheckBox->setChecked(true);
    strategyStatusCheckBox->setToolTip("Show/hide strategy status overlay");

    macdSettingsButton = new QToolButton(this);
    macdSettingsButton->setText("⚙");
    macdSettingsButton->setToolTip("MACD Settings");
    macdSettingsButton->setPopupMode(QToolButton::InstantPopup);
    macdSettingsButton->setFixedSize(22, 22);

    macdSettingsMenu = new QMenu(this);
    macdSettingsButton->setMenu(macdSettingsMenu);

    QWidgetAction* macdSettingsAction = new QWidgetAction(macdSettingsMenu);
    QWidget* macdSettingsWidget = new QWidget(macdSettingsMenu);
    QFormLayout* macdSettingsLayout = new QFormLayout(macdSettingsWidget);
    macdSettingsLayout->setContentsMargins(8, 6, 8, 6);
    macdSettingsLayout->setSpacing(6);

    macdFastLengthSpin = new QSpinBox(macdSettingsWidget);
    macdFastLengthSpin->setRange(1, 500);
    macdFastLengthSpin->setValue(12);

    macdSlowLengthSpin = new QSpinBox(macdSettingsWidget);
    macdSlowLengthSpin->setRange(2, 500);
    macdSlowLengthSpin->setValue(26);

    macdSignalLengthSpin = new QSpinBox(macdSettingsWidget);
    macdSignalLengthSpin->setRange(1, 500);
    macdSignalLengthSpin->setValue(9);

    macdMaTypeCombo = new QComboBox(macdSettingsWidget);
    macdMaTypeCombo->addItem("EMA", 0);
    macdMaTypeCombo->addItem("SMA", 1);

    macdSignalMaTypeCombo = new QComboBox(macdSettingsWidget);
    macdSignalMaTypeCombo->addItem("EMA", 0);
    macdSignalMaTypeCombo->addItem("SMA", 1);

    macdShowHistogramCheckBox = new QCheckBox("Show Histogram", macdSettingsWidget);
    macdShowHistogramCheckBox->setChecked(true);

    macdSettingsLayout->addRow("Fast:", macdFastLengthSpin);
    macdSettingsLayout->addRow("Slow:", macdSlowLengthSpin);
    macdSettingsLayout->addRow("Signal:", macdSignalLengthSpin);
    macdSettingsLayout->addRow("MACD MA:", macdMaTypeCombo);
    macdSettingsLayout->addRow("Signal MA:", macdSignalMaTypeCombo);
    macdSettingsLayout->addRow("", macdShowHistogramCheckBox);
    macdSettingsAction->setDefaultWidget(macdSettingsWidget);
    macdSettingsMenu->addAction(macdSettingsAction);

    connect(macdFastLengthSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value)
            {
                if (value >= macdSlowLengthSpin->value())
                {
                    macdSlowLengthSpin->setValue(qMin(500, value + 1));
                }
            });
    connect(macdSlowLengthSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value)
            {
                if (value <= macdFastLengthSpin->value())
                {
                    macdFastLengthSpin->setValue(qMax(1, value - 1));
                }
            });

    rsiCheckBox = new QCheckBox("RSI", this);
    rsiCheckBox->setChecked(false);
    rsiCheckBox->setToolTip("Show/hide the RSI indicator pane");

    rsiSettingsButton = new QToolButton(this);
    rsiSettingsButton->setText("⚙");
    rsiSettingsButton->setToolTip("RSI Settings");
    rsiSettingsButton->setPopupMode(QToolButton::InstantPopup);
    rsiSettingsButton->setFixedSize(22, 22);

    rsiSettingsMenu = new QMenu(this);
    rsiSettingsButton->setMenu(rsiSettingsMenu);

    QWidgetAction* rsiSettingsAction = new QWidgetAction(rsiSettingsMenu);
    QWidget* rsiSettingsWidget = new QWidget(rsiSettingsMenu);
    QFormLayout* rsiSettingsLayout = new QFormLayout(rsiSettingsWidget);
    rsiSettingsLayout->setContentsMargins(8, 6, 8, 6);
    rsiSettingsLayout->setSpacing(6);

    rsiPeriodSpin = new QSpinBox(rsiSettingsWidget);
    rsiPeriodSpin->setRange(RSI_MIN_PERIOD, RSI_MAX_PERIOD);
    rsiPeriodSpin->setValue(ChartIndicatorConstants::DEFAULT_RSI_PERIOD);

    rsiOverboughtSpin = new QSpinBox(rsiSettingsWidget);
    rsiOverboughtSpin->setRange(RSI_MIN_OVERBOUGHT, RSI_MAX_OVERBOUGHT);
    rsiOverboughtSpin->setValue(ChartIndicatorConstants::DEFAULT_RSI_OVERBOUGHT);

    rsiOversoldSpin = new QSpinBox(rsiSettingsWidget);
    rsiOversoldSpin->setRange(RSI_MIN_OVERSOLD, RSI_MAX_OVERSOLD);
    rsiOversoldSpin->setValue(ChartIndicatorConstants::DEFAULT_RSI_OVERSOLD);

    rsiColorButton = new QPushButton(rsiSettingsWidget);
    rsiColorButton->setToolTip("RSI line color (supports alpha)");
    applyColorButtonStyle(rsiColorButton, m_rsiLineColor);

    rsiSettingsLayout->addRow("Period:", rsiPeriodSpin);
    rsiSettingsLayout->addRow("Overbought:", rsiOverboughtSpin);
    rsiSettingsLayout->addRow("Oversold:", rsiOversoldSpin);
    rsiSettingsLayout->addRow("Color:", rsiColorButton);
    rsiSettingsAction->setDefaultWidget(rsiSettingsWidget);
    rsiSettingsMenu->addAction(rsiSettingsAction);

    connect(rsiOverboughtSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value)
            {
                if (value <= rsiOversoldSpin->value())
                {
                    rsiOversoldSpin->setValue(qMax(RSI_MIN_OVERSOLD, value - 1));
                }
            });
    connect(rsiOversoldSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value)
            {
                if (value >= rsiOverboughtSpin->value())
                {
                    rsiOverboughtSpin->setValue(qMin(RSI_MAX_OVERBOUGHT, value + 1));
                }
            });

    // Settings button with cog icon
    settingsButton = new QToolButton(this);
    settingsButton->setText("⚙");
    settingsButton->setToolTip("Chart Settings");
    settingsButton->setPopupMode(QToolButton::InstantPopup);

    settingsMenu = new QMenu(this);
    settingsButton->setMenu(settingsMenu);

    QWidgetAction* wheelRatioAction = new QWidgetAction(settingsMenu);
    QWidget* wheelRatioWidget = new QWidget();
    QHBoxLayout* wheelRatioLayout = new QHBoxLayout(wheelRatioWidget);
    wheelRatioLayout->setContentsMargins(5, 5, 5, 5);
    QLabel* wheelRatioLabel = new QLabel("Wheel Sensitivity:", wheelRatioWidget);
    wheelRatioCombo = new QComboBox(wheelRatioWidget);
    wheelRatioCombo->addItem("Very Low (0.1x)", 0.1);
    wheelRatioCombo->addItem("Low (0.2x)", 0.2);
    wheelRatioCombo->addItem("Medium-Low (0.5x)", 0.5);
    wheelRatioCombo->addItem("Normal (1.0x)", 1.0);
    wheelRatioCombo->addItem("High (1.5x)", 1.5);
    wheelRatioCombo->addItem("Very High (2.0x)", 2.0);
    wheelRatioCombo->setEditable(true);
    wheelRatioCombo->setCurrentIndex(3);
    wheelRatioLayout->addWidget(wheelRatioLabel);
    wheelRatioLayout->addWidget(wheelRatioCombo);
    wheelRatioAction->setDefaultWidget(wheelRatioWidget);
    settingsMenu->addAction(wheelRatioAction);

    settingsMenu->addSeparator();

    QWidgetAction* sessionColorsAction = new QWidgetAction(settingsMenu);
    QWidget* sessionColorsWidget = new QWidget(settingsMenu);
    QFormLayout* sessionColorsLayout = new QFormLayout(sessionColorsWidget);
    sessionColorsLayout->setContentsMargins(8, 6, 8, 6);
    sessionColorsLayout->setSpacing(6);

    m_earlyPreMarketColorButton = new QPushButton(sessionColorsWidget);
    m_preMarketColorButton = new QPushButton(sessionColorsWidget);
    m_afterHoursColorButton = new QPushButton(sessionColorsWidget);
    m_resetSessionColorsButton = new QPushButton("Reset Defaults", sessionColorsWidget);
    m_resetSessionColorsButton->setToolTip("Reset session background colors to defaults");

    sessionColorsLayout->addRow("Early PM:", m_earlyPreMarketColorButton);
    sessionColorsLayout->addRow("Pre-Market:", m_preMarketColorButton);
    sessionColorsLayout->addRow("After-Hours:", m_afterHoursColorButton);
    sessionColorsLayout->addRow("", m_resetSessionColorsButton);

    sessionColorsAction->setDefaultWidget(sessionColorsWidget);
    settingsMenu->addAction(sessionColorsAction);
    updateSessionColorButtonStyles();

    // Stock status indicators (right-aligned, before settings button)
    static const QString inactiveStatusStyle = "QLabel { background-color: #3a3a3a; color: #808080; padding: 4px 8px; "
                                               "border-radius: 4px; font-weight: bold; }";
    m_haltedLabel = new QLabel("HALTED", this);
    m_haltedLabel->setStyleSheet(inactiveStatusStyle);
    m_haltedLabel->setToolTip("Trading is halted for this symbol");

    m_delayedLabel = new QLabel("DELAYED", this);
    m_delayedLabel->setStyleSheet(inactiveStatusStyle);
    m_delayedLabel->setToolTip("Data is delayed (not real-time)");

    m_hardToBorrowLabel = new QLabel("HTB", this);
    m_hardToBorrowLabel->setStyleSheet(inactiveStatusStyle);
    m_hardToBorrowLabel->setToolTip("Hard to borrow - short selling may be restricted");

    populateTimeFrames();
    setCurrentTimeFrame(TimeFrame::ONE_MINUTE);

    QToolButton* indicatorsButton = new QToolButton(this);
    indicatorsButton->setText("Indicators");
    indicatorsButton->setToolTip("Show/hide chart indicators and overlays");
    indicatorsButton->setPopupMode(QToolButton::InstantPopup);

    QMenu* indicatorsMenu = new QMenu(this);
    indicatorsButton->setMenu(indicatorsMenu);

    QWidgetAction* indicatorsAction = new QWidgetAction(indicatorsMenu);
    QWidget* indicatorsWidget = new QWidget(indicatorsMenu);
    QVBoxLayout* indicatorsLayout = new QVBoxLayout(indicatorsWidget);
    indicatorsLayout->setContentsMargins(8, 6, 8, 6);
    indicatorsLayout->setSpacing(6);

    auto addIndicatorRow = [indicatorsLayout](QCheckBox* p_toggle, QToolButton* p_settingsButton = nullptr)
    {
        QWidget* row = new QWidget();
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);
        rowLayout->addWidget(p_toggle, 1);
        if (p_settingsButton != nullptr)
        {
            rowLayout->addWidget(p_settingsButton, 0, Qt::AlignRight);
        }
        indicatorsLayout->addWidget(row);
    };

    addIndicatorRow(ordersCheckBox);
    addIndicatorRow(volumeCheckBox, volumeSettingsButton);
    addIndicatorRow(bboCheckBox);
    addIndicatorRow(level2DepthCheckBox);
    addIndicatorRow(vwapCheckBox, vwapSettingsButton);
    for (int slot = 0; slot < EMA_SLOT_COUNT; ++slot)
    {
        addIndicatorRow(m_emaCheckBoxes[slot], m_emaSettingsButtons[slot]);
    }
    addIndicatorRow(macdCheckBox, macdSettingsButton);
    addIndicatorRow(rsiCheckBox, rsiSettingsButton);
    addIndicatorRow(strategyStatusCheckBox);
    indicatorsLayout->addStretch(1);

    indicatorsAction->setDefaultWidget(indicatorsWidget);
    indicatorsMenu->addAction(indicatorsAction);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(5);
    layout->addWidget(label);
    layout->addWidget(comboBox);
    layout->addWidget(timeFrameSettingsButton);
    layout->addWidget(indicatorsButton);
    layout->addStretch();
    layout->addWidget(m_haltedLabel);
    layout->addWidget(m_delayedLabel);
    layout->addWidget(m_hardToBorrowLabel);
    layout->addWidget(settingsButton);

    connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChartToolbar::onComboBoxChanged);
    connect(autoCheckBox,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { onAutoCheckBoxChanged(checked ? Qt::Checked : Qt::Unchecked); });
    connect(volumeCheckBox,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { onVolumeCheckBoxChanged(checked ? Qt::Checked : Qt::Unchecked); });
    connect(volumeAutoScaleCheckBox, &QCheckBox::toggled, this, &ChartToolbar::onVolumeSettingsWidgetChanged);
    connect(volumeAutoScaleModeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &ChartToolbar::onVolumeSettingsWidgetChanged);
    connect(ordersCheckBox,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { onOrdersCheckBoxChanged(checked ? Qt::Checked : Qt::Unchecked); });
    connect(bboCheckBox,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { onBboCheckBoxChanged(checked ? Qt::Checked : Qt::Unchecked); });
    connect(level2DepthCheckBox,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { onLevel2DepthCheckBoxChanged(checked ? Qt::Checked : Qt::Unchecked); });
    connect(vwapCheckBox,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { onVwapCheckBoxChanged(checked ? Qt::Checked : Qt::Unchecked); });
    connect(macdCheckBox,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { onMacdCheckBoxChanged(checked ? Qt::Checked : Qt::Unchecked); });
    connect(strategyStatusCheckBox,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { onStrategyStatusCheckBoxChanged(checked ? Qt::Checked : Qt::Unchecked); });
    connect(rsiCheckBox,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { onRsiCheckBoxChanged(checked ? Qt::Checked : Qt::Unchecked); });
    connect(vwapSourceCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &ChartToolbar::onVwapSettingsWidgetChanged);
    connect(vwapResetTimeEdit, &QTimeEdit::timeChanged, this, &ChartToolbar::onVwapSettingsWidgetChanged);
    connect(vwapColorButton, &QPushButton::clicked, this, [this]() { chooseVwapColor(); });
    for (int slot = 0; slot < EMA_SLOT_COUNT; ++slot)
    {
        connect(m_emaCheckBoxes[slot],
                &QCheckBox::toggled,
                this,
                [this, slot](bool checked) { onEmaCheckBoxChanged(slot, checked ? Qt::Checked : Qt::Unchecked); });
        connect(m_emaPeriodSpins[slot],
                QOverload<int>::of(&QSpinBox::valueChanged),
                this,
                [this, slot](int) { onEmaSettingsWidgetChanged(slot); });
        connect(m_emaColorButtons[slot],
                &QPushButton::clicked,
                this,
                [this, slot]() { onEmaColorButtonClicked(slot); });
    }
    connect(macdFastLengthSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ChartToolbar::onMacdSettingsWidgetChanged);
    connect(macdSlowLengthSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ChartToolbar::onMacdSettingsWidgetChanged);
    connect(macdSignalLengthSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ChartToolbar::onMacdSettingsWidgetChanged);
    connect(macdMaTypeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &ChartToolbar::onMacdSettingsWidgetChanged);
    connect(macdSignalMaTypeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &ChartToolbar::onMacdSettingsWidgetChanged);
    connect(macdShowHistogramCheckBox, &QCheckBox::toggled, this, &ChartToolbar::onMacdSettingsWidgetChanged);
    connect(rsiPeriodSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ChartToolbar::onRsiSettingsWidgetChanged);
    connect(rsiOverboughtSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ChartToolbar::onRsiSettingsWidgetChanged);
    connect(rsiOversoldSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ChartToolbar::onRsiSettingsWidgetChanged);
    connect(rsiColorButton, &QPushButton::clicked, this, &ChartToolbar::onRsiColorButtonClicked);
    connect(wheelRatioCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &ChartToolbar::onWheelRatioChanged);
    connect(wheelRatioCombo, &QComboBox::editTextChanged, this, [this]() { onWheelRatioChanged(-1); });
    connect(m_earlyPreMarketColorButton,
            &QPushButton::clicked,
            this,
            &ChartToolbar::onEarlyPreMarketColorButtonClicked);
    connect(m_preMarketColorButton, &QPushButton::clicked, this, &ChartToolbar::onPreMarketColorButtonClicked);
    connect(m_afterHoursColorButton, &QPushButton::clicked, this, &ChartToolbar::onAfterHoursColorButtonClicked);
    connect(m_resetSessionColorsButton, &QPushButton::clicked, this, &ChartToolbar::onResetSessionColorsButtonClicked);
}

TimeFrame ChartToolbar::getCurrentTimeFrame() const
{
    int currentIndex = comboBox->currentIndex();
    if (currentIndex >= 0 && currentIndex < comboBox->count())
        return static_cast<TimeFrame>(comboBox->itemData(currentIndex).toInt());
    return TimeFrame::ONE_MINUTE;
}

void ChartToolbar::setCurrentTimeFrame(TimeFrame timeframe)
{
    for (int i = 0; i < comboBox->count(); ++i)
    {
        if (static_cast<TimeFrame>(comboBox->itemData(i).toInt()) == timeframe)
        {
            comboBox->setCurrentIndex(i);
            break;
        }
    }
}

bool ChartToolbar::isAutoTimeFrameEnabled() const
{
    return autoCheckBox->isChecked();
}

void ChartToolbar::setAutoTimeFrameEnabled(bool enabled)
{
    autoCheckBox->setChecked(enabled);
}

bool ChartToolbar::isVolumeChartVisible() const
{
    return volumeCheckBox->isChecked();
}

void ChartToolbar::setVolumeChartVisible(bool visible)
{
    volumeCheckBox->setChecked(visible);
}

bool ChartToolbar::isVolumeAutoScaleEnabled() const
{
    return volumeAutoScaleCheckBox->isChecked();
}

int ChartToolbar::getVolumeAutoScaleMode() const
{
    const int index = volumeAutoScaleModeCombo->currentIndex();
    if (index >= 0 && index < volumeAutoScaleModeCombo->count())
    {
        return volumeAutoScaleModeCombo->itemData(index).toInt();
    }
    return 1;
}

void ChartToolbar::setVolumeSettings(const bool autoScaleEnabled, const int autoScaleMode)
{
    {
        const QSignalBlocker blocker(volumeAutoScaleCheckBox);
        volumeAutoScaleCheckBox->setChecked(autoScaleEnabled);
    }
    {
        const QSignalBlocker blocker(volumeAutoScaleModeCombo);
        const int index = volumeAutoScaleModeCombo->findData(autoScaleMode);
        volumeAutoScaleModeCombo->setCurrentIndex(index >= 0 ? index : 1);
    }
}

bool ChartToolbar::isOrderVisualizationsVisible() const
{
    return ordersCheckBox->isChecked();
}

void ChartToolbar::setOrderVisualizationsVisible(bool visible)
{
    ordersCheckBox->setChecked(visible);
}

bool ChartToolbar::isBboOverlayVisible() const
{
    return bboCheckBox->isChecked();
}

bool ChartToolbar::isLevel2DepthOverlayVisible() const
{
    return level2DepthCheckBox->isChecked();
}

bool ChartToolbar::isVwapVisible() const
{
    return vwapCheckBox->isChecked();
}

bool ChartToolbar::isMacdVisible() const
{
    return macdCheckBox->isChecked();
}

bool ChartToolbar::isStrategyStatusPanelVisible() const
{
    return strategyStatusCheckBox->isChecked();
}

bool ChartToolbar::isRsiVisible() const
{
    return rsiCheckBox->isChecked();
}

void ChartToolbar::setBboOverlayVisible(bool visible)
{
    {
        const QSignalBlocker blocker(bboCheckBox);
        bboCheckBox->setChecked(visible);
    }
    if (visible)
    {
        const QSignalBlocker blocker(level2DepthCheckBox);
        level2DepthCheckBox->setChecked(false);
    }
}

void ChartToolbar::setLevel2DepthOverlayVisible(bool visible)
{
    {
        const QSignalBlocker blocker(level2DepthCheckBox);
        level2DepthCheckBox->setChecked(visible);
    }
    if (visible)
    {
        const QSignalBlocker blocker(bboCheckBox);
        bboCheckBox->setChecked(false);
    }
}

void ChartToolbar::setVwapVisible(bool visible)
{
    vwapCheckBox->setChecked(visible);
}

void ChartToolbar::setMacdVisible(bool visible)
{
    macdCheckBox->setChecked(visible);
}

void ChartToolbar::setStrategyStatusPanelVisible(bool visible)
{
    strategyStatusCheckBox->setChecked(visible);
}

void ChartToolbar::setRsiVisible(bool visible)
{
    rsiCheckBox->setChecked(visible);
}

int ChartToolbar::getVwapSourceMode() const
{
    const int index = vwapSourceCombo->currentIndex();
    if (index >= 0 && index < vwapSourceCombo->count())
    {
        return vwapSourceCombo->itemData(index).toInt();
    }
    return 0;
}

void ChartToolbar::setVwapSourceMode(const int mode)
{
    const QSignalBlocker blocker(vwapSourceCombo);
    const int index = vwapSourceCombo->findData(mode);
    vwapSourceCombo->setCurrentIndex(index >= 0 ? index : 0);
}

QTime ChartToolbar::getVwapSessionResetTime() const
{
    return vwapResetTimeEdit->time();
}

void ChartToolbar::setVwapSessionResetTime(const QTime& time)
{
    const QSignalBlocker blocker(vwapResetTimeEdit);
    vwapResetTimeEdit->setTime(time.isValid() ? time : QTime(4, 0));
}

QColor ChartToolbar::getVwapLineColor() const
{
    return m_vwapLineColor;
}

void ChartToolbar::setVwapLineColor(const QColor& color)
{
    m_vwapLineColor =
        normalizedColor(color,
                        normalizedColor(QColor(ChartIndicatorConstants::DEFAULT_VWAP_COLOR), QColor(0, 220, 220)));
    applyColorButtonStyle(vwapColorButton, m_vwapLineColor);
}

bool ChartToolbar::isEmaVisible(const int slot) const
{
    if (!isValidEmaSlot(slot))
    {
        return false;
    }

    return m_emaCheckBoxes[slot]->isChecked();
}

void ChartToolbar::setEmaVisible(const int slot, const bool visible)
{
    if (!isValidEmaSlot(slot))
    {
        return;
    }

    const QSignalBlocker blocker(m_emaCheckBoxes[slot]);
    m_emaCheckBoxes[slot]->setChecked(visible);
}

int ChartToolbar::getEmaPeriod(const int slot) const
{
    if (!isValidEmaSlot(slot))
    {
        return defaultEmaPeriodForSlot(0);
    }

    return m_emaPeriodSpins[slot]->value();
}

QColor ChartToolbar::getEmaColor(const int slot) const
{
    if (!isValidEmaSlot(slot))
    {
        return defaultEmaColorForSlot(0);
    }

    return m_emaColors[slot];
}

void ChartToolbar::setEmaSettings(const int slot, const int period, const QColor& color)
{
    if (!isValidEmaSlot(slot))
    {
        return;
    }

    const int boundedPeriod = qBound(EMA_MIN_PERIOD, period, EMA_MAX_PERIOD);
    const QColor normalized = normalizedColor(color, defaultEmaColorForSlot(slot));

    {
        const QSignalBlocker blocker(m_emaPeriodSpins[slot]);
        m_emaPeriodSpins[slot]->setValue(boundedPeriod);
    }

    m_emaColors[slot] = normalized;
    updateEmaColorButtonStyle(slot);
    updateEmaLabel(slot);
}

void ChartToolbar::setMacdSettings(const int fastLength,
                                   const int slowLength,
                                   const int signalLength,
                                   const int macdMaType,
                                   const int signalMaType,
                                   const bool showHistogram)
{
    const int boundedFast = qBound(1, fastLength, 499);
    const int boundedSlow = qBound(boundedFast + 1, slowLength, 500);
    const int boundedSignal = qBound(1, signalLength, 500);

    {
        const QSignalBlocker blocker(macdFastLengthSpin);
        macdFastLengthSpin->setValue(boundedFast);
    }
    {
        const QSignalBlocker blocker(macdSlowLengthSpin);
        macdSlowLengthSpin->setValue(boundedSlow);
    }
    {
        const QSignalBlocker blocker(macdSignalLengthSpin);
        macdSignalLengthSpin->setValue(boundedSignal);
    }
    {
        const QSignalBlocker blocker(macdMaTypeCombo);
        const int index = macdMaTypeCombo->findData(macdMaType);
        macdMaTypeCombo->setCurrentIndex(index >= 0 ? index : 0);
    }
    {
        const QSignalBlocker blocker(macdSignalMaTypeCombo);
        const int index = macdSignalMaTypeCombo->findData(signalMaType);
        macdSignalMaTypeCombo->setCurrentIndex(index >= 0 ? index : 0);
    }
    {
        const QSignalBlocker blocker(macdShowHistogramCheckBox);
        macdShowHistogramCheckBox->setChecked(showHistogram);
    }
}

int ChartToolbar::getMacdFastLength() const
{
    return macdFastLengthSpin->value();
}

int ChartToolbar::getMacdSlowLength() const
{
    return macdSlowLengthSpin->value();
}

int ChartToolbar::getMacdSignalLength() const
{
    return macdSignalLengthSpin->value();
}

int ChartToolbar::getMacdMaType() const
{
    const int index = macdMaTypeCombo->currentIndex();
    if (index >= 0 && index < macdMaTypeCombo->count())
    {
        return macdMaTypeCombo->itemData(index).toInt();
    }
    return 0;
}

int ChartToolbar::getMacdSignalMaType() const
{
    const int index = macdSignalMaTypeCombo->currentIndex();
    if (index >= 0 && index < macdSignalMaTypeCombo->count())
    {
        return macdSignalMaTypeCombo->itemData(index).toInt();
    }
    return 0;
}

bool ChartToolbar::isMacdHistogramVisible() const
{
    return macdShowHistogramCheckBox->isChecked();
}

void ChartToolbar::setRsiSettings(const int period,
                                  const int overboughtLevel,
                                  const int oversoldLevel,
                                  const QColor& color)
{
    const int boundedOverbought = qBound(RSI_MIN_OVERBOUGHT, overboughtLevel, RSI_MAX_OVERBOUGHT);
    const int boundedOversold = qBound(RSI_MIN_OVERSOLD, oversoldLevel, qMin(RSI_MAX_OVERSOLD, boundedOverbought - 1));

    {
        const QSignalBlocker blocker(rsiPeriodSpin);
        rsiPeriodSpin->setValue(qBound(RSI_MIN_PERIOD, period, RSI_MAX_PERIOD));
    }
    {
        const QSignalBlocker blocker(rsiOverboughtSpin);
        rsiOverboughtSpin->setValue(boundedOverbought);
    }
    {
        const QSignalBlocker blocker(rsiOversoldSpin);
        rsiOversoldSpin->setValue(boundedOversold);
    }

    m_rsiLineColor =
        normalizedColor(color,
                        normalizedColor(QColor(ChartIndicatorConstants::DEFAULT_RSI_COLOR), QColor(179, 136, 255)));
    applyColorButtonStyle(rsiColorButton, m_rsiLineColor);
}

int ChartToolbar::getRsiPeriod() const
{
    return rsiPeriodSpin->value();
}

int ChartToolbar::getRsiOverboughtLevel() const
{
    return rsiOverboughtSpin->value();
}

int ChartToolbar::getRsiOversoldLevel() const
{
    return rsiOversoldSpin->value();
}

QColor ChartToolbar::getRsiColor() const
{
    return m_rsiLineColor;
}

void ChartToolbar::onComboBoxChanged(int index)
{
    if (index >= 0 && index < comboBox->count())
    {
        TimeFrame selectedTimeFrame = static_cast<TimeFrame>(comboBox->itemData(index).toInt());
        logInputEvent(u"ChartToolbar", u"set-timeframe", {inputDetail(u"timeframe", comboBox->itemText(index))});
        emit timeFrameChanged(selectedTimeFrame);
    }
}

void ChartToolbar::onAutoCheckBoxChanged(Qt::CheckState state)
{
    logInputEvent(u"ChartToolbar", u"toggle-auto-timeframe", {inputDetail(u"enabled", state == Qt::Checked)});
    emit autoTimeFrameChanged(state == Qt::Checked);
}

void ChartToolbar::onVolumeCheckBoxChanged(Qt::CheckState state)
{
    logInputEvent(u"ChartToolbar", u"toggle-volume-chart", {inputDetail(u"visible", state == Qt::Checked)});
    emit volumeChartVisibilityChanged(state == Qt::Checked);
}

void ChartToolbar::onVolumeSettingsWidgetChanged()
{
    const bool autoScaleEnabled = isVolumeAutoScaleEnabled();
    const int autoScaleMode = getVolumeAutoScaleMode();
    logInputEvent(
        u"ChartToolbar",
        u"set-volume-settings",
        {inputDetail(u"auto_scale_enabled", autoScaleEnabled), inputDetail(u"auto_scale_mode", autoScaleMode)});
    emit volumeSettingsChanged(autoScaleEnabled, autoScaleMode);
}

void ChartToolbar::onOrdersCheckBoxChanged(Qt::CheckState state)
{
    logInputEvent(u"ChartToolbar", u"toggle-order-visualizations", {inputDetail(u"visible", state == Qt::Checked)});
    emit orderVisualizationsVisibilityChanged(state == Qt::Checked);
}

void ChartToolbar::onBboCheckBoxChanged(Qt::CheckState state)
{
    const bool visible = state == Qt::Checked;
    logInputEvent(u"ChartToolbar", u"toggle-show-bbo", {inputDetail(u"visible", visible)});
    emit bboOverlayVisibilityChanged(visible);
}

void ChartToolbar::onLevel2DepthCheckBoxChanged(Qt::CheckState state)
{
    const bool visible = state == Qt::Checked;
    logInputEvent(u"ChartToolbar", u"toggle-level2-depth-overlay", {inputDetail(u"visible", visible)});
    emit level2DepthOverlayVisibilityChanged(visible);
}

void ChartToolbar::onVwapCheckBoxChanged(Qt::CheckState state)
{
    const bool visible = state == Qt::Checked;
    logInputEvent(u"ChartToolbar", u"toggle-vwap", {inputDetail(u"visible", visible)});
    emit vwapVisibilityChanged(visible);
}

void ChartToolbar::onMacdCheckBoxChanged(Qt::CheckState state)
{
    const bool visible = state == Qt::Checked;
    logInputEvent(u"ChartToolbar", u"toggle-macd", {inputDetail(u"visible", visible)});
    emit macdVisibilityChanged(visible);
}

void ChartToolbar::onStrategyStatusCheckBoxChanged(Qt::CheckState state)
{
    const bool visible = state == Qt::Checked;
    logInputEvent(u"ChartToolbar", u"toggle-strategy-status-panel", {inputDetail(u"visible", visible)});
    emit strategyStatusPanelVisibilityChanged(visible);
}

void ChartToolbar::onRsiCheckBoxChanged(Qt::CheckState state)
{
    const bool visible = state == Qt::Checked;
    logInputEvent(u"ChartToolbar", u"toggle-rsi", {inputDetail(u"visible", visible)});
    emit rsiVisibilityChanged(visible);
}

void ChartToolbar::onVwapSettingsWidgetChanged()
{
    const int sourceMode = getVwapSourceMode();
    const QTime resetTime = getVwapSessionResetTime();
    const QColor lineColor = getVwapLineColor();
    logInputEvent(u"ChartToolbar",
                  u"set-vwap-settings",
                  {inputDetail(u"source_mode", sourceMode),
                   inputDetail(u"reset_time", resetTime.toString("HH:mm")),
                   inputDetail(u"line_color", lineColor.name(QColor::HexArgb))});
    emit vwapSettingsChanged(sourceMode, resetTime, lineColor);
}

void ChartToolbar::onEmaCheckBoxChanged(const int slot, const Qt::CheckState state)
{
    if (!isValidEmaSlot(slot))
    {
        return;
    }

    const bool visible = state == Qt::Checked;
    logInputEvent(u"ChartToolbar", u"toggle-ema", {inputDetail(u"slot", slot + 1), inputDetail(u"visible", visible)});
    emit emaVisibilityChanged(slot, visible);
}

void ChartToolbar::onEmaSettingsWidgetChanged(const int slot)
{
    if (!isValidEmaSlot(slot))
    {
        return;
    }

    updateEmaLabel(slot);
    const int period = getEmaPeriod(slot);
    const QColor color = getEmaColor(slot);
    logInputEvent(u"ChartToolbar",
                  u"set-ema-settings",
                  {inputDetail(u"slot", slot + 1),
                   inputDetail(u"period", period),
                   inputDetail(u"color", color.name(QColor::HexArgb))});
    emit emaSettingsChanged(slot, period, color);
}

void ChartToolbar::onEmaColorButtonClicked(const int slot)
{
    if (!isValidEmaSlot(slot))
    {
        return;
    }

    chooseEmaColor(slot);
}

void ChartToolbar::onMacdSettingsWidgetChanged()
{
    const int fastLength = getMacdFastLength();
    const int slowLength = getMacdSlowLength();
    const int signalLength = getMacdSignalLength();
    const int macdMaType = getMacdMaType();
    const int signalMaType = getMacdSignalMaType();
    const bool showHistogram = isMacdHistogramVisible();

    logInputEvent(u"ChartToolbar",
                  u"set-macd-settings",
                  {inputDetail(u"fast", fastLength),
                   inputDetail(u"slow", slowLength),
                   inputDetail(u"signal", signalLength),
                   inputDetail(u"macd_ma_type", macdMaType),
                   inputDetail(u"signal_ma_type", signalMaType),
                   inputDetail(u"show_histogram", showHistogram)});

    emit macdSettingsChanged(fastLength, slowLength, signalLength, macdMaType, signalMaType, showHistogram);
}

void ChartToolbar::onRsiSettingsWidgetChanged()
{
    const int period = getRsiPeriod();
    const int overboughtLevel = getRsiOverboughtLevel();
    const int oversoldLevel = getRsiOversoldLevel();
    const QColor color = getRsiColor();

    logInputEvent(u"ChartToolbar",
                  u"set-rsi-settings",
                  {inputDetail(u"period", period),
                   inputDetail(u"overbought", overboughtLevel),
                   inputDetail(u"oversold", oversoldLevel),
                   inputDetail(u"color", color.name(QColor::HexArgb))});

    emit rsiSettingsChanged(period, overboughtLevel, oversoldLevel, color);
}

void ChartToolbar::onRsiColorButtonClicked()
{
    chooseRsiColor();
}

qreal ChartToolbar::getWheelRatio() const
{
    int index = wheelRatioCombo->currentIndex();
    if (index >= 0 && index < wheelRatioCombo->count())
    {
        QVariant itemData = wheelRatioCombo->itemData(index);
        if (itemData.isValid())
            return itemData.toReal();
    }
    QString text = wheelRatioCombo->currentText();
    bool ok;
    qreal ratio = text.toDouble(&ok);
    return ok && ratio > 0.0 ? ratio : 1.0;
}

void ChartToolbar::setWheelRatio(qreal ratio)
{
    for (int i = 0; i < wheelRatioCombo->count(); ++i)
    {
        if (qFuzzyCompare(wheelRatioCombo->itemData(i).toReal(), ratio))
        {
            wheelRatioCombo->setCurrentIndex(i);
            return;
        }
    }
    wheelRatioCombo->setCurrentText(QString::number(ratio, 'f', 2));
}

void ChartToolbar::setSessionBackgroundColors(const QColor& earlyPreMarketColor,
                                              const QColor& preMarketColor,
                                              const QColor& afterHoursColor)
{
    m_earlyPreMarketColor = normalizeColorOrDefault(
        earlyPreMarketColor,
        normalizeColorOrDefault(QColor(ChartBackgroundConstants::DEFAULT_EARLY_PRE_MARKET_COLOR),
                                QColor(255, 165, 0, 90)));
    m_preMarketColor = normalizeColorOrDefault(
        preMarketColor,
        normalizeColorOrDefault(QColor(ChartBackgroundConstants::DEFAULT_PRE_MARKET_COLOR), QColor(255, 165, 0, 180)));
    m_afterHoursColor =
        normalizeColorOrDefault(afterHoursColor,
                                normalizeColorOrDefault(QColor(ChartBackgroundConstants::DEFAULT_AFTER_HOURS_COLOR),
                                                        QColor(138, 43, 226, 180)));
    updateSessionColorButtonStyles();
}

void ChartToolbar::onWheelRatioChanged(int index)
{
    qreal ratio = 1.0;

    if (index >= 0 && index < wheelRatioCombo->count())
    {
        QVariant itemData = wheelRatioCombo->itemData(index);
        if (itemData.isValid())
        {
            ratio = itemData.toReal();
        }
        else
        {
            QString text = wheelRatioCombo->itemText(index);
            bool ok;
            ratio = text.toDouble(&ok);
            if (!ok || ratio <= 0.0)
            {
                ratio = 1.0;
                wheelRatioCombo->setCurrentText("1.0");
            }
        }
    }
    else
    {
        QString text = wheelRatioCombo->currentText();
        bool ok;
        ratio = text.toDouble(&ok);
        if (!ok || ratio <= 0.0)
        {
            ratio = 1.0;
            wheelRatioCombo->setCurrentText("1.0");
        }
    }

    logInputEvent(u"ChartToolbar", u"set-wheel-sensitivity", {inputDetail(u"ratio", QString::number(ratio, 'f', 2))});
    emit wheelRatioChanged(ratio);
}

void ChartToolbar::onEarlyPreMarketColorButtonClicked()
{
    chooseSessionColor(m_earlyPreMarketColor, "Select Early Pre-Market Background");
}

void ChartToolbar::onPreMarketColorButtonClicked()
{
    chooseSessionColor(m_preMarketColor, "Select Pre-Market Background");
}

void ChartToolbar::onAfterHoursColorButtonClicked()
{
    chooseSessionColor(m_afterHoursColor, "Select After-Hours Background");
}

void ChartToolbar::onResetSessionColorsButtonClicked()
{
    setSessionBackgroundColors(QColor(ChartBackgroundConstants::DEFAULT_EARLY_PRE_MARKET_COLOR),
                               QColor(ChartBackgroundConstants::DEFAULT_PRE_MARKET_COLOR),
                               QColor(ChartBackgroundConstants::DEFAULT_AFTER_HOURS_COLOR));

    logInputEvent(u"ChartToolbar", u"reset-session-background-colors", {});
    emitSessionBackgroundColorsChanged();
}

void ChartToolbar::applyColorButtonStyle(QPushButton* button, const QColor& color)
{
    Q_CHECK_PTR(button);
    const QColor textColor = (color.lightness() > 140) ? QColor(0, 0, 0) : QColor(255, 255, 255);
    button->setText(color.name(QColor::HexArgb).toUpper());
    button->setToolTip(QString("Click to change color (%1)").arg(color.name(QColor::HexArgb).toUpper()));
    button->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; border: 1px solid #666; "
                                  "border-radius: 3px; min-width: 96px; padding: 2px 6px; }"
                                  "QPushButton:hover { border: 1px solid #888; }")
                              .arg(color.name(QColor::HexArgb))
                              .arg(textColor.name()));
}

QColor ChartToolbar::normalizedColor(const QColor& candidate, const QColor& fallback)
{
    return candidate.isValid() ? candidate : fallback;
}

bool ChartToolbar::isValidEmaSlot(const int slot)
{
    return slot >= 0 && slot < EMA_SLOT_COUNT;
}

QColor ChartToolbar::defaultEmaColorForSlot(const int slot)
{
    switch (slot)
    {
    case 0:
        return normalizedColor(QColor(ChartIndicatorConstants::DEFAULT_EMA1_COLOR), QColor(243, 198, 35));
    case 1:
        return normalizedColor(QColor(ChartIndicatorConstants::DEFAULT_EMA2_COLOR), QColor(41, 182, 246));
    case 2:
        return normalizedColor(QColor(ChartIndicatorConstants::DEFAULT_EMA3_COLOR), QColor(240, 98, 146));
    default:
        return normalizedColor(QColor(ChartIndicatorConstants::DEFAULT_EMA1_COLOR), QColor(243, 198, 35));
    }
}

int ChartToolbar::defaultEmaPeriodForSlot(const int slot)
{
    switch (slot)
    {
    case 0:
        return ChartIndicatorConstants::DEFAULT_EMA1_PERIOD;
    case 1:
        return ChartIndicatorConstants::DEFAULT_EMA2_PERIOD;
    case 2:
        return ChartIndicatorConstants::DEFAULT_EMA3_PERIOD;
    default:
        return ChartIndicatorConstants::DEFAULT_EMA1_PERIOD;
    }
}

void ChartToolbar::updateEmaLabel(const int slot)
{
    if (!isValidEmaSlot(slot))
    {
        return;
    }

    m_emaCheckBoxes[slot]->setText(QString("%1 EMA").arg(getEmaPeriod(slot)));
}

void ChartToolbar::updateEmaColorButtonStyle(const int slot)
{
    if (!isValidEmaSlot(slot))
    {
        return;
    }

    applyColorButtonStyle(m_emaColorButtons[slot], m_emaColors[slot]);
}

void ChartToolbar::chooseEmaColor(const int slot)
{
    if (!isValidEmaSlot(slot))
    {
        return;
    }

    QColorDialog colorDialog(m_emaColors[slot], this);
    colorDialog.setOption(QColorDialog::ShowAlphaChannel, true);
    colorDialog.setWindowTitle(QString("Select %1 EMA Color").arg(getEmaPeriod(slot)));

    if (colorDialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QColor selectedColor = colorDialog.selectedColor();
    if (!selectedColor.isValid())
    {
        return;
    }

    m_emaColors[slot] = selectedColor;
    updateEmaColorButtonStyle(slot);
    onEmaSettingsWidgetChanged(slot);
}

void ChartToolbar::chooseVwapColor()
{
    QColorDialog colorDialog(m_vwapLineColor, this);
    colorDialog.setOption(QColorDialog::ShowAlphaChannel, true);
    colorDialog.setWindowTitle("Select VWAP Line Color");

    if (colorDialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QColor selectedColor = colorDialog.selectedColor();
    if (!selectedColor.isValid())
    {
        return;
    }

    setVwapLineColor(selectedColor);
    onVwapSettingsWidgetChanged();
}

void ChartToolbar::chooseRsiColor()
{
    QColorDialog colorDialog(m_rsiLineColor, this);
    colorDialog.setOption(QColorDialog::ShowAlphaChannel, true);
    colorDialog.setWindowTitle("Select RSI Line Color");

    if (colorDialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QColor selectedColor = colorDialog.selectedColor();
    if (!selectedColor.isValid())
    {
        return;
    }

    m_rsiLineColor = selectedColor;
    applyColorButtonStyle(rsiColorButton, m_rsiLineColor);
    onRsiSettingsWidgetChanged();
}

void ChartToolbar::updateSessionColorButtonStyles()
{
    applyColorButtonStyle(m_earlyPreMarketColorButton, m_earlyPreMarketColor);
    applyColorButtonStyle(m_preMarketColorButton, m_preMarketColor);
    applyColorButtonStyle(m_afterHoursColorButton, m_afterHoursColor);
}

void ChartToolbar::chooseSessionColor(QColor& targetColor, const QString& dialogTitle)
{
    QColorDialog colorDialog(targetColor, this);
    colorDialog.setOption(QColorDialog::ShowAlphaChannel, true);
    colorDialog.setWindowTitle(dialogTitle);

    if (colorDialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QColor selectedColor = colorDialog.selectedColor();
    if (!selectedColor.isValid())
    {
        return;
    }

    targetColor = selectedColor;
    updateSessionColorButtonStyles();
    emitSessionBackgroundColorsChanged();
}

void ChartToolbar::emitSessionBackgroundColorsChanged()
{
    logInputEvent(u"ChartToolbar",
                  u"set-session-background-colors",
                  {inputDetail(u"early_pre_market", m_earlyPreMarketColor.name(QColor::HexArgb)),
                   inputDetail(u"pre_market", m_preMarketColor.name(QColor::HexArgb)),
                   inputDetail(u"after_hours", m_afterHoursColor.name(QColor::HexArgb))});
    emit sessionBackgroundColorsChanged(m_earlyPreMarketColor, m_preMarketColor, m_afterHoursColor);
}

void ChartToolbar::setHalted(bool halted, const QString& reason)
{
    static const QString inactiveStyle = "QLabel { background-color: #3a3a3a; color: #808080; padding: 4px 8px; "
                                         "border-radius: 4px; font-weight: bold; }";
    static const QString activeStyle = "QLabel { background-color: #cc0000; color: #ffffff; padding: 4px 8px; "
                                       "border-radius: 4px; font-weight: bold; }";

    if (halted)
    {
        m_haltedLabel->setStyleSheet(activeStyle);
        m_haltedLabel->setToolTip(reason.isEmpty() ? "Trading is halted for this symbol" : "Halt reason: " + reason);
    }
    else
    {
        m_haltedLabel->setStyleSheet(inactiveStyle);
        m_haltedLabel->setToolTip("Trading is halted for this symbol");
    }
}

void ChartToolbar::setDelayed(bool delayed)
{
    static const QString inactiveStyle = "QLabel { background-color: #3a3a3a; color: #808080; padding: 4px 8px; "
                                         "border-radius: 4px; font-weight: bold; }";
    static const QString activeStyle = "QLabel { background-color: #ccaa00; color: #ffffff; padding: 4px 8px; "
                                       "border-radius: 4px; font-weight: bold; }";

    m_delayedLabel->setStyleSheet(delayed ? activeStyle : inactiveStyle);
}

void ChartToolbar::setHardToBorrow(bool active)
{
    static const QString inactiveStyle = "QLabel { background-color: #3a3a3a; color: #808080; padding: 4px 8px; "
                                         "border-radius: 4px; font-weight: bold; }";
    static const QString activeStyle = "QLabel { background-color: #e65c00; color: #ffffff; padding: 4px 8px; "
                                       "border-radius: 4px; font-weight: bold; }";

    if (active)
    {
        m_hardToBorrowLabel->setStyleSheet(activeStyle);
        m_hardToBorrowLabel->setToolTip("Short sale restriction (SSR) active");
    }
    else
    {
        m_hardToBorrowLabel->setStyleSheet(inactiveStyle);
        m_hardToBorrowLabel->setToolTip("Hard to borrow - short selling may be restricted");
    }
}

void ChartToolbar::populateTimeFrames()
{
    comboBox->clear();
    comboBox->addItem("1m", static_cast<int>(TimeFrame::ONE_MINUTE));
    comboBox->addItem("5m", static_cast<int>(TimeFrame::FIVE_MINUTES));
    comboBox->addItem("15m", static_cast<int>(TimeFrame::FIFTEEN_MINUTES));
    comboBox->addItem("30m", static_cast<int>(TimeFrame::THIRTY_MINUTES));
    comboBox->addItem("1h", static_cast<int>(TimeFrame::ONE_HOUR));
    comboBox->addItem("4h", static_cast<int>(TimeFrame::FOUR_HOURS));
    comboBox->addItem("1d", static_cast<int>(TimeFrame::ONE_DAY));
    comboBox->addItem("1w", static_cast<int>(TimeFrame::ONE_WEEK));
    comboBox->addItem("1M", static_cast<int>(TimeFrame::ONE_MONTH));
}
