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

#include "ExternalStrategyManifest.h"

namespace
{
    [[nodiscard]] QString autoDerivedStrategyName(const QString& p_runtimePath)
    {
        const QFileInfo fileInfo(p_runtimePath);
        return fileInfo.completeSuffix().isEmpty() ? fileInfo.fileName() : fileInfo.completeBaseName();
    }
} // namespace

StrategyLoadDialog::StrategyLoadDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Load Strategy");
    setMinimumSize(520, 320);
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

    // --- Runtime file selection ---
    auto* runtimeGroup = new QGroupBox("Strategy Runtime");
    auto* runtimeLayout = new QFormLayout();

    auto* pathRowLayout = new QHBoxLayout();
    m_runtimePathEdit = new QLineEdit();
    m_runtimePathEdit->setReadOnly(true);
    m_runtimePathEdit->setPlaceholderText("Select a strategy executable or manifest...");
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
    m_nameEdit->setPlaceholderText("Auto-derived from executable filename");
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

    // --- Custom parameters section (shown only when the manifest declares a schema) ---
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
    connect(m_browseButton, &QPushButton::clicked, this, &StrategyLoadDialog::onBrowseClicked);
    connect(m_loadButton, &QPushButton::clicked, this, &StrategyLoadDialog::onLoadClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &StrategyLoadDialog::onCancelClicked);

    connect(m_runtimePathEdit, &QLineEdit::textChanged, this, [this]() { validateForm(); });
    connect(m_symbolsEdit, &QLineEdit::textChanged, this, [this]() { validateForm(); });
}

void StrategyLoadDialog::onBrowseClicked()
{
    QString startDir;
    if (!m_runtimePathEdit->text().trimmed().isEmpty())
    {
        startDir = QFileInfo(m_runtimePathEdit->text().trimmed()).absolutePath();
    }
    else
    {
        startDir = QDir::homePath() + "/.config/L2Trader/Strategies";
        if (!QDir(startDir).exists())
        {
            startDir = QDir::homePath() + "/.local/share/L2Trader/Strategies";
        }
        if (!QDir(startDir).exists())
        {
            startDir = QDir::homePath();
        }
    }

    const QString runtimePath = QFileDialog::getOpenFileName(this,
                                                             "Select Strategy Executable or Manifest",
                                                             startDir,
                                                             "Strategy files (*.json);;All Files (*)");

    if (runtimePath.isEmpty())
    {
        return;
    }

    loadSelection(runtimePath);
}

void StrategyLoadDialog::loadSelection(const QString& p_selectedPath)
{
    const QString previousPath = m_runtimePathEdit->text().trimmed();
    const QString previousDerivedName = autoDerivedStrategyName(previousPath);

    m_manifestCustomParamDefaults.clear();
    clearCustomParams();

    if (p_selectedPath.endsWith(".json", Qt::CaseInsensitive))
    {
        const auto manifestResult = ExternalStrategyManifest::loadFromFile(p_selectedPath);
        if (!manifestResult.has_value())
        {
            QMessageBox::warning(this, "Load Strategy", manifestResult.error());
            return;
        }

        applyManifestDefaults(manifestResult.value());
        return;
    }

    m_runtimePathEdit->setText(p_selectedPath);

    if (auto manifest = ExternalStrategyManifest::findForRuntime(p_selectedPath); manifest.has_value())
    {
        applyManifestDefaults(*manifest);
        return;
    }

    if (m_nameEdit->text().isEmpty() || m_nameEdit->text() == previousDerivedName)
    {
        m_nameEdit->setText(autoDerivedStrategyName(p_selectedPath));
    }
}

void StrategyLoadDialog::applyManifestDefaults(const ExternalStrategyManifestData& p_manifest)
{
    m_runtimePathEdit->setText(p_manifest.config.executablePath);
    m_nameEdit->setText(!p_manifest.config.name.isEmpty() ? p_manifest.config.name
                                                          : autoDerivedStrategyName(p_manifest.config.executablePath));
    m_symbolsEdit->setText(p_manifest.config.symbols.join(", "));
    m_positionSizeSpin->setValue(p_manifest.config.positionSize);
    m_riskLimitSpin->setValue(p_manifest.config.riskLimit);
    m_manifestCustomParamDefaults = p_manifest.config.customParams;
    populateCustomParams(p_manifest.parameterSchema, m_manifestCustomParamDefaults);
}

void StrategyLoadDialog::onLoadClicked()
{
    const QString runtimePath = m_runtimePathEdit->text().trimmed();
    const QString name = m_nameEdit->text().trimmed();
    const QString symbolsText = m_symbolsEdit->text().trimmed();

    if (runtimePath.isEmpty() || !QFileInfo::exists(runtimePath))
    {
        QMessageBox::warning(this, "Load Strategy", "Please select a valid strategy executable.");
        return;
    }

    if (!QFileInfo(runtimePath).isExecutable())
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
    config.runtimeType = StrategyRuntimeType::ExternalProcess;
    config.executablePath = runtimePath;
    config.name = name.isEmpty() ? autoDerivedStrategyName(runtimePath) : name;
    config.symbols = symbols;
    config.positionSize = m_positionSizeSpin->value();
    config.riskLimit = m_riskLimitSpin->value();
    config.customParams = m_manifestCustomParamDefaults;

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

void StrategyLoadDialog::populateCustomParams(const QJsonArray& p_schema,
                                              const std::map<QString, QJsonValue>& p_initialValues)
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
        const auto initialValueIt = p_initialValues.find(key);
        const QJsonValue defaultVal =
            initialValueIt != p_initialValues.end() ? initialValueIt->second : param.value("default");

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
