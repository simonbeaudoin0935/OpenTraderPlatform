#include "TimeFrameSelector.h"
#include <QHBoxLayout>
#include <QCheckBox>

/**
 * @brief Constructs a TimeFrameSelector widget.
 */
TimeFrameSelector::TimeFrameSelector(QWidget* parent)
    : QWidget(parent) {

    // Create the label
    label = new QLabel("Timeframe:", this);
    label->setStyleSheet("font-weight: bold;");

    // Create the combobox
    comboBox = new QComboBox(this);
    comboBox->setMinimumWidth(80);
    comboBox->setMaximumWidth(100);

    // Create the auto checkbox
    autoCheckBox = new QCheckBox("Auto", this);

    // Create the volume visibility checkbox
    volumeCheckBox = new QCheckBox("Volume", this);
    volumeCheckBox->setChecked(true); // Volume visible by default

    // Populate with timeframe options
    populateTimeFrames();

    // Set default to 1 minute
    setCurrentTimeFrame(TimeFrame::ONE_MINUTE);

    // Create layout
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(5);
    layout->addWidget(label);
    layout->addWidget(comboBox);
    layout->addWidget(autoCheckBox);
    layout->addWidget(volumeCheckBox);
    layout->addStretch(); // Push widgets to the left

    // Connect signals
    connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TimeFrameSelector::onComboBoxChanged);
    connect(autoCheckBox, &QCheckBox::stateChanged,
            this, &TimeFrameSelector::onAutoCheckBoxChanged);
    connect(volumeCheckBox, &QCheckBox::stateChanged,
            this, &TimeFrameSelector::onVolumeCheckBoxChanged);

    // Set a nice background and border
    setStyleSheet(
        "TimeFrameSelector {"
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
    );
}

/**
 * @brief Gets the currently selected timeframe.
 */
TimeFrame TimeFrameSelector::getCurrentTimeFrame() const {
    int currentIndex = comboBox->currentIndex();
    if (currentIndex >= 0 && currentIndex < comboBox->count()) {
        return static_cast<TimeFrame>(comboBox->itemData(currentIndex).toInt());
    }
    return TimeFrame::ONE_MINUTE; // Default fallback
}

/**
 * @brief Sets the selected timeframe.
 */
void TimeFrameSelector::setCurrentTimeFrame(TimeFrame timeframe) {
    for (int i = 0; i < comboBox->count(); ++i) {
        if (static_cast<TimeFrame>(comboBox->itemData(i).toInt()) == timeframe) {
            comboBox->setCurrentIndex(i);
            break;
        }
    }
}

/**
 * @brief Checks if auto timeframe selection is enabled.
 */
bool TimeFrameSelector::isAutoTimeFrameEnabled() const {
    return autoCheckBox->isChecked();
}

/**
 * @brief Sets the auto timeframe selection state.
 */
void TimeFrameSelector::setAutoTimeFrameEnabled(bool enabled) {
    autoCheckBox->setChecked(enabled);
}

/**
 * @brief Checks if volume chart is visible.
 */
bool TimeFrameSelector::isVolumeChartVisible() const {
    return volumeCheckBox->isChecked();
}

/**
 * @brief Sets the volume chart visibility state.
 */
void TimeFrameSelector::setVolumeChartVisible(bool visible) {
    volumeCheckBox->setChecked(visible);
}

/**
 * @brief Handles combobox selection changes.
 */
void TimeFrameSelector::onComboBoxChanged(int index) {
    if (index >= 0 && index < comboBox->count()) {
        TimeFrame selectedTimeFrame = static_cast<TimeFrame>(comboBox->itemData(index).toInt());
        emit timeFrameChanged(selectedTimeFrame);
    }
}

/**
 * @brief Handles checkbox state changes.
 */
void TimeFrameSelector::onAutoCheckBoxChanged(int state) {
    bool enabled = (state == Qt::Checked);
    emit autoTimeFrameChanged(enabled);
}

/**
 * @brief Handles volume chart visibility checkbox state changes.
 */
void TimeFrameSelector::onVolumeCheckBoxChanged(int state) {
    bool visible = (state == Qt::Checked);
    emit volumeChartVisibilityChanged(visible);
}

/**
 * @brief Populates the combobox with timeframe options.
 */
void TimeFrameSelector::populateTimeFrames() {
    // Clear existing items
    comboBox->clear();

    // Add timeframe options with their enum values as user data
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