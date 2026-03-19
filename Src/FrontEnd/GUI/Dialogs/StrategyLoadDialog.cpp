#include "StrategyLoadDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

#include "StrategyLoader.h"

StrategyLoadDialog::StrategyLoadDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Load Strategy");
    setMinimumSize(520, 320);
    setModal(true);

    setupUI();
    updateRuntimeTypeUi();
    validateForm();
}

std::optional<StrategyConfig> StrategyLoadDialog::getSelectedConfig() const
{
    return m_selectedConfig;
}

void StrategyLoadDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // --- Runtime file selection ---
    auto* runtimeGroup = new QGroupBox("Strategy Runtime");
    auto* runtimeLayout = new QFormLayout();
    m_runtimeTypeCombo = new QComboBox();
    m_runtimeTypeCombo->addItem("Plugin (.so)");
    m_runtimeTypeCombo->addItem("External Process");
    runtimeLayout->addRow("Type:", m_runtimeTypeCombo);

    auto* pathRowLayout = new QHBoxLayout();
    m_runtimePathEdit = new QLineEdit();
    m_runtimePathEdit->setReadOnly(true);
    m_browseButton = new QPushButton("Browse…");
    pathRowLayout->addWidget(m_runtimePathEdit, 1);
    pathRowLayout->addWidget(m_browseButton);
    auto* pathRowWidget = new QWidget();
    pathRowWidget->setLayout(pathRowLayout);
    runtimeLayout->addRow("Path:", pathRowWidget);
    runtimeGroup->setLayout(runtimeLayout);
    mainLayout->addWidget(runtimeGroup);

    // --- Standard parameters form ---
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

    // --- Custom parameters section (shown only when plugin exports a schema) ---
    m_customParamsGroup = new QGroupBox("Custom Parameters");
    m_customParamsFormLayout = new QFormLayout();
    m_customParamsGroup->setLayout(m_customParamsFormLayout);
    m_customParamsGroup->setVisible(false);
    mainLayout->addWidget(m_customParamsGroup);

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
    connect(m_runtimeTypeCombo,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { onRuntimeTypeChanged(); });
    connect(m_browseButton, &QPushButton::clicked, this, &StrategyLoadDialog::onBrowseClicked);
    connect(m_loadButton, &QPushButton::clicked, this, &StrategyLoadDialog::onLoadClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &StrategyLoadDialog::onCancelClicked);

    connect(m_runtimePathEdit, &QLineEdit::textChanged, this, [this]() { validateForm(); });
    connect(m_symbolsEdit, &QLineEdit::textChanged, this, [this]() { validateForm(); });
}

void StrategyLoadDialog::onRuntimeTypeChanged()
{
    updateRuntimeTypeUi();
    clearCustomParams();
    validateForm();
}

void StrategyLoadDialog::onBrowseClicked()
{
    const StrategyRuntimeType runtimeType = selectedRuntimeType();

    QString startDir;
    if (!m_runtimePathEdit->text().trimmed().isEmpty())
    {
        startDir = QFileInfo(m_runtimePathEdit->text().trimmed()).absolutePath();
    }
    else if (runtimeType == StrategyRuntimeType::PluginSharedLibrary)
    {
        startDir = QDir::homePath() + "/.local/share/L2Trader/Strategies";
        if (!QDir(startDir).exists())
        {
            startDir = QDir::homePath();
        }
    }
    else
    {
        startDir = QDir::homePath();
    }

    const QString browseTitle =
        runtimeType == StrategyRuntimeType::PluginSharedLibrary ? "Select Strategy Plugin" : "Select Strategy Process";
    const QString browseFilter = runtimeType == StrategyRuntimeType::PluginSharedLibrary
                                     ? "Shared Libraries (*.so);;All Files (*)"
                                     : "All Files (*)";
    const QString previousPath = m_runtimePathEdit->text().trimmed();
    const QString runtimePath = QFileDialog::getOpenFileName(this, browseTitle, startDir, browseFilter);

    if (runtimePath.isEmpty())
    {
        return;
    }

    m_runtimePathEdit->setText(runtimePath);

    // Auto-derive name from filename if name field is empty or was auto-derived
    const QFileInfo fileInfo(runtimePath);
    const QString derivedName =
        runtimeType == StrategyRuntimeType::PluginSharedLibrary ? fileInfo.completeBaseName() : fileInfo.fileName();
    const QFileInfo previousInfo(previousPath);
    const QString previousDerivedName = runtimeType == StrategyRuntimeType::PluginSharedLibrary
                                            ? previousInfo.completeBaseName()
                                            : previousInfo.fileName();
    if (m_nameEdit->text().isEmpty() || m_nameEdit->text() == previousDerivedName)
    {
        m_nameEdit->setText(derivedName);
    }

    if (runtimeType == StrategyRuntimeType::PluginSharedLibrary)
    {
        QJsonArray schema = StrategyLoader::peekParameterSchema(runtimePath);
        populateCustomParams(schema);
        return;
    }

    clearCustomParams();
}

void StrategyLoadDialog::onLoadClicked()
{
    const StrategyRuntimeType runtimeType = selectedRuntimeType();
    const QString runtimePath = m_runtimePathEdit->text().trimmed();
    const QString name = m_nameEdit->text().trimmed();
    const QString symbolsText = m_symbolsEdit->text().trimmed();

    if (runtimePath.isEmpty() || !QFileInfo::exists(runtimePath))
    {
        const QString errorMessage = runtimeType == StrategyRuntimeType::PluginSharedLibrary
                                         ? "Please select a valid .so plugin file."
                                         : "Please select a valid strategy executable.";
        QMessageBox::warning(this, "Load Strategy", errorMessage);
        return;
    }

    if (runtimeType == StrategyRuntimeType::ExternalProcess && !QFileInfo(runtimePath).isExecutable())
    {
        QMessageBox::warning(this, "Load Strategy", "The selected strategy process is not executable.");
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
    config.runtimeType = runtimeType;
    if (runtimeType == StrategyRuntimeType::PluginSharedLibrary)
    {
        config.soPath = runtimePath;
    }
    else
    {
        config.executablePath = runtimePath;
    }
    config.name = name.isEmpty() ? QFileInfo(runtimePath).completeBaseName() : name;
    config.symbols = symbols;
    config.positionSize = m_positionSizeSpin->value();
    config.riskLimit = m_riskLimitSpin->value();

    // Collect custom parameter values from dynamically-generated widgets
    for (const auto& [key, widget]: m_customParamWidgets)
    {
        if (auto* spin = qobject_cast<QSpinBox*>(widget))
            config.customParams[key] = QJsonValue(spin->value());
        else if (auto* dblSpin = qobject_cast<QDoubleSpinBox*>(widget))
            config.customParams[key] = QJsonValue(dblSpin->value());
        else if (auto* check = qobject_cast<QCheckBox*>(widget))
            config.customParams[key] = QJsonValue(check->isChecked());
        else if (auto* lineEdit = qobject_cast<QLineEdit*>(widget))
            config.customParams[key] = QJsonValue(lineEdit->text());
    }

    m_selectedConfig = config;
    accept();
}

void StrategyLoadDialog::onCancelClicked()
{
    reject();
}

void StrategyLoadDialog::validateForm()
{
    const bool runtimeValid = !m_runtimePathEdit->text().trimmed().isEmpty();
    const bool symbolsValid = !m_symbolsEdit->text().trimmed().isEmpty();
    m_loadButton->setEnabled(runtimeValid && symbolsValid);
}

void StrategyLoadDialog::clearCustomParams()
{
    // Remove all rows from the form layout
    while (m_customParamsFormLayout->rowCount() > 0)
        m_customParamsFormLayout->removeRow(0);

    m_customParamWidgets.clear();
    m_customParamsGroup->setVisible(false);
    adjustSize();
}

void StrategyLoadDialog::populateCustomParams(const QJsonArray& p_schema)
{
    clearCustomParams();

    if (p_schema.isEmpty())
        return;

    for (const QJsonValue& entry: p_schema)
    {
        const QJsonObject param = entry.toObject();
        const QString key = param["key"].toString();
        const QString type = param["type"].toString();
        const QString label = param.value("label").toString(key);
        const QString description = param.value("description").toString();
        const QJsonValue defaultVal = param.value("default");

        if (key.isEmpty() || type.isEmpty())
            continue;

        QWidget* inputWidget = nullptr;

        if (type == "int")
        {
            auto* spin = new QSpinBox();
            spin->setRange(-1000000, 1000000);
            spin->setValue(defaultVal.toInt(0));
            inputWidget = spin;
        }
        else if (type == "double")
        {
            auto* dblSpin = new QDoubleSpinBox();
            dblSpin->setRange(-1000000.0, 1000000.0);
            dblSpin->setDecimals(4);
            dblSpin->setValue(defaultVal.toDouble(0.0));
            inputWidget = dblSpin;
        }
        else if (type == "bool")
        {
            auto* check = new QCheckBox();
            check->setChecked(defaultVal.toBool(false));
            inputWidget = check;
        }
        else // "string" or unknown
        {
            auto* lineEdit = new QLineEdit();
            lineEdit->setText(defaultVal.toString());
            inputWidget = lineEdit;
        }

        // Build label text; append description as tooltip if present
        QString labelText = label + ":";
        if (!description.isEmpty())
            inputWidget->setToolTip(description);

        m_customParamsFormLayout->addRow(labelText, inputWidget);
        m_customParamWidgets.append({key, inputWidget});
    }

    m_customParamsGroup->setVisible(true);
    adjustSize();
}

StrategyRuntimeType StrategyLoadDialog::selectedRuntimeType() const
{
    return m_runtimeTypeCombo->currentIndex() == 1 ? StrategyRuntimeType::ExternalProcess
                                                   : StrategyRuntimeType::PluginSharedLibrary;
}

void StrategyLoadDialog::updateRuntimeTypeUi()
{
    if (selectedRuntimeType() == StrategyRuntimeType::PluginSharedLibrary)
    {
        m_runtimePathEdit->setPlaceholderText("Select a .so plugin file…");
        return;
    }

    m_runtimePathEdit->setPlaceholderText("Select a strategy executable…");
}
