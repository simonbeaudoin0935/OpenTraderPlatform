#include "ConfigTab.h"
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

#include "Misc/Settings.h"
#include "Misc/CONSTANTS.h"

ConfigTab::ConfigTab(QWidget* parent)
    : QWidget(parent)
    , m_marketDepthLevelSpinBox(nullptr)
{
    setupUI();
    loadSettings();
}

void ConfigTab::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Market Data Configuration section
    QGroupBox* marketDataGroupBox = new QGroupBox("Market Data Configuration");
    QVBoxLayout* marketDataLayout = new QVBoxLayout(marketDataGroupBox);

    // Market depth level control
    QHBoxLayout* marketDepthLayout = new QHBoxLayout();
    QLabel* marketDepthLabel = new QLabel("Market Depth Levels:");
    m_marketDepthLevelSpinBox = new QSpinBox();
    m_marketDepthLevelSpinBox->setMinimum(1);
    m_marketDepthLevelSpinBox->setMaximum(20);
    m_marketDepthLevelSpinBox->setValue(MarketDepthConstants::DEFAULT_MARKET_DEPTH_LEVELS);
    m_marketDepthLevelSpinBox->setSingleStep(1);
    m_marketDepthLevelSpinBox->setToolTip("Number of market depth levels to request from TradeStation (1-20).\n"
                                          "Lower values reduce data usage when recording.");
    connect(m_marketDepthLevelSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ConfigTab::onMarketDepthLevelChanged);
    marketDepthLayout->addWidget(marketDepthLabel);
    marketDepthLayout->addWidget(m_marketDepthLevelSpinBox);
    marketDepthLayout->addStretch();
    marketDataLayout->addLayout(marketDepthLayout);

    mainLayout->addWidget(marketDataGroupBox);
    mainLayout->addStretch();
}

void ConfigTab::loadSettings()
{
    Q_CHECK_PTR(appStateSettings);

    // Load market depth level, default to constant if not set
    int marketDepthLevel = appStateSettings->value("Config/MarketDepthLevel",
                                                   MarketDepthConstants::DEFAULT_MARKET_DEPTH_LEVELS).toInt();
    m_marketDepthLevelSpinBox->setValue(marketDepthLevel);
}

void ConfigTab::saveSetting(const QString& key, const QVariant& value)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue(key, value);
    appStateSettings->sync();
}

void ConfigTab::onMarketDepthLevelChanged(int value)
{
    saveSetting("Config/MarketDepthLevel", value);
}
