#pragma once

#include <QWidget>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QToolButton>
#include <QMenu>
#include <QWidgetAction>
#include "Misc/TimeFrame.h"

/**
 * @class ChartToolbar
 * @brief A toolbar widget for chart-specific controls above the price chart.
 *
 * Provides controls for selecting the chart timeframe interval, auto timeframe,
 * volume chart visibility, order visualizations, and wheel sensitivity.
 *
 * Replay controls were moved to ReplayControlsBar in the application-level top
 * toolbar so that they remain accessible regardless of which chart is focused.
 */
class ChartToolbar : public QWidget
{
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
     * @brief Checks if order visualizations are visible.
     * @return True if order markers and position lines are visible, false otherwise.
     */
    bool isOrderVisualizationsVisible() const;

    /**
     * @brief Sets the order visualizations visibility state.
     * @param visible True to show order markers and position lines, false to hide.
     */
    void setOrderVisualizationsVisible(bool visible);

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

  signals:
    /**
     * @brief Emitted when the user selects a different timeframe.
     * Thread context: Emitted from Main/GUI thread
     * @param timeframe The newly selected TimeFrame.
     */
    void timeFrameChanged(TimeFrame timeframe);

    /**
     * @brief Emitted when the auto timeframe selection state changes.
     * Thread context: Emitted from Main/GUI thread
     * @param enabled True if auto selection is enabled, false otherwise.
     */
    void autoTimeFrameChanged(bool enabled);

    /**
     * @brief Emitted when the volume chart visibility changes.
     * Thread context: Emitted from Main/GUI thread
     * @param visible True if volume chart is visible, false otherwise.
     */
    void volumeChartVisibilityChanged(bool visible);

    /**
     * @brief Emitted when the volume auto-rescale state changes.
     * Thread context: Emitted from Main/GUI thread
     * @param enabled True if volume Y-axis should auto-rescale to visible range.
     */
    void volumeAutoRescaleChanged(bool enabled);

    /**
     * @brief Emitted when the order visualizations visibility changes.
     * Thread context: Emitted from Main/GUI thread
     * @param visible True if order markers and position lines are visible.
     */
    void orderVisualizationsVisibilityChanged(bool visible);

    /**
     * @brief Emitted when the wheel scrolling ratio changes.
     * Thread context: Emitted from Main/GUI thread
     * @param ratio The new wheel scrolling ratio (e.g., 0.5 for less sensitive, 2.0 for more sensitive).
     */
    void wheelRatioChanged(qreal ratio);

  private slots:
    void onComboBoxChanged(int index);
    void onAutoCheckBoxChanged(int state);
    void onVolumeCheckBoxChanged(int state);
    void onVolumeAutoRescaleCheckBoxChanged(int state);
    void onOrdersCheckBoxChanged(int state);
    void onWheelRatioChanged(int index);

  private:
    QComboBox* comboBox;                  ///< Timeframe dropdown
    QLabel* label;                        ///< "Timeframe:" label
    QCheckBox* autoCheckBox;              ///< Auto timeframe selection
    QCheckBox* volumeCheckBox;            ///< Volume chart visibility
    QCheckBox* volumeAutoRescaleCheckBox; ///< Volume Y-axis auto-rescale
    QCheckBox* ordersCheckBox;            ///< Order visualizations visibility

    QToolButton* settingsButton; ///< Settings button with cog icon
    QMenu* settingsMenu;         ///< Settings popup menu
    QComboBox* wheelRatioCombo;  ///< Wheel scrolling sensitivity

    void populateTimeFrames();
};
