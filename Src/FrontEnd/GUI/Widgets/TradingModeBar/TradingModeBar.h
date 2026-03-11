#pragma once

#include <QWidget>
#include <QPushButton>
#include "Core/MainApp.h"

/**
 * @class TradingModeBar
 * @brief A tristate mode indicator showing LIVE, SIM, and REPLAY as clickable pills.
 *
 * The active mode is highlighted green; the other two are rendered in a muted grey.
 * Clicking a grey pill triggers the corresponding mode switch (SIM↔LIVE requires
 * restart; REPLAY enters/exits replay mode).
 *
 * Three modes are represented:
 *  - LIVE   → TradingMode::Live + not in replay
 *  - SIM    → TradingMode::Sim  + not in replay
 *  - REPLAY → any TradingMode   + in replay
 */
class TradingModeBar : public QWidget
{
    Q_OBJECT

  public:
    enum class Mode
    {
        Live,
        Sim,
        Replay
    };

    explicit TradingModeBar(QWidget* parent = nullptr);

    /** @brief Updates which pill is highlighted as the current active mode. */
    void setActiveMode(Mode mode);

    /** @brief Returns the currently highlighted mode. */
    [[nodiscard]] Mode activeMode() const;

  signals:
    /**
     * @brief Emitted when the user clicks the LIVE pill while not in LIVE mode.
     * Thread context: Emitted from Main/GUI thread
     */
    void liveRequested();

    /**
     * @brief Emitted when the user clicks the SIM pill while not in SIM mode.
     * Thread context: Emitted from Main/GUI thread
     */
    void simRequested();

    /**
     * @brief Emitted when the user clicks the REPLAY pill while not in REPLAY mode.
     * Thread context: Emitted from Main/GUI thread
     */
    void replayRequested();

    /**
     * @brief Emitted when the user clicks the REPLAY pill while already in REPLAY mode.
     * Thread context: Emitted from Main/GUI thread
     */
    void replayExitRequested();

  private:
    QPushButton* m_liveBtn;
    QPushButton* m_simBtn;
    QPushButton* m_replayBtn;

    Mode m_activeMode = Mode::Live;

    void applyStyles();
    void setupButton(QPushButton* btn, const QString& text, const QString& tooltip);
};
