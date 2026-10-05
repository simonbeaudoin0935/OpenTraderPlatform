#pragma once

#include <QWidget>
#include <array>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QColor>
#include <QToolButton>
#include <QMenu>
#include <QWidgetAction>
#include <QTime>
#include <QTimeEdit>
#include <QPushButton>
#include <QSpinBox>
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
     * @brief Enables/disables the 10-second timeframe option in the selector.
     * @param enabled True to show 10s in the selector, false to hide it.
     */
    void setTenSecondTimeFrameEnabled(bool enabled);

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
     * @brief Checks if volume auto-scale is enabled.
     * @return True if volume Y-axis auto-scales to visible range, false otherwise.
     */
    [[nodiscard]] bool isVolumeAutoScaleEnabled() const;

    /**
     * @brief Gets the volume auto-scale mode.
     * @return Mode enum value as integer.
     */
    [[nodiscard]] int getVolumeAutoScaleMode() const;

    /**
     * @brief Sets all volume settings controls at once.
     */
    void setVolumeSettings(bool autoScaleEnabled, int autoScaleMode);

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
     * @brief Checks if the BBO (best bid/offer) overlay is visible on chart.
     * @return True if BBO overlay is visible, false otherwise.
     */
    bool isBboOverlayVisible() const;

    /**
     * @brief Checks if the Level2 depth overlay is visible on chart.
     * @return True if Level2 depth overlay is visible, false otherwise.
     */
    bool isLevel2DepthOverlayVisible() const;

    /**
     * @brief Checks if the VWAP indicator is visible on chart.
     * @return True if VWAP is visible, false otherwise.
     */
    bool isVwapVisible() const;

    /**
     * @brief Sets the BBO overlay visibility state.
     * @param visible True to show BBO overlay, false to hide.
     */
    void setBboOverlayVisible(bool visible);

    /**
     * @brief Sets the Level2 depth overlay visibility state.
     * @param visible True to show Level2 depth overlay, false to hide.
     */
    void setLevel2DepthOverlayVisible(bool visible);

    /**
     * @brief Sets the VWAP visibility state.
     * @param visible True to show VWAP, false to hide.
     */
    void setVwapVisible(bool visible);

    /**
     * @brief Checks if the MACD indicator is visible on chart.
     * @return True if MACD is visible, false otherwise.
     */
    bool isMacdVisible() const;

    /**
     * @brief Checks if the strategy status panel is visible.
     * @return True if strategy status panel is visible, false otherwise.
     */
    bool isStrategyStatusPanelVisible() const;

    /**
     * @brief Sets the MACD visibility state.
     * @param visible True to show MACD, false to hide.
     */
    void setMacdVisible(bool visible);

    /**
     * @brief Sets the strategy status panel visibility state.
     * @param visible True to show strategy status panel, false to hide.
     */
    void setStrategyStatusPanelVisible(bool visible);

    /**
     * @brief Gets the VWAP source price mode.
     * @return Source mode enum value as integer.
     */
    [[nodiscard]] int getVwapSourceMode() const;

    /**
     * @brief Sets the VWAP source price mode.
     * @param mode Source mode enum value as integer.
     */
    void setVwapSourceMode(int mode);

    /**
     * @brief Gets the VWAP session reset time.
     * @return Session reset time in market timezone.
     */
    [[nodiscard]] QTime getVwapSessionResetTime() const;

    /**
     * @brief Sets the VWAP session reset time.
     * @param time Session reset time in market timezone.
     */
    void setVwapSessionResetTime(const QTime& time);

    /**
     * @brief Gets the VWAP line color.
     * @return VWAP line color.
     */
    [[nodiscard]] QColor getVwapLineColor() const;

    /**
     * @brief Sets the VWAP line color.
     * @param color VWAP line color.
     */
    void setVwapLineColor(const QColor& color);

    /**
     * @brief Gets whether an EMA slot is visible.
     * @param slot Zero-based EMA slot index [0..2].
     */
    [[nodiscard]] bool isEmaVisible(int slot) const;

    /**
     * @brief Sets an EMA slot visibility.
     * @param slot Zero-based EMA slot index [0..2].
     * @param visible True to show EMA.
     */
    void setEmaVisible(int slot, bool visible);

    /**
     * @brief Gets EMA period for a slot.
     * @param slot Zero-based EMA slot index [0..2].
     */
    [[nodiscard]] int getEmaPeriod(int slot) const;

    /**
     * @brief Gets EMA line color for a slot.
     * @param slot Zero-based EMA slot index [0..2].
     */
    [[nodiscard]] QColor getEmaColor(int slot) const;

    /**
     * @brief Sets EMA settings for a slot.
     * @param slot Zero-based EMA slot index [0..2].
     * @param period EMA length.
     * @param color EMA line color.
     */
    void setEmaSettings(int slot, int period, const QColor& color);

    /**
     * @brief Sets all MACD settings controls at once.
     */
    void setMacdSettings(int fastLength,
                         int slowLength,
                         int signalLength,
                         int macdMaType,
                         int signalMaType,
                         bool showHistogram);

    [[nodiscard]] int getMacdFastLength() const;
    [[nodiscard]] int getMacdSlowLength() const;
    [[nodiscard]] int getMacdSignalLength() const;
    [[nodiscard]] int getMacdMaType() const;
    [[nodiscard]] int getMacdSignalMaType() const;
    [[nodiscard]] bool isMacdHistogramVisible() const;

    /**
     * @brief Checks if the RSI indicator is visible on chart.
     * @return True if RSI is visible, false otherwise.
     */
    [[nodiscard]] bool isRsiVisible() const;

    /**
     * @brief Sets the RSI visibility state.
     * @param visible True to show RSI, false to hide.
     */
    void setRsiVisible(bool visible);

    /**
     * @brief Sets all RSI settings controls at once.
     */
    void setRsiSettings(int period, int overboughtLevel, int oversoldLevel, const QColor& color);

    [[nodiscard]] int getRsiPeriod() const;
    [[nodiscard]] int getRsiOverboughtLevel() const;
    [[nodiscard]] int getRsiOversoldLevel() const;
    [[nodiscard]] QColor getRsiColor() const;

    /**
     * @brief Sets the halted state of the chart's symbol.
     * @param halted True if trading is halted, false otherwise.
     * @param reason Optional halt reason (shown in tooltip when halted).
     */
    void setHalted(bool halted, const QString& reason = {});

    /**
     * @brief Sets the delayed-data state of the chart's symbol.
     * @param delayed True if data is delayed (not real-time), false otherwise.
     */
    void setDelayed(bool delayed);

    /**
     * @brief Sets the hard-to-borrow / short-sale-restriction state.
     * @param active True if SSR/HTB is active, false otherwise.
     */
    void setHardToBorrow(bool active);

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
     * @brief Sets session background colors shown in chart settings.
     * @param earlyPreMarketColor Background color for early pre-market session.
     * @param preMarketColor Background color for pre-market session.
     * @param afterHoursColor Background color for after-hours session.
     */
    void setSessionBackgroundColors(const QColor& earlyPreMarketColor,
                                    const QColor& preMarketColor,
                                    const QColor& afterHoursColor);

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
     * @brief Emitted when volume settings change.
     * Thread context: Emitted from Main/GUI thread
     * @param autoScaleEnabled True when volume Y-axis should auto-scale to visible range.
     * @param autoScaleMode Scale mode enum value.
     */
    void volumeSettingsChanged(bool autoScaleEnabled, int autoScaleMode);

    /**
     * @brief Emitted when the order visualizations visibility changes.
     * Thread context: Emitted from Main/GUI thread
     * @param visible True if order markers and position lines are visible.
     */
    void orderVisualizationsVisibilityChanged(bool visible);

    /**
     * @brief Emitted when the BBO overlay visibility changes.
     * Thread context: Emitted from Main/GUI thread
     * @param visible True if best bid/offer overlay should be shown.
     */
    void bboOverlayVisibilityChanged(bool visible);

    /**
     * @brief Emitted when the Level2 depth overlay visibility changes.
     * Thread context: Emitted from Main/GUI thread
     * @param visible True if Level2 depth overlay should be shown.
     */
    void level2DepthOverlayVisibilityChanged(bool visible);

    /**
     * @brief Emitted when the VWAP indicator visibility changes.
     * Thread context: Emitted from Main/GUI thread
     * @param visible True if VWAP should be shown.
     */
    void vwapVisibilityChanged(bool visible);

    /**
     * @brief Emitted when the VWAP settings change.
     * Thread context: Emitted from Main/GUI thread
     * @param sourceMode VWAP source mode enum value.
     * @param sessionResetTime Session reset time in market timezone.
     * @param lineColor VWAP line color.
     */
    void vwapSettingsChanged(int sourceMode, QTime sessionResetTime, QColor lineColor);

    /**
     * @brief Emitted when an EMA visibility changes.
     * Thread context: Emitted from Main/GUI thread
     * @param slot Zero-based EMA slot index [0..2].
     * @param visible True if EMA should be shown.
     */
    void emaVisibilityChanged(int slot, bool visible);

    /**
     * @brief Emitted when EMA settings change.
     * Thread context: Emitted from Main/GUI thread
     * @param slot Zero-based EMA slot index [0..2].
     * @param period EMA length.
     * @param color EMA line color.
     */
    void emaSettingsChanged(int slot, int period, QColor color);

    /**
     * @brief Emitted when the MACD indicator visibility changes.
     * Thread context: Emitted from Main/GUI thread
     * @param visible True if MACD should be shown.
     */
    void macdVisibilityChanged(bool visible);

    /**
     * @brief Emitted when the strategy status panel visibility changes.
     * Thread context: Emitted from Main/GUI thread
     * @param visible True if strategy status panel should be shown.
     */
    void strategyStatusPanelVisibilityChanged(bool visible);

    /**
     * @brief Emitted when MACD settings change.
     * Thread context: Emitted from Main/GUI thread
     * @param fastLength Fast moving average length.
     * @param slowLength Slow moving average length.
     * @param signalLength Signal moving average length.
     * @param macdMaType MA type enum value for MACD line.
     * @param signalMaType MA type enum value for signal line.
     * @param showHistogram True when histogram should be visible.
     */
    void macdSettingsChanged(int fastLength,
                             int slowLength,
                             int signalLength,
                             int macdMaType,
                             int signalMaType,
                             bool showHistogram);

    /**
     * @brief Emitted when the RSI indicator visibility changes.
     * Thread context: Emitted from Main/GUI thread
     * @param visible True if RSI should be shown.
     */
    void rsiVisibilityChanged(bool visible);

    /**
     * @brief Emitted when RSI settings change.
     * Thread context: Emitted from Main/GUI thread
     * @param period RSI period length.
     * @param overboughtLevel Overbought threshold level.
     * @param oversoldLevel Oversold threshold level.
     * @param color RSI line color.
     */
    void rsiSettingsChanged(int period, int overboughtLevel, int oversoldLevel, QColor color);

    /**
     * @brief Emitted when the wheel scrolling ratio changes.
     * Thread context: Emitted from Main/GUI thread
     * @param ratio The new wheel scrolling ratio (e.g., 0.5 for less sensitive, 2.0 for more sensitive).
     */
    void wheelRatioChanged(qreal ratio);

    /**
     * @brief Emitted when chart session background colors are changed.
     * Thread context: Emitted from Main/GUI thread
     * @param earlyPreMarketColor New early pre-market background color.
     * @param preMarketColor New pre-market background color.
     * @param afterHoursColor New after-hours background color.
     */
    void sessionBackgroundColorsChanged(const QColor& earlyPreMarketColor,
                                        const QColor& preMarketColor,
                                        const QColor& afterHoursColor);

  private slots:
    void onComboBoxChanged(int index);
    void onAutoCheckBoxChanged(Qt::CheckState state);
    void onVolumeCheckBoxChanged(Qt::CheckState state);
    void onVolumeSettingsWidgetChanged();
    void onOrdersCheckBoxChanged(Qt::CheckState state);
    void onBboCheckBoxChanged(Qt::CheckState state);
    void onLevel2DepthCheckBoxChanged(Qt::CheckState state);
    void onVwapCheckBoxChanged(Qt::CheckState state);
    void onMacdCheckBoxChanged(Qt::CheckState state);
    void onStrategyStatusCheckBoxChanged(Qt::CheckState state);
    void onRsiCheckBoxChanged(Qt::CheckState state);
    void onVwapSettingsWidgetChanged();
    void onMacdSettingsWidgetChanged();
    void onRsiSettingsWidgetChanged();
    void onEmaCheckBoxChanged(int slot, Qt::CheckState state);
    void onEmaSettingsWidgetChanged(int slot);
    void onEmaColorButtonClicked(int slot);
    void onRsiColorButtonClicked();
    void onWheelRatioChanged(int index);
    void onEarlyPreMarketColorButtonClicked();
    void onPreMarketColorButtonClicked();
    void onAfterHoursColorButtonClicked();
    void onResetSessionColorsButtonClicked();

  private:
    QComboBox* comboBox;                  ///< Timeframe dropdown
    QLabel* label;                        ///< "Timeframe:" label
    QToolButton* timeFrameSettingsButton; ///< Timeframe settings button
    QMenu* timeFrameSettingsMenu;         ///< Timeframe settings popup
    QCheckBox* autoCheckBox;              ///< Auto timeframe selection
    QCheckBox* ordersCheckBox;            ///< Order visualizations visibility
    QCheckBox* volumeCheckBox;            ///< Volume chart visibility
    QToolButton* volumeSettingsButton;    ///< Volume settings button
    QMenu* volumeSettingsMenu;            ///< Volume settings popup
    QCheckBox* volumeAutoScaleCheckBox;   ///< Volume Y-axis auto-scale toggle
    QComboBox* volumeAutoScaleModeCombo;  ///< Volume Y-axis auto-scale mode selector
    QCheckBox* bboCheckBox;               ///< BBO overlay visibility
    QCheckBox* level2DepthCheckBox;       ///< Level2 depth overlay visibility
    QCheckBox* vwapCheckBox;              ///< VWAP indicator visibility
    QCheckBox* strategyStatusCheckBox;    ///< Strategy status panel visibility
    QToolButton* vwapSettingsButton;      ///< VWAP settings button
    QMenu* vwapSettingsMenu;              ///< VWAP settings popup
    QComboBox* vwapSourceCombo;           ///< VWAP source selector
    QTimeEdit* vwapResetTimeEdit;         ///< VWAP reset time editor
    QPushButton* vwapColorButton;         ///< VWAP color selector
    QColor m_vwapLineColor;
    static constexpr int EMA_SLOT_COUNT = 3;
    std::array<QCheckBox*, EMA_SLOT_COUNT> m_emaCheckBoxes{};
    std::array<QToolButton*, EMA_SLOT_COUNT> m_emaSettingsButtons{};
    std::array<QMenu*, EMA_SLOT_COUNT> m_emaSettingsMenus{};
    std::array<QSpinBox*, EMA_SLOT_COUNT> m_emaPeriodSpins{};
    std::array<QPushButton*, EMA_SLOT_COUNT> m_emaColorButtons{};
    std::array<QColor, EMA_SLOT_COUNT> m_emaColors{};
    QCheckBox* macdCheckBox;              ///< MACD indicator visibility
    QToolButton* macdSettingsButton;      ///< MACD settings button
    QMenu* macdSettingsMenu;              ///< MACD settings popup
    QSpinBox* macdFastLengthSpin;         ///< MACD fast length
    QSpinBox* macdSlowLengthSpin;         ///< MACD slow length
    QSpinBox* macdSignalLengthSpin;       ///< MACD signal length
    QComboBox* macdMaTypeCombo;           ///< MA type for MACD line
    QComboBox* macdSignalMaTypeCombo;     ///< MA type for signal line
    QCheckBox* macdShowHistogramCheckBox; ///< MACD histogram toggle
    QCheckBox* rsiCheckBox;               ///< RSI indicator visibility
    QToolButton* rsiSettingsButton;       ///< RSI settings button
    QMenu* rsiSettingsMenu;               ///< RSI settings popup
    QSpinBox* rsiPeriodSpin;              ///< RSI period
    QSpinBox* rsiOverboughtSpin;          ///< RSI overbought level
    QSpinBox* rsiOversoldSpin;            ///< RSI oversold level
    QPushButton* rsiColorButton;          ///< RSI color selector
    QColor m_rsiLineColor;

    QToolButton* settingsButton; ///< Settings button with cog icon
    QMenu* settingsMenu;         ///< Settings popup menu
    QComboBox* wheelRatioCombo;  ///< Wheel scrolling sensitivity
    QPushButton* m_earlyPreMarketColorButton = nullptr;
    QPushButton* m_preMarketColorButton = nullptr;
    QPushButton* m_afterHoursColorButton = nullptr;
    QPushButton* m_resetSessionColorsButton = nullptr;
    QColor m_earlyPreMarketColor;
    QColor m_preMarketColor;
    QColor m_afterHoursColor;

    QLabel* m_haltedLabel;       ///< "HALTED" status indicator
    QLabel* m_delayedLabel;      ///< "DELAYED" status indicator
    QLabel* m_hardToBorrowLabel; ///< "HTB" (hard-to-borrow / SSR) status indicator
    bool m_tenSecondTimeFrameEnabled = true;

    void populateTimeFrames();
    static void applyColorButtonStyle(QPushButton* button, const QColor& color);
    [[nodiscard]] static QColor normalizedColor(const QColor& candidate, const QColor& fallback);
    [[nodiscard]] static bool isValidEmaSlot(int slot);
    [[nodiscard]] static QColor defaultEmaColorForSlot(int slot);
    [[nodiscard]] static int defaultEmaPeriodForSlot(int slot);
    void updateEmaLabel(int slot);
    void updateEmaColorButtonStyle(int slot);
    void chooseEmaColor(int slot);
    void chooseVwapColor();
    void chooseRsiColor();
    void updateSessionColorButtonStyles();
    void chooseSessionColor(QColor& targetColor, const QString& dialogTitle);
    void emitSessionBackgroundColorsChanged();
};
