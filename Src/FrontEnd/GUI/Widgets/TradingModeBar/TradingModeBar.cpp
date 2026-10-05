#include "TradingModeBar.h"

#include <QHBoxLayout>

#include "Misc/Logging/Logging.h"

// ---------------------------------------------------------------------------
// Style constants
// ---------------------------------------------------------------------------

static const QString ACTIVE_STYLE = "QPushButton { background-color: #1a6e1a; color: #ffffff; padding: 4px 10px; "
                                    "border-radius: 4px; font-weight: bold; border: 1.5px solid #2ecc2e; }"
                                    "QPushButton:hover { background-color: #1e7e1e; }";

static const QString INACTIVE_STYLE =
    "QPushButton { background-color: #2a2a2a; color: #666666; padding: 4px 10px; "
    "border-radius: 4px; font-weight: bold; border: 1px solid #444444; }"
    "QPushButton:hover { background-color: #333333; color: #999999; border: 1px solid #555555; }";

// ---------------------------------------------------------------------------

TradingModeBar::TradingModeBar(QWidget* parent) : QWidget(parent)
{
    m_liveBtn = new QPushButton("LIVE", this);
    m_simBtn = new QPushButton("SIM", this);
    m_replayBtn = new QPushButton("REPLAY", this);
    m_reviewBtn = new QPushButton("REVIEW", this);

    setupButton(m_liveBtn, "LIVE", "Real money trading via TradeStation live API.\nClick to switch to LIVE mode.");
    setupButton(m_simBtn, "SIM", "Paper trading via TradeStation simulation API.\nClick to switch to SIM mode.");
    setupButton(m_replayBtn, "REPLAY", "Historical data replay (no network).\nClick to enter/exit REPLAY mode.");
    setupButton(m_reviewBtn,
                "REVIEW",
                "Read-only inspection of a persisted replay ledger.\nClick to enter REVIEW mode.");

    connect(m_liveBtn,
            &QPushButton::clicked,
            this,
            [this]()
            {
                if (m_activeMode != Mode::Live)
                {
                    logInputEvent(u"TradingModeBar", u"request-live-mode");
                    emit liveRequested();
                }
            });

    connect(m_simBtn,
            &QPushButton::clicked,
            this,
            [this]()
            {
                if (m_activeMode != Mode::Sim)
                {
                    logInputEvent(u"TradingModeBar", u"request-sim-mode");
                    emit simRequested();
                }
            });

    connect(m_replayBtn,
            &QPushButton::clicked,
            this,
            [this]()
            {
                if (m_activeMode == Mode::Replay)
                {
                    logInputEvent(u"TradingModeBar", u"request-exit-replay-mode");
                    emit replayExitRequested();
                }
                else
                {
                    logInputEvent(u"TradingModeBar", u"request-enter-replay-mode");
                    emit replayRequested();
                }
            });

    connect(m_reviewBtn,
            &QPushButton::clicked,
            this,
            [this]()
            {
                if (m_activeMode != Mode::Review)
                {
                    logInputEvent(u"TradingModeBar", u"request-review-mode");
                    emit reviewRequested();
                }
            });

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
    layout->addWidget(m_liveBtn);
    layout->addWidget(m_simBtn);
    layout->addWidget(m_replayBtn);
    layout->addWidget(m_reviewBtn);
    setLayout(layout);

    applyStyles();
}

// ---------------------------------------------------------------------------

void TradingModeBar::setupButton(QPushButton* btn, const QString& text, const QString& tooltip)
{
    btn->setText(text);
    btn->setToolTip(tooltip);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    btn->setFocusPolicy(Qt::NoFocus);
}

// ---------------------------------------------------------------------------

void TradingModeBar::setActiveMode(Mode mode)
{
    m_activeMode = mode;
    applyStyles();
}

TradingModeBar::Mode TradingModeBar::activeMode() const
{
    return m_activeMode;
}

// ---------------------------------------------------------------------------

void TradingModeBar::applyStyles()
{
    m_liveBtn->setStyleSheet(m_activeMode == Mode::Live ? ACTIVE_STYLE : INACTIVE_STYLE);
    m_simBtn->setStyleSheet(m_activeMode == Mode::Sim ? ACTIVE_STYLE : INACTIVE_STYLE);
    m_replayBtn->setStyleSheet(m_activeMode == Mode::Replay ? ACTIVE_STYLE : INACTIVE_STYLE);
    m_reviewBtn->setStyleSheet(m_activeMode == Mode::Review ? ACTIVE_STYLE : INACTIVE_STYLE);
}
