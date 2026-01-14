# OAuth Browser Migration: From QWebEngineView to System Browser

## Overview

This document describes the migration from using Qt's embedded QWebEngineView browser to using the system's default web browser for the OAuth authentication flow.

## Motivation

The original OAuth implementation used `QWebEngineView` from Qt's `WebEngineWidgets` module to display the TradeStation login page within the application. This approach had several drawbacks:

1. **Heavy Dependency**: QtWebEngineWidgets is a large module that significantly increases application size and complexity
2. **Build Complexity**: Requires additional Qt modules and system dependencies (Chromium-based engine)
3. **TUI Incompatibility**: The embedded browser is a GUI-only feature, preventing OAuth from working in TUI (Terminal UI) mode
4. **Maintenance Overhead**: Embedded browsers require security updates and can introduce additional bugs

## Solution

The new implementation uses `QDesktopServices::openUrl()` to launch the system's default web browser for OAuth authentication. This approach provides:

1. **Lighter Dependencies**: Only requires Qt Core and Widgets (no WebEngine)
2. **TUI Compatibility**: Works in both GUI and TUI modes
3. **Better UX**: Users authenticate in their familiar browser environment
4. **Simpler Maintenance**: No embedded browser to maintain or update

## Changes Made

### Code Changes

#### AuthWindow.h
- Removed: `#include <QWebEngineView>`
- Removed: `QWebEngineView *webView = nullptr;` member variable

#### AuthWindow.cpp
- Added: `#include <QDesktopServices>` and `#include <QLabel>`
- Modified `setupUi()`: Replaced embedded browser with informational label
- Modified `startAuthorization()`: Changed from `webView->load()` to `QDesktopServices::openUrl()`
- Modified `handleCodeReceived()`: Removed `webView->hide()` call
- Updated dialog size from fixed 800x800 to minimum 500x200

### Build System Changes

#### CMakeLists.txt (root)
- Removed: `WebEngineWidgets` from `find_package(Qt6 REQUIRED COMPONENTS ...)`

#### Src/CMakeLists.txt
- Removed: `Qt6::WebEngineWidgets` from GUI link libraries
- Removed: `list(FILTER CLIENTS_SOURCES EXCLUDE REGEX "AuthWindow\\.(cpp|h)$")` 
  - AuthWindow is now compatible with TUI mode

### Documentation Changes

#### OAuth_Authentication_Process.md
- Updated sequence diagram: Changed "Embedded Browser" to "System Web Browser"
- Updated AuthWindow description: Removed QWebEngineView references
- Added note about GUI and TUI compatibility

#### README.md
- Updated dependencies: Removed `WebEngineWidgets` requirement
- Updated installation instructions: Removed `qt6-webengine-dev` package
- Updated OAuth description: Changed from "embedded web browser" to "system's default web browser"
- Clarified token storage mechanism (QKeychain instead of "Qt WebEngine")

### Packaging Changes

#### debian/control
- Removed: `qt6-webengine-dev` build dependency

#### debian/README.md
- Updated build prerequisites: Removed `qt6-webengine-dev`
- Updated Qt6 module list: Removed webengine

## User Experience Changes

### Before
1. User launches application
2. OAuth dialog appears with embedded browser (800x800 window)
3. User logs in within the embedded browser
4. Authentication completes within the application

### After
1. User launches application
2. OAuth dialog appears with instructions (500x200 minimum window)
3. System browser automatically opens to TradeStation login
4. User logs in their familiar browser environment
5. Browser redirects to localhost, authentication completes
6. User can close the browser tab after seeing success message

## Technical Details

### QDesktopServices::openUrl()

The `QDesktopServices::openUrl()` function is a Qt utility that opens a URL using the appropriate application for the given URL scheme. For HTTP/HTTPS URLs, it uses the system's default web browser.

**Platform Behavior:**
- **Linux**: Uses `xdg-open` to determine the default browser
- **macOS**: Uses the default browser set in System Preferences
- **Windows**: Uses the default browser set in Windows settings

**Error Handling:**
If `QDesktopServices::openUrl()` fails (returns false), the application displays a warning dialog with the full URL so users can manually copy and open it.

### Local HTTP Server

The OAuth flow still uses a local HTTP server (unchanged) to receive the authorization callback:
- Server listens on localhost:8080-8089 (automatic port selection)
- Receives the authorization code from TradeStation's redirect
- Validates the CSRF state parameter
- Exchanges code for tokens
- Returns success/error page to browser

### Security

The security model remains unchanged:
- CSRF protection via random state parameter
- Secure token storage via QKeychain
- Local HTTP server only accepts connections from localhost
- All OAuth traffic uses HTTPS (except localhost callback)

## Benefits Summary

| Aspect | Before (QWebEngineView) | After (System Browser) |
|--------|------------------------|------------------------|
| **App Size** | +50-100MB (Chromium engine) | No additional size |
| **Build Dependencies** | Qt WebEngine + system libs | None additional |
| **TUI Support** | ❌ Not available | ✅ Fully supported |
| **User Experience** | Embedded, unfamiliar UI | Native browser, familiar |
| **Maintenance** | Security updates required | System managed |
| **Memory Usage** | +100-200MB runtime | Minimal |

## Backward Compatibility

This change is **fully backward compatible** from a user perspective:
- Stored credentials and tokens work exactly the same
- OAuth flow produces identical results
- No configuration changes required
- Users may notice their browser opens instead of embedded view

## Testing Considerations

When testing the OAuth flow:
1. Ensure system has a default browser configured
2. Test on all supported platforms (Linux, macOS, Windows)
3. Verify localhost redirect works correctly
4. Test with browser already open and closed states
5. Test cancellation (closing browser before completing auth)
6. Verify error handling when browser fails to open

## Migration Notes for Developers

If you have a local development environment with the old code:

1. **Clean rebuild required**: The dependency changes require a full rebuild
   ```bash
   rm -rf build/
   mkdir build
   cd build
   cmake -S .. -B . -DENABLE_GUI=ON
   cmake --build . --parallel
   ```

2. **Update dependencies**: You can now remove Qt WebEngine packages if only used for this project
   ```bash
   # These packages are no longer needed for L2Trader
   # sudo apt remove qt6-webengine-dev  # (if no other apps need it)
   ```

3. **No code changes required**: If you have custom branches, they should merge cleanly as the changes are localized to AuthWindow

## Future Enhancements

Possible future improvements building on this change:

1. **TUI OAuth Support**: Implement text-based OAuth instructions for pure terminal usage
2. **Browser Selection**: Allow users to specify a specific browser to use
3. **QR Code Option**: Generate QR code for mobile browser authentication
4. **Headless Support**: Support for headless environments (server deployments)

## References

- Qt Documentation: [QDesktopServices::openUrl()](https://doc.qt.io/qt-6/qdesktopservices.html#openUrl)
- TradeStation OAuth: [Authentication Overview](https://api.tradestation.com/docs/fundamentals/authentication/auth-overview)
- Original Issue: [GitHub Issue #XX - OAuth process without QWebView embedded browser]
