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
    setStyleSheet("background-color: transparent;");
    setMinimumSize(250, 120);

    // Create refresh timer (updates every 1 second)
    m_refreshTimer = std::make_unique<QTimer>();
    connect(m_refreshTimer.get(), &QTimer::timeout, this, &StrategyTile::onRefreshTimer);
    m_refreshTimer->start(1000);

    setupUI();
}

void StrategyTile::setupUI()
{
    // Main layout for the tile
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Content frame with green border
    m_contentFrame = new QFrame(this);
    m_contentFrame->setStyleSheet(
        "background-color: #2b2b2b; border: 2px solid #555; border-radius: 8px; padding: 10px;");
    m_contentFrame->setFrameShape(QFrame::StyledPanel);
    m_contentFrame->setFrameShadow(QFrame::Plain);
    mainLayout->addWidget(m_contentFrame);

    // Layout inside the content frame
    auto contentLayout = new QVBoxLayout(m_contentFrame);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(5);

    // Title
    m_titleLabel = new QLabel(m_name);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #fff;");
    contentLayout->addWidget(m_titleLabel);

    // Symbols
    QString symbolsStr = m_symbols.join(", ");
    m_symbolsLabel = new QLabel("Symbols: " + symbolsStr);
    m_symbolsLabel->setStyleSheet("font-size: 11px; color: #aaa;");
    m_symbolsLabel->setWordWrap(true);
    contentLayout->addWidget(m_symbolsLabel);

    // Status
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("font-size: 11px; font-weight: bold;");
    contentLayout->addWidget(m_statusLabel);

    // Positions
    m_positionsLabel = new QLabel("Open Positions: 0");
    m_positionsLabel->setStyleSheet("font-size: 11px; color: #ccc;");
    contentLayout->addWidget(m_positionsLabel);

    // Thread info (will be updated by timer)
    m_threadInfoLabel = new QLabel("Thread: - (CPU: -%)");
    m_threadInfoLabel->setStyleSheet("font-size: 10px; color: #999;");
    contentLayout->addWidget(m_threadInfoLabel);

    contentLayout->addStretch();

    updateStatusDisplay();
    updateThreadAndMemoryInfo();
}

void StrategyTile::setSelected(bool selected)
{
    m_isSelected = selected;
    // No visual feedback for selection - panel always visible anyway
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
