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
#include <QToolButton>
#include <QMenu>
#include <QWidgetAction>
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
     * @brief Checks if volume auto-rescale is enabled.
     * @return True if volume Y-axis auto-rescales to visible range, false otherwise.
     */
    bool isVolumeAutoRescaleEnabled() const;

    /**
     * @brief Sets the volume auto-rescale state.
     * @param enabled True to auto-rescale volume Y-axis to visible range, false to use full data range.
     */
    void setVolumeAutoRescaleEnabled(bool enabled);

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
     * @brief Updates the replay info label with time range and bar count.
     * @param startTime The start time of available data.
     * @param endTime The end time of available data.
     * @param barCount The number of bars available.
     */
    void updateReplayInfo(const QTime& startTime, const QTime& endTime, int barCount);

    /**
     * @brief Gets the current wheel scrolling ratio.
     * @return The wheel scrolling ratio.
     */
    qreal getWheelRatio() const;

    /**
     * @brief Sets the wheel scrolling ratio.
     * @param ratio The wheel scrolling ratio.
     */
    void setWheelRatio(qreal ratio);

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
     * @brief Emitted when the volume auto-rescale state changes.
     * @param enabled True if volume Y-axis should auto-rescale to visible range.
     */
    void volumeAutoRescaleChanged(bool enabled);

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

    /**
     * @brief Emitted when the wheel scrolling ratio changes.
     * @param ratio The new wheel scrolling ratio (e.g., 0.5 for less sensitive, 2.0 for more sensitive).
     */
    void wheelRatioChanged(qreal ratio);

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
     * @brief Handles volume auto-rescale checkbox state changes.
     * @param state The new state of the checkbox.
     */
    void onVolumeAutoRescaleCheckBoxChanged(int state);

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

    /**
     * @brief Handles wheel ratio combo box changes.
     * @param index The index of the selected item.
     */
    void onWheelRatioChanged(int index);

private:
    QComboBox* comboBox;  ///< The dropdown selection widget for timeframe
    QLabel* label;        ///< Label showing "Timeframe:"
    QCheckBox* autoCheckBox;  ///< Checkbox for auto timeframe selection
    QCheckBox* volumeCheckBox;  ///< Checkbox for volume chart visibility
    QCheckBox* volumeAutoRescaleCheckBox;  ///< Checkbox for volume Y-axis auto-rescale to visible range

    QLabel* replayLabel;       ///< Label showing "Replay:"
    QLabel* replayInfoLabel;   ///< Label showing replay time range and bar count info
    QComboBox* replayDayCombo; ///< Dropdown for selecting replay day
    QTimeEdit* replayTimeEdit; ///< Time input for replay start time
    QPushButton* playPauseButton; ///< Play/pause button for replay

    QToolButton* settingsButton; ///< Settings button with cog icon
    QMenu* settingsMenu;         ///< Settings popup menu
    QComboBox* wheelRatioCombo;  ///< Combo box for wheel scrolling ratio

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