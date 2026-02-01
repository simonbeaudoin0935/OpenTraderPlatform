#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QString>
#include <optional>

#include "StrategyConfig.h"

/**
 * @brief Dialog for loading a strategy plugin
 *
 * Shows available strategy configs from ~/.config/L2Trader/strategies/
 * Allows user to select one and preview its configuration
 */
class StrategyLoadDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit StrategyLoadDialog(QWidget* parent = nullptr);
    ~StrategyLoadDialog() override = default;

    /// @brief Get the selected strategy config if dialog was accepted
    /// @return Optional config (empty if cancelled)
    [[nodiscard]] std::optional<StrategyConfig> getSelectedConfig() const;

  private slots:
    void onConfigSelected(QListWidgetItem* item);
    void onLoadClicked();
    void onCancelClicked();
    void refreshConfigList();

  private:
    void setupUI();
    void loadAvailableConfigs();
    void updatePreview(const QString& configPath);

    // UI components
    QListWidget* m_configList;
    QLabel* m_previewLabel;
    QPushButton* m_loadButton;
    QPushButton* m_cancelButton;

    // Selected config
    std::optional<StrategyConfig> m_selectedConfig;
    QString m_selectedConfigPath;
};
