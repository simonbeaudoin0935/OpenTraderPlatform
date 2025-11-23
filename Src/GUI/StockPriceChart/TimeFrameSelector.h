#pragma once

#include <QWidget>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
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

signals:
    /**
     * @brief Emitted when the user selects a different timeframe.
     * @param timeframe The newly selected TimeFrame.
     */
    void timeFrameChanged(TimeFrame timeframe);

private slots:
    /**
     * @brief Handles combobox selection changes.
     * @param index The index of the selected item.
     */
    void onComboBoxChanged(int index);

private:
    QComboBox* comboBox;  ///< The dropdown selection widget
    QLabel* label;        ///< Label showing "Timeframe:"

    /**
     * @brief Populates the combobox with timeframe options.
     */
    void populateTimeFrames();
};