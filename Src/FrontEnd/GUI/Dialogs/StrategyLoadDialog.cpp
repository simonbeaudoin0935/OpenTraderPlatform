#include "StrategyLoadDialog.h"

#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

StrategyLoadDialog::StrategyLoadDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Load Strategy");
    setMinimumSize(500, 300);
    setModal(true);

    setupUI();
    validateForm();
}

std::optional<StrategyConfig> StrategyLoadDialog::getSelectedConfig() const
{
    return m_selectedConfig;
}

void StrategyLoadDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // --- Plugin file selection ---
    auto* pluginGroup = new QGroupBox("Strategy Plugin");
    auto* pluginLayout = new QHBoxLayout();
    m_soPathEdit = new QLineEdit();
    m_soPathEdit->setPlaceholderText("Select a .so plugin file…");
    m_soPathEdit->setReadOnly(true);
    m_browseButton = new QPushButton("Browse…");
    pluginLayout->addWidget(m_soPathEdit, 1);
    pluginLayout->addWidget(m_browseButton);
    pluginGroup->setLayout(pluginLayout);
    mainLayout->addWidget(pluginGroup);

    // --- Parameters form ---
    auto* paramsGroup = new QGroupBox("Parameters");
    auto* formLayout = new QFormLayout();

    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText("Auto-derived from plugin filename");
    formLayout->addRow("Strategy Name:", m_nameEdit);

    m_symbolsEdit = new QLineEdit();
    m_symbolsEdit->setPlaceholderText("e.g. AAPL, TSLA, MSFT");
    formLayout->addRow("Symbols:", m_symbolsEdit);

    m_positionSizeSpin = new QSpinBox();
    m_positionSizeSpin->setRange(1, 100000);
    m_positionSizeSpin->setValue(100);
    formLayout->addRow("Position Size:", m_positionSizeSpin);

    m_riskLimitSpin = new QDoubleSpinBox();
    m_riskLimitSpin->setRange(0.0, 1000000.0);
    m_riskLimitSpin->setDecimals(2);
    m_riskLimitSpin->setPrefix("$ ");
    m_riskLimitSpin->setValue(500.0);
    formLayout->addRow("Risk Limit:", m_riskLimitSpin);

    paramsGroup->setLayout(formLayout);
    mainLayout->addWidget(paramsGroup);

    mainLayout->addStretch();

    // --- Buttons ---
    auto* buttonLayout = new QHBoxLayout();
    m_loadButton = new QPushButton("Load Strategy");
    m_cancelButton = new QPushButton("Cancel");
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_loadButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);

    // --- Connections ---
    connect(m_browseButton, &QPushButton::clicked, this, &StrategyLoadDialog::onBrowseClicked);
    connect(m_loadButton, &QPushButton::clicked, this, &StrategyLoadDialog::onLoadClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &StrategyLoadDialog::onCancelClicked);

    connect(m_soPathEdit, &QLineEdit::textChanged, this, [this]() { validateForm(); });
    connect(m_symbolsEdit, &QLineEdit::textChanged, this, [this]() { validateForm(); });
}

void StrategyLoadDialog::onBrowseClicked()
{
    // Start in user's strategy directory if it exists, otherwise home
    QString startDir = QDir::homePath() + "/.local/share/L2Trader/Strategies";
    if (!QDir(startDir).exists())
    {
        startDir = QDir::homePath();
    }

    QString soPath = QFileDialog::getOpenFileName(this,
                                                  "Select Strategy Plugin",
                                                  startDir,
                                                  "Shared Libraries (*.so);;All Files (*)");

    if (soPath.isEmpty())
    {
        return;
    }

    m_soPathEdit->setText(soPath);

    // Auto-derive name from filename if name field is empty or was auto-derived
    QFileInfo fileInfo(soPath);
    QString baseName = fileInfo.completeBaseName(); // "DumpPatternStrategy" from "DumpPatternStrategy.so"
    if (m_nameEdit->text().isEmpty() || m_nameEdit->text() == QFileInfo(m_soPathEdit->text()).completeBaseName())
    {
        m_nameEdit->setText(baseName);
    }
}

void StrategyLoadDialog::onLoadClicked()
{
    QString soPath = m_soPathEdit->text().trimmed();
    QString name = m_nameEdit->text().trimmed();
    QString symbolsText = m_symbolsEdit->text().trimmed();

    if (soPath.isEmpty() || !QFileInfo::exists(soPath))
    {
        QMessageBox::warning(this, "Load Strategy", "Please select a valid .so plugin file.");
        return;
    }

    if (symbolsText.isEmpty())
    {
        QMessageBox::warning(this, "Load Strategy", "Please enter at least one symbol.");
        return;
    }

    // Parse comma-separated symbols, trim whitespace, uppercase, remove empties
    QStringList symbols;
    for (const QString& s: symbolsText.split(","))
    {
        QString trimmed = s.trimmed().toUpper();
        if (!trimmed.isEmpty())
        {
            symbols.append(trimmed);
        }
    }

    if (symbols.isEmpty())
    {
        QMessageBox::warning(this, "Load Strategy", "Please enter at least one valid symbol.");
        return;
    }

    StrategyConfig config;
    config.soPath = soPath;
    config.name = name.isEmpty() ? QFileInfo(soPath).completeBaseName() : name;
    config.symbols = symbols;
    config.positionSize = m_positionSizeSpin->value();
    config.riskLimit = m_riskLimitSpin->value();

    m_selectedConfig = config;
    accept();
}

void StrategyLoadDialog::onCancelClicked()
{
    reject();
}

void StrategyLoadDialog::validateForm()
{
    bool soValid = !m_soPathEdit->text().trimmed().isEmpty();
    bool symbolsValid = !m_symbolsEdit->text().trimmed().isEmpty();
    m_loadButton->setEnabled(soValid && symbolsValid);
}
