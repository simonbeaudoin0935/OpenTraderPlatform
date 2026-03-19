#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QVector>
#include <optional>

#include "StrategyConfig.h"

struct ExternalStrategyManifestData;

/**
 * @brief Dialog for loading a strategy runtime
 *
 * Lets the user browse for an external strategy executable or manifest, edit
 * standard parameters (name, symbols, position size, risk limit), and build a
 * StrategyConfig.
 *
 * If an external manifest declares `parameterSchema`, additional rows are
 * dynamically generated below the standard fields for each declared custom
 * parameter.
 */
class StrategyLoadDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit StrategyLoadDialog(QWidget* parent = nullptr);
    ~StrategyLoadDialog() override = default;

    /// @brief Get the configured strategy config if dialog was accepted
    [[nodiscard]] std::optional<StrategyConfig> getSelectedConfig() const;

  private slots:
    void onBrowseClicked();
    void onLoadClicked();
    void onCancelClicked();

  private:
    void setupUI();
    void validateForm();
    void loadSelection(const QString& p_selectedPath);
    void applyManifestDefaults(const ExternalStrategyManifestData& p_manifest);

    /// @brief Rebuild the custom params section from a parameter schema.
    void populateCustomParams(const QJsonArray& p_schema, const std::map<QString, QJsonValue>& p_initialValues = {});

    /// @brief Remove all dynamically-added custom param rows.
    void clearCustomParams();

    // UI components — standard fields
    QLineEdit* m_runtimePathEdit;
    QPushButton* m_browseButton;
    QLineEdit* m_nameEdit;
    QLineEdit* m_symbolsEdit;
    QSpinBox* m_positionSizeSpin;
    QDoubleSpinBox* m_riskLimitSpin;
    QPushButton* m_loadButton;
    QPushButton* m_cancelButton;

    // Custom params section (shown only when the external manifest declares a schema)
    QGroupBox* m_customParamsGroup = nullptr;
    QFormLayout* m_customParamsFormLayout = nullptr;

    // Each entry: (json-key, input-widget)
    // Widget is one of: QLineEdit, QSpinBox, QDoubleSpinBox, QCheckBox
    QVector<QPair<QString, QWidget*>> m_customParamWidgets;
    std::map<QString, QJsonValue> m_manifestCustomParamDefaults;

    // Result
    std::optional<StrategyConfig> m_selectedConfig;
};
