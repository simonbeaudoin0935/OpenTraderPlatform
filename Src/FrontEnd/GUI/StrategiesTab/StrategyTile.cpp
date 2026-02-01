#include "StrategyTile.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QFrame>
#include <QMouseEvent>
#include <QDateTime>
#include <QProcess>
#include <QFile>

StrategyTile::StrategyTile(const QString& strategyID,
                           const QString& name,
                           const QVector<QString>& symbols,
                           bool isRunning,
                           QWidget* parent)
    : QWidget(parent)
    , m_strategyID(strategyID)
    , m_name(name)
    , m_symbols(symbols)
    , m_isRunning(isRunning)
    , m_errorMessage("")
    , m_isSelected(false)
{
    setStyleSheet("background-color: #2b2b2b; border: 2px solid #555; border-radius: 5px; padding: 10px;");
    setMinimumSize(250, 200);
    setupUI();
}

void StrategyTile::setupUI()
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(5);

    // Title
    m_titleLabel = new QLabel(m_name);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #fff;");
    layout->addWidget(m_titleLabel);

    // Symbols
    QString symbolsStr = m_symbols.join(", ");
    m_symbolsLabel = new QLabel("Symbols: " + symbolsStr);
    m_symbolsLabel->setStyleSheet("font-size: 11px; color: #aaa;");
    m_symbolsLabel->setWordWrap(true);
    layout->addWidget(m_symbolsLabel);

    // Status
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("font-size: 11px; font-weight: bold;");
    layout->addWidget(m_statusLabel);

    // PnL (show 0 for now)
    m_pnlLabel = new QLabel("PnL: $0.00");
    m_pnlLabel->setStyleSheet("font-size: 11px; color: #ccc;");
    layout->addWidget(m_pnlLabel);

    // Positions
    m_positionsLabel = new QLabel("Open Positions: 0");
    m_positionsLabel->setStyleSheet("font-size: 11px; color: #ccc;");
    layout->addWidget(m_positionsLabel);

    // Thread info (TODO: get CPU/memory from /proc)
    m_threadInfoLabel = new QLabel("Thread: N/A (CPU: N/A)");
    m_threadInfoLabel->setStyleSheet("font-size: 10px; color: #999;");
    layout->addWidget(m_threadInfoLabel);

    // Timestamp
    m_timestampLabel = new QLabel("Updated: " + QDateTime::currentDateTime().toString("hh:mm:ss"));
    m_timestampLabel->setStyleSheet("font-size: 9px; color: #777;");
    layout->addWidget(m_timestampLabel);

    layout->addStretch();

    updateStatusDisplay();
}

void StrategyTile::setSelected(bool selected)
{
    m_isSelected = selected;
    if (selected)
    {
        setStyleSheet("background-color: #3d5a3d; border: 2px solid #4db84d; border-radius: 5px; padding: 10px;");
    }
    else
    {
        setStyleSheet("background-color: #2b2b2b; border: 2px solid #555; border-radius: 5px; padding: 10px;");
    }
}

void StrategyTile::setStatus(bool isRunning, const QString& errorMessage)
{
    m_isRunning = isRunning;
    m_errorMessage = errorMessage;
    updateStatusDisplay();
}

void StrategyTile::updateStatusDisplay()
{
    m_timestampLabel->setText("Updated: " + QDateTime::currentDateTime().toString("hh:mm:ss"));

    if (!m_errorMessage.isEmpty())
    {
        m_statusLabel->setText("Status: ERROR");
        m_statusLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #ff6b6b;");
    }
    else if (m_isRunning)
    {
        m_statusLabel->setText("Status: RUNNING");
        m_statusLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #51cf66;");
    }
    else
    {
        m_statusLabel->setText("Status: STOPPED");
        m_statusLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #ffd93d;");
    }
}

void StrategyTile::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);
    emit tileClicked(m_strategyID);
}
