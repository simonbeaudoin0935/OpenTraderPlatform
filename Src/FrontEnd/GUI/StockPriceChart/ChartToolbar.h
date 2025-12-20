#pragma once

#include <QWidget>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QTimeEdit>
#include <QPushButton>
#include <QDir>
#include <QRegularExpression>
#include "Misc/TimeFrame.h"

/**
 * @class ChartToolbar
 * @brief A toolbar widget for chart controls above the price chart.
 *
 * Provides controls for selecting chart timeframe intervals, auto timeframe,
 * volume chart visibility, and market replay functionality.
 */
class ChartToolbar : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs a ChartToolbar widget.
     * @param parent The parent widget.
     */
    explicit ChartToolbar(QWidget* parent = nullptr);

    /**
     * @brief Gets the currently selected timeframe.
     * @return The selected TimeFrame enum value.
     */
    TimeFrame getCurrentTimeFrame() const;

    /**
     * @brief Sets the selected timeframe.
     * @param timeframe The TimeFrame to select.
     */
    void setCurrentTimeFrame(TimeFrame timeframe);

    /**
     * @brief Checks if auto timeframe selection is enabled.
     * @return True if auto selection is enabled, false otherwise.
     */
    bool isAutoTimeFrameEnabled() const;

    /**
     * @brief Sets the auto timeframe selection state.
     * @param enabled True to enable auto selection, false to disable.
     */
    void setAutoTimeFrameEnabled(bool enabled);

    /**
     * @brief Checks if volume chart is visible.
     * @return True if volume chart is visible, false otherwise.
     */
    bool isVolumeChartVisible() const;

    /**
     * @brief Sets the volume chart visibility state.
     * @param visible True to show volume chart, false to hide.
     */
    void setVolumeChartVisible(bool visible);

    /**
     * @brief Sets the available days for market replay.
     * @param days List of dates available for replay.
     */
    void setAvailableReplayDays(const QList<QDate>& days);

    /**
     * @brief Gets the currently selected replay day.
     * @return The selected date for replay.
     */
    QDate getSelectedReplayDay() const;

    /**
     * @brief Sets the selected replay day.
     * @param date The date to select for replay.
     */
    void setSelectedReplayDay(const QDate& date);

    /**
     * @brief Gets the replay start time.
     * @return The selected start time for replay.
     */
    QTime getReplayStartTime() const;

    /**
     * @brief Sets the replay start time.
     * @param time The start time for replay.
     */
    void setReplayStartTime(const QTime& time);

    /**
     * @brief Checks if replay is currently playing.
     * @return True if replay is playing, false otherwise.
     */
    bool isReplayPlaying() const;

    /**
     * @brief Sets the replay play/pause state.
     * @param playing True to start playing, false to pause.
     */
    void setReplayPlaying(bool playing);

    /**
     * @brief Scans the cache directory and populates available replay days.
     * Looks for files in ~/.cache/L2Trader/RecordedLiveData/Bars/
     * and extracts dates from filenames.
     */
    void scanAndPopulateReplayDays();

signals:
    /**
     * @brief Emitted when the user selects a different timeframe.
     * @param timeframe The newly selected TimeFrame.
     */
    void timeFrameChanged(TimeFrame timeframe);

    /**
     * @brief Emitted when the auto timeframe selection state changes.
     * @param enabled True if auto selection is enabled, false otherwise.
     */
    void autoTimeFrameChanged(bool enabled);

    /**
     * @brief Emitted when the volume chart visibility changes.
     * @param visible True if volume chart is visible, false otherwise.
     */
    void volumeChartVisibilityChanged(bool visible);

    /**
     * @brief Emitted when the user selects a different replay day.
     * @param date The newly selected date for replay.
     */
    void replayDayChanged(const QDate& date);

    /**
     * @brief Emitted when the replay start time changes.
     * @param time The new start time for replay.
     */
    void replayStartTimeChanged(const QTime& time);

    /**
     * @brief Emitted when the play/pause button is toggled.
     * @param playing True if replay should start playing, false if paused.
     */
    void replayPlayPauseToggled(bool playing);

private slots:
    /**
     * @brief Handles combobox selection changes.
     * @param index The index of the selected item.
     */
    void onComboBoxChanged(int index);

    /**
     * @brief Handles checkbox state changes.
     * @param state The new state of the checkbox.
     */
    void onAutoCheckBoxChanged(int state);

    /**
     * @brief Handles volume chart visibility checkbox state changes.
     * @param state The new state of the checkbox.
     */
    void onVolumeCheckBoxChanged(int state);

    /**
     * @brief Handles replay day combobox selection changes.
     * @param index The index of the selected item.
     */
    void onReplayDayChanged(int index);

    /**
     * @brief Handles replay time edit changes.
     * @param time The new time.
     */
    void onReplayTimeChanged(const QTime& time);

    /**
     * @brief Handles play/pause button clicks.
     */
    void onPlayPauseClicked();

private:
    QComboBox* comboBox;  ///< The dropdown selection widget for timeframe
    QLabel* label;        ///< Label showing "Timeframe:"
    QCheckBox* autoCheckBox;  ///< Checkbox for auto timeframe selection
    QCheckBox* volumeCheckBox;  ///< Checkbox for volume chart visibility

    QLabel* replayLabel;       ///< Label showing "Replay:"
    QComboBox* replayDayCombo; ///< Dropdown for selecting replay day
    QTimeEdit* replayTimeEdit; ///< Time input for replay start time
    QPushButton* playPauseButton; ///< Play/pause button for replay

    /**
     * @brief Populates the combobox with timeframe options.
     */
    void populateTimeFrames();

    /**
     * @brief Updates the play/pause button text based on current state.
     */
    void updatePlayPauseButton();

    /**
     * @brief Extracts date from a filename in the Bars directory.
     * @param fileName The filename to parse.
     * @return QDate extracted from filename, or invalid date if parsing fails.
     */
    QDate extractDateFromFileName(const QString& fileName);
};