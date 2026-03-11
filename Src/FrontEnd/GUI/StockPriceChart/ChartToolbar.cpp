#include "ChartToolbar.h"
#include <QHBoxLayout>
#include <QCheckBox>

/**
 * @brief Constructs a ChartToolbar widget.
 */
ChartToolbar::ChartToolbar(QWidget* parent) : QWidget(parent)
{
    label = new QLabel("Timeframe:", this);
    label->setStyleSheet("font-weight: bold;");

    comboBox = new QComboBox(this);
    comboBox->setMinimumWidth(80);
    comboBox->setMaximumWidth(100);

    autoCheckBox = new QCheckBox("Auto", this);

    volumeCheckBox = new QCheckBox("Volume", this);
    volumeCheckBox->setChecked(true);

    volumeAutoRescaleCheckBox = new QCheckBox("Vol Auto-Scale", this);
    volumeAutoRescaleCheckBox->setChecked(true);
    volumeAutoRescaleCheckBox->setToolTip("Auto-rescale volume Y-axis to visible bar range");

    ordersCheckBox = new QCheckBox("Orders", this);
    ordersCheckBox->setChecked(true);
    ordersCheckBox->setToolTip("Show/hide order markers and position lines on chart");

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

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(5);
    layout->addWidget(label);
    layout->addWidget(comboBox);
    layout->addWidget(autoCheckBox);
    layout->addWidget(volumeCheckBox);
    layout->addWidget(volumeAutoRescaleCheckBox);
    layout->addWidget(ordersCheckBox);
    layout->addStretch();
    layout->addWidget(m_haltedLabel);
    layout->addWidget(m_delayedLabel);
    layout->addWidget(m_hardToBorrowLabel);
    layout->addWidget(settingsButton);

    connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChartToolbar::onComboBoxChanged);
    connect(autoCheckBox, &QCheckBox::stateChanged, this, &ChartToolbar::onAutoCheckBoxChanged);
    connect(volumeCheckBox, &QCheckBox::stateChanged, this, &ChartToolbar::onVolumeCheckBoxChanged);
    connect(volumeAutoRescaleCheckBox,
            &QCheckBox::stateChanged,
            this,
            &ChartToolbar::onVolumeAutoRescaleCheckBoxChanged);
    connect(ordersCheckBox, &QCheckBox::stateChanged, this, &ChartToolbar::onOrdersCheckBoxChanged);
    connect(wheelRatioCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &ChartToolbar::onWheelRatioChanged);
    connect(wheelRatioCombo, &QComboBox::editTextChanged, this, [this]() { onWheelRatioChanged(-1); });

    setStyleSheet("ChartToolbar {"
                  "    background-color: #2a2a2a;"
                  "    border: 1px solid #555;"
                  "    border-radius: 3px;"
                  "}"
                  "QLabel {"
                  "    color: #ffffff;"
                  "}"
                  "QComboBox {"
                  "    background-color: #3a3a3a;"
                  "    color: #ffffff;"
                  "    border: 1px solid #666;"
                  "    border-radius: 3px;"
                  "    padding: 2px;"
                  "}"
                  "QComboBox::drop-down {"
                  "    border: none;"
                  "}"
                  "QComboBox::down-arrow {"
                  "    image: url(down_arrow.png);"
                  "    width: 12px;"
                  "    height: 12px;"
                  "}"
                  "QComboBox QAbstractItemView {"
                  "    background-color: #3a3a3a;"
                  "    color: #ffffff;"
                  "    selection-background-color: #555;"
                  "    border: 1px solid #666;"
                  "}"
                  "QCheckBox {"
                  "    color: #ffffff;"
                  "}"
                  "QCheckBox::indicator {"
                  "    width: 13px;"
                  "    height: 13px;"
                  "}"
                  "QCheckBox::indicator:unchecked {"
                  "    border: 1px solid #666;"
                  "    background-color: #3a3a3a;"
                  "}"
                  "QCheckBox::indicator:checked {"
                  "    border: 1px solid #666;"
                  "    background-color: #555;"
                  "}"
                  "QToolButton {"
                  "    background-color: #3a3a3a;"
                  "    color: #ffffff;"
                  "    border: 1px solid #666;"
                  "    border-radius: 3px;"
                  "    padding: 4px;"
                  "    font-size: 14px;"
                  "}"
                  "QToolButton:hover {"
                  "    background-color: #555;"
                  "}"
                  "QToolButton:pressed {"
                  "    background-color: #666;"
                  "}"
                  "QMenu {"
                  "    background-color: #3a3a3a;"
                  "    color: #ffffff;"
                  "    border: 1px solid #666;"
                  "}"
                  "QMenu::item {"
                  "    padding: 5px 20px;"
                  "}"
                  "QMenu::item:selected {"
                  "    background-color: #555;"
                  "}");
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

bool ChartToolbar::isVolumeAutoRescaleEnabled() const
{
    return volumeAutoRescaleCheckBox->isChecked();
}

void ChartToolbar::setVolumeAutoRescaleEnabled(bool enabled)
{
    volumeAutoRescaleCheckBox->setChecked(enabled);
}

bool ChartToolbar::isOrderVisualizationsVisible() const
{
    return ordersCheckBox->isChecked();
}

void ChartToolbar::setOrderVisualizationsVisible(bool visible)
{
    ordersCheckBox->setChecked(visible);
}

void ChartToolbar::onComboBoxChanged(int index)
{
    if (index >= 0 && index < comboBox->count())
    {
        TimeFrame selectedTimeFrame = static_cast<TimeFrame>(comboBox->itemData(index).toInt());
        emit timeFrameChanged(selectedTimeFrame);
    }
}

void ChartToolbar::onAutoCheckBoxChanged(int state)
{
    emit autoTimeFrameChanged(state == Qt::Checked);
}

void ChartToolbar::onVolumeCheckBoxChanged(int state)
{
    emit volumeChartVisibilityChanged(state == Qt::Checked);
}

void ChartToolbar::onVolumeAutoRescaleCheckBoxChanged(int state)
{
    emit volumeAutoRescaleChanged(state == Qt::Checked);
}

void ChartToolbar::onOrdersCheckBoxChanged(int state)
{
    emit orderVisualizationsVisibilityChanged(state == Qt::Checked);
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

    emit wheelRatioChanged(ratio);
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
