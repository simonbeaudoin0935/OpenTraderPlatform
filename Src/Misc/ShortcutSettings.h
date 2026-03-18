#pragma once

#include <QObject>
#include <QString>
#include <QKeySequence>
#include <QMap>
#include <QSettings>

/**
 * @brief Singleton class to manage application shortcuts
 *
 * This class handles loading, saving, and validating keyboard shortcuts.
 * Shortcuts are persisted to disk immediately when changed.
 */
class ShortcutSettings : public QObject
{
    Q_OBJECT

  public:
    // Shortcut identifiers
    enum ShortcutId
    {
        QuitApplication,
        FocusStockInput,
        ExecuteBuyOrder,
        ExecuteSellOrder,
        ExecuteBuyToCoverOrder,
        ExecuteSellToCoverOrder,
        CancelAllOrders,
        ToggleReplayPlayPause,
        ToggleReplayMode,
        OpenNewChart,
        CloseChartWindow,
        // Timescale shortcuts
        TimeFrame10s,
        TimeFrame1m,
        TimeFrame5m,
        TimeFrame15m,
        TimeFrame30m,
        TimeFrame1h,
        TimeFrame4h,
        TimeFrame1d,
        TimeFrame1w,
        TimeFrame1M
    };
    Q_ENUM(ShortcutId)

    /**
     * @brief Get the singleton instance
     */
    static ShortcutSettings& getInstance();

    /**
     * @brief Get a shortcut key sequence by ID
     * @param p_id The shortcut identifier
     * @return The QKeySequence for the shortcut
     */
    QKeySequence getShortcut(ShortcutId p_id) const;

    /**
     * @brief Set a shortcut key sequence
     * @param p_id The shortcut identifier
     * @param p_sequence The new key sequence
     * @return true if the shortcut was set successfully, false if it would create a duplicate
     */
    bool setShortcut(ShortcutId p_id, const QKeySequence& p_sequence);

    /**
     * @brief Get a human-readable name for a shortcut
     * @param p_id The shortcut identifier
     * @return The display name
     */
    QString getShortcutName(ShortcutId p_id) const;

    /**
     * @brief Get the default key sequence for a shortcut
     * @param p_id The shortcut identifier
     * @return The default QKeySequence
     */
    QKeySequence getDefaultShortcut(ShortcutId p_id) const;

    /**
     * @brief Check if a key sequence is already in use by another shortcut
     * @param p_sequence The key sequence to check
     * @param p_excludeId Optional ID to exclude from the check (when updating an existing shortcut)
     * @return true if the sequence is already in use
     */
    bool isShortcutInUse(const QKeySequence& p_sequence, ShortcutId p_excludeId = static_cast<ShortcutId>(-1)) const;

    /**
     * @brief Reset a shortcut to its default value
     * @param p_id The shortcut identifier
     * @return true if the reset was successful, false if it would create a conflict
     */
    bool resetToDefault(ShortcutId p_id);

    /**
     * @brief Get all shortcut IDs
     * @return List of all shortcut IDs
     */
    QList<ShortcutId> getAllShortcutIds() const;

  signals:
    /**
     * @brief Emitted when a shortcut is changed
     * @param p_id The shortcut that was changed
     * @param p_newSequence The new key sequence
     */
    void shortcutChanged(ShortcutId p_id, const QKeySequence& p_newSequence);

  private:
    ShortcutSettings();
    ~ShortcutSettings() override = default;

    Q_DISABLE_COPY(ShortcutSettings) // Delete copy constructor and assignment operator

    /**
     * @brief Load shortcuts from settings
     */
    void loadShortcuts();

    /**
     * @brief Save a shortcut to settings
     * @param p_id The shortcut identifier
     */
    void saveShortcut(ShortcutId p_id);

    /**
     * @brief Get the settings key for a shortcut
     * @param p_id The shortcut identifier
     * @return The settings key string
     */
    QString getSettingsKey(ShortcutId p_id) const;

    QMap<ShortcutId, QKeySequence> m_shortcuts;
    QSettings* m_settings;
};
