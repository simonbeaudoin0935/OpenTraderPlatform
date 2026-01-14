# Shortcuts Tab Implementation

## Overview

This document describes the implementation of the keyboard shortcuts configuration tab in the L2Trader GUI application.

## Components

### 1. ShortcutSettings Singleton

**Location:** `Src/Misc/ShortcutSettings.h` and `Src/Misc/ShortcutSettings.cpp`

The `ShortcutSettings` class is a singleton that manages all keyboard shortcuts in the application. It provides:

- **Persistent Storage:** Shortcuts are stored in the application's settings file (`AppState.ini`) under the `Shortcuts/` section
- **Duplicate Prevention:** The class validates that no two actions have the same shortcut
- **Default Values:** Each shortcut has a default value that can be restored
- **Change Notifications:** Emits signals when shortcuts are changed so the UI can update in real-time

#### Key Methods:

- `getInstance()` - Get the singleton instance
- `getShortcut(ShortcutId)` - Retrieve the current key sequence for a shortcut
- `setShortcut(ShortcutId, QKeySequence)` - Set a new key sequence (validates for duplicates)
- `isShortcutInUse(QKeySequence)` - Check if a key sequence is already assigned
- `resetToDefault(ShortcutId)` - Reset a shortcut to its default value

#### Defined Shortcuts:

1. **QuitApplication** - Default: `Ctrl+Q` - Quits the application
2. **FocusStockInput** - Default: `i` - Clears and focuses the stock symbol input field

### 2. ShortcutsTab Widget

**Location:** `Src/FrontEnd/GUI/Tabs/ShortcutsTab.h` and `Src/FrontEnd/GUI/Tabs/ShortcutsTab.cpp`

The `ShortcutsTab` is a UI component that displays all available shortcuts and allows users to edit them.

#### Features:

- **Visual Editor:** Uses `QKeySequenceEdit` widgets for intuitive keyboard shortcut input
- **Immediate Persistence:** Changes are saved to disk immediately when a user finishes editing
- **Status Feedback:** Shows visual feedback (Saved/Conflict/Reset) after each action
- **Reset Buttons:** Each shortcut has a reset button to restore the default value
- **Conflict Detection:** Warns users if they try to assign a shortcut that's already in use

#### UI Layout:

```
┌─ Keyboard Shortcuts ────────────────────────────┐
│                                                  │
│  Quit Application:      [Ctrl+Q    ] [Reset] ✓  │
│  Focus Stock Input:     [i         ] [Reset]    │
│                                                  │
│  ℹ️ Press the desired key combination...        │
└──────────────────────────────────────────────────┘
```

### 3. GUIFrontend Integration

**Modified Files:** `Src/FrontEnd/GUI/GUIFrontend.h` and `Src/FrontEnd/GUI/GUIFrontend.cpp`

The main GUI frontend was updated to:

1. **Initialize from Settings:** Load shortcuts from `ShortcutSettings` on startup
2. **Dynamic Updates:** Listen for shortcut changes and update active shortcuts in real-time
3. **Add Shortcuts Tab:** The new "Shortcuts" tab is added to the main tab widget

#### Changes Made:

- Added `m_quitShortcut` and `m_focusShortcut` member variables to track QShortcut objects
- Added `onShortcutChanged()` slot to handle dynamic shortcut updates
- Connected to `ShortcutSettings::shortcutChanged` signal
- Added ShortcutsTab to the main tab widget

## Usage

### For Users:

1. Open the L2Trader application
2. Navigate to the "Shortcuts" tab
3. Click in any shortcut field and press the desired key combination
4. The shortcut is saved automatically
5. To reset a shortcut, click the "Reset" button next to it

### For Developers:

To add a new shortcut:

1. Add a new enum value to `ShortcutSettings::ShortcutId`
2. Update `getSettingsKey()` to map the ID to a settings key
3. Update `getShortcutName()` to provide a display name
4. Update `getDefaultShortcut()` to specify the default key sequence
5. Update `getAllShortcutIds()` to include the new ID
6. In GUIFrontend, create a QShortcut object and handle the signal
7. Connect to `ShortcutSettings::shortcutChanged` to handle updates

## Implementation Details

### Settings Storage

Shortcuts are stored in `AppState.ini` under the `Shortcuts/` section:

```ini
[Shortcuts]
QuitApplication=Ctrl+Q
FocusStockInput=i
```

### Validation Logic

The duplicate detection works as follows:

1. When a user changes a shortcut, `ShortcutsTab::onShortcutChanged()` is called
2. This calls `ShortcutSettings::setShortcut()` which internally calls `isShortcutInUse()`
3. If the shortcut is already in use by another action (excluding the current one), the change is rejected
4. The UI shows a warning and reverts to the previous value
5. If successful, the change is persisted to disk immediately via `QSettings::sync()`

### Real-time Updates

When a shortcut is changed:

1. `ShortcutSettings` emits the `shortcutChanged` signal
2. `GUIFrontend::onShortcutChanged()` receives the signal
3. The corresponding `QShortcut` object's key sequence is updated via `setKey()`
4. The shortcut becomes active immediately without requiring an application restart

## Testing

To test the implementation:

1. **Load Test:** Start the application and verify default shortcuts work (Ctrl+Q, i)
2. **Change Test:** Go to Shortcuts tab, change a shortcut, verify it works immediately
3. **Persistence Test:** Change a shortcut, restart the application, verify the change persists
4. **Duplicate Test:** Try to assign the same shortcut to two actions, verify it's rejected
5. **Reset Test:** Change a shortcut, click Reset, verify it returns to the default

## Future Enhancements

Possible future improvements:

1. Add more shortcuts (e.g., for navigating tabs, placing orders, etc.)
2. Support for shortcut sequences (e.g., Ctrl+K, Ctrl+S)
3. Import/export shortcut configurations
4. Shortcut conflict resolution wizard
5. Search/filter functionality for large numbers of shortcuts
