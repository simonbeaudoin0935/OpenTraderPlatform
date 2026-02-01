#include "StrategyLoadDialog.h"
#include "StrategyConfig.h"
#include "StrategyConfigLoader.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QJsonObject>

StrategyLoadDialog::StrategyLoadDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Load Strategy");
    setMinimumSize(500, 400);
    setModal(true);

    setupUI();
    loadAvailableConfigs();

    // Disable load button initially (no selection)
    m_loadButton->setEnabled(false);
}

std::optional<StrategyConfig> StrategyLoadDialog::getSelectedConfig() const
{
    return m_selectedConfig;
}

void StrategyLoadDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Title
    auto* titleLabel = new QLabel("Select a Strategy Configuration to Load");
    mainLayout->addWidget(titleLabel);

    // Config list
    auto* listGroupBox = new QGroupBox("Available Strategies");
    auto* listLayout = new QVBoxLayout();
    m_configList = new QListWidget();
    listLayout->addWidget(m_configList);
    listGroupBox->setLayout(listLayout);
    mainLayout->addWidget(listGroupBox, 2);

    // Preview panel
    auto* previewGroupBox = new QGroupBox("Configuration Preview");
    auto* previewLayout = new QVBoxLayout();
    m_previewLabel = new QLabel("(Select a strategy to view details)");
    m_previewLabel->setWordWrap(true);
    previewLayout->addWidget(m_previewLabel);
    previewGroupBox->setLayout(previewLayout);
    mainLayout->addWidget(previewGroupBox, 1);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    m_loadButton = new QPushButton("Load Strategy");
    m_cancelButton = new QPushButton("Cancel");
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_loadButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(m_configList,
            &QListWidget::itemSelectionChanged,
            this,
            [this]()
            {
                auto items = m_configList->selectedItems();
                if (!items.isEmpty())
                {
                    onConfigSelected(items[0]);
                }
            });

    connect(m_loadButton, &QPushButton::clicked, this, &StrategyLoadDialog::onLoadClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &StrategyLoadDialog::onCancelClicked);
}

void StrategyLoadDialog::loadAvailableConfigs()
{
    m_configList->clear();

    QStringList configPaths = StrategyConfigLoader::discoverConfigs();

    for (const auto& path: configPaths)
    {
        auto config = StrategyConfigLoader::loadConfig(path);
        if (!config.name.isEmpty())
        {
            auto* item = new QListWidgetItem(config.name);
            item->setData(Qt::UserRole, path); // Store full path as user data
            m_configList->addItem(item);
        }
    }

    if (m_configList->count() == 0)
    {
        auto* item = new QListWidgetItem("(No strategies found)");
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        m_configList->addItem(item);
    }
}

void StrategyLoadDialog::onConfigSelected(QListWidgetItem* item)
{
    m_selectedConfigPath = item->data(Qt::UserRole).toString();
    updatePreview(m_selectedConfigPath);
    m_loadButton->setEnabled(true);
}

void StrategyLoadDialog::updatePreview(const QString& configPath)
{
    auto config = StrategyConfigLoader::loadConfig(configPath);

    QString preview;
    preview += QString("<b>Name:</b> %1<br>").arg(config.name);
    preview += QString("<b>Plugin:</b> %1<br>").arg(config.soPath);
    preview += QString("<b>Symbols:</b> %1<br>").arg(config.symbols.join(", "));
    preview += QString("<b>Position Size:</b> %1<br>").arg(config.positionSize);
    preview += QString("<b>Risk Limit:</b> $%1<br>").arg(config.riskLimit);

    if (!config.customParams.empty())
    {
        preview += "<b>Custom Parameters:</b><ul>";
        for (auto it = config.customParams.begin(); it != config.customParams.end(); ++it)
        {
            preview += QString("<li>%1: %2</li>").arg(it->first).arg(it->second.toString());
        }
        preview += "</ul>";
    }

    m_previewLabel->setText(preview);
}

void StrategyLoadDialog::onLoadClicked()
{
    if (!m_selectedConfigPath.isEmpty())
    {
        m_selectedConfig = StrategyConfigLoader::loadConfig(m_selectedConfigPath);
        accept();
    }
}

void StrategyLoadDialog::onCancelClicked()
{
    reject();
}
