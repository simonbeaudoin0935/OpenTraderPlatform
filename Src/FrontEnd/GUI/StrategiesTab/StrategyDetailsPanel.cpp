#include "StrategyDetailsPanel.h"
#include "StrategyManager.h"
#include "StrategyConfig.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScrollArea>

StrategyDetailsPanel::StrategyDetailsPanel(StrategyManager* p_strategyManager, QWidget* parent)
    : QWidget(parent), m_strategyManager(p_strategyManager), m_currentStrategyID("")
{
    setStyleSheet("background-color: #1e1e1e; border-left: 1px solid #444;");
    setupUI();
}

void StrategyDetailsPanel::setupUI()
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // Title
    m_titleLabel = new QLabel("Strategy Details");
    m_titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #fff;");
    layout->addWidget(m_titleLabel);

    // Config display
    m_configLabel = new QLabel();
    m_configLabel->setStyleSheet("font-size: 11px; color: #aaa; font-family: monospace;");
    m_configLabel->setWordWrap(true);
    m_configLabel->setAlignment(Qt::AlignTop);

    auto scrollArea = new QScrollArea();
    scrollArea->setWidget(m_configLabel);
    scrollArea->setWidgetResizable(true);
    layout->addWidget(scrollArea);

    // Status
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("font-size: 10px; color: #999;");
    layout->addWidget(m_statusLabel);

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
    updateDisplay();
}

void StrategyDetailsPanel::clearStrategy()
{
    m_currentStrategyID = "";
    m_titleLabel->setText("Strategy Details");
    m_configLabel->setText("");
    m_statusLabel->setText("");
    m_stopButton->setEnabled(false);
    m_viewLogsButton->setEnabled(false);
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

    // Update config display
    QString configText;
    configText += "Name: " + config.name + "\n";
    configText += "Symbols: " + config.symbols.join(", ") + "\n";
    configText += "Position Size: " + QString::number(config.positionSize) + "\n";
    configText += "Risk Limit: " + QString::number(config.riskLimit) + "\n";
    configText += "Plugin: " + config.soPath + "\n";
    configText += "\nCustom Parameters:\n";

    // Serialize custom params as JSON for display
    QJsonObject jsonObj;
    for (auto it = config.customParams.cbegin(); it != config.customParams.cend(); ++it)
    {
        jsonObj[it->first] = it->second;
    }
    QJsonDocument doc(jsonObj);
    QString jsonStr = doc.toJson(QJsonDocument::Indented);
    configText += jsonStr;

    m_configLabel->setText(configText);

    // Update status
    m_statusLabel->setText(isRunning ? "Status: RUNNING" : "Status: STOPPED");
    m_stopButton->setEnabled(isRunning);
}
