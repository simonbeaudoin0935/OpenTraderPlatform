#include "ShortcutSettings.h"
#include "Settings.h"
#include <QCoreApplication>

ShortcutSettings::ShortcutSettings() : QObject(nullptr)
{
    Q_CHECK_PTR(appStateSettings);
    m_settings = appStateSettings;

    migrateLegacyTimeFrameShortcuts(*m_settings);
    loadShortcuts();
}

ShortcutSettings& ShortcutSettings::getInstance()
{
    static ShortcutSettings instance;
    return instance;
}

QString ShortcutSettings::getSettingsKey(ShortcutId p_id) const
{
    QString key = "Shortcuts/";
    switch (p_id)
    {
    case QuitApplication:
        key += "QuitApplication";
        break;
    case FocusStockInput:
        key += "FocusStockInput";
        break;
    case ExecuteBuyOrder:
        key += "ExecuteBuyOrder";
        break;
    case ExecuteSellOrder:
        key += "ExecuteSellOrder";
        break;
    case ExecuteBuyToCoverOrder:
        key += "ExecuteBuyToCoverOrder";
        break;
    case ExecuteSellToCoverOrder:
        key += "ExecuteSellToCoverOrder";
        break;
    case CancelAllOrders:
        key += "CancelAllOrders";
        break;
    case CloseAllPositions:
        key += "CloseAllPositions";
        break;
    case CloseAllPositionsPassive:
        key += "CloseAllPositionsPassive";
        break;
    case ToggleReplayPlayPause:
        key += "ToggleReplayPlayPause";
        break;
    case ToggleReplayMode:
        key += "ToggleReplayMode";
        break;
    case OpenNewChart:
        key += "OpenNewChart";
        break;
    case CloseChartWindow:
        key += "CloseChartWindow";
        break;
    case TimeFrame1m:
        key += "TimeFrame1m";
        break;
    case TimeFrame5m:
        key += "TimeFrame5m";
        break;
    case TimeFrame15m:
        key += "TimeFrame15m";
        break;
    case TimeFrame30m:
        key += "TimeFrame30m";
        break;
    case TimeFrame1h:
        key += "TimeFrame1h";
        break;
    case TimeFrame4h:
        key += "TimeFrame4h";
        break;
    case TimeFrame1d:
        key += "TimeFrame1d";
        break;
    case TimeFrame1w:
        key += "TimeFrame1w";
        break;
    case TimeFrame1M:
        key += "TimeFrame1M";
        break;
    }
    return key;
}

QString ShortcutSettings::getShortcutName(ShortcutId p_id) const
{
    switch (p_id)
    {
    case QuitApplication:
        return "Quit Application";
    case FocusStockInput:
        return "Focus Stock Input";
    case ExecuteBuyOrder:
        return "Execute Buy Order";
    case ExecuteSellOrder:
        return "Execute Sell Order";
    case ExecuteBuyToCoverOrder:
        return "Execute Buy to Cover Order";
    case ExecuteSellToCoverOrder:
        return "Execute Sell to Cover Order";
    case CancelAllOrders:
        return "Cancel All Orders";
    case CloseAllPositions:
        return "Close All Positions";
    case CloseAllPositionsPassive:
        return "Close All Positions (Passive)";
    case ToggleReplayPlayPause:
        return "Toggle Replay Play/Pause";
    case ToggleReplayMode:
        return "Toggle Replay Mode";
    case OpenNewChart:
        return "Open New Chart";
    case CloseChartWindow:
        return "Close Chart Window";
    case TimeFrame1m:
        return "Timescale 1 minute";
    case TimeFrame5m:
        return "Timescale 5 minutes";
    case TimeFrame15m:
        return "Timescale 15 minutes";
    case TimeFrame30m:
        return "Timescale 30 minutes";
    case TimeFrame1h:
        return "Timescale 1 hour";
    case TimeFrame4h:
        return "Timescale 4 hours";
    case TimeFrame1d:
        return "Timescale 1 day";
    case TimeFrame1w:
        return "Timescale 1 week";
    case TimeFrame1M:
        return "Timescale 1 month";
    default:
        return "Unknown";
    }
}

QKeySequence ShortcutSettings::getDefaultShortcut(ShortcutId p_id) const
{
    switch (p_id)
    {
    case QuitApplication:
        return QKeySequence("Ctrl+Q");
    case FocusStockInput:
        return QKeySequence("i");
    case ExecuteBuyOrder:
        return QKeySequence("Ctrl+B");
    case ExecuteSellOrder:
        return QKeySequence("Ctrl+S");
    case ExecuteBuyToCoverOrder:
        return QKeySequence("Ctrl+Shift+B");
    case ExecuteSellToCoverOrder:
        return QKeySequence("Ctrl+Shift+S");
    case CancelAllOrders:
        return QKeySequence("Ctrl+X");
    case CloseAllPositions:
        return QKeySequence("Ctrl+Z");
    case CloseAllPositionsPassive:
        return QKeySequence("Ctrl+Shift+Z");
    case ToggleReplayPlayPause:
        return QKeySequence(Qt::Key_Space);
    case ToggleReplayMode:
        return QKeySequence("r");
    case OpenNewChart:
        return QKeySequence("Ctrl+T");
    case CloseChartWindow:
        return QKeySequence("Ctrl+W");
    case TimeFrame1m:
        return QKeySequence("1");
    case TimeFrame5m:
        return QKeySequence("2");
    case TimeFrame15m:
        return QKeySequence("3");
    case TimeFrame30m:
        return QKeySequence("4");
    case TimeFrame1h:
        return QKeySequence("5");
    case TimeFrame4h:
        return QKeySequence("6");
    case TimeFrame1d:
        return QKeySequence("7");
    case TimeFrame1w:
        return QKeySequence("8");
    case TimeFrame1M:
        return QKeySequence("9");
    default:
        return QKeySequence();
    }
}

void ShortcutSettings::migrateLegacyTimeFrameShortcuts(QSettings& p_settings)
{
    const QStringList names{"TimeFrame1m",
                            "TimeFrame5m",
                            "TimeFrame15m",
                            "TimeFrame30m",
                            "TimeFrame1h",
                            "TimeFrame4h",
                            "TimeFrame1d",
                            "TimeFrame1w",
                            "TimeFrame1M"};
    bool legacyLayout = true;
    for (int i = 0; i < names.size(); ++i)
    {
        const QString key = QStringLiteral("Shortcuts/") + names[i];
        const QString oldDefault = QString::number((i + 2) % 10);
        if (QKeySequence::fromString(p_settings.value(key, oldDefault).toString()) != QKeySequence(oldDefault))
        {
            legacyLayout = false;
            break;
        }
    }
    if (legacyLayout)
    {
        for (const auto& name: names)
        {
            p_settings.remove(QStringLiteral("Shortcuts/") + name);
        }
    }
    p_settings.remove(QStringLiteral("Shortcuts/TimeFrame10s"));
}

void ShortcutSettings::loadShortcuts()
{
    // Load all shortcuts from settings or use defaults
    for (ShortcutId id: getAllShortcutIds())
    {
        QString key = getSettingsKey(id);
        QString sequenceString = m_settings->value(key, getDefaultShortcut(id).toString()).toString();
        m_shortcuts[id] = QKeySequence::fromString(sequenceString);
    }
}

void ShortcutSettings::saveShortcut(ShortcutId p_id)
{
    Q_CHECK_PTR(m_settings);
    QString key = getSettingsKey(p_id);
    m_settings->setValue(key, m_shortcuts[p_id].toString());
    // Sync immediately to ensure persistence as per requirements
    m_settings->sync();
}

QKeySequence ShortcutSettings::getShortcut(ShortcutId p_id) const
{
    return m_shortcuts.value(p_id, getDefaultShortcut(p_id));
}

bool ShortcutSettings::setShortcut(ShortcutId p_id, const QKeySequence& p_sequence)
{
    // Check if the sequence is already in use by another shortcut
    if (isShortcutInUse(p_sequence, p_id))
    {
        return false;
    }

    m_shortcuts[p_id] = p_sequence;
    saveShortcut(p_id);

    emit shortcutChanged(p_id, p_sequence);
    return true;
}

bool ShortcutSettings::isShortcutInUse(const QKeySequence& p_sequence, ShortcutId p_excludeId) const
{
    // Empty sequences are always allowed
    if (p_sequence.isEmpty())
    {
        return false;
    }

    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it)
    {
        if (it.key() != p_excludeId && it.value() == p_sequence)
        {
            return true;
        }
    }
    return false;
}

bool ShortcutSettings::resetToDefault(ShortcutId p_id)
{
    QKeySequence defaultSeq = getDefaultShortcut(p_id);

    // Check if default would conflict with another shortcut
    if (isShortcutInUse(defaultSeq, p_id))
    {
        // If there's a conflict, we can't reset to default
        return false;
    }

    m_shortcuts[p_id] = defaultSeq;
    saveShortcut(p_id);

    emit shortcutChanged(p_id, defaultSeq);
    return true;
}

QList<ShortcutSettings::ShortcutId> ShortcutSettings::getAllShortcutIds() const
{
    return {QuitApplication,
            FocusStockInput,
            ExecuteBuyOrder,
            ExecuteSellOrder,
            ExecuteBuyToCoverOrder,
            ExecuteSellToCoverOrder,
            CancelAllOrders,
            CloseAllPositions,
            CloseAllPositionsPassive,
            ToggleReplayPlayPause,
            ToggleReplayMode,
            OpenNewChart,
            CloseChartWindow,
            TimeFrame1m,
            TimeFrame5m,
            TimeFrame15m,
            TimeFrame30m,
            TimeFrame1h,
            TimeFrame4h,
            TimeFrame1d,
            TimeFrame1w,
            TimeFrame1M};
}
