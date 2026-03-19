#pragma once

#include <QComboBox>
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

/**
 * @brief Dialog for loading a strategy runtime
 *
 * Lets the user browse for a strategy artifact, edit standard parameters
 * (name, symbols, position size, risk limit), and build a StrategyConfig.
 *
 * If a plugin exports `getParameterSchema()`, additional rows are
 * dynamically generated below the standard fields for each declared
 * custom parameter.
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
    void onRuntimeTypeChanged();
    void onBrowseClicked();
    void onLoadClicked();
    void onCancelClicked();

  private:
    void setupUI();
    void validateForm();

    /// @brief Rebuild the custom params section from a parameter schema.
    void populateCustomParams(const QJsonArray& p_schema);

    /// @brief Remove all dynamically-added custom param rows.
    void clearCustomParams();

    [[nodiscard]] StrategyRuntimeType selectedRuntimeType() const;
    void updateRuntimeTypeUi();

    // UI components — standard fields
    QComboBox* m_runtimeTypeCombo;
    QLineEdit* m_runtimePathEdit;
    QPushButton* m_browseButton;
    QLineEdit* m_nameEdit;
    QLineEdit* m_symbolsEdit;
    QSpinBox* m_positionSizeSpin;
    QDoubleSpinBox* m_riskLimitSpin;
    QPushButton* m_loadButton;
    QPushButton* m_cancelButton;

    // Custom params section (shown only when plugin exports a schema)
    QGroupBox* m_customParamsGroup = nullptr;
    QFormLayout* m_customParamsFormLayout = nullptr;

    // Each entry: (json-key, input-widget)
    // Widget is one of: QLineEdit, QSpinBox, QDoubleSpinBox, QCheckBox
    QVector<QPair<QString, QWidget*>> m_customParamWidgets;

    // Result
    std::optional<StrategyConfig> m_selectedConfig;
};
