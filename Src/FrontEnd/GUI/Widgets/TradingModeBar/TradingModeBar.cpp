#include "TradingModeBar.h"

#include <QHBoxLayout>
#include <QEvent>

// ---------------------------------------------------------------------------
// Style constants
// ---------------------------------------------------------------------------

static const QString ACTIVE_STYLE = "QLabel { background-color: #1a6e1a; color: #ffffff; padding: 4px 10px; "
                                    "border-radius: 4px; font-weight: bold; border: 1.5px solid #2ecc2e; }";

static const QString INACTIVE_STYLE = "QLabel { background-color: #2a2a2a; color: #666666; padding: 4px 10px; "
                                      "border-radius: 4px; font-weight: bold; border: 1px solid #444444; }";

static const QString INACTIVE_HOVER_STYLE = "QLabel { background-color: #333333; color: #999999; padding: 4px 10px; "
                                            "border-radius: 4px; font-weight: bold; border: 1px solid #555555; }";

// ---------------------------------------------------------------------------

TradingModeBar::TradingModeBar(QWidget* parent) : QWidget(parent)
{
    m_liveLabel = new QLabel("LIVE", this);
    m_simLabel = new QLabel("SIM", this);
    m_replayLabel = new QLabel("REPLAY", this);

    setupLabel(m_liveLabel, "LIVE", "Real money trading via TradeStation live API.\nClick to switch to LIVE mode.");
    setupLabel(m_simLabel, "SIM", "Paper trading via TradeStation simulation API.\nClick to switch to SIM mode.");
    setupLabel(m_replayLabel, "REPLAY", "Historical data replay (no network).\nClick to enter/exit REPLAY mode.");

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
    layout->addWidget(m_liveLabel);
    layout->addWidget(m_simLabel);
    layout->addWidget(m_replayLabel);
    setLayout(layout);

    applyStyles();
}

// ---------------------------------------------------------------------------

void TradingModeBar::setupLabel(QLabel* label, const QString& text, const QString& tooltip)
{
    label->setText(text);
    label->setToolTip(tooltip);
    label->setAlignment(Qt::AlignCenter);
    label->setCursor(Qt::PointingHandCursor);
    label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    label->installEventFilter(this);
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
    m_liveLabel->setStyleSheet(m_activeMode == Mode::Live ? ACTIVE_STYLE : INACTIVE_STYLE);
    m_simLabel->setStyleSheet(m_activeMode == Mode::Sim ? ACTIVE_STYLE : INACTIVE_STYLE);
    m_replayLabel->setStyleSheet(m_activeMode == Mode::Replay ? ACTIVE_STYLE : INACTIVE_STYLE);
}

// ---------------------------------------------------------------------------

bool TradingModeBar::eventFilter(QObject* watched, QEvent* event)
{
    // Use MouseButtonPress (not Release): QLabel ignores press events by default,
    // so Qt never delivers the release back to the label. Fire on press instead.
    if (event->type() == QEvent::MouseButtonPress)
    {
        if (watched == m_liveLabel && m_activeMode != Mode::Live)
        {
            emit liveRequested();
            return true;
        }
        if (watched == m_simLabel && m_activeMode != Mode::Sim)
        {
            emit simRequested();
            return true;
        }
        if (watched == m_replayLabel)
        {
            if (m_activeMode == Mode::Replay)
                emit replayExitRequested();
            else
                emit replayRequested();
            return true;
        }
    }

    // Hover highlight for inactive pills
    if (event->type() == QEvent::Enter)
    {
        auto* label = qobject_cast<QLabel*>(watched);
        if (label)
        {
            bool isActive = (label == m_liveLabel && m_activeMode == Mode::Live) ||
                            (label == m_simLabel && m_activeMode == Mode::Sim) ||
                            (label == m_replayLabel && m_activeMode == Mode::Replay);
            if (!isActive)
                label->setStyleSheet(INACTIVE_HOVER_STYLE);
        }
    }
    if (event->type() == QEvent::Leave)
    {
        auto* label = qobject_cast<QLabel*>(watched);
        if (label)
        {
            bool isActive = (label == m_liveLabel && m_activeMode == Mode::Live) ||
                            (label == m_simLabel && m_activeMode == Mode::Sim) ||
                            (label == m_replayLabel && m_activeMode == Mode::Replay);
            label->setStyleSheet(isActive ? ACTIVE_STYLE : INACTIVE_STYLE);
        }
    }

    return QWidget::eventFilter(watched, event);
}
