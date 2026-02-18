#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QLabel>
#include <QMap>

#include "Misc/ShortcutSettings.h"

class ShortcutsTab : public QWidget
{
    Q_OBJECT

  public:
    explicit ShortcutsTab(QWidget* parent = nullptr);
    ~ShortcutsTab() override = default;

  private slots:
    /// Handle keyboard shortcut change
    /// Validates and saves the new shortcut
    /// @param p_id Shortcut identifier that changed
    void onShortcutChanged(ShortcutSettings::ShortcutId p_id);

    /// Handle reset button click
    /// Restores default shortcut for the specified action
    /// @param p_id Shortcut identifier to reset
    void onResetButtonClicked(ShortcutSettings::ShortcutId p_id);

  private:
    void setupUI();
    void populateShortcuts();
    void updateShortcutDisplay(ShortcutSettings::ShortcutId p_id);

    struct ShortcutWidgets
    {
        QKeySequenceEdit* keySequenceEdit;
        QPushButton* resetButton;
        QLabel* statusLabel;
    };

    QFormLayout* m_shortcutsFormLayout;
    QMap<ShortcutSettings::ShortcutId, ShortcutWidgets> m_shortcutWidgets;
};
