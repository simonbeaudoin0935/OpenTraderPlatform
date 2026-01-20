#include "ShortcutsTab.h"
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QTimer>

ShortcutsTab::ShortcutsTab(QWidget* parent) : QWidget(parent), m_shortcutsFormLayout(nullptr)
{
    setupUI();
    populateShortcuts();
}

void ShortcutsTab::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Shortcuts configuration section
    QGroupBox* shortcutsGroupBox = new QGroupBox("Keyboard Shortcuts");
    QVBoxLayout* shortcutsLayout = new QVBoxLayout(shortcutsGroupBox);

    // Form layout for shortcuts
    m_shortcutsFormLayout = new QFormLayout();
    m_shortcutsFormLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    m_shortcutsFormLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    shortcutsLayout->addLayout(m_shortcutsFormLayout);

    // Add info label at the bottom
    QLabel* infoLabel = new QLabel("Press the desired key combination in the input field. "
                                   "Changes are saved immediately. "
                                   "Click 'Reset' to restore the default shortcut.");
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("QLabel { color: #AAAAAA; font-size: 10px; padding: 10px; }");
    shortcutsLayout->addWidget(infoLabel);

    mainLayout->addWidget(shortcutsGroupBox);
    mainLayout->addStretch();
}

void ShortcutsTab::populateShortcuts()
{
    ShortcutSettings& settings = ShortcutSettings::getInstance();

    // Get all shortcut IDs and create UI elements for each
    for (ShortcutSettings::ShortcutId id: settings.getAllShortcutIds())
    {
        // Create widgets for this shortcut
        QWidget* rowWidget = new QWidget();
        QHBoxLayout* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);

        QKeySequenceEdit* keySequenceEdit = new QKeySequenceEdit();
        keySequenceEdit->setKeySequence(settings.getShortcut(id));

        QPushButton* resetButton = new QPushButton("Reset");
        resetButton->setMaximumWidth(80);

        QLabel* statusLabel = new QLabel();
        statusLabel->setStyleSheet("QLabel { color: #AAAAAA; font-size: 9px; }");
        statusLabel->setMinimumWidth(100);

        rowLayout->addWidget(keySequenceEdit, 1);
        rowLayout->addWidget(resetButton);
        rowLayout->addWidget(statusLabel);

        // Store widgets for later access
        ShortcutWidgets widgets;
        widgets.keySequenceEdit = keySequenceEdit;
        widgets.resetButton = resetButton;
        widgets.statusLabel = statusLabel;
        m_shortcutWidgets[id] = widgets;

        // Add to form layout
        m_shortcutsFormLayout->addRow(settings.getShortcutName(id) + ":", rowWidget);

        // Connect signals
        // Note: Qt::UniqueConnection cannot be used with lambda functions
        auto keySequenceConnection =
            connect(keySequenceEdit, &QKeySequenceEdit::editingFinished, this, [this, id]() { onShortcutChanged(id); });
        Q_ASSERT(keySequenceConnection);

        auto resetButtonConnection =
            connect(resetButton, &QPushButton::clicked, this, [this, id]() { onResetButtonClicked(id); });
        Q_ASSERT(resetButtonConnection);
    }
}

void ShortcutsTab::onShortcutChanged(ShortcutSettings::ShortcutId p_id)
{
    ShortcutSettings& settings = ShortcutSettings::getInstance();

    Q_ASSERT(m_shortcutWidgets.contains(p_id));
    ShortcutWidgets& widgets = m_shortcutWidgets[p_id];

    QKeySequence newSequence = widgets.keySequenceEdit->keySequence();

    // Attempt to set the shortcut
    if (settings.setShortcut(p_id, newSequence))
    {
        // Success
        widgets.statusLabel->setText("Saved");
        widgets.statusLabel->setStyleSheet("QLabel { color: #4CAF50; font-size: 9px; }");

        // Clear status after 2 seconds
        QTimer::singleShot(2000,
                           this,
                           [this, p_id]()
                           {
                               if (m_shortcutWidgets.contains(p_id))
                               {
                                   m_shortcutWidgets[p_id].statusLabel->clear();
                               }
                           });
    }
    else
    {
        // Failed - shortcut is already in use
        QMessageBox::warning(
            this,
            "Shortcut Conflict",
            QString("The shortcut '%1' is already in use by another action. Please choose a different shortcut.")
                .arg(newSequence.toString()));

        // Revert to previous value
        widgets.keySequenceEdit->setKeySequence(settings.getShortcut(p_id));

        widgets.statusLabel->setText("Conflict");
        widgets.statusLabel->setStyleSheet("QLabel { color: #f44336; font-size: 9px; }");

        // Clear status after 3 seconds
        QTimer::singleShot(3000,
                           this,
                           [this, p_id]()
                           {
                               if (m_shortcutWidgets.contains(p_id))
                               {
                                   m_shortcutWidgets[p_id].statusLabel->clear();
                               }
                           });
    }
}

void ShortcutsTab::onResetButtonClicked(ShortcutSettings::ShortcutId p_id)
{
    ShortcutSettings& settings = ShortcutSettings::getInstance();

    QKeySequence defaultSeq = settings.getDefaultShortcut(p_id);

    // Attempt to reset to default
    if (!settings.resetToDefault(p_id))
    {
        QMessageBox::warning(
            this,
            "Cannot Reset",
            QString("Cannot reset to default shortcut '%1' because it is already in use by another action. "
                    "Please change the conflicting shortcut first.")
                .arg(defaultSeq.toString()));
        return;
    }

    updateShortcutDisplay(p_id);
}

void ShortcutsTab::updateShortcutDisplay(ShortcutSettings::ShortcutId p_id)
{
    ShortcutSettings& settings = ShortcutSettings::getInstance();

    Q_ASSERT(m_shortcutWidgets.contains(p_id));
    ShortcutWidgets& widgets = m_shortcutWidgets[p_id];

    widgets.keySequenceEdit->setKeySequence(settings.getShortcut(p_id));
    widgets.statusLabel->setText("Reset");
    widgets.statusLabel->setStyleSheet("QLabel { color: #2196F3; font-size: 9px; }");

    // Clear status after 2 seconds
    QTimer::singleShot(2000,
                       this,
                       [this, p_id]()
                       {
                           if (m_shortcutWidgets.contains(p_id))
                           {
                               m_shortcutWidgets[p_id].statusLabel->clear();
                           }
                       });
}
