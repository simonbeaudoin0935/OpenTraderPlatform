#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QLabel>
#include <QMap>

#include "Misc/ShortcutSettings.h"

class ShortcutsTab : public QWidget {
    Q_OBJECT

public:
    explicit ShortcutsTab(QWidget* parent = nullptr);
    ~ShortcutsTab() override = default;

private slots:
    void onShortcutChanged(ShortcutSettings::ShortcutId p_id);
    void onResetButtonClicked(ShortcutSettings::ShortcutId p_id);

private:
    void setupUI();
    void populateShortcuts();
    void updateShortcutDisplay(ShortcutSettings::ShortcutId p_id);

    struct ShortcutWidgets {
        QKeySequenceEdit* keySequenceEdit;
        QPushButton* resetButton;
        QLabel* statusLabel;
    };

    QFormLayout* m_shortcutsFormLayout;
    QMap<ShortcutSettings::ShortcutId, ShortcutWidgets> m_shortcutWidgets;
};
