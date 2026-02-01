#include "StrategyTile.h"
#include "StrategyManager.h"
#include "ThreadStats.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QFrame>
#include <QMouseEvent>
#include <QDateTime>
#include <QTimer>

StrategyTile::StrategyTile(const QString& strategyID,
                           const QString& name,
                           const QVector<QString>& symbols,
                           bool isRunning,
                           StrategyManager* p_strategyManager,
                           QWidget* parent)
    : QWidget(parent)
    , m_strategyID(strategyID)
    , m_name(name)
    , m_symbols(symbols)
    , m_isRunning(isRunning)
    , m_errorMessage("")
    , m_isSelected(false)
    , m_strategyManager(p_strategyManager)
{
    setStyleSheet("background-color: #2b2b2b; border: 2px solid #555; border-radius: 5px; padding: 10px;");
    setMinimumSize(250, 200);

    // Create refresh timer (updates every 1 second)
    m_refreshTimer = std::make_unique<QTimer>();
    connect(m_refreshTimer.get(), &QTimer::timeout, this, &StrategyTile::onRefreshTimer);
    m_refreshTimer->start(1000);

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

    // Thread info (will be updated by timer)
    m_threadInfoLabel = new QLabel("Thread: - (CPU: -%)");
    m_threadInfoLabel->setStyleSheet("font-size: 10px; color: #999;");
    layout->addWidget(m_threadInfoLabel);

    // Timestamp (will be updated by timer)
    m_timestampLabel = new QLabel("Updated: " + QDateTime::currentDateTime().toString("hh:mm:ss"));
    m_timestampLabel->setStyleSheet("font-size: 9px; color: #777;");
    layout->addWidget(m_timestampLabel);

    layout->addStretch();

    updateStatusDisplay();
    updateThreadAndMemoryInfo();
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

void StrategyTile::refreshDisplay()
{
    onRefreshTimer();
}

void StrategyTile::onRefreshTimer()
{
    m_timestampLabel->setText("Updated: " + QDateTime::currentDateTime().toString("hh:mm:ss"));
    updateThreadAndMemoryInfo();

    // Update positions count
    if (m_strategyManager)
    {
        int posCount = m_strategyManager->getStrategyPositionCount(m_strategyID);
        m_positionsLabel->setText("Open Positions: " + QString::number(posCount));
    }
}

void StrategyTile::updateStatusDisplay()
{
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

void StrategyTile::updateThreadAndMemoryInfo()
{
    if (!m_strategyManager)
    {
        m_threadInfoLabel->setText("Thread: - (CPU: -%, Mem: -)");
        return;
    }

    // Get thread ID
    qint64 threadId = m_strategyManager->getStrategyThreadId(m_strategyID);
    if (threadId <= 0)
    {
        m_threadInfoLabel->setText("Thread: - (CPU: -%, Mem: -)");
        return;
    }

    // Get CPU and memory stats
    ThreadStats::Stats stats = ThreadStats::getThreadStats(threadId);

    if (!stats.valid)
    {
        m_threadInfoLabel->setText(QString("Thread: %1 (CPU: -%, Mem: -)").arg(threadId));
        return;
    }

    // Format memory (KB or MB)
    QString memStr;
    if (stats.memoryBytes < 1024 * 1024)
    {
        memStr = QString("%1 KB").arg(stats.memoryBytes / 1024);
    }
    else
    {
        memStr = QString("%1 MB").arg(stats.memoryBytes / (1024 * 1024));
    }

    m_threadInfoLabel->setText(
        QString("Thread: %1 (CPU: %2%, Mem: %3)").arg(threadId).arg((int)stats.cpuUsagePercent).arg(memStr));
}

void StrategyTile::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);
    emit tileClicked(m_strategyID);
}
