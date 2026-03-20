#pragma once

#include <QWidget>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimeEdit>
#include <QPushButton>
#include <QDir>
#include <QRegularExpression>
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
    /** @brief Populates the day combo box from the on-disk replay data directory. */
    void scanAndPopulateReplayDays();

    /** @brief Returns the currently selected replay date (invalid if none). */
    [[nodiscard]] QDate getSelectedReplayDay() const;

    /** @brief Selects the given date in the day combo box (no-op if not found). */
    void setSelectedReplayDay(const QDate& date);

    /** @brief Replaces the day combo box contents with the given list. */
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

    /**
     * @brief Sets the replay UI state and updates enabled/disabled controls.
     *
     * Control enable matrix:
     * - Inactive:         all disabled
     * - PreloadingPaused: day ✔  time ✔  speed ✔  play ✔
     * - Playing:          day ✘  time ✘  speed ✔  play ✔
     * - Paused:           day ✔  time ✔  speed ✔  play ✔
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
     * @brief Emitted when the playback speed changes.
     * Thread context: Emitted from Main/GUI thread
     */
    void replaySpeedChanged(Playback::Speed speed);

  private slots:
    void onReplayDayChanged(int index);
    void onReplayTimeChanged(const QTime& time);
    void onPlayPauseClicked();

  private:
    QComboBox* m_dayCombo;       ///< Date selector
    QTimeEdit* m_timeEdit;       ///< Start-time editor
    QComboBox* m_speedCombo;     ///< Playback speed selector
    QPushButton* m_playPauseBtn; ///< Play / Pause toggle

    ReplayState m_replayState = ReplayState::Inactive;

    void updatePlayPauseButton();
    void updateUIControlStates();
    void updateTimeEditStep();
};
