#include "StrategyDetailsPanel.h"
#include "StrategyManager.h"
#include "StrategyConfig.h"
#include "Order.h"
#include "Position.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextEdit>
#include <QTimer>
#include <QDateTime>
#include <QProcess>
#include <QStringList>
#include <QDebug>
#include <iomanip>
#include <sstream>

StrategyDetailsPanel::StrategyDetailsPanel(StrategyManager* p_strategyManager, QWidget* parent)
    : QWidget(parent), m_strategyManager(p_strategyManager), m_currentStrategyID("")
{
    setStyleSheet("background-color: #1e1e1e; border-left: 1px solid #444;");
    setupUI();

    // Connect to balance updates
    if (m_strategyManager)
    {
        connect(m_strategyManager,
                &StrategyManager::strategyBalanceUpdated,
                this,
                &StrategyDetailsPanel::onStrategyBalanceUpdated);
    }

    // Setup timer for periodic stats refresh (CPU/Memory)
    m_statsRefreshTimer = std::make_unique<QTimer>(this);
    connect(m_statsRefreshTimer.get(), &QTimer::timeout, this, &StrategyDetailsPanel::onRefreshStatsTimer);
}

StrategyDetailsPanel::~StrategyDetailsPanel()
{
    if (m_statsRefreshTimer)
    {
        m_statsRefreshTimer->stop();
    }
}

void StrategyDetailsPanel::setupUI()
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    // Title
    m_titleLabel = new QLabel("Strategy Details");
    m_titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #fff;");
    layout->addWidget(m_titleLabel);

    // Status and Balance (side by side)
    auto statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("Status: -");
    m_statusLabel->setStyleSheet("font-size: 11px; color: #aaa;");
    statusLayout->addWidget(m_statusLabel);

    m_balanceLabel = new QLabel("Balance: -");
    m_balanceLabel->setStyleSheet("font-size: 11px; color: #aaa;");
    statusLayout->addWidget(m_balanceLabel);

    statusLayout->addStretch();
    layout->addLayout(statusLayout);

    // CPU/Memory stats
    m_statsLabel = new QLabel();
    m_statsLabel->setStyleSheet("font-size: 10px; color: #666;");
    layout->addWidget(m_statsLabel);

    // Orders section
    auto ordersHeaderLabel = new QLabel("Recent Orders:");
    ordersHeaderLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #fff; margin-top: 10px;");
    layout->addWidget(ordersHeaderLabel);

    m_ordersDisplay = new QTextEdit();
    m_ordersDisplay->setReadOnly(true);
    m_ordersDisplay->setStyleSheet("background-color: #2d2d2d; color: #aaa; font-family: monospace; "
                                   "font-size: 9px; border: 1px solid #444;");
    m_ordersDisplay->setMaximumHeight(100);
    layout->addWidget(m_ordersDisplay);

    // Positions section
    auto positionsHeaderLabel = new QLabel("Open Positions:");
    positionsHeaderLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #fff;");
    layout->addWidget(positionsHeaderLabel);

    m_positionsDisplay = new QTextEdit();
    m_positionsDisplay->setReadOnly(true);
    m_positionsDisplay->setStyleSheet("background-color: #2d2d2d; color: #aaa; font-family: monospace; "
                                      "font-size: 9px; border: 1px solid #444;");
    m_positionsDisplay->setMaximumHeight(100);
    layout->addWidget(m_positionsDisplay);

    // Control buttons
    auto buttonLayout = new QHBoxLayout();
    m_stopButton = new QPushButton("Stop");
    m_stopButton->setMaximumWidth(80);
    buttonLayout->addWidget(m_stopButton);

    m_viewLogsButton = new QPushButton("View Logs");
    m_viewLogsButton->setMaximumWidth(80);
    buttonLayout->addWidget(m_viewLogsButton);

    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    // Empty state
    m_emptyStateWidget = new QWidget();
    auto emptyLayout = new QVBoxLayout(m_emptyStateWidget);
    auto emptyLabel = new QLabel("Select a strategy to view details");
    emptyLabel->setStyleSheet("color: #666; font-size: 12px;");
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyLabel);
    emptyLayout->addStretch();

    clearStrategy();
}

void StrategyDetailsPanel::setStrategy(const QString& strategyID)
{
    m_currentStrategyID = strategyID;

    // Start refresh timer when a strategy is selected
    if (!m_currentStrategyID.isEmpty() && m_statsRefreshTimer)
    {
        m_statsRefreshTimer->start(1000); // Refresh every second
    }

    updateDisplay();
}

void StrategyDetailsPanel::clearStrategy()
{
    m_currentStrategyID = "";

    // Stop refresh timer
    if (m_statsRefreshTimer)
    {
        m_statsRefreshTimer->stop();
    }

    m_titleLabel->setText("Strategy Details");
    m_statusLabel->setText("Status: -");
    m_balanceLabel->setText("Balance: -");
    m_statsLabel->setText("");
    m_ordersDisplay->clear();
    m_positionsDisplay->clear();
    m_stopButton->setEnabled(false);
    m_viewLogsButton->setEnabled(false);
    m_currentBalance = 0.0;
}

void StrategyDetailsPanel::updateDisplay()
{
    if (m_currentStrategyID.isEmpty())
    {
        clearStrategy();
        return;
    }

    StrategyConfig config = m_strategyManager->getStrategyConfig(m_currentStrategyID);
    bool isRunning = m_strategyManager->isStrategyRunning(m_currentStrategyID);

    // Update title
    m_titleLabel->setText("Strategy: " + config.name);

    // Update status
    m_statusLabel->setText(isRunning ? "Status: RUNNING" : "Status: STOPPED");
    m_stopButton->setEnabled(isRunning);

    // Update balance display
    m_balanceLabel->setText(QString::asprintf("Balance: $%.2f", m_currentBalance));

    // Update orders and positions
    updateOrdersList();
    updatePositionsList();
    updateStats();
}

void StrategyDetailsPanel::updateOrdersList()
{
    if (m_currentStrategyID.isEmpty())
    {
        return;
    }

    QVector<Order> recentOrders = m_strategyManager->getStrategyRecentOrders(m_currentStrategyID, 20);

    QString ordersText;
    if (recentOrders.isEmpty())
    {
        ordersText = "No orders yet";
    }
    else
    {
        // Show last 5 orders
        int start = std::max(0, (int)recentOrders.size() - 5);
        for (int i = recentOrders.size() - 1; i >= start; --i)
        {
            const auto& order = recentOrders[i];
            QString limitStr =
                order.getLimitPrice().has_value() ? QString::asprintf("%.2f", order.getLimitPrice().value()) : "N/A";
            ordersText += QString("%1 %2 %3 @ $%4 [%5]\n")
                              .arg(order.getSymbol())
                              .arg(order.getTradeAction())
                              .arg(order.getQuantity())
                              .arg(limitStr)
                              .arg(order.getStatusDescription());
        }
    }

    m_ordersDisplay->setPlainText(ordersText);
}

void StrategyDetailsPanel::updatePositionsList()
{
    if (m_currentStrategyID.isEmpty())
    {
        return;
    }

    QVector<Position> positions = m_strategyManager->getStrategyOpenPositions(m_currentStrategyID);

    QString positionsText;
    if (positions.isEmpty())
    {
        positionsText = "No open positions";
    }
    else
    {
        double totalPnL = 0.0;
        for (const auto& pos: positions)
        {
            double avgPrice = pos.getAveragePrice().toDouble();
            double lastPrice = pos.getLast().toDouble();
            double pnl = avgPrice > 0 ? pos.getQuantity().toInt() * (lastPrice - avgPrice) : 0.0;
            totalPnL += pnl;

            positionsText += QString("%1: %2 @ $%3 [PnL: $%4]\n")
                                 .arg(pos.getSymbol())
                                 .arg(pos.getQuantity())
                                 .arg(pos.getAveragePrice())
                                 .arg(QString::number(pnl, 'f', 2));
        }
        positionsText += QString("\nTotal PnL: $%1").arg(QString::number(totalPnL, 'f', 2));
    }

    m_positionsDisplay->setPlainText(positionsText);
}

void StrategyDetailsPanel::updateStats()
{
    if (m_currentStrategyID.isEmpty())
    {
        return;
    }

    qint64 threadId = m_strategyManager->getStrategyThreadId(m_currentStrategyID);
    if (threadId == 0)
    {
        m_statsLabel->setText("Stats: Thread not running");
        return;
    }

    // Try to read CPU and memory from /proc/[tid]/stat
    QString statPath = QString("/proc/%1/stat").arg(threadId);
    QProcess proc;
    proc.start("cat", QStringList() << statPath);
    proc.waitForFinished(500);

    QString stats;
    if (proc.exitCode() == 0)
    {
        QString statContent = QString::fromUtf8(proc.readAllStandardOutput());
        // /proc/[pid]/stat format is complex; just show basic TID info for now
        stats = QString("TID: %1").arg(threadId);
    }
    else
    {
        stats = QString("TID: %1 (stats unavailable)").arg(threadId);
    }

    m_statsLabel->setText(stats);
}

void StrategyDetailsPanel::onStrategyBalanceUpdated(const QString& strategyID, double newBalance)
{
    if (strategyID == m_currentStrategyID)
    {
        m_currentBalance = newBalance;
        m_balanceLabel->setText(QString::asprintf("Balance: $%.2f", m_currentBalance));
    }
}

void StrategyDetailsPanel::onRefreshStatsTimer()
{
    if (!m_currentStrategyID.isEmpty())
    {
        updateOrdersList();
        updatePositionsList();
        updateStats();
    }
}
