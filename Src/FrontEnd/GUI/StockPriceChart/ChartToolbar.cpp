#include "ChartToolbar.h"
#include <QHBoxLayout>
#include <QCheckBox>

/**
 * @brief Constructs a ChartToolbar widget.
 */
ChartToolbar::ChartToolbar(QWidget* parent)
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

    // Create replay controls
    replayLabel = new QLabel("Replay:", this);
    replayLabel->setStyleSheet("font-weight: bold;");

    replayInfoLabel = new QLabel("", this);
    replayInfoLabel->setStyleSheet("color: #cccccc; font-size: 12px;");

    replayDayCombo = new QComboBox(this);
    replayDayCombo->setMinimumWidth(100);
    replayDayCombo->setMaximumWidth(120);

    replayTimeEdit = new QTimeEdit(this);
    replayTimeEdit->setDisplayFormat("hh:mm");
    replayTimeEdit->setTime(QTime(9, 30)); // Default to 9:30 AM

    playPauseButton = new QPushButton("Play", this);
    playPauseButton->setCheckable(true);

    // Create settings button with cog icon
    settingsButton = new QToolButton(this);
    settingsButton->setText("⚙"); // Unicode cog icon
    settingsButton->setToolTip("Chart Settings");
    settingsButton->setPopupMode(QToolButton::InstantPopup);

    // Create settings menu
    settingsMenu = new QMenu(this);
    settingsButton->setMenu(settingsMenu);

    // Create wheel ratio combo box
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
    wheelRatioCombo->setCurrentIndex(3); // Default to Normal
    wheelRatioLayout->addWidget(wheelRatioLabel);
    wheelRatioLayout->addWidget(wheelRatioCombo);
    wheelRatioAction->setDefaultWidget(wheelRatioWidget);
    settingsMenu->addAction(wheelRatioAction);

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
    layout->addStretch(); // Push replay widgets to the right
    layout->addWidget(replayLabel);
    layout->addWidget(replayInfoLabel);
    layout->addWidget(replayDayCombo);
    layout->addWidget(replayTimeEdit);
    layout->addWidget(playPauseButton);
    layout->addWidget(settingsButton); // Add settings button at the end

    // Connect signals
    connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChartToolbar::onComboBoxChanged);
    connect(autoCheckBox, &QCheckBox::stateChanged,
            this, &ChartToolbar::onAutoCheckBoxChanged);
    connect(volumeCheckBox, &QCheckBox::stateChanged,
            this, &ChartToolbar::onVolumeCheckBoxChanged);
    connect(replayDayCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChartToolbar::onReplayDayChanged);
    connect(replayTimeEdit, &QTimeEdit::timeChanged,
            this, &ChartToolbar::onReplayTimeChanged);
    connect(playPauseButton, &QPushButton::clicked,
            this, &ChartToolbar::onPlayPauseClicked);
    connect(wheelRatioCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChartToolbar::onWheelRatioChanged);
    connect(wheelRatioCombo, &QComboBox::editTextChanged,
            this, [this]() { onWheelRatioChanged(-1); }); // -1 to indicate custom text

    // Set a nice background and border
    setStyleSheet(
        "ChartToolbar {"
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
        "QTimeEdit {"
        "    background-color: #3a3a3a;"
        "    color: #ffffff;"
        "    border: 1px solid #666;"
        "    border-radius: 3px;"
        "    padding: 2px;"
        "}"
        "QPushButton {"
        "    background-color: #3a3a3a;"
        "    color: #ffffff;"
        "    border: 1px solid #666;"
        "    border-radius: 3px;"
        "    padding: 4px 8px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #555;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #666;"
        "}"
        "QPushButton:checked {"
        "    background-color: #4a4a4a;"
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
        "}"
    );

    // Scan and populate available replay days from cache
    scanAndPopulateReplayDays();
}

/**
 * @brief Gets the currently selected timeframe.
 */
TimeFrame ChartToolbar::getCurrentTimeFrame() const {
    int currentIndex = comboBox->currentIndex();
    if (currentIndex >= 0 && currentIndex < comboBox->count()) {
        return static_cast<TimeFrame>(comboBox->itemData(currentIndex).toInt());
    }
    return TimeFrame::ONE_MINUTE; // Default fallback
}

/**
 * @brief Sets the selected timeframe.
 */
void ChartToolbar::setCurrentTimeFrame(TimeFrame timeframe) {
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
bool ChartToolbar::isAutoTimeFrameEnabled() const {
    return autoCheckBox->isChecked();
}

/**
 * @brief Sets the auto timeframe selection state.
 */
void ChartToolbar::setAutoTimeFrameEnabled(bool enabled) {
    autoCheckBox->setChecked(enabled);
}

/**
 * @brief Checks if volume chart is visible.
 */
bool ChartToolbar::isVolumeChartVisible() const {
    return volumeCheckBox->isChecked();
}

/**
 * @brief Sets the volume chart visibility state.
 */
void ChartToolbar::setVolumeChartVisible(bool visible) {
    volumeCheckBox->setChecked(visible);
}

/**
 * @brief Handles combobox selection changes.
 */
void ChartToolbar::onComboBoxChanged(int index) {
    if (index >= 0 && index < comboBox->count()) {
        TimeFrame selectedTimeFrame = static_cast<TimeFrame>(comboBox->itemData(index).toInt());
        emit timeFrameChanged(selectedTimeFrame);
    }
}

/**
 * @brief Handles checkbox state changes.
 */
void ChartToolbar::onAutoCheckBoxChanged(int state) {
    bool enabled = (state == Qt::Checked);
    emit autoTimeFrameChanged(enabled);
}

/**
 * @brief Handles volume chart visibility checkbox state changes.
 */
void ChartToolbar::onVolumeCheckBoxChanged(int state) {
    bool visible = (state == Qt::Checked);
    emit volumeChartVisibilityChanged(visible);
}

/**
 * @brief Sets the available days for market replay.
 */
void ChartToolbar::setAvailableReplayDays(const QList<QDate>& days) {
    replayDayCombo->clear();
    for (const QDate& date : days) {
        replayDayCombo->addItem(date.toString("yyyy-MM-dd"), date);
    }
    if (!days.isEmpty()) {
        replayDayCombo->setCurrentIndex(0);
    }
}

/**
 * @brief Gets the currently selected replay day.
 */
QDate ChartToolbar::getSelectedReplayDay() const {
    int currentIndex = replayDayCombo->currentIndex();
    if (currentIndex >= 0 && currentIndex < replayDayCombo->count()) {
        return replayDayCombo->itemData(currentIndex).toDate();
    }
    return QDate(); // Invalid date if no selection
}

/**
 * @brief Sets the selected replay day.
 */
void ChartToolbar::setSelectedReplayDay(const QDate& date) {
    for (int i = 0; i < replayDayCombo->count(); ++i) {
        if (replayDayCombo->itemData(i).toDate() == date) {
            replayDayCombo->setCurrentIndex(i);
            break;
        }
    }
}

/**
 * @brief Gets the replay start time.
 */
QTime ChartToolbar::getReplayStartTime() const {
    return replayTimeEdit->time();
}

/**
 * @brief Sets the replay start time.
 */
void ChartToolbar::setReplayStartTime(const QTime& time) {
    replayTimeEdit->setTime(time);
}

/**
 * @brief Checks if replay is currently playing.
 */
bool ChartToolbar::isReplayPlaying() const {
    return playPauseButton->isChecked();
}

/**
 * @brief Sets the replay play/pause state.
 */
void ChartToolbar::setReplayPlaying(bool playing) {
    playPauseButton->setChecked(playing);
    updatePlayPauseButton();
}

/**
 * @brief Handles replay day combobox selection changes.
 */
void ChartToolbar::onReplayDayChanged(int index) {
    if (index >= 0 && index < replayDayCombo->count()) {
        QDate selectedDate = replayDayCombo->itemData(index).toDate();
        emit replayDayChanged(selectedDate);
    }
}

/**
 * @brief Handles replay time edit changes.
 */
void ChartToolbar::onReplayTimeChanged(const QTime& time) {
    emit replayStartTimeChanged(time);
}

/**
 * @brief Handles play/pause button clicks.
 */
void ChartToolbar::onPlayPauseClicked() {
    bool playing = playPauseButton->isChecked();
    updatePlayPauseButton();
    emit replayPlayPauseToggled(playing);
}

/**
 * @brief Updates the play/pause button text based on current state.
 */
void ChartToolbar::updatePlayPauseButton() {
    if (playPauseButton->isChecked()) {
        playPauseButton->setText("Pause");
    } else {
        playPauseButton->setText("Play");
    }
}

/**
 * @brief Populates the combobox with timeframe options.
 */
void ChartToolbar::populateTimeFrames() {
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

/**
 * @brief Scans the cache directory and populates available replay days.
 */
void ChartToolbar::scanAndPopulateReplayDays() {
    QList<QDate> availableDates;

    // Get the cache directory path
    QString cacheDirPath = QDir::homePath() + "/.cache/L2Trader/RecordedLiveData/Bars";
    QDir barsDir(cacheDirPath);

    if (barsDir.exists()) {
        // Get all files in the Bars directory
        QStringList filters;
        filters << "*"; // All files
        QStringList fileList = barsDir.entryList(filters, QDir::Files);

        // Extract dates from filenames
        for (const QString& fileName : fileList) {
            QDate date = extractDateFromFileName(fileName);
            if (date.isValid() && !availableDates.contains(date)) {
                availableDates.append(date);
            }
        }

        // Sort dates in descending order (most recent first)
        std::sort(availableDates.begin(), availableDates.end(), std::greater<QDate>());
    }

    // Populate the combo box with the found dates
    setAvailableReplayDays(availableDates);
}

/**
 * @brief Extracts date from a filename in the Bars directory.
 */
QDate ChartToolbar::extractDateFromFileName(const QString& fileName) {
    // Handle RecordedLiveBars_YYYY-MM-DD.db format
    if (fileName.startsWith("RecordedLiveBars_")) {
        QString datePart = fileName.mid(18); // Skip "RecordedLiveBars_" (18 chars)
        int dotIndex = datePart.indexOf('.');
        if (dotIndex != -1) {
            datePart = datePart.left(dotIndex); // Remove extension
        }
        QDate date = QDate::fromString(datePart, "yyyy-MM-dd");
        if (date.isValid()) {
            return date;
        }
    }

    // Fallback: Try different common date formats that might be used in filenames
    // Remove file extension if present
    QString baseName = fileName;
    int dotIndex = baseName.lastIndexOf('.');
    if (dotIndex != -1) {
        baseName = baseName.left(dotIndex);
    }

    // Try YYYY-MM-DD format
    QDate date = QDate::fromString(baseName, "yyyy-MM-dd");
    if (date.isValid()) {
        return date;
    }

    // Try YYYYMMDD format
    date = QDate::fromString(baseName, "yyyyMMdd");
    if (date.isValid()) {
        return date;
    }

    // Try DD-MM-YYYY format
    date = QDate::fromString(baseName, "dd-MM-yyyy");
    if (date.isValid()) {
        return date;
    }

    // Try MM-DD-YYYY format
    date = QDate::fromString(baseName, "MM-dd-yyyy");
    if (date.isValid()) {
        return date;
    }

    // If no standard format works, try to extract date components
    // Look for patterns like 8 digits that could be a date
    QRegularExpression dateRegex("(\\d{4})[-_]?(\\d{2})[-_]?(\\d{2})");
    QRegularExpressionMatch match = dateRegex.match(baseName);
    if (match.hasMatch()) {
        int year = match.captured(1).toInt();
        int month = match.captured(2).toInt();
        int day = match.captured(3).toInt();
        QDate extractedDate(year, month, day);
        if (extractedDate.isValid()) {
            return extractedDate;
        }
    }

    // Return invalid date if no pattern matches
    return QDate();
}

/**
 * @brief Updates the replay info label with time range and bar count.
 */
void ChartToolbar::updateReplayInfo(const QTime& startTime, const QTime& endTime, int barCount) {
    if (startTime.isValid() && endTime.isValid() && barCount > 0) {
        QString infoText = QString("%1-%2 (%3 bars)")
                          .arg(startTime.toString("hh:mm"))
                          .arg(endTime.toString("hh:mm"))
                          .arg(barCount);
        replayInfoLabel->setText(infoText);
    } else {
        replayInfoLabel->setText("No data");
    }
}

/**
 * @brief Gets the current wheel scrolling ratio.
 */
qreal ChartToolbar::getWheelRatio() const {
    int index = wheelRatioCombo->currentIndex();
    if (index >= 0 && index < wheelRatioCombo->count()) {
        QVariant data = wheelRatioCombo->itemData(index);
        if (data.isValid()) {
            return data.toReal();
        }
    }
    // Custom text
    QString text = wheelRatioCombo->currentText();
    bool ok;
    qreal ratio = text.toDouble(&ok);
    return ok && ratio > 0.0 ? ratio : 1.0;
}

/**
 * @brief Sets the wheel scrolling ratio.
 */
void ChartToolbar::setWheelRatio(qreal ratio) {
    for (int i = 0; i < wheelRatioCombo->count(); ++i) {
        if (qFuzzyCompare(wheelRatioCombo->itemData(i).toReal(), ratio)) {
            wheelRatioCombo->setCurrentIndex(i);
            return;
        }
    }
    // If exact match not found, set custom text
    wheelRatioCombo->setCurrentText(QString::number(ratio, 'f', 2));
}

/**
 * @brief Handles wheel ratio combo box changes.
 */
void ChartToolbar::onWheelRatioChanged(int index) {
    qreal ratio = 1.0; // default
    
    if (index >= 0 && index < wheelRatioCombo->count()) {
        // Check if it's a standard item
        QVariant data = wheelRatioCombo->itemData(index);
        if (data.isValid()) {
            ratio = data.toReal();
        } else {
            // Custom value entered
            QString text = wheelRatioCombo->itemText(index);
            bool ok;
            ratio = text.toDouble(&ok);
            if (!ok || ratio <= 0.0) {
                ratio = 1.0; // fallback
                wheelRatioCombo->setCurrentText("1.0");
            }
        }
    } else {
        // Custom text entered
        QString text = wheelRatioCombo->currentText();
        bool ok;
        ratio = text.toDouble(&ok);
        if (!ok || ratio <= 0.0) {
            ratio = 1.0; // fallback
            wheelRatioCombo->setCurrentText("1.0");
        }
    }
    
    emit wheelRatioChanged(ratio);
}