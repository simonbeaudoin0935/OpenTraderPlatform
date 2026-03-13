#pragma once

#include <QDialog>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <optional>

#include "StrategyConfig.h"

/**
 * @brief Dialog for loading a strategy plugin
 *
 * Lets the user browse for a .so file, edit standard parameters
 * (name, symbols, position size, risk limit), and build a StrategyConfig.
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

    // UI components
    QLineEdit* m_soPathEdit;
    QPushButton* m_browseButton;
    QLineEdit* m_nameEdit;
    QLineEdit* m_symbolsEdit;
    QSpinBox* m_positionSizeSpin;
    QDoubleSpinBox* m_riskLimitSpin;
    QPushButton* m_loadButton;
    QPushButton* m_cancelButton;

    // Result
    std::optional<StrategyConfig> m_selectedConfig;
};
