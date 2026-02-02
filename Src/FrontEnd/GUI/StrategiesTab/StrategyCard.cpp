#include "StrategyCard.h"
#include "StrategyManager.h"
#include "StrategyLogger.h"
#include "ThreadStats.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QMouseEvent>
#include <QDateTime>

StrategyCard::StrategyCard(const QString& strategyID,
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
    , m_strategyManager(p_strategyManager)
{
    setFixedWidth(400);

    // Create refresh timer
    m_refreshTimer = std::make_unique<QTimer>();
    connect(m_refreshTimer.get(), &QTimer::timeout, this, &StrategyCard::onRefreshTimer);
    m_refreshTimer->start(1000);

    setupUI();
}

void StrategyCard::setupUI()
{
    auto outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // Main border frame containing everything
    auto borderFrame = new QFrame(this);
    borderFrame->setStyleSheet("background-color: #1e1e1e; border: 1px solid #444; border-radius: 6px;");
    borderFrame->setFrameShape(QFrame::StyledPanel);
    borderFrame->setFrameShadow(QFrame::Plain);
    outerLayout->addWidget(borderFrame);

    // Main layout inside frame
    auto mainLayout = new QVBoxLayout(borderFrame);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(4);

    // ===== HEADER SECTION =====
    setupHeader();

    // Add all header labels to main layout
    m_titleLabel = new QLabel(m_name);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 12px; color: #ffffff;");
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    mainLayout->addWidget(m_titleLabel);

    m_symbolsLabel = new QLabel("Symbols: " + m_symbols.join(", "));
    m_symbolsLabel->setStyleSheet("font-size: 10px; color: #b8b8b8;");
    m_symbolsLabel->setWordWrap(true);
    m_symbolsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    mainLayout->addWidget(m_symbolsLabel);

    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("font-size: 10px; font-weight: bold;");
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    mainLayout->addWidget(m_statusLabel);

    m_positionsLabel = new QLabel("Positions: 0");
    m_positionsLabel->setStyleSheet("font-size: 10px; color: #b8b8b8;");
    m_positionsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    mainLayout->addWidget(m_positionsLabel);

    m_threadInfoLabel = new QLabel("Thread: - | CPU: -%");
    m_threadInfoLabel->setStyleSheet("font-size: 9px; color: #808080;");
    m_threadInfoLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    mainLayout->addWidget(m_threadInfoLabel);

    // Control buttons (Start/Stop)
    auto buttonLayout = new QHBoxLayout();
    m_startButton = new QPushButton("Start");
    m_startButton->setStyleSheet("background-color: #51cf66; color: #000; font-size: 9px; padding: 4px 8px; "
                                 "border-radius: 3px; font-weight: bold;");
    m_startButton->setMaximumWidth(60);
    connect(m_startButton, &QPushButton::clicked, this, &StrategyCard::onStartClicked);
    buttonLayout->addWidget(m_startButton);

    m_stopButton = new QPushButton("Stop");
    m_stopButton->setStyleSheet("background-color: #ff6b6b; color: #fff; font-size: 9px; padding: 4px 8px; "
                                "border-radius: 3px; font-weight: bold;");
    m_stopButton->setMaximumWidth(60);
    connect(m_stopButton, &QPushButton::clicked, this, &StrategyCard::onStopClicked);
    buttonLayout->addWidget(m_stopButton);

    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    // Separator line
    auto separator = new QFrame();
    separator->setStyleSheet("background-color: #333;");
    separator->setFixedHeight(1);
    mainLayout->addWidget(separator);

    // ===== CONTENT SECTION =====
    setupContent();

    // Orders
    auto ordersHeaderLabel = new QLabel("Orders:");
    ordersHeaderLabel->setStyleSheet("font-size: 10px; font-weight: bold; color: #fff; margin-top: 4px;");
    mainLayout->addWidget(ordersHeaderLabel);

    m_ordersDisplay = new QTextEdit();
    m_ordersDisplay->setReadOnly(true);
    m_ordersDisplay->setStyleSheet("background-color: #2d2d2d; color: #aaa; font-family: monospace; "
                                   "font-size: 8px; border: 1px solid #333;");
    m_ordersDisplay->setMaximumHeight(35);
    mainLayout->addWidget(m_ordersDisplay);

    // Positions
    auto positionsHeaderLabel = new QLabel("Positions:");
    positionsHeaderLabel->setStyleSheet("font-size: 10px; font-weight: bold; color: #fff;");
    mainLayout->addWidget(positionsHeaderLabel);

    m_positionsDisplay = new QTextEdit();
    m_positionsDisplay->setReadOnly(true);
    m_positionsDisplay->setStyleSheet("background-color: #2d2d2d; color: #aaa; font-family: monospace; "
                                      "font-size: 8px; border: 1px solid #333;");
    m_positionsDisplay->setMaximumHeight(35);
    mainLayout->addWidget(m_positionsDisplay);

    // Logs section
    auto logsHeaderLabel = new QLabel("Logs:");
    logsHeaderLabel->setStyleSheet("font-size: 10px; font-weight: bold; color: #fff;");
    mainLayout->addWidget(logsHeaderLabel);

    // Logs controls
    auto logsControlLayout = new QHBoxLayout();
    auto filterLabel = new QLabel("Level:");
    filterLabel->setStyleSheet("color: #aaa; font-size: 9px;");
    logsControlLayout->addWidget(filterLabel);

    m_logsLevelFilter = new QComboBox();
    m_logsLevelFilter->setStyleSheet("background-color: #2d2d2d; color: #fff; border: 1px solid #333; "
                                     "font-size: 8px;");
    m_logsLevelFilter->setMaximumWidth(80);
    m_logsLevelFilter->addItem("All", -1);
    m_logsLevelFilter->addItem("Debug", QtDebugMsg);
    m_logsLevelFilter->addItem("Info", QtInfoMsg);
    m_logsLevelFilter->addItem("Warning", QtWarningMsg);
    m_logsLevelFilter->addItem("Critical", QtCriticalMsg);
    connect(m_logsLevelFilter,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &StrategyCard::onLogsLevelFilterChanged);
    logsControlLayout->addWidget(m_logsLevelFilter);

    logsControlLayout->addStretch();

    m_logsStatsLabel = new QLabel();
    m_logsStatsLabel->setStyleSheet("font-size: 8px; color: #666;");
    logsControlLayout->addWidget(m_logsStatsLabel);

    mainLayout->addLayout(logsControlLayout);

    // Logs display
    m_logsDisplay = new QTextEdit();
    m_logsDisplay->setReadOnly(true);
    m_logsDisplay->setStyleSheet("background-color: #0d0d0d; color: #0f0; font-family: monospace; "
                                 "font-size: 7px; border: 1px solid #333;");
    mainLayout->addWidget(m_logsDisplay, 1); // stretch to fill

    // Connect scroll signal to detect manual scrolling
    connect(m_logsDisplay->verticalScrollBar(), &QScrollBar::sliderMoved, this, &StrategyCard::onLogsScrolled);

    updateStatus();
    updateThreadInfo();
    updateLogs();
}

void StrategyCard::setupHeader()
{
    // Header setup already done in setupUI
}

void StrategyCard::setupContent()
{
    // Content setup already done in setupUI
}

void StrategyCard::setStatus(bool isRunning, const QString& errorMessage)
{
    m_isRunning = isRunning;
    m_errorMessage = errorMessage;
    updateStatus();
}

void StrategyCard::refreshDisplay()
{
    onRefreshTimer();
}

QString StrategyCard::getStrategyID() const
{
    return m_strategyID;
}

void StrategyCard::onRefreshTimer()
{
    updateThreadInfo();

    if (m_strategyManager)
    {
        int posCount = m_strategyManager->getStrategyPositionCount(m_strategyID);
        m_positionsLabel->setText("Positions: " + QString::number(posCount));
    }

    updateLogs();
}

void StrategyCard::updateStatus()
{
    if (!m_errorMessage.isEmpty())
    {
        m_statusLabel->setText("Status: ERROR");
        m_statusLabel->setStyleSheet("font-size: 10px; font-weight: bold; color: #ff6b6b;");
        m_startButton->setEnabled(true);
        m_startButton->setStyleSheet("background-color: #51cf66; color: #000; font-size: 9px; padding: 4px 8px; "
                                     "border-radius: 3px; font-weight: bold;");
        m_stopButton->setEnabled(false);
        m_stopButton->setStyleSheet("background-color: #999; color: #333; font-size: 9px; padding: 4px 8px; "
                                    "border-radius: 3px; font-weight: bold;");
    }
    else if (m_isRunning)
    {
        m_statusLabel->setText("Status: RUNNING");
        m_statusLabel->setStyleSheet("font-size: 10px; font-weight: bold; color: #51cf66;");
        m_startButton->setEnabled(false);
        m_startButton->setStyleSheet("background-color: #999; color: #333; font-size: 9px; padding: 4px 8px; "
                                     "border-radius: 3px; font-weight: bold;");
        m_stopButton->setEnabled(true);
        m_stopButton->setStyleSheet("background-color: #ff6b6b; color: #fff; font-size: 9px; padding: 4px 8px; "
                                    "border-radius: 3px; font-weight: bold;");
    }
    else
    {
        m_statusLabel->setText("Status: STOPPED");
        m_statusLabel->setStyleSheet("font-size: 10px; font-weight: bold; color: #ffd93d;");
        m_startButton->setEnabled(true);
        m_startButton->setStyleSheet("background-color: #51cf66; color: #000; font-size: 9px; padding: 4px 8px; "
                                     "border-radius: 3px; font-weight: bold;");
        m_stopButton->setEnabled(false);
        m_stopButton->setStyleSheet("background-color: #999; color: #333; font-size: 9px; padding: 4px 8px; "
                                    "border-radius: 3px; font-weight: bold;");
    }
}

void StrategyCard::updateThreadInfo()
{
    if (!m_strategyManager)
    {
        m_threadInfoLabel->setText("Thread: - | CPU: -%");
        return;
    }

    qint64 threadId = m_strategyManager->getStrategyThreadId(m_strategyID);
    if (threadId <= 0)
    {
        m_threadInfoLabel->setText("Thread: - | CPU: -%");
        return;
    }

    ThreadStats::Stats stats = ThreadStats::getThreadStats(threadId);
    if (!stats.valid)
    {
        m_threadInfoLabel->setText(QString("Thread: %1 | CPU: -%").arg(threadId));
        return;
    }

    m_threadInfoLabel->setText(QString("Thread: %1 | CPU: %2%").arg(threadId).arg((int)stats.cpuUsagePercent));
}

void StrategyCard::updateLogs()
{
    if (!m_strategyManager)
        return;

    const StrategyLogger* logger = m_strategyManager->getStrategyLogger(m_strategyID);
    if (!logger)
    {
        m_logsDisplay->setPlainText("(No logs available)");
        m_logsStatsLabel->setText("0 logs");
        return;
    }

    // Get all logs
    QVector<StrategyLogMessage> allLogs = logger->getMessages();

    // Filter by level if needed
    QVector<StrategyLogMessage> logs;
    if (m_selectedLogLevel == -1)
    {
        logs = allLogs;
    }
    else
    {
        for (const auto& log: allLogs)
        {
            if (log.level == static_cast<QtMsgType>(m_selectedLogLevel))
            {
                logs.append(log);
            }
        }
    }

    // Display logs
    QString logsText;
    for (const auto& log: logs)
    {
        logsText +=
            QString("[%1] %2: %3\n").arg(log.timestamp.toString("hh:mm:ss.zzz"), levelToString(log.level), log.message);
    }

    m_logsDisplay->setPlainText(logsText);
    m_logsStatsLabel->setText(QString("%1 logs").arg(logs.size()));

    // Auto-scroll to bottom if enabled
    if (m_logsAutoScroll)
    {
        m_logsDisplay->verticalScrollBar()->setValue(m_logsDisplay->verticalScrollBar()->maximum());
    }
}

void StrategyCard::onLogsScrolled()
{
    // Detect if user scrolled away from bottom
    auto scrollBar = m_logsDisplay->verticalScrollBar();
    m_logsAutoScroll = (scrollBar->value() == scrollBar->maximum());
}

void StrategyCard::onLogsLevelFilterChanged(int index)
{
    m_selectedLogLevel = m_logsLevelFilter->itemData(index).toInt();
    updateLogs();
}

QString StrategyCard::levelToString(QtMsgType level) const
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
        return "FATAL";
    default:
        return "????";
    }
}

QString StrategyCard::levelToColor(QtMsgType level) const
{
    switch (level)
    {
    case QtDebugMsg:
        return "#888";
    case QtInfoMsg:
        return "#0f0";
    case QtWarningMsg:
        return "#ff0";
    case QtCriticalMsg:
        return "#f00";
    case QtFatalMsg:
        return "#f0f";
    default:
        return "#fff";
    }
}

void StrategyCard::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        emit cardClicked(m_strategyID);
    }
    QWidget::mousePressEvent(event);
}

void StrategyCard::onStartClicked()
{
    if (m_strategyManager)
    {
        [[maybe_unused]] auto result = m_strategyManager->startStrategy(m_strategyID);
    }
}

void StrategyCard::onStopClicked()
{
    if (m_strategyManager)
    {
        [[maybe_unused]] auto result = m_strategyManager->unloadStrategy(m_strategyID);
    }
}
