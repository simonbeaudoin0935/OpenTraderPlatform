#include "ReplayControlsBar.h"
#include "DBClient.h"
#include <QHBoxLayout>
#include <QLabel>

/**
 * @brief Constructs the ReplayControlsBar.
 *
 * All widgets are created and laid out inside a styled amber-bordered frame so that
 * the bar is visually distinct from the rest of the top toolbar.
 */
ReplayControlsBar::ReplayControlsBar(QWidget* parent) : QWidget(parent)
{
    // Day selector
    m_dayCombo = new QComboBox(this);
    m_dayCombo->setMinimumWidth(100);
    m_dayCombo->setMaximumWidth(120);
    m_dayCombo->setToolTip("Replay date");

    // Start-time editor
    m_timeEdit = new QTimeEdit(this);
    m_timeEdit->setDisplayFormat("hh:mm");
    m_timeEdit->setTime(QTime(7, 0)); // default 07:00 (pre-market)
    m_timeEdit->setToolTip("Replay start time");

    // Speed selector
    m_speedCombo = new QComboBox(this);
    m_speedCombo->setMinimumWidth(70);
    m_speedCombo->setMaximumWidth(90);
    m_speedCombo->addItem("0.01x", static_cast<int>(ReplayEngine::PlaybackSpeed::SuperSlow));
    m_speedCombo->addItem("0.1x", static_cast<int>(ReplayEngine::PlaybackSpeed::VerySlow));
    m_speedCombo->addItem("0.5x", static_cast<int>(ReplayEngine::PlaybackSpeed::Half));
    m_speedCombo->addItem("1x", static_cast<int>(ReplayEngine::PlaybackSpeed::Normal));
    m_speedCombo->addItem("2x", static_cast<int>(ReplayEngine::PlaybackSpeed::Double));
    m_speedCombo->addItem("5x", static_cast<int>(ReplayEngine::PlaybackSpeed::Fast5x));
    m_speedCombo->addItem("10x", static_cast<int>(ReplayEngine::PlaybackSpeed::Fast10x));
    m_speedCombo->addItem("50x", static_cast<int>(ReplayEngine::PlaybackSpeed::Fast50x));
    m_speedCombo->addItem("100x", static_cast<int>(ReplayEngine::PlaybackSpeed::Fast100x));
    m_speedCombo->addItem("Max", static_cast<int>(ReplayEngine::PlaybackSpeed::AsFastAsPossible));
    m_speedCombo->setCurrentIndex(3); // default 1x
    m_speedCombo->setToolTip("Replay playback speed");

    // Play / Pause button
    m_playPauseBtn = new QPushButton("▶ Play", this);
    m_playPauseBtn->setCheckable(true);
    updatePlayPauseButton();

    // Populate dates from on-disk replay data
    scanAndPopulateReplayDays();

    // -----------------------------------------------------------------------
    // Layout
    // Wrap everything in an inner QFrame-like container using this widget's
    // own styled border.  An inner QHBoxLayout holds the controls.
    // -----------------------------------------------------------------------
    QLabel* headerLabel = new QLabel("⏱ Replay", this);
    headerLabel->setStyleSheet("QLabel { color: #ffaa00; font-weight: bold; font-size: 11px; padding-right: 4px; }");

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 3, 8, 3);
    layout->setSpacing(5);
    layout->addWidget(headerLabel);
    layout->addWidget(m_dayCombo);
    layout->addWidget(m_timeEdit);
    layout->addWidget(m_speedCombo);
    layout->addWidget(m_playPauseBtn);
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
                  "QTimeEdit {"
                  "    background-color: #3a3a2a;"
                  "    color: #ffffff;"
                  "    border: 1px solid #776600;"
                  "    border-radius: 3px;"
                  "    padding: 2px 4px;"
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
                  "QPushButton:checked:hover { background-color: #BF360C; }");

    // Connections
    connect(m_dayCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &ReplayControlsBar::onReplayDayChanged);
    connect(m_timeEdit, &QTimeEdit::timeChanged, this, &ReplayControlsBar::onReplayTimeChanged);
    connect(m_playPauseBtn, &QPushButton::clicked, this, &ReplayControlsBar::onPlayPauseClicked);
    connect(m_speedCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index)
            {
                if (index >= 0)
                {
                    auto speed = static_cast<ReplayEngine::PlaybackSpeed>(m_speedCombo->itemData(index).toInt());
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

    QString replayBaseDir = DBClient::getReplayDataDir(QDate::currentDate());
    QDir base(replayBaseDir);
    base.cdUp(); // Navigate from ReplayData/YYYY-MM-DD → ReplayData/

    if (base.exists())
    {
        const QStringList dateDirs = base.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& dirName: dateDirs)
        {
            QDate date = QDate::fromString(dirName, Qt::ISODate);
            if (!date.isValid())
                continue;

            QDir dateDir(base.absoluteFilePath(dirName));
            if (!dateDir.entryList({"*.dbn.zst"}, QDir::Files).isEmpty())
                availableDates.append(date);
        }

        std::sort(availableDates.begin(), availableDates.end(), std::greater<QDate>());
    }

    setAvailableReplayDays(availableDates);
}

QDate ReplayControlsBar::getSelectedReplayDay() const
{
    int idx = m_dayCombo->currentIndex();
    if (idx >= 0 && idx < m_dayCombo->count())
        return m_dayCombo->itemData(idx).toDate();
    return QDate();
}

void ReplayControlsBar::setSelectedReplayDay(const QDate& date)
{
    for (int i = 0; i < m_dayCombo->count(); ++i)
    {
        if (m_dayCombo->itemData(i).toDate() == date)
        {
            m_dayCombo->setCurrentIndex(i);
            return;
        }
    }
}

void ReplayControlsBar::setAvailableReplayDays(const QList<QDate>& days)
{
    m_dayCombo->clear();
    for (const QDate& date: days)
        m_dayCombo->addItem(date.toString("yyyy-MM-dd"), date);
    if (!days.isEmpty())
        m_dayCombo->setCurrentIndex(0);
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
    m_timeEdit->setTime(time);
    m_lastReplayTime = time;
}

// ---------------------------------------------------------------------------
// Speed
// ---------------------------------------------------------------------------

ReplayEngine::PlaybackSpeed ReplayControlsBar::getReplaySpeed() const
{
    int idx = m_speedCombo->currentIndex();
    if (idx >= 0 && idx < m_speedCombo->count())
        return static_cast<ReplayEngine::PlaybackSpeed>(m_speedCombo->itemData(idx).toInt());
    return ReplayEngine::PlaybackSpeed::Normal;
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

void ReplayControlsBar::setReplayState(ReplayState state)
{
    m_replayState = state;
    updateUIControlStates();
    updatePlayPauseButton();
}

// ---------------------------------------------------------------------------
// Timeframe sync
// ---------------------------------------------------------------------------

void ReplayControlsBar::setCurrentTimeFrame(TimeFrame tf)
{
    m_currentTimeFrame = tf;
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

void ReplayControlsBar::onReplayDayChanged(int index)
{
    if (m_replayState == ReplayState::Playing)
        return;

    if (index >= 0 && index < m_dayCombo->count())
        emit replayDayChanged(m_dayCombo->itemData(index).toDate());
}

void ReplayControlsBar::onReplayTimeChanged(const QTime& time)
{
    if (m_replayState == ReplayState::Playing)
        return;

    QTime steppedTime = calculateSteppedTime(m_lastReplayTime, time);

    if (steppedTime != time)
    {
        m_timeEdit->blockSignals(true);
        m_timeEdit->setTime(steppedTime);
        m_timeEdit->blockSignals(false);
    }

    m_lastReplayTime = steppedTime;
    emit replayStartTimeChanged(steppedTime);
}

void ReplayControlsBar::onPlayPauseClicked()
{
    bool playing = m_playPauseBtn->isChecked();

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

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void ReplayControlsBar::updatePlayPauseButton()
{
    if (m_playPauseBtn->isChecked())
    {
        // Replay is running — offer to pause
        m_playPauseBtn->setText("⏸ Pause");
    }
    else if (m_replayState == ReplayState::PreloadingPaused || m_replayState == ReplayState::Inactive)
    {
        // Never started yet — pressing the button will begin from scratch
        m_playPauseBtn->setText("▶ Start");
    }
    else
    {
        // Was playing at least once, currently paused — pressing the button resumes
        m_playPauseBtn->setText("▶ Play");
    }
}

void ReplayControlsBar::updateUIControlStates()
{
    switch (m_replayState)
    {
    case ReplayState::Inactive:
        m_dayCombo->setEnabled(false);
        m_timeEdit->setEnabled(false);
        m_speedCombo->setEnabled(false);
        m_playPauseBtn->setEnabled(false);
        break;

    case ReplayState::PreloadingPaused:
        m_dayCombo->setEnabled(true);
        m_timeEdit->setEnabled(true);
        m_speedCombo->setEnabled(true);
        m_playPauseBtn->setEnabled(true);
        break;

    case ReplayState::Playing:
        m_dayCombo->setEnabled(false);
        m_timeEdit->setEnabled(false);
        m_speedCombo->setEnabled(true);
        m_playPauseBtn->setEnabled(true);
        break;

    case ReplayState::Paused:
        m_dayCombo->setEnabled(true);
        m_timeEdit->setEnabled(true);
        m_speedCombo->setEnabled(true);
        m_playPauseBtn->setEnabled(true);
        break;
    }
}

void ReplayControlsBar::updateTimeEditStep()
{
    QTime currentTime = m_timeEdit->time();

    switch (m_currentTimeFrame)
    {
    case TimeFrame::ONE_MINUTE:
        m_timeEdit->setDisplayFormat("hh:mm");
        break;

    case TimeFrame::FIVE_MINUTES:
    case TimeFrame::FIFTEEN_MINUTES:
    case TimeFrame::THIRTY_MINUTES:
        m_timeEdit->setDisplayFormat("hh:mm");
        break;

    case TimeFrame::ONE_HOUR:
    case TimeFrame::FOUR_HOURS:
        m_timeEdit->setDisplayFormat("hh:00");
        break;

    default:
        m_timeEdit->setDisplayFormat("hh:00");
        break;
    }

    QTime snappedTime = snapTimeToStep(currentTime);
    if (snappedTime != currentTime)
    {
        m_timeEdit->blockSignals(true);
        m_timeEdit->setTime(snappedTime);
        m_timeEdit->blockSignals(false);
    }

    m_lastReplayTime = snappedTime;
}

QTime ReplayControlsBar::calculateSteppedTime(const QTime& oldTime, const QTime& newTime) const
{
    int stepMinutes = static_cast<int>(m_currentTimeFrame);

    if (stepMinutes <= 1)
        return newTime;

    if (!oldTime.isValid())
        return snapTimeToStep(newTime);

    int oldTotalMins = oldTime.hour() * 60 + oldTime.minute();
    int newTotalMins = newTime.hour() * 60 + newTime.minute();

    if (newTotalMins == oldTotalMins)
        return newTime;

    int direction = (newTotalMins > oldTotalMins) ? 1 : -1;
    int steppedMins = qBound(0, oldTotalMins + (direction * stepMinutes), 23 * 60 + 59);

    if (stepMinutes >= 60)
    {
        int stepHours = stepMinutes / 60;
        int hour = (steppedMins / 60 / stepHours) * stepHours;
        return QTime(hour, 0, 0);
    }

    int hour = steppedMins / 60;
    int minute = (steppedMins % 60 / stepMinutes) * stepMinutes;
    return QTime(hour, minute, 0);
}

QTime ReplayControlsBar::snapTimeToStep(const QTime& time) const
{
    int hour = time.hour();
    int minute = time.minute();
    int stepMinutes = static_cast<int>(m_currentTimeFrame);

    if (stepMinutes >= 60)
    {
        int stepHours = stepMinutes / 60;
        hour = (hour / stepHours) * stepHours;
        minute = 0;
    }
    else if (stepMinutes > 1)
    {
        minute = (minute / stepMinutes) * stepMinutes;
    }

    return QTime(hour, minute, 0);
}
