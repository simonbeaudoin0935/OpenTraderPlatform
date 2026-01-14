# Shortcuts Tab - UI Mockup

## Visual Layout

Since we cannot build and screenshot the actual UI in this environment, here's a detailed description of what the Shortcuts tab will look like when the application is run:

```
┌───────────────────────────────────────────────────────────────────┐
│  Trade  │  Logging  │  Cache  │  Recorder  │  Shortcuts  ◄────────┤  (New Tab)
└───────────────────────────────────────────────────────────────────┘

┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃ Keyboard Shortcuts                                                ┃
┃ ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ ┃
┃                                                                   ┃
┃   Quit Application:    [ Ctrl+Q        ▼ ]  [ Reset ]  Saved ✓  ┃
┃                                                                   ┃
┃   Focus Stock Input:   [ i             ▼ ]  [ Reset ]           ┃
┃                                                                   ┃
┃                                                                   ┃
┃   ℹ️ Press the desired key combination in the input field.       ┃
┃   Changes are saved immediately.                                 ┃
┃   Click 'Reset' to restore the default shortcut.                 ┃
┃                                                                   ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
```

## UI Components Breakdown

### Tab Bar
- The new "Shortcuts" tab appears after "Recorder" in the main tab widget
- Follows the same dark theme as other tabs

### Keyboard Shortcuts Group
A QGroupBox titled "Keyboard Shortcuts" containing:

#### For Each Shortcut (2 rows currently):
```
Label                    Input Field         Reset Button    Status
───────────────────────────────────────────────────────────────────
Quit Application:       [Ctrl+Q        ▼]    [ Reset ]      Saved ✓
Focus Stock Input:      [i             ▼]    [ Reset ]
```

1. **Label (left-aligned)**
   - Human-readable shortcut name (e.g., "Quit Application:")
   - Right-aligned within the form layout
   - White text on dark background

2. **Input Field (QKeySequenceEdit)**
   - Shows current key sequence
   - Click to activate, then press desired keys
   - Dropdown indicator shows it's editable
   - Dark input field with light text
   - Expands to fill available horizontal space

3. **Reset Button (QPushButton)**
   - Fixed width (~80px)
   - Restores shortcut to default value
   - Standard dark theme button styling
   - Shows hover effect

4. **Status Label**
   - Shows temporary feedback:
     - "Saved" in green (after successful change)
     - "Conflict" in red (if duplicate detected)
     - "Reset" in blue (after reset)
   - Small font (9px)
   - Auto-clears after 2-3 seconds

### Information Label (Bottom)
- Light gray text
- Smaller font (10px)
- Provides usage instructions
- Word-wrapped for readability

## User Interactions

### Changing a Shortcut
1. **Click** in the input field (e.g., next to "Quit Application")
2. The field becomes active (focus rectangle appears)
3. **Press** desired key combination (e.g., "Alt+Q")
4. **Click outside** the field or press Tab
5. One of two things happens:
   - ✅ **Success:** "Saved" appears in green, shortcut works immediately
   - ❌ **Conflict:** Warning dialog appears, field reverts, "Conflict" shows in red

### Resetting a Shortcut
1. **Click** the "Reset" button next to any shortcut
2. One of two things happens:
   - ✅ **Success:** Field shows default value, "Reset" appears in blue
   - ❌ **Conflict:** Warning dialog explains which shortcut is blocking

### Warning Dialogs

**Duplicate Shortcut Dialog:**
```
┌────────────────────────────────┐
│ ⚠️  Shortcut Conflict          │
├────────────────────────────────┤
│ The shortcut 'Ctrl+Q' is       │
│ already in use by another      │
│ action. Please choose a        │
│ different shortcut.            │
├────────────────────────────────┤
│                      [ OK ]    │
└────────────────────────────────┘
```

**Cannot Reset Dialog:**
```
┌────────────────────────────────┐
│ ⚠️  Cannot Reset               │
├────────────────────────────────┤
│ Cannot reset to default        │
│ shortcut 'Ctrl+Q' because it   │
│ is already in use by another   │
│ action. Please change the      │
│ conflicting shortcut first.    │
├────────────────────────────────┤
│                      [ OK ]    │
└────────────────────────────────┘
```

## Color Scheme (Dark Theme)

- **Background:** Dark gray (#333333)
- **Text:** White (#FFFFFF)
- **Input fields:** Darker gray (#222222)
- **Borders:** Medium gray (#555555)
- **Success status:** Green (#4CAF50)
- **Error status:** Red (#f44336)
- **Info status:** Blue (#2196F3)
- **Info text:** Light gray (#AAAAAA)
- **Buttons:** Medium gray (#444444) with hover effect

## Behavior Notes

### Real-time Updates
- Changes take effect **immediately** - no "Apply" button needed
- If you change "Quit Application" from Ctrl+Q to Alt+Q:
  - The old shortcut (Ctrl+Q) stops working
  - The new shortcut (Alt+Q) starts working
  - No application restart required

### Persistence
- Every change is written to `~/.config/L2Trader/AppState.ini` immediately
- Next time you start the app, your customizations are loaded automatically

### Validation
- The system prevents you from assigning the same shortcut to multiple actions
- Empty shortcuts are allowed (disables that action's shortcut)
- Invalid key combinations are rejected

## Future Shortcuts (Examples)

When more shortcuts are added later, they would appear as additional rows:

```
Quit Application:     [ Ctrl+Q  ▼ ]  [ Reset ]
Focus Stock Input:    [ i       ▼ ]  [ Reset ]
Place Buy Order:      [ Ctrl+B  ▼ ]  [ Reset ]
Place Sell Order:     [ Ctrl+S  ▼ ]  [ Reset ]
Next Tab:             [ Ctrl+Tab▼ ]  [ Reset ]
Previous Tab:         [ Ctrl+Shift+Tab▼ ]  [ Reset ]
...
```

The tab would become scrollable if there are many shortcuts.

## Accessibility

- All controls are keyboard-navigable (Tab to move between fields)
- Labels are properly associated with input fields
- Status messages provide non-visual feedback
- High contrast colors for readability

## Comparison to Other Tabs

This new tab follows the same pattern as existing tabs:

- **LoggingTab:** Controls for logging configuration
- **CacheTab:** Information about cache usage
- **RecorderTab:** Recorder-related controls
- **ShortcutsTab:** Keyboard shortcut configuration ← NEW

All use:
- QGroupBox for sections
- Dark theme styling
- Same padding and spacing
- Consistent typography
