#include "StrategyLogWidget.h"

#include "MainAlgo.h"
#include "StrategyManager.h"
#include "StrategyLogger.h"
#include "Assume.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QScrollBar>

StrategyLogWidget::StrategyLogWidget(MainAlgo* p_mainAlgo, QWidget* parent)
    : QWidget(parent)
    , m_mainAlgo(p_mainAlgo)
    , m_titleLabel(new QLabel("Strategy Log", this))
    , m_levelFilter(new QComboBox(this))
    , m_closeButton(new QPushButton("✕", this))
    , m_logDisplay(new QTextEdit(this))
    , m_timer(new QTimer(this))
{
    OBJ_ASSUME_DIFF(m_mainAlgo, nullptr);
    setupUI();
    setupStyles();

    connect(m_timer, &QTimer::timeout, this, &StrategyLogWidget::onRefreshTimer);
    connect(m_levelFilter,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &StrategyLogWidget::onLevelFilterChanged);
    connect(m_logDisplay->verticalScrollBar(),
            &QScrollBar::valueChanged,
            this,
            &StrategyLogWidget::onScrolled);
    connect(m_closeButton, &QPushButton::clicked, this, &StrategyLogWidget::closeRequested);
}

void StrategyLogWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header bar
    auto* header = new QWidget(this);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(6, 3, 6, 3);
    headerLayout->setSpacing(6);

    headerLayout->addWidget(m_titleLabel, 1);

    m_levelFilter->addItem("All Levels", -1);
    m_levelFilter->addItem("Debug", static_cast<int>(QtDebugMsg));
    m_levelFilter->addItem("Info", static_cast<int>(QtInfoMsg));
    m_levelFilter->addItem("Warning", static_cast<int>(QtWarningMsg));
    m_levelFilter->addItem("Critical", static_cast<int>(QtCriticalMsg));
    headerLayout->addWidget(m_levelFilter);

    m_closeButton->setFixedSize(20, 20);
    m_closeButton->setToolTip("Close strategy log");
    headerLayout->addWidget(m_closeButton);

    mainLayout->addWidget(header);

    // Log display
    m_logDisplay->setReadOnly(true);
    QFont font("Monospace");
    font.setPointSize(9);
    m_logDisplay->setFont(font);
    mainLayout->addWidget(m_logDisplay, 1);
}

void StrategyLogWidget::setupStyles()
{
    // Header background matches the logging tab aesthetic
    setStyleSheet(
        "StrategyLogWidget {"
        "  background-color: #1A1A1A;"
        "  border-left: 1px solid #3A3A3A;"
        "}"
        "QLabel {"
        "  color: #AAAAAA;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "}"
        "QComboBox {"
        "  background-color: #2D2D2D;"
        "  color: #CCCCCC;"
        "  border: 1px solid #3D3D3D;"
        "  font-size: 10px;"
        "  padding: 1px 4px;"
        "}"
        "QPushButton {"
        "  background-color: #3D3D3D;"
        "  color: #AAAAAA;"
        "  border: none;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "  border-radius: 2px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #E05050;"
        "  color: white;"
        "}"
        "QTextEdit {"
        "  background-color: #111111;"
        "  color: #CCCCCC;"
        "  border: none;"
        "  selection-background-color: #2C539E;"
        "}");
}

void StrategyLogWidget::setStrategy(const QString& p_strategyID, const QString& p_strategyName)
{
    m_strategyID = p_strategyID;
    m_lastLogCount = 0;
    m_autoScroll = true;
    m_logDisplay->clear();
    m_titleLabel->setText(QString("● Strategy Log: %1").arg(p_strategyName));
    m_timer->start(1000);
}

void StrategyLogWidget::clearStrategy()
{
    m_timer->stop();
    m_strategyID.clear();
    m_lastLogCount = 0;
    m_logDisplay->clear();
    m_titleLabel->setText("Strategy Log");
}

void StrategyLogWidget::onRefreshTimer()
{
    if (m_strategyID.isEmpty())
        return;

    const StrategyLogger* logger = m_mainAlgo->getStrategyManager()->getStrategyLogger(m_strategyID);
    if (!logger)
    {
        m_logDisplay->setPlainText("(Strategy no longer available)");
        m_timer->stop();
        return;
    }

    QVector<StrategyLogMessage> allLogs = logger->getMessages();

    // Filter by level if needed
    QVector<StrategyLogMessage> logs;
    if (m_selectedLogLevel == -1)
    {
        logs = allLogs;
    }
    else
    {
        for (const auto& msg: allLogs)
        {
            if (msg.level == static_cast<QtMsgType>(m_selectedLogLevel))
                logs.append(msg);
        }
    }

    if (logs.size() == m_lastLogCount)
        return;

    // Append only new messages for efficiency
    int firstNew = m_lastLogCount;
    m_lastLogCount = logs.size();

    for (int i = firstNew; i < logs.size(); ++i)
    {
        const auto& msg = logs[i];
        m_logDisplay->append(
            QString("[%1] %2: %3").arg(msg.timestamp.toString("hh:mm:ss.zzz"), levelToString(msg.level), msg.message));
    }

    if (m_autoScroll)
        m_logDisplay->verticalScrollBar()->setValue(m_logDisplay->verticalScrollBar()->maximum());
}

void StrategyLogWidget::onLevelFilterChanged(int index)
{
    m_selectedLogLevel = m_levelFilter->itemData(index).toInt();
    m_lastLogCount = 0; // Force full redraw with new filter
    m_logDisplay->clear();
}

void StrategyLogWidget::onScrolled()
{
    auto* scrollBar = m_logDisplay->verticalScrollBar();
    m_autoScroll = (scrollBar->value() == scrollBar->maximum());
}

QString StrategyLogWidget::levelToString(QtMsgType level)
{
    switch (level)
    {
        case QtDebugMsg:
            return "DEBUG";
        case QtInfoMsg:
            return "INFO";
        case QtWarningMsg:
            return "WARN";
        case QtCriticalMsg:
            return "CRIT";
        case QtFatalMsg:
            return "FATAL";
        default:
            return "???";
    }
}
