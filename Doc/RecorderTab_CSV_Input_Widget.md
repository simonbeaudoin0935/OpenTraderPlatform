# RecorderTab CSV Input Widget Feature

## Overview
Added a CSV file input widget to the Recorder tab in the L2Trader GUI application, allowing users to specify the stock CSV file through the user interface rather than only via command-line arguments.

## Changes Made

### User Interface Changes
The Recorder tab now includes a new section in the "Recording Status" group box with:
- **Label**: "Stock CSV File:" 
- **QLineEdit**: A text input field showing the currently selected CSV file path
  - Placeholder text: "Select a CSV file containing stock symbols..."
  - User can manually edit the path or use the Browse button
- **Browse Button**: Opens a file dialog to select a CSV file
  - Filter: "CSV Files (*.csv);;All Files (*)"
  - Button width: 100 pixels

### Implementation Details

#### Header File Changes (`RecorderTab.h`)
1. Added `#include <QLineEdit>` for the input widget
2. Added new private slot: `void onBrowseButtonClicked()`
3. Added new member variables:
   - `QPushButton* m_browseButton` - Browse button widget
   - `QLineEdit* m_stockCsvFileInput` - CSV file path input field
   - `QString m_stockCsvFilePath` - Stores the selected CSV file path

#### Implementation File Changes (`RecorderTab.cpp`)
1. Added `#include <QFileDialog>` for file selection dialog
2. Initialized new member variables in constructor
3. Added backward compatibility: If global `stockCsvFile` is set, it's used as the default value
4. Updated `setupUI()`:
   - Created CSV file input section with label, QLineEdit, and Browse button
   - Added to status layout before control buttons
   - Connected browse button to `onBrowseButtonClicked()` slot
   - Connected QLineEdit text changes to update `m_stockCsvFilePath`
5. Implemented `onBrowseButtonClicked()`:
   - Opens QFileDialog with CSV file filter
   - Updates both `m_stockCsvFilePath` and the QLineEdit display
6. Updated `onStartRecording()`:
   - Changed from using global `stockCsvFile` to instance member `m_stockCsvFilePath`
   - Updated error messages to reflect the new input method

### Behavior

#### Initialization
- On startup, if the global `stockCsvFile` variable is set (from command-line arguments), it will be used as the default value and displayed in the input field
- Otherwise, the input field starts empty with placeholder text

#### User Workflow
1. User clicks the "Browse..." button
2. A file dialog opens, filtered to show CSV files
3. User selects a CSV file
4. The file path is displayed in the QLineEdit
5. When "Start Recording" is clicked, the selected CSV file is loaded

#### Error Handling
- If no CSV file is specified when starting recording, a warning message is shown: "Stock CSV file not specified. Please select a CSV file using the Browse button."
- If the specified file cannot be opened, an error message shows the file path and reason

### Backward Compatibility
The implementation maintains backward compatibility with the command-line `--stock-csv` argument. If the argument is provided, it will be used as the default value in the GUI input field.

## Visual Layout

```
┌─────────────────────────────────────────────────────────────┐
│ Recording Status                                            │
├─────────────────────────────────────────────────────────────┤
│ Status: Not Recording                                       │
│ Uptime: --:--:--                                           │
│ Bars Records: 0                                            │
│ Market Depth Records: 0                                    │
│ Memory Usage: N/A                                          │
│                                                            │
│ Stock CSV File: [Input Field...........................] [Browse...] │
│                                                            │
│ [Start Recording] [Stop Recording]    [Refresh Stats]     │
└─────────────────────────────────────────────────────────────┘
```

## Testing Notes
- The RecorderTab is a GUI component and does not have dedicated automated tests
- Manual testing should verify:
  1. Browse button opens file dialog
  2. Selected file path appears in the input field
  3. Manual path entry works correctly
  4. Recording starts successfully with the specified file
  5. Error messages display correctly for missing/invalid files
  6. Backward compatibility with command-line argument works

## Related Files
- `Src/FrontEnd/GUI/Tabs/RecorderTab.h`
- `Src/FrontEnd/GUI/Tabs/RecorderTab.cpp`
- `Src/Misc/Settings.h` (global `stockCsvFile` variable)
