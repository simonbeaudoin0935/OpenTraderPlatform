#include "StrategyDetailsPanel.h"
#include "StrategyManager.h"
#include "StrategyConfig.h"
#include "StrategyLogger.h"
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
#include <QComboBox>
#include <QTimer>
#include <QDateTime>
#include <QProcess>
#include <QStringList>
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QStandardPaths>
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
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // Title
    m_titleLabel = new QLabel("Strategy Details");
    m_titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #fff;");
    mainLayout->addWidget(m_titleLabel);

    // Status and Balance (side by side)
    auto statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("Status: -");
    m_statusLabel->setStyleSheet("font-size: 11px; color: #aaa;");
    statusLayout->addWidget(m_statusLabel);

    m_balanceLabel = new QLabel("Balance: -");
    m_balanceLabel->setStyleSheet("font-size: 11px; color: #aaa;");
    statusLayout->addWidget(m_balanceLabel);

    statusLayout->addStretch();
    mainLayout->addLayout(statusLayout);

    // CPU/Memory stats
    m_statsLabel = new QLabel();
    m_statsLabel->setStyleSheet("font-size: 10px; color: #666;");
    mainLayout->addWidget(m_statsLabel);

    // Tabs: Details, Orders, Positions, Logs
    // We'll use a simple approach: stack different sections vertically with collapsible headers

    // Orders section
    auto ordersHeaderLabel = new QLabel("Recent Orders:");
    ordersHeaderLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #fff; margin-top: 10px;");
    mainLayout->addWidget(ordersHeaderLabel);

    m_ordersDisplay = new QTextEdit();
    m_ordersDisplay->setReadOnly(true);
    m_ordersDisplay->setStyleSheet("background-color: #2d2d2d; color: #aaa; font-family: monospace; "
                                   "font-size: 9px; border: 1px solid #444;");
    m_ordersDisplay->setMaximumHeight(80);
    mainLayout->addWidget(m_ordersDisplay);

    // Positions section
    auto positionsHeaderLabel = new QLabel("Open Positions:");
    positionsHeaderLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #fff;");
    mainLayout->addWidget(positionsHeaderLabel);

    m_positionsDisplay = new QTextEdit();
    m_positionsDisplay->setReadOnly(true);
    m_positionsDisplay->setStyleSheet("background-color: #2d2d2d; color: #aaa; font-family: monospace; "
                                      "font-size: 9px; border: 1px solid #444;");
    m_positionsDisplay->setMaximumHeight(80);
    mainLayout->addWidget(m_positionsDisplay);

    // Logs section header
    auto logsHeaderLabel = new QLabel("Strategy Logs:");
    logsHeaderLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #fff; margin-top: 10px;");
    mainLayout->addWidget(logsHeaderLabel);

    // Logs filter and controls
    auto logsControlLayout = new QHBoxLayout();
    auto filterLabel = new QLabel("Level:");
    filterLabel->setStyleSheet("color: #aaa; font-size: 10px;");
    logsControlLayout->addWidget(filterLabel);

    m_logsLevelFilter = new QComboBox();
    m_logsLevelFilter->setStyleSheet("background-color: #2d2d2d; color: #fff; border: 1px solid #444; "
                                     "font-size: 9px;");
    m_logsLevelFilter->setMaximumWidth(100);
    m_logsLevelFilter->addItem("All", -1);
    m_logsLevelFilter->addItem("Debug", QtDebugMsg);
    m_logsLevelFilter->addItem("Info", QtInfoMsg);
    m_logsLevelFilter->addItem("Warning", QtWarningMsg);
    m_logsLevelFilter->addItem("Critical", QtCriticalMsg);
    connect(m_logsLevelFilter,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &StrategyDetailsPanel::onLogsLevelFilterChanged);
    logsControlLayout->addWidget(m_logsLevelFilter);

    logsControlLayout->addStretch();

    m_logsStatsLabel = new QLabel();
    m_logsStatsLabel->setStyleSheet("font-size: 9px; color: #666;");
    logsControlLayout->addWidget(m_logsStatsLabel);

    mainLayout->addLayout(logsControlLayout);

    // Logs display
    m_logsDisplay = new QTextEdit();
    m_logsDisplay->setReadOnly(true);
    m_logsDisplay->setStyleSheet("background-color: #0d0d0d; color: #0f0; font-family: monospace; "
                                 "font-size: 8px; border: 1px solid #444;");
    mainLayout->addWidget(m_logsDisplay);

    // Control buttons
    auto buttonLayout = new QHBoxLayout();

    m_startButton = new QPushButton("Start");
    m_startButton->setMaximumWidth(60);
    m_startButton->setStyleSheet(
        "QPushButton { background-color: #4db84d; color: #000; font-weight: bold; border-radius: 3px; padding: 4px; }"
        "QPushButton:hover { background-color: #66cc66; }"
        "QPushButton:pressed { background-color: #3d9d3d; }"
        "QPushButton:disabled { background-color: #555; color: #999; }");
    connect(m_startButton, &QPushButton::clicked, this, &StrategyDetailsPanel::onStartStrategy);
    buttonLayout->addWidget(m_startButton);

    m_stopButton = new QPushButton("Stop");
    m_stopButton->setMaximumWidth(60);
    m_stopButton->setStyleSheet(
        "QPushButton { background-color: #cc4444; color: #fff; font-weight: bold; border-radius: 3px; padding: 4px; }"
        "QPushButton:hover { background-color: #ff6666; }"
        "QPushButton:pressed { background-color: #aa2222; }"
        "QPushButton:disabled { background-color: #555; color: #999; }");
    connect(m_stopButton, &QPushButton::clicked, this, &StrategyDetailsPanel::onStopStrategy);
    buttonLayout->addWidget(m_stopButton);

    m_exportLogsButton = new QPushButton("Export");
    m_exportLogsButton->setMaximumWidth(70);
    connect(m_exportLogsButton, &QPushButton::clicked, this, &StrategyDetailsPanel::onExportLogs);
    buttonLayout->addWidget(m_exportLogsButton);

    m_clearLogsButton = new QPushButton("Clear");
    m_clearLogsButton->setMaximumWidth(60);
    connect(m_clearLogsButton, &QPushButton::clicked, this, &StrategyDetailsPanel::onClearLogs);
    buttonLayout->addWidget(m_clearLogsButton);

    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

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
        m_statsRefreshTimer->start(500); // Refresh every 500ms for logs
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
    m_logsDisplay->clear();
    m_logsStatsLabel->setText("");
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(false);
    m_exportLogsButton->setEnabled(false);
    m_clearLogsButton->setEnabled(false);
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

    // Update button states
    m_startButton->setEnabled(!isRunning);
    m_stopButton->setEnabled(isRunning);

    // Update balance display
    m_balanceLabel->setText(QString::asprintf("Balance: $%.2f", m_currentBalance));

    // Update orders and positions
    updateOrdersList();
    updatePositionsList();
    updateStats();
    updateLogs();
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

    m_statsLabel->setText(QString("TID: %1").arg(threadId));
}

void StrategyDetailsPanel::updateLogs()
{
    if (m_currentStrategyID.isEmpty())
    {
        return;
    }

    const StrategyLogger* logger = m_strategyManager->getStrategyLogger(m_currentStrategyID);
    if (!logger)
    {
        m_logsDisplay->clear();
        m_logsStatsLabel->setText("Logger not available");
        return;
    }

    QVector<StrategyLogMessage> messages = logger->getMessages();

    // Filter by selected level if not "All"
    if (m_selectedLogLevel != -1)
    {
        QVector<StrategyLogMessage> filtered;
        QtMsgType selectedType = static_cast<QtMsgType>(m_selectedLogLevel);
        for (const auto& msg: messages)
        {
            if (msg.level == selectedType)
            {
                filtered.append(msg);
            }
        }
        messages = filtered;
    }

    // Build HTML display with colors
    QString displayText;
    for (const auto& msg: messages)
    {
        QString levelStr = levelToString(msg.level);
        QString color = levelToColor(msg.level);
        QString timeStr = msg.timestamp.toString("hh:mm:ss.zzz");

        displayText += QString("<span style=\"color: %1;\">[%2] %3: %4</span><br>")
                           .arg(color)
                           .arg(timeStr)
                           .arg(levelStr)
                           .arg(msg.message);
    }

    m_logsDisplay->setHtml(displayText);

    // Auto-scroll to bottom
    QTextCursor cursor = m_logsDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logsDisplay->setTextCursor(cursor);

    // Update stats label
    m_logsStatsLabel->setText(QString("%1 shown | %2 total").arg(messages.size()).arg(logger->messageCount()));
}

QString StrategyDetailsPanel::levelToString(QtMsgType level) const
{
    switch (level)
    {
    case QtDebugMsg:
        return "DEBG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "CRIT";
    case QtFatalMsg:
        return "FATL";
    default:
        return "????";
    }
}

QString StrategyDetailsPanel::levelToColor(QtMsgType level) const
{
    switch (level)
    {
    case QtDebugMsg:
        return "#666"; // Gray
    case QtInfoMsg:
        return "#0f0"; // Green
    case QtWarningMsg:
        return "#ff0"; // Yellow
    case QtCriticalMsg:
        return "#f00"; // Red
    case QtFatalMsg:
        return "#f0f"; // Magenta
    default:
        return "#fff";
    }
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
        updateLogs();
    }
}

void StrategyDetailsPanel::onLogsLevelFilterChanged(int index)
{
    int levelInt = m_logsLevelFilter->itemData(index).toInt();
    m_selectedLogLevel = static_cast<QtMsgType>(levelInt);
    updateLogs();
}

void StrategyDetailsPanel::onExportLogs()
{
    if (m_currentStrategyID.isEmpty())
    {
        QMessageBox::warning(this, "Error", "No strategy selected");
        return;
    }

    const StrategyLogger* logger = m_strategyManager->getStrategyLogger(m_currentStrategyID);
    if (!logger)
    {
        QMessageBox::warning(this, "Error", "Logger not available");
        return;
    }

    StrategyConfig config = m_strategyManager->getStrategyConfig(m_currentStrategyID);
    QString suggestedPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Desktop/" + config.name +
                            "_logs_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".txt";

    QString filePath = QFileDialog::getSaveFileName(this, "Export Logs", suggestedPath, "Text Files (*.txt)");
    if (filePath.isEmpty())
    {
        return; // User cancelled
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, "Error", QString("Failed to open file: %1").arg(filePath));
        return;
    }

    QVector<StrategyLogMessage> messages = logger->getMessages();
    for (const auto& msg: messages)
    {
        QString line = QString("[%1] %2: %3\n")
                           .arg(msg.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"))
                           .arg(levelToString(msg.level))
                           .arg(msg.message);
        file.write(line.toUtf8());
    }

    file.close();
    QMessageBox::information(this, "Success", QString("Logs exported to:\n%1").arg(filePath));
}

void StrategyDetailsPanel::onClearLogs()
{
    if (m_currentStrategyID.isEmpty())
    {
        return;
    }

    StrategyLogger* logger = const_cast<StrategyLogger*>(m_strategyManager->getStrategyLogger(m_currentStrategyID));
    if (!logger)
    {
        return;
    }

    int ret = QMessageBox::question(this,
                                    "Confirm Clear",
                                    "Clear all logs for this strategy?",
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes)
    {
        logger->clear();
        updateLogs();
    }
}

void StrategyDetailsPanel::onStopStrategy()
{
    if (m_currentStrategyID.isEmpty())
    {
        return;
    }

    QString error = m_strategyManager->unloadStrategy(m_currentStrategyID);
    if (!error.isEmpty())
    {
        QMessageBox::critical(this, "Error", QString("Failed to stop strategy: %1").arg(error));
        return;
    }

    // Strategy unloaded successfully, clear display
    m_currentStrategyID = "";
    clearStrategy();
}

void StrategyDetailsPanel::onStartStrategy()
{
    if (m_currentStrategyID.isEmpty())
    {
        QMessageBox::warning(this, "Error", "No strategy selected");
        return;
    }

    if (!m_strategyManager)
    {
        QMessageBox::critical(this, "Error", "StrategyManager not available");
        return;
    }

    auto error = m_strategyManager->startStrategy(m_currentStrategyID);
    if (!error.isEmpty())
    {
        QMessageBox::critical(this, "Error", "Failed to start strategy: " + error);
    }
}
