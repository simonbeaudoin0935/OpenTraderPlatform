#pragma once

#include <QWidget>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include "Misc/TimeFrame.h"

/**
 * @class TimeFrameSelector
 * @brief A widget for selecting chart timeframe intervals.
 *
 * Provides a dropdown selection widget for choosing between different
 * chart timeframes (1m, 5m, 15m, 30m, 1h, 4h, 1d, 1w, 1M).
 * Emits a signal when the selected timeframe changes.
 */
class TimeFrameSelector : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs a TimeFrameSelector widget.
     * @param parent The parent widget.
     */
    explicit TimeFrameSelector(QWidget* parent = nullptr);

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

private:
    QComboBox* comboBox;  ///< The dropdown selection widget
    QLabel* label;        ///< Label showing "Timeframe:"
    QCheckBox* autoCheckBox;  ///< Checkbox for auto timeframe selection
    QCheckBox* volumeCheckBox;  ///< Checkbox for volume chart visibility

    /**
     * @brief Populates the combobox with timeframe options.
     */
    void populateTimeFrames();
};