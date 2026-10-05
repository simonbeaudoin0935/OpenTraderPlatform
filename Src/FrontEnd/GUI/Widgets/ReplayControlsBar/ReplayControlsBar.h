#pragma once

#include <QWidget>
#include <QComboBox>
#include <QDateEdit>
#include <QCalendarWidget>
#include <QLabel>
#include <QTimeEdit>
#include <QPushButton>
#include <QSet>
#include "Misc/TimeFrame.h"
#include "PlaybackTypes.h"

/**
 * @class ReplayControlsBar
 * @brief A self-contained widget that groups all replay controls into a visually distinct bar.
 *
 * Displays a date selector, start-time editor, playback-speed selector and a play/pause
 * button, all contained in an amber-bordered frame to clearly identify them as a unit.
 *
 * The bar lives in the application-level top toolbar (next to the clock), NOT inside
 * the chart toolbar.  It is hidden when the application is in live mode and shown when
 * replay mode is entered.
 */
class ReplayControlsBar : public QWidget
{
    Q_OBJECT

  public:
    /**
     * @brief UI state of the replay controls.
     */
    enum class ReplayState
    {
        Inactive,         ///< Replay mode not active — all controls disabled
        PreloadingPaused, ///< Data loaded, waiting for first Play press
        Playing,          ///< Actively replaying
        Paused            ///< Mid-playback pause — can resume
    };

    explicit ReplayControlsBar(QWidget* parent = nullptr);

    // -----------------------------------------------------------------------
    // Day
    // -----------------------------------------------------------------------
    /** @brief Populates the replay-day picker from on-disk replay availability. */
    void scanAndPopulateReplayDays();

    /** @brief Returns the currently selected replay date (invalid if none). */
    [[nodiscard]] QDate getSelectedReplayDay() const;

    /** @brief Selects the given date in the replay-day picker (no-op if unavailable). */
    void setSelectedReplayDay(const QDate& date);

    /** @brief Replaces the available replay days with the given list. */
    void setAvailableReplayDays(const QList<QDate>& days);

    // -----------------------------------------------------------------------
    // Start time
    // -----------------------------------------------------------------------
    /** @brief Returns the current start time shown in the time editor. */
    [[nodiscard]] QTime getReplayStartTime() const;

    /** @brief Sets the start time shown in the time editor. */
    void setReplayStartTime(const QTime& time);

    // -----------------------------------------------------------------------
    // Speed
    // -----------------------------------------------------------------------
    /** @brief Returns the currently selected playback speed. */
    [[nodiscard]] Playback::Speed getReplaySpeed() const;

    /** @brief Sets the playback speed shown in the combo box without emitting a signal. */
    void setReplaySpeed(Playback::Speed speed);
    /** @brief Temporarily disables only the speed selector (used during manual confirmations). */
    void setReplaySpeedControlLocked(bool locked);

    // -----------------------------------------------------------------------
    // Play / Pause
    // -----------------------------------------------------------------------
    /** @brief Returns true if the play button is currently checked (playing). */
    [[nodiscard]] bool isReplayPlaying() const;

    /** @brief Updates the play/pause button appearance without emitting a signal. */
    void setReplayPlaying(bool playing);

    /** @brief Programmatically clicks the play/pause button (emits the signal). */
    void togglePlayPause();

    // -----------------------------------------------------------------------
    // State machine
    // -----------------------------------------------------------------------
    /** @brief Returns the current replay UI state. */
    [[nodiscard]] ReplayState getReplayState() const;

    /** @brief Returns true once replay playback has started at least once in the current session. */
    [[nodiscard]] bool hasStartedPlayback() const;

    /**
     * @brief Sets the replay UI state and updates enabled/disabled controls.
     *
     * Control enable matrix:
     * - Inactive:         all disabled
     * - PreloadingPaused: day ✔  time ✔  speed ✔  play ✔
     * - Playing:          day ✘  time ✘  speed ✔  play ✔
     * - Paused:           day ✘  time ✘  speed ✔  play ✔
     */
    void setReplayState(ReplayState state);

    // -----------------------------------------------------------------------
    // Timeframe sync (adjusts time-edit step granularity)
    // -----------------------------------------------------------------------
    /** @brief Informs the bar of the current chart timeframe so the time editor steps correctly. */
    void setCurrentTimeFrame(TimeFrame tf);

    // -----------------------------------------------------------------------
    // Deprecated / no-op kept for API compatibility
    // -----------------------------------------------------------------------
    /** @deprecated Full-day Databento data makes the info label unnecessary. */
    void updateReplayInfo(const QTime& startTime, const QTime& endTime, int barCount);

  signals:
    /**
     * @brief Emitted when the user selects a different replay date.
     * Thread context: Emitted from Main/GUI thread
     */
    void replayDayChanged(const QDate& date);

    /**
     * @brief Emitted when the replay start time changes.
     * Thread context: Emitted from Main/GUI thread
     */
    void replayStartTimeChanged(const QTime& time);

    /**
     * @brief Emitted when the play/pause button is toggled.
     * Thread context: Emitted from Main/GUI thread
     * @param playing True → start/resume, false → pause
     */
    void replayPlayPauseToggled(bool playing);

    /**
     * @brief Emitted when the user clicks the Restart button.
     * Thread context: Emitted from Main/GUI thread
     */
    void replayRestartRequested();

    /**
     * @brief Emitted when the playback speed changes.
     * Thread context: Emitted from Main/GUI thread
     */
    void replaySpeedChanged(Playback::Speed speed);

  private slots:
    void onReplayDayChanged(const QDate& date);
    void onRandomDayClicked();
    void onReplayTimeChanged(const QTime& time);
    void onPlayPauseClicked();
    void onRestartClicked();

  private:
    QDateEdit* m_dayEdit;           ///< Replay day picker
    QPushButton* m_randomDayBtn;    ///< Random replay-day picker button
    QCalendarWidget* m_dayCalendar; ///< Calendar popup for replay-day picker
    QLabel* m_dayStatusLabel;       ///< Inline replay-day availability status
    QTimeEdit* m_timeEdit;          ///< Start-time editor
    QComboBox* m_speedCombo;        ///< Playback speed selector
    QPushButton* m_playPauseBtn;    ///< Play / Pause toggle
    QPushButton* m_restartBtn;      ///< Reset replay to the configured start
    QSet<QDate> m_availableReplayDays;
    QDate m_lastValidReplayDay;

    ReplayState m_replayState = ReplayState::Inactive;
    bool m_hasStartedPlayback = false;
    bool m_replaySpeedControlLocked = false;

    [[nodiscard]] bool isReplayDayAvailable(const QDate& date) const;
    [[nodiscard]] QString replayDayHolidayName(const QDate& date) const;
    [[nodiscard]] int completeReplaySymbolCount(const QDate& date) const;
    void updateReplayDayVisuals();
    void updateReplayDayStatus(const QString& text, const QString& colorHex);
    void updateReplayDayStatusForDate(const QDate& date);
    void updatePlayPauseButton();
    void updateUIControlStates();
    void updateTimeEditStep();
};
