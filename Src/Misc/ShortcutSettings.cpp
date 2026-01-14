#include "ShortcutSettings.h"
#include "Settings.h"
#include <QCoreApplication>

ShortcutSettings::ShortcutSettings()
    : QObject(nullptr)
{
    Q_CHECK_PTR(appStateSettings);
    m_settings = appStateSettings;
    
    loadShortcuts();
}

ShortcutSettings& ShortcutSettings::getInstance() {
    static ShortcutSettings instance;
    return instance;
}

QString ShortcutSettings::getSettingsKey(ShortcutId p_id) const {
    QString key = "Shortcuts/";
    switch (p_id) {
        case QuitApplication:
            key += "QuitApplication";
            break;
        case FocusStockInput:
            key += "FocusStockInput";
            break;
    }
    return key;
}

QString ShortcutSettings::getShortcutName(ShortcutId p_id) const {
    switch (p_id) {
        case QuitApplication:
            return "Quit Application";
        case FocusStockInput:
            return "Focus Stock Input";
        default:
            return "Unknown";
    }
}

QKeySequence ShortcutSettings::getDefaultShortcut(ShortcutId p_id) const {
    switch (p_id) {
        case QuitApplication:
            return QKeySequence("Ctrl+Q");
        case FocusStockInput:
            return QKeySequence("i");
        default:
            return QKeySequence();
    }
}

void ShortcutSettings::loadShortcuts() {
    // Load all shortcuts from settings or use defaults
    for (ShortcutId id : getAllShortcutIds()) {
        QString key = getSettingsKey(id);
        QString sequenceString = m_settings->value(key, getDefaultShortcut(id).toString()).toString();
        m_shortcuts[id] = QKeySequence::fromString(sequenceString);
    }
}

void ShortcutSettings::saveShortcut(ShortcutId p_id) {
    Q_CHECK_PTR(m_settings);
    QString key = getSettingsKey(p_id);
    m_settings->setValue(key, m_shortcuts[p_id].toString());
    m_settings->sync();
}

QKeySequence ShortcutSettings::getShortcut(ShortcutId p_id) const {
    return m_shortcuts.value(p_id, getDefaultShortcut(p_id));
}

bool ShortcutSettings::setShortcut(ShortcutId p_id, const QKeySequence& p_sequence) {
    // Check if the sequence is already in use by another shortcut
    if (isShortcutInUse(p_sequence, p_id)) {
        return false;
    }

    m_shortcuts[p_id] = p_sequence;
    saveShortcut(p_id);
    
    emit shortcutChanged(p_id, p_sequence);
    return true;
}

bool ShortcutSettings::isShortcutInUse(const QKeySequence& p_sequence, ShortcutId p_excludeId) const {
    // Empty sequences are always allowed
    if (p_sequence.isEmpty()) {
        return false;
    }

    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it) {
        if (it.key() != p_excludeId && it.value() == p_sequence) {
            return true;
        }
    }
    return false;
}

void ShortcutSettings::resetToDefault(ShortcutId p_id) {
    QKeySequence defaultSeq = getDefaultShortcut(p_id);
    
    // Check if default would conflict with another shortcut
    if (isShortcutInUse(defaultSeq, p_id)) {
        // If there's a conflict, we can't reset to default
        // This is an edge case that shouldn't normally happen
        return;
    }
    
    m_shortcuts[p_id] = defaultSeq;
    saveShortcut(p_id);
    
    emit shortcutChanged(p_id, defaultSeq);
}

QList<ShortcutSettings::ShortcutId> ShortcutSettings::getAllShortcutIds() const {
    return { QuitApplication, FocusStockInput };
}
