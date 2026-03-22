#include "StrategyLoadDialog.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>

StrategyLoadDialog::StrategyLoadDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Load Strategy");
    setMinimumSize(520, 260);
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
    mainLayout->addWidget(m_fieldsGroup);

    mainLayout->addStretch();

    auto* buttonLayout = new QHBoxLayout();
    m_loadButton = new QPushButton("Load Strategy");
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
    QString startDir;
    if (!m_runtimePathEdit->text().trimmed().isEmpty())
    {
        startDir = QFileInfo(m_runtimePathEdit->text().trimmed()).absolutePath();
    }
    else
    {
        startDir = QDir::homePath() + "/.local/share/L2Trader/Strategies";
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

    loadSelection(runtimePath);
}

void StrategyLoadDialog::loadSelection(const QString& p_selectedPath)
{
    if (p_selectedPath.endsWith(".json", Qt::CaseInsensitive))
    {
        QMessageBox::warning(this,
                             "Load Strategy",
                             "Manifest files are no longer supported. Please select the strategy executable directly.");
        return;
    }

    const auto descriptionResult = ExternalStrategyDescription::describeExecutable(p_selectedPath);
    if (!descriptionResult.has_value())
    {
        QMessageBox::warning(this, "Load Strategy", descriptionResult.error());
        return;
    }

    applyDescription(descriptionResult.value());
}

void StrategyLoadDialog::applyDescription(const ExternalStrategyDescriptionData& p_description,
                                          const std::map<QString, QJsonValue>& p_initialValues)
{
    m_description = p_description;
    m_runtimePathEdit->setText(p_description.executablePath);
    m_descriptionLabel->setText(QString("%1 (%2)").arg(p_description.name, p_description.version));
    populateFields(p_description.parameterSchema, p_initialValues);
    validateForm();
}

void StrategyLoadDialog::onLoadClicked()
{
    const QString runtimePath = m_runtimePathEdit->text().trimmed();
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

    if (!m_description.has_value())
    {
        QMessageBox::warning(this, "Load Strategy", "Failed to introspect the selected strategy executable.");
        return;
    }

    const auto collectedValues = collectFieldValues();
    if (!collectedValues.has_value())
    {
        QMessageBox::warning(this, "Load Strategy", collectedValues.error());
        return;
    }

    StrategyConfig config;
    config.runtimeType = StrategyRuntimeType::ExternalProcess;
    config.executablePath = runtimePath;
    config.name = m_description->name;
    config.version = m_description->version;
    config.fieldValues = collectedValues.value();

    m_selectedConfig = std::move(config);
    accept();
}

void StrategyLoadDialog::onCancelClicked()
{
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
            check->setChecked(initialValue.toBool(false));
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
