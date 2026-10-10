#include "ReplayControlsBar.h"
#include "DBClient.h"
#include "CONSTANTS.h"
#include "Misc/Logging/Logging.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QRandomGenerator>
#include <QSignalBlocker>
#include <QTextCharFormat>
#include <algorithm>

namespace
{
    // Glyphs from the Geometric Shapes block render in the regular UI font. ⏸ (U+23F8) is missing from it and
    // falls back to the taller color-emoji font, which grew the button height when switching to Pause.
    const QString kStartLabel = QStringLiteral(u"\u25B6 Start");
    const QString kPlayLabel = QStringLiteral(u"\u25B6 Play");
    const QString kPauseLabel = QStringLiteral(u"\u25AE\u25AE Pause");
} // namespace

/**
 * @brief Constructs the ReplayControlsBar.
 *
 * All widgets are created and laid out inside a styled amber-bordered frame so that
 * the bar is visually distinct from the rest of the top toolbar.
 */
ReplayControlsBar::ReplayControlsBar(QWidget* parent) : QWidget(parent)
{
    // Day selector
    m_dayEdit = new QDateEdit(this);
    m_dayEdit->setCalendarPopup(true);
    m_dayEdit->setDisplayFormat("yyyy-MM-dd");
    const int dayTextWidth = m_dayEdit->fontMetrics().horizontalAdvance(QStringLiteral("2026-12-31"));
    const int dayEditWidth = std::max(132, dayTextWidth + 54); // text + left/right padding + drop-down button
    m_dayEdit->setFixedWidth(dayEditWidth);
    m_dayEdit->setDate(QDate::currentDate());
    m_dayEdit->setToolTip("Replay date");

    m_randomDayBtn = new QPushButton(QStringLiteral("🎲"), this);
    m_randomDayBtn->setObjectName("ReplayRandomDayButton");
    m_randomDayBtn->setFixedWidth(28);
    m_randomDayBtn->setToolTip("Pick random replay day");

    m_dayCalendar = new QCalendarWidget(this);
    m_dayCalendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    m_dayCalendar->setNavigationBarVisible(true);
    m_dayEdit->setCalendarWidget(m_dayCalendar);

    m_dayStatusLabel = new QLabel(this);
    m_dayStatusLabel->setMinimumWidth(0);
    m_dayStatusLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_dayStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_dayStatusLabel->setToolTip("Replay date availability");
    updateReplayDayStatus("Checking replay data...", "#bbbbbb");

    // Start-time editor
    m_timeEdit = new QTimeEdit(this);
    m_timeEdit->setDisplayFormat("hh:mm");
    m_timeEdit->setTime(QTime(7, 0)); // default 07:00 (pre-market)
    m_timeEdit->setToolTip("Replay start time");

    // Speed selector
    m_speedCombo = new QComboBox(this);
    m_speedCombo->setMinimumWidth(70);
    m_speedCombo->setMaximumWidth(90);
    m_speedCombo->addItem("0.01x", static_cast<int>(Playback::Speed::SuperSlow));
    m_speedCombo->addItem("0.1x", static_cast<int>(Playback::Speed::VerySlow));
    m_speedCombo->addItem("0.5x", static_cast<int>(Playback::Speed::Half));
    m_speedCombo->addItem("1x", static_cast<int>(Playback::Speed::Normal));
    m_speedCombo->addItem("2x", static_cast<int>(Playback::Speed::Double));
    m_speedCombo->addItem("5x", static_cast<int>(Playback::Speed::Fast5x));
    m_speedCombo->addItem("10x", static_cast<int>(Playback::Speed::Fast10x));
    m_speedCombo->addItem("25x", static_cast<int>(Playback::Speed::Fast25x));
    m_speedCombo->addItem("50x", static_cast<int>(Playback::Speed::Fast50x));
    m_speedCombo->addItem("100x", static_cast<int>(Playback::Speed::Fast100x));
    m_speedCombo->addItem("Max", static_cast<int>(Playback::Speed::AsFastAsPossible));
    m_speedCombo->setCurrentIndex(3); // default 1x
    m_speedCombo->setToolTip("Replay playback speed");

    // Play / Pause button
    m_playPauseBtn = new QPushButton(kPlayLabel, this);
    m_playPauseBtn->setCheckable(true);
    updatePlayPauseButton();

    // Restart button (kept in layout at all times to avoid toolbar jitter)
    m_restartBtn = new QPushButton("↺ Restart", this);
    m_restartBtn->setToolTip("Reset replay to the configured start time");
    m_restartBtn->setEnabled(false);

    // Populate dates from on-disk replay data
    scanAndPopulateReplayDays();

    // -----------------------------------------------------------------------
    // Layout
    // Wrap everything in an inner QFrame-like container using this widget's
    // own styled border.  An inner QHBoxLayout holds the controls.
    // -----------------------------------------------------------------------
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 3, 8, 3);
    layout->setSpacing(5);
    layout->addWidget(m_dayEdit);
    layout->addWidget(m_randomDayBtn);
    layout->addWidget(m_dayStatusLabel);
    layout->addWidget(m_timeEdit);
    layout->addWidget(m_speedCombo);
    layout->addWidget(m_playPauseBtn);
    layout->addWidget(m_restartBtn);
    setLayout(layout);

    // Amber border to visually group the controls as a single "Replay" feature
    setObjectName("ReplayControlsBar");
    setStyleSheet("QWidget#ReplayControlsBar {"
                  "    background-color: #1c1800;"
                  "    border: 1.5px solid #aa7700;"
                  "    border-radius: 6px;"
                  "}"
                  "QComboBox {"
                  "    background-color: #3a3a2a;"
                  "    color: #ffffff;"
                  "    border: 1px solid #776600;"
                  "    border-radius: 3px;"
                  "    padding: 2px 4px;"
                  "}"
                  "QComboBox::drop-down { border: none; }"
                  "QComboBox QAbstractItemView {"
                  "    background-color: #3a3a2a;"
                  "    color: #ffffff;"
                  "    selection-background-color: #554400;"
                  "    border: 1px solid #776600;"
                  "}"
                  "QDateEdit {"
                  "    background-color: #3a3a2a;"
                  "    color: #ffffff;"
                  "    border: 1px solid #776600;"
                  "    border-radius: 3px;"
                  "    padding: 2px 24px 2px 4px;"
                  "}"
                  "QDateEdit::drop-down {"
                  "    subcontrol-origin: padding;"
                  "    subcontrol-position: top right;"
                  "    width: 18px;"
                  "    border-left: 1px solid #aa8800;"
                  "    background-color: #5f490a;"
                  "    border-top-right-radius: 3px;"
                  "    border-bottom-right-radius: 3px;"
                  "}"
                  "QDateEdit::drop-down:hover {"
                  "    background-color: #7a5f12;"
                  "}"
                  "QTimeEdit {"
                  "    background-color: #3a3a2a;"
                  "    color: #ffffff;"
                  "    border: 1px solid #776600;"
                  "    border-radius: 3px;"
                  "    padding: 2px 4px;"
                  "}"
                  "QCalendarWidget QWidget {"
                  "    background-color: #2f2f22;"
                  "    color: #ffffff;"
                  "}"
                  "QCalendarWidget QToolButton {"
                  "    color: #ffffff;"
                  "    background-color: #3a3a2a;"
                  "    border: 1px solid #776600;"
                  "    border-radius: 3px;"
                  "}"
                  "QCalendarWidget QMenu {"
                  "    background-color: #3a3a2a;"
                  "    color: #ffffff;"
                  "}"
                  "QCalendarWidget QSpinBox {"
                  "    background-color: #3a3a2a;"
                  "    color: #ffffff;"
                  "    selection-background-color: #554400;"
                  "}"
                  "QCalendarWidget QAbstractItemView:enabled {"
                  "    color: #ffffff;"
                  "    selection-background-color: #554400;"
                  "    selection-color: #ffffff;"
                  "}"
                  "QPushButton {"
                  "    background-color: #2E7D32;"
                  "    color: white;"
                  "    font-weight: bold;"
                  "    padding: 4px 10px;"
                  "    border-radius: 4px;"
                  "    border: none;"
                  "}"
                  "QPushButton:hover { background-color: #1B5E20; }"
                  "QPushButton:checked {"
                  "    background-color: #D84315;"
                  "}"
                  "QPushButton:checked:hover { background-color: #BF360C; }"
                  // Declared after the hover/checked rules so it wins; otherwise disabled buttons stay green.
                  "QPushButton:disabled {"
                  "    background-color: #3a3a2a;"
                  "    color: #7a7a7a;"
                  "}"
                  "QPushButton#ReplayRandomDayButton {"
                  "    background-color: #5f490a;"
                  "    border: 1px solid #aa8800;"
                  "    border-radius: 4px;"
                  "    padding: 3px 4px;"
                  "    min-width: 24px;"
                  "}"
                  "QPushButton#ReplayRandomDayButton:hover {"
                  "    background-color: #7a5f12;"
                  "}"
                  "QPushButton#ReplayRandomDayButton:disabled {"
                  "    background-color: #3a3a2a;"
                  "    color: #7a7a7a;"
                  "    border: 1px solid #5a5a5a;"
                  "}");

    // The label (and its glyph) changes with the replay state; pin the button to the largest
    // label so the toolbar layout doesn't shift on every Play/Pause toggle.
    m_playPauseBtn->ensurePolished();
    QSize playPauseSize;
    for (const QString& label: {kStartLabel, kPlayLabel, kPauseLabel})
    {
        m_playPauseBtn->setText(label);
        playPauseSize = playPauseSize.expandedTo(m_playPauseBtn->sizeHint());
    }
    m_playPauseBtn->setFixedSize(playPauseSize);
    updatePlayPauseButton();

    // Connections
    connect(m_dayEdit, &QDateEdit::dateChanged, this, &ReplayControlsBar::onReplayDayChanged);
    connect(m_randomDayBtn, &QPushButton::clicked, this, &ReplayControlsBar::onRandomDayClicked);
    connect(m_timeEdit, &QTimeEdit::timeChanged, this, &ReplayControlsBar::onReplayTimeChanged);
    connect(m_playPauseBtn, &QPushButton::clicked, this, &ReplayControlsBar::onPlayPauseClicked);
    connect(m_restartBtn, &QPushButton::clicked, this, &ReplayControlsBar::onRestartClicked);
    connect(m_speedCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index)
            {
                if (index >= 0)
                {
                    auto speed = static_cast<Playback::Speed>(m_speedCombo->itemData(index).toInt());
                    if (isVisible())
                    {
                        logInputEvent(u"ReplayControlsBar",
                                      u"set-replay-speed",
                                      {inputDetail(u"speed", m_speedCombo->itemText(index))});
                    }
                    emit replaySpeedChanged(speed);
                }
            });

    // Start with all controls disabled (Inactive state)
    updateUIControlStates();
}

// ---------------------------------------------------------------------------
// Day
// ---------------------------------------------------------------------------

void ReplayControlsBar::scanAndPopulateReplayDays()
{
    QList<QDate> availableDates;
    const QVector<DBClient::ReplayDayInfo> recordedDays = DBClient::listAvailableReplayDates();
    for (const DBClient::ReplayDayInfo& dayInfo: recordedDays)
    {
        const QVector<DBClient::ReplaySymbolInfo> symbolInfos = DBClient::listAvailableReplaySymbols(dayInfo.date);
        const bool hasCompleteReplaySymbol =
            std::any_of(symbolInfos.cbegin(),
                        symbolInfos.cend(),
                        [](const DBClient::ReplaySymbolInfo& info) { return info.isComplete(); });
        if (hasCompleteReplaySymbol)
        {
            availableDates.append(dayInfo.date);
        }
    }
    std::sort(availableDates.begin(), availableDates.end(), std::greater<QDate>());

    setAvailableReplayDays(availableDates);
}

QDate ReplayControlsBar::getSelectedReplayDay() const
{
    if (m_availableReplayDays.isEmpty())
    {
        return QDate();
    }

    const QDate selectedDate = m_dayEdit->date();
    return isReplayDayAvailable(selectedDate) ? selectedDate : QDate();
}

void ReplayControlsBar::setSelectedReplayDay(const QDate& date)
{
    if (!isReplayDayAvailable(date))
    {
        return;
    }

    const QSignalBlocker blocker(m_dayEdit);
    m_dayEdit->setDate(date);
    m_lastValidReplayDay = date;
    updateReplayDayStatusForDate(date);
}

void ReplayControlsBar::setAvailableReplayDays(const QList<QDate>& days)
{
    const QDate previouslySelectedDate = getSelectedReplayDay();
    QSet<QDate> uniqueDays;
    QList<QDate> sortedDays;
    for (const QDate& date: days)
    {
        if (!date.isValid() || uniqueDays.contains(date))
        {
            continue;
        }
        uniqueDays.insert(date);
        sortedDays.append(date);
    }
    std::sort(sortedDays.begin(), sortedDays.end(), std::greater<QDate>());
    m_availableReplayDays = uniqueDays;

    const QDate today = QDate::currentDate();
    if (sortedDays.isEmpty())
    {
        m_lastValidReplayDay = QDate();
        const QSignalBlocker blocker(m_dayEdit);
        m_dayEdit->setMinimumDate(today);
        m_dayEdit->setMaximumDate(today);
        m_dayEdit->setDate(today);
        updateReplayDayVisuals();
        updateReplayDayStatus("No replay data. Download in Downloads tab.", "#ffb347");
        updateUIControlStates();
        return;
    }

    QDate selectedDate = previouslySelectedDate;
    if (!isReplayDayAvailable(selectedDate))
    {
        selectedDate = isReplayDayAvailable(m_lastValidReplayDay) ? m_lastValidReplayDay : sortedDays.first();
    }

    {
        const QSignalBlocker blocker(m_dayEdit);
        m_dayEdit->setMinimumDate(sortedDays.last());
        m_dayEdit->setMaximumDate(today);
        m_dayEdit->setDate(selectedDate);
    }

    m_lastValidReplayDay = selectedDate;
    updateReplayDayVisuals();
    updateReplayDayStatusForDate(selectedDate);
    updateUIControlStates();
}

// ---------------------------------------------------------------------------
// Start time
// ---------------------------------------------------------------------------

QTime ReplayControlsBar::getReplayStartTime() const
{
    return m_timeEdit->time();
}

void ReplayControlsBar::setReplayStartTime(const QTime& time)
{
    const QSignalBlocker blocker(m_timeEdit);
    m_timeEdit->setTime(time);
}

// ---------------------------------------------------------------------------
// Speed
// ---------------------------------------------------------------------------

Playback::Speed ReplayControlsBar::getReplaySpeed() const
{
    int idx = m_speedCombo->currentIndex();
    if (idx >= 0 && idx < m_speedCombo->count())
        return static_cast<Playback::Speed>(m_speedCombo->itemData(idx).toInt());
    return Playback::Speed::Normal;
}

void ReplayControlsBar::setReplaySpeed(Playback::Speed speed)
{
    const QSignalBlocker blocker(m_speedCombo);
    for (int i = 0; i < m_speedCombo->count(); ++i)
    {
        if (m_speedCombo->itemData(i).toInt() == static_cast<int>(speed))
        {
            m_speedCombo->setCurrentIndex(i);
            return;
        }
    }
}

void ReplayControlsBar::setReplaySpeedControlLocked(const bool locked)
{
    if (m_replaySpeedControlLocked == locked)
    {
        return;
    }

    m_replaySpeedControlLocked = locked;
    updateUIControlStates();
}

// ---------------------------------------------------------------------------
// Play / Pause
// ---------------------------------------------------------------------------

bool ReplayControlsBar::isReplayPlaying() const
{
    return m_playPauseBtn->isChecked();
}

void ReplayControlsBar::setReplayPlaying(bool playing)
{
    m_playPauseBtn->setChecked(playing);
    updatePlayPauseButton();
}

void ReplayControlsBar::togglePlayPause()
{
    m_playPauseBtn->click();
}

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------

ReplayControlsBar::ReplayState ReplayControlsBar::getReplayState() const
{
    return m_replayState;
}

bool ReplayControlsBar::hasStartedPlayback() const
{
    return m_hasStartedPlayback;
}

void ReplayControlsBar::setReplayState(ReplayState state)
{
    if (state == ReplayState::Playing)
    {
        m_hasStartedPlayback = true;
    }
    else if (state == ReplayState::Inactive || state == ReplayState::PreloadingPaused)
    {
        m_hasStartedPlayback = false;
    }

    m_replayState = state;
    updateUIControlStates();
    updatePlayPauseButton();
}

// ---------------------------------------------------------------------------
// Timeframe sync
// ---------------------------------------------------------------------------

void ReplayControlsBar::setCurrentTimeFrame(TimeFrame tf)
{
    Q_UNUSED(tf)
    updateTimeEditStep();
}

// ---------------------------------------------------------------------------
// Deprecated no-op
// ---------------------------------------------------------------------------

void ReplayControlsBar::updateReplayInfo(const QTime& /*startTime*/, const QTime& /*endTime*/, int /*barCount*/)
{
    // No-op: full-day Databento data makes range display unnecessary.
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void ReplayControlsBar::onReplayDayChanged(const QDate& date)
{
    if (m_replayState == ReplayState::Playing || m_hasStartedPlayback)
    {
        return;
    }

    if (!isReplayDayAvailable(date))
    {
        if (isReplayDayAvailable(m_lastValidReplayDay))
        {
            const QSignalBlocker blocker(m_dayEdit);
            m_dayEdit->setDate(m_lastValidReplayDay);
        }

        const QString holidayName = replayDayHolidayName(date);
        if (!holidayName.isEmpty())
        {
            updateReplayDayStatus(QString("Holiday: %1").arg(holidayName), "#ffb347");
        }
        else
        {
            updateReplayDayStatus("Unavailable: no replay files", "#ff9f43");
        }
        return;
    }

    m_lastValidReplayDay = date;
    updateReplayDayStatusForDate(date);

    if (isVisible())
    {
        logInputEvent(u"ReplayControlsBar", u"set-replay-day", {inputDetail(u"date", date.toString(Qt::ISODate))});
    }
    emit replayDayChanged(date);
}

void ReplayControlsBar::onRandomDayClicked()
{
    if (m_replayState != ReplayState::PreloadingPaused || m_availableReplayDays.isEmpty())
    {
        return;
    }

    const QList<QDate> candidateDates = m_availableReplayDays.values();
    if (candidateDates.isEmpty())
    {
        return;
    }

    const int randomIndex = QRandomGenerator::global()->bounded(candidateDates.size());
    const QDate randomDate = candidateDates.at(randomIndex);
    if (!randomDate.isValid())
    {
        return;
    }

    if (isVisible())
    {
        logInputEvent(u"ReplayControlsBar",
                      u"pick-random-replay-day",
                      {inputDetail(u"date", randomDate.toString(Qt::ISODate))});
    }
    m_dayEdit->setDate(randomDate);
}

void ReplayControlsBar::onReplayTimeChanged(const QTime& time)
{
    if (m_replayState == ReplayState::Playing || m_hasStartedPlayback)
        return;

    if (isVisible())
    {
        logInputEvent(u"ReplayControlsBar",
                      u"set-replay-start-time",
                      {inputDetail(u"time", time.toString("HH:mm:ss"))});
    }
    emit replayStartTimeChanged(time);
}

void ReplayControlsBar::onPlayPauseClicked()
{
    bool playing = m_playPauseBtn->isChecked();
    if (isVisible())
    {
        logInputEvent(u"ReplayControlsBar", playing ? u"play-replay" : u"pause-replay");
    }

    // Emit BEFORE updating m_replayState so connected handlers can read the
    // pre-transition state (same pattern as the original ChartToolbar).
    emit replayPlayPauseToggled(playing);

    if (playing)
    {
        setReplayState(ReplayState::Playing);
    }
    else if (m_replayState == ReplayState::Playing)
    {
        setReplayState(ReplayState::Paused);
    }

    updatePlayPauseButton();
}

void ReplayControlsBar::onRestartClicked()
{
    if (isVisible())
    {
        logInputEvent(u"ReplayControlsBar", u"restart-replay");
    }
    emit replayRestartRequested();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void ReplayControlsBar::updatePlayPauseButton()
{
    if (m_playPauseBtn->isChecked())
    {
        // Replay is running — offer to pause
        m_playPauseBtn->setText(kPauseLabel);
    }
    else if (m_replayState == ReplayState::PreloadingPaused || m_replayState == ReplayState::Inactive)
    {
        // Never started yet — pressing the button will begin from scratch
        m_playPauseBtn->setText(kStartLabel);
    }
    else
    {
        // Was playing at least once, currently paused — pressing the button resumes
        m_playPauseBtn->setText(kPlayLabel);
    }
}

void ReplayControlsBar::updateUIControlStates()
{
    const bool speedControlEnabled = !m_replaySpeedControlLocked;
    const bool dayControlEnabled = !m_availableReplayDays.isEmpty();

    switch (m_replayState)
    {
    case ReplayState::Inactive:
        m_dayEdit->setEnabled(false);
        m_randomDayBtn->setEnabled(false);
        m_timeEdit->setEnabled(false);
        m_speedCombo->setEnabled(false);
        m_playPauseBtn->setEnabled(false);
        m_restartBtn->setEnabled(false);
        break;

    case ReplayState::PreloadingPaused:
        m_dayEdit->setEnabled(dayControlEnabled);
        m_randomDayBtn->setEnabled(dayControlEnabled);
        m_timeEdit->setEnabled(true);
        m_speedCombo->setEnabled(speedControlEnabled);
        m_playPauseBtn->setEnabled(true);
        m_restartBtn->setEnabled(false);
        break;

    case ReplayState::Playing:
        m_dayEdit->setEnabled(false);
        m_randomDayBtn->setEnabled(false);
        m_timeEdit->setEnabled(false);
        m_speedCombo->setEnabled(speedControlEnabled);
        m_playPauseBtn->setEnabled(true);
        m_restartBtn->setEnabled(false);
        break;

    case ReplayState::Paused:
        m_dayEdit->setEnabled(false);
        m_randomDayBtn->setEnabled(false);
        m_timeEdit->setEnabled(false);
        m_speedCombo->setEnabled(speedControlEnabled);
        m_playPauseBtn->setEnabled(true);
        m_restartBtn->setEnabled(m_hasStartedPlayback);
        break;
    }
}

void ReplayControlsBar::updateTimeEditStep()
{
    m_timeEdit->setDisplayFormat("hh:mm");
}

bool ReplayControlsBar::isReplayDayAvailable(const QDate& date) const
{
    return date.isValid() && m_availableReplayDays.contains(date);
}

QString ReplayControlsBar::replayDayHolidayName(const QDate& date) const
{
    if (!date.isValid())
    {
        return {};
    }

    return MarketCalendar::getHolidayName(date);
}

int ReplayControlsBar::completeReplaySymbolCount(const QDate& date) const
{
    if (!date.isValid())
    {
        return 0;
    }

    const QVector<DBClient::ReplaySymbolInfo> symbolInfos = DBClient::listAvailableReplaySymbols(date);
    return std::count_if(symbolInfos.cbegin(),
                         symbolInfos.cend(),
                         [](const DBClient::ReplaySymbolInfo& info) { return info.isComplete(); });
}

void ReplayControlsBar::updateReplayDayVisuals()
{
    if (m_dayCalendar == nullptr)
    {
        return;
    }

    m_dayCalendar->setDateTextFormat(QDate(), QTextCharFormat());

    QTextCharFormat availableFormat;
    availableFormat.setForeground(QColor("#72d98a"));
    availableFormat.setFontWeight(QFont::DemiBold);

    QTextCharFormat holidayFormat;
    holidayFormat.setForeground(QColor("#f7b267"));
    holidayFormat.setBackground(QColor("#4f3a16"));
    holidayFormat.setFontWeight(QFont::DemiBold);

    for (const QDate& holidayDate: MarketCalendar::HOLIDAYS_2025)
    {
        m_dayCalendar->setDateTextFormat(holidayDate, holidayFormat);
    }
    for (const QDate& holidayDate: MarketCalendar::HOLIDAYS_2026)
    {
        m_dayCalendar->setDateTextFormat(holidayDate, holidayFormat);
    }

    for (auto it = m_availableReplayDays.cbegin(); it != m_availableReplayDays.cend(); ++it)
    {
        m_dayCalendar->setDateTextFormat(*it, availableFormat);
    }
}

void ReplayControlsBar::updateReplayDayStatus(const QString& text, const QString& colorHex)
{
    m_dayStatusLabel->setText(text);
    m_dayStatusLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 10px; padding-left: 2px; }").arg(colorHex));
}

void ReplayControlsBar::updateReplayDayStatusForDate(const QDate& date)
{
    const QString holidayName = replayDayHolidayName(date);
    if (!holidayName.isEmpty() && !isReplayDayAvailable(date))
    {
        updateReplayDayStatus(QString("Holiday: %1").arg(holidayName), "#ffb347");
        return;
    }

    if (!isReplayDayAvailable(date))
    {
        updateReplayDayStatus("Unavailable: no replay files", "#ff9f43");
        return;
    }

    const int symbolCount = completeReplaySymbolCount(date);
    updateReplayDayStatus(QString("%1 symbols").arg(symbolCount), "#72d98a");
}
