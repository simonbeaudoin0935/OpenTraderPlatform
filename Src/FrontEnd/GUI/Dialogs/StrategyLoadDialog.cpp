#include "StrategyLoadDialog.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Misc/Logging/Logging.h"

StrategyLoadDialog::StrategyLoadDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Load Strategy");
    setMinimumSize(520, 260);
    setModal(true);

    setupUI();
    validateForm();
}

StrategyLoadDialog::StrategyLoadDialog(const StrategyConfig& p_existingConfig, QWidget* parent)
    : QDialog(parent), m_editMode(true)
{
    setWindowTitle("Edit Strategy Parameters");
    setMinimumSize(520, 260);
    setModal(true);

    setupUI();

    m_runtimePathEdit->setText(p_existingConfig.executablePath);
    m_browseButton->setEnabled(false);
    m_browseButton->setVisible(false);
    m_loadButton->setText("Save Parameters");

    if (p_existingConfig.executablePath.trimmed().isEmpty())
    {
        m_descriptionLabel->setText("Saved strategy config has no executable path.");
    }
    else
    {
        loadSelection(p_existingConfig.executablePath, p_existingConfig.fieldValues);
    }

    validateForm();
}

std::optional<StrategyConfig> StrategyLoadDialog::getSelectedConfig() const
{
    return m_selectedConfig;
}

void StrategyLoadDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* runtimeGroup = new QGroupBox("Strategy Runtime");
    auto* runtimeLayout = new QFormLayout();

    auto* pathRowLayout = new QHBoxLayout();
    m_runtimePathEdit = new QLineEdit();
    m_runtimePathEdit->setReadOnly(true);
    m_runtimePathEdit->setPlaceholderText("Select a strategy executable...");
    m_browseButton = new QPushButton("Browse…");
    pathRowLayout->addWidget(m_runtimePathEdit, 1);
    pathRowLayout->addWidget(m_browseButton);
    auto* pathRowWidget = new QWidget();
    pathRowWidget->setLayout(pathRowLayout);
    runtimeLayout->addRow("Path:", pathRowWidget);

    m_descriptionLabel = new QLabel("No strategy executable selected.");
    m_descriptionLabel->setWordWrap(true);
    runtimeLayout->addRow("Description:", m_descriptionLabel);

    runtimeGroup->setLayout(runtimeLayout);
    mainLayout->addWidget(runtimeGroup);

    m_fieldsGroup = new QGroupBox("Parameters");
    m_fieldsFormLayout = new QFormLayout();
    m_fieldsGroup->setLayout(m_fieldsFormLayout);
    m_fieldsGroup->setVisible(false);
    m_fieldsScrollArea = new QScrollArea();
    m_fieldsScrollArea->setWidget(m_fieldsGroup);
    m_fieldsScrollArea->setWidgetResizable(true);
    m_fieldsScrollArea->setVisible(false);
    m_fieldsScrollArea->setMinimumHeight(220);
    mainLayout->addWidget(m_fieldsScrollArea, 1);

    auto* buttonLayout = new QHBoxLayout();
    m_loadButton = new QPushButton(m_editMode ? "Save Parameters" : "Load Strategy");
    m_cancelButton = new QPushButton("Cancel");
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_loadButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);

    connect(m_browseButton, &QPushButton::clicked, this, &StrategyLoadDialog::onBrowseClicked);
    connect(m_loadButton, &QPushButton::clicked, this, &StrategyLoadDialog::onLoadClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &StrategyLoadDialog::onCancelClicked);
    connect(m_runtimePathEdit, &QLineEdit::textChanged, this, [this]() { validateForm(); });
}

void StrategyLoadDialog::onBrowseClicked()
{
    if (m_editMode)
    {
        return;
    }

    logInputEvent(u"StrategyLoadDialog", u"browse-runtime-clicked");
    QString startDir;
    if (!m_runtimePathEdit->text().trimmed().isEmpty())
    {
        startDir = QFileInfo(m_runtimePathEdit->text().trimmed()).absolutePath();
    }
    else
    {
        startDir = QDir::homePath() + "/.local/share/OpenTraderPlatform/Strategies";
        if (!QDir(startDir).exists())
        {
            startDir = QDir::homePath();
        }
    }

    const QString runtimePath =
        QFileDialog::getOpenFileName(this, "Select Strategy Executable", startDir, "All Files (*)");
    if (runtimePath.isEmpty())
    {
        return;
    }

    logInputEvent(u"StrategyLoadDialog", u"runtime-selected", {inputDetail(u"path", runtimePath)});
    loadSelection(runtimePath);
}

void StrategyLoadDialog::loadSelection(const QString& p_selectedPath,
                                       const std::map<QString, QJsonValue>& p_initialValues)
{
    const QString operationTitle = m_editMode ? "Edit Strategy" : "Load Strategy";

    if (p_selectedPath.endsWith(".json", Qt::CaseInsensitive))
    {
        QMessageBox::warning(this,
                             operationTitle,
                             "Manifest files are no longer supported. Please select the strategy executable directly.");
        return;
    }

    const auto descriptionResult = ExternalStrategyDescription::describeExecutable(p_selectedPath);
    if (!descriptionResult.has_value())
    {
        QMessageBox::warning(this, operationTitle, descriptionResult.error());
        return;
    }

    applyDescription(descriptionResult.value(), p_initialValues);
}

void StrategyLoadDialog::applyDescription(const ExternalStrategyDescriptionData& p_description,
                                          const std::map<QString, QJsonValue>& p_initialValues)
{
    m_description = p_description;
    m_runtimePathEdit->setText(p_description.executablePath);
    m_descriptionLabel->setText(QString("%1 (%2)").arg(p_description.name, p_description.version));
    logInputEvent(u"StrategyLoadDialog",
                  u"runtime-introspected",
                  {inputDetail(u"name", p_description.name), inputDetail(u"version", p_description.version)});
    populateFields(p_description.parameterSchema, p_initialValues);
    validateForm();
}

void StrategyLoadDialog::onLoadClicked()
{
    const QString operationTitle = m_editMode ? "Edit Strategy" : "Load Strategy";
    const QString runtimePath = m_runtimePathEdit->text().trimmed();
    if (runtimePath.isEmpty() || !QFileInfo::exists(runtimePath))
    {
        QMessageBox::warning(this, operationTitle, "Please select a valid strategy executable.");
        return;
    }

    if (!QFileInfo(runtimePath).isExecutable())
    {
        QMessageBox::warning(this, operationTitle, "The selected strategy process is not executable.");
        return;
    }

    if (!m_description.has_value())
    {
        QMessageBox::warning(this, operationTitle, "Failed to introspect the selected strategy executable.");
        return;
    }

    const auto collectedValues = collectFieldValues();
    if (!collectedValues.has_value())
    {
        QMessageBox::warning(this, operationTitle, collectedValues.error());
        return;
    }

    StrategyConfig config;
    config.runtimeType = StrategyRuntimeType::ExternalProcess;
    config.executablePath = runtimePath;
    config.name = m_description->name;
    config.version = m_description->version;
    config.fieldValues = collectedValues.value();

    m_selectedConfig = std::move(config);
    logInputEvent(u"StrategyLoadDialog",
                  m_editMode ? u"edit-strategy-confirmed" : u"load-strategy-confirmed",
                  {inputDetail(u"name", m_selectedConfig->name),
                   inputDetail(u"version", m_selectedConfig->version),
                   inputDetail(u"path", m_selectedConfig->executablePath)});
    accept();
}

void StrategyLoadDialog::onCancelClicked()
{
    logInputEvent(u"StrategyLoadDialog", u"cancel-dialog");
    reject();
}

void StrategyLoadDialog::validateForm()
{
    const bool runtimeValid = !m_runtimePathEdit->text().trimmed().isEmpty();
    const bool descriptionValid = m_description.has_value();
    m_loadButton->setEnabled(runtimeValid && descriptionValid);
}

void StrategyLoadDialog::clearFields()
{
    while (m_fieldsFormLayout->rowCount() > 0)
    {
        m_fieldsFormLayout->removeRow(0);
    }

    m_fieldWidgets.clear();
    m_fieldsGroup->setVisible(false);
    if (m_fieldsScrollArea != nullptr)
    {
        m_fieldsScrollArea->setVisible(false);
    }
    adjustSize();
}

void StrategyLoadDialog::populateFields(const QVector<ExternalStrategyFieldDefinition>& p_schema,
                                        const std::map<QString, QJsonValue>& p_initialValues)
{
    clearFields();

    if (p_schema.isEmpty())
    {
        return;
    }

    for (const ExternalStrategyFieldDefinition& field: p_schema)
    {
        const auto initialValueIt = p_initialValues.find(field.key);
        const QJsonValue initialValue =
            initialValueIt != p_initialValues.end() ? initialValueIt->second : field.defaultValue;

        QWidget* inputWidget = nullptr;
        switch (field.type)
        {
        case ExternalStrategyFieldType::Int:
        {
            auto* spin = new QSpinBox();
            spin->setRange(-1000000, 1000000);
            spin->setValue(initialValue.toInt(0));
            inputWidget = spin;
            break;
        }
        case ExternalStrategyFieldType::Double:
        {
            auto* dblSpin = new QDoubleSpinBox();
            dblSpin->setRange(-1000000.0, 1000000.0);
            dblSpin->setDecimals(4);
            dblSpin->setValue(initialValue.toDouble(0.0));
            inputWidget = dblSpin;
            break;
        }
        case ExternalStrategyFieldType::Bool:
        {
            auto* check = new QCheckBox();
            bool checked = initialValue.toBool(false);
            if (!initialValue.isBool() && initialValue.isDouble())
            {
                checked = initialValue.toDouble(0.0) != 0.0;
            }
            else if (!initialValue.isBool() && initialValue.isString())
            {
                const QString lowered = initialValue.toString().trimmed().toLower();
                checked = lowered == "true" || lowered == "1" || lowered == "yes" || lowered == "on" ||
                          lowered == "autonomous" || lowered == "auto";
            }
            check->setChecked(checked);
            inputWidget = check;
            break;
        }
        case ExternalStrategyFieldType::String:
        default:
        {
            auto* lineEdit = new QLineEdit();
            lineEdit->setText(initialValue.toString());
            inputWidget = lineEdit;
            break;
        }
        }

        if (!field.description.isEmpty())
        {
            inputWidget->setToolTip(field.description);
        }

        QString labelText = field.label.isEmpty() ? field.key : field.label;
        if (field.required)
        {
            labelText += " *";
        }
        m_fieldsFormLayout->addRow(labelText + ":", inputWidget);
        m_fieldWidgets.append({field, inputWidget});
    }

    m_fieldsGroup->setVisible(true);
    if (m_fieldsScrollArea != nullptr)
    {
        m_fieldsScrollArea->setVisible(true);
    }
    adjustSize();
}

std::expected<std::map<QString, QJsonValue>, QString> StrategyLoadDialog::collectFieldValues() const
{
    std::map<QString, QJsonValue> values;
    for (const FieldWidget& fieldWidget: m_fieldWidgets)
    {
        const QString& key = fieldWidget.definition.key;
        QWidget* const widget = fieldWidget.widget;
        QJsonValue value;

        if (auto* spin = qobject_cast<QSpinBox*>(widget))
        {
            value = QJsonValue(spin->value());
        }
        else if (auto* dblSpin = qobject_cast<QDoubleSpinBox*>(widget))
        {
            value = QJsonValue(dblSpin->value());
        }
        else if (auto* check = qobject_cast<QCheckBox*>(widget))
        {
            value = QJsonValue(check->isChecked());
        }
        else if (auto* lineEdit = qobject_cast<QLineEdit*>(widget))
        {
            const QString text = lineEdit->text();
            if (fieldWidget.definition.required && text.trimmed().isEmpty() &&
                (fieldWidget.definition.defaultValue.isUndefined() || fieldWidget.definition.defaultValue.isNull()))
            {
                return std::unexpected(QString("Please provide a value for required field \"%1\".").arg(key));
            }
            value = QJsonValue(text);
        }
        else
        {
            return std::unexpected(QString("Unsupported input widget for field \"%1\".").arg(key));
        }

        values[key] = value;
    }

    return values;
}
