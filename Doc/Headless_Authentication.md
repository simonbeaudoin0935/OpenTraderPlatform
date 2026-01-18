# Headless Authentication Implementation

## Overview

The L2Trader authentication system has been refactored to support both GUI and TUI (headless) modes. This allows the application to authenticate with TradeStation's OAuth service regardless of whether it's compiled with GUI support.

## Architecture

### Class Hierarchy

```
QObject
  └─ AuthHandler (base class)
      ├─ AuthHandlerGUI (GUI implementation)
      └─ AuthHandlerHeadless (TUI implementation)

QDialog
  └─ AuthWindow (GUI dialog wrapper)
```

### Core Components

#### AuthHandler (Base Class)
**Location**: `Src/Clients/TSClient/Auth/AuthHandler.{h,cpp}`

Base class that provides common OAuth 2.0 authentication logic:
- Local HTTP server for OAuth redirect URI handling
- CSRF state generation and validation  
- Token exchange with TradeStation
- Network request management

**Pure Virtual Methods** (must be implemented by subclasses):
- `promptForCredentials()` - Prompt user for Client ID and Secret
- `displayAuthorizationUrl(authUrl)` - Show authorization URL to user
- `showError(title, message)` - Display error messages

#### AuthHandlerGUI
**Location**: `Src/Clients/TSClient/Auth/AuthHandlerGUI.{h,cpp}`

GUI implementation using Qt dialogs:
- Uses `QInputDialog` for credential prompts
- Uses `QMessageBox` for errors and confirmations
- Opens authorization URL in system browser via `QDesktopServices`

#### AuthHandlerHeadless
**Location**: `Src/Clients/TSClient/Auth/AuthHandlerHeadless.{h,cpp}`

Headless implementation using command-line I/O:
- Uses `QTextStream` for stdin/stdout/stderr
- Prompts for credentials via console
- Displays authorization URL as text for manual copy/paste
- No GUI dependencies

#### AuthWindow
**Location**: `Src/Clients/TSClient/Auth/AuthWindow.{h,cpp}`

Dialog wrapper for GUI mode:
- Creates and manages `AuthHandlerGUI` instance
- Provides modal dialog UI with status messages
- Forwards authentication results to TSClient

## Compilation Modes

### GUI Mode (ENABLE_GUI=ON)
```cmake
cmake -DENABLE_GUI=ON ..
```

- Uses `AuthWindow` which internally uses `AuthHandlerGUI`
- Opens browser automatically
- Shows dialog with authentication status
- Full Qt Widgets support required

### TUI Mode (ENABLE_GUI=OFF)
```cmake
cmake -DENABLE_GUI=OFF ..
```

- Uses `AuthHandlerHeadless` directly
- Displays URL in terminal for manual copy/paste
- All I/O via stdin/stdout/stderr
- No Qt Widgets dependencies

## Authentication Flow

### Common Flow (Both Modes)

1. **Credential Loading**
   - Try to load Client ID/Secret from secure storage
   - If not found, prompt user for credentials
   - Optionally save credentials to secure storage

2. **HTTP Server Setup**
   - Start local HTTP server on port 8080-8089
   - Listen for OAuth callback at `http://localhost:PORT/callback`

3. **State Generation**
   - Generate random 32-character state for CSRF protection

4. **Authorization Request**
   - Construct TradeStation authorization URL
   - **GUI**: Open in system browser automatically
   - **TUI**: Display URL in terminal for manual copy/paste

5. **Callback Handling**
   - Receive authorization code via HTTP callback
   - Validate state parameter matches expected value
   - Return success/error HTML page to browser

6. **Token Exchange**
   - Exchange authorization code for access/refresh tokens
   - Validate token response
   - Store tokens securely

7. **Completion**
   - Emit `authFinished` signal with result
   - **GUI**: Close dialog automatically
   - **TUI**: Print status message

### GUI-Specific Flow

```
User → AuthWindow → AuthHandlerGUI → Browser (auto-opens)
                         ↓
                    HTTP Server (callback)
                         ↓
                    TSClient (authenticated)
```

### TUI-Specific Flow

```
User → TSClient → AuthHandlerHeadless → Terminal (displays URL)
                         ↓
                    User copies URL to browser manually
                         ↓
                    HTTP Server (callback)
                         ↓
                    TSClient (authenticated)
```

## TSClient Integration

### Conditional Compilation

**TSClient.h**:
```cpp
#ifdef GUI_ENABLED
#include "AuthWindow.h"
#else
#include "AuthHandler.h"
#endif

// ...

#ifdef GUI_ENABLED
    AuthWindow* m_authWindow = nullptr;
#else
    AuthHandler* m_authHandler = nullptr;
#endif
```

**TSClientRefreshToken.cpp**:
```cpp
#ifdef GUI_ENABLED
void TSClient::launchAuthProcess() {
    m_authWindow = new AuthWindow();
    connect(m_authWindow, &AuthWindow::authFinished, 
            this, &TSClient::onAuthFinished);
    m_authWindow->startAuthenticationDialog();
}
#else
void TSClient::launchAuthProcess() {
    m_authHandler = new AuthHandlerHeadless(this);
    connect(m_authHandler, &AuthHandler::authFinished,
            this, &TSClient::onAuthFinished);
    m_authHandler->startAuthentication();
}
#endif
```

## Usage Examples

### GUI Mode Authentication

When compiled with GUI support, authentication happens automatically:

1. Application starts
2. If no valid tokens, `AuthWindow` appears
3. Browser opens automatically with login page
4. User logs in to TradeStation
5. Dialog closes automatically on success

### TUI Mode Authentication

When compiled without GUI support:

1. Application starts
2. If no valid tokens, console prompts appear:

```
=== TradeStation API Setup ===

Enter your Client ID: YOUR_CLIENT_ID
Enter your Client Secret: (Note: input will be visible)
YOUR_CLIENT_SECRET

Would you like to save these credentials for future use? (yes/no) [no]: yes
Credentials saved successfully.

================================================================================
                   TradeStation Authentication Required
================================================================================

To complete the authentication process, please follow these steps:

1. Copy the URL below and paste it into your web browser
2. Log in to your TradeStation account
3. Authorize the application
4. Wait for the authentication to complete automatically

Authorization URL:
--------------------------------------------------------------------------------
https://signin.tradestation.com/authorize?response_type=code&client_id=...
--------------------------------------------------------------------------------

Waiting for authentication callback...
(The application will continue once you complete the login in your browser)
```

3. User copies URL and opens in browser
4. User completes authentication in browser
5. Application detects callback and continues automatically

## Security Considerations

### CSRF Protection
- Random 32-character state parameter generated for each auth session
- State validated on callback to prevent CSRF attacks

### Credential Storage
- Client ID/Secret stored via `SecureStorage` (QKeychain or encrypted fallback)
- Access/refresh tokens stored securely
- User can opt out of credential persistence

### Token Handling
- Access tokens expire after 20 minutes
- Automatic refresh before expiration
- Refresh tokens preserved across refreshes

### HTTP Server
- Binds only to localhost (127.0.0.1)
- Limited to callback endpoint
- Automatic port selection (8080-8089) if ports are busy
- Server closes after successful authentication

## Testing

### Manual Testing - GUI Mode

```bash
# Build with GUI
cmake -DENABLE_GUI=ON ..
make

# Run application
./src/L2Trader --criterias=../Example_Config/selection_criteria.ini

# Clear existing credentials to force re-auth
rm ~/.config/L2Trader/SecureStorage.ini

# Verify:
# 1. Dialog appears
# 2. Browser opens automatically
# 3. After login, dialog closes
# 4. Application continues normally
```

### Manual Testing - TUI Mode

```bash
# Build without GUI
cmake -DENABLE_GUI=OFF ..
make

# Run application
./src/L2Trader --criterias=../Example_Config/selection_criteria.ini

# Clear existing credentials to force re-auth
rm ~/.config/L2Trader/SecureStorage.ini

# Verify:
# 1. Console prompts appear
# 2. URL displayed in terminal
# 3. After copying URL and completing login in browser
# 4. Application detects callback and continues
```

## Migration Notes

### For Existing Users

No changes required. Existing credentials and tokens will continue to work.

### For Developers

If you have code that directly instantiates `AuthWindow`, update to use the new API:

**Old**:
```cpp
AuthWindow* window = new AuthWindow();
connect(window, &AuthWindow::authFinished, ...);
window->show();
```

**New**:
```cpp
AuthWindow* window = new AuthWindow();
connect(window, &AuthWindow::authFinished, ...);
window->startAuthenticationDialog();
```

## Future Enhancements

### Potential Improvements

1. **Terminal Password Masking**: Implement proper password masking for TUI credential input (currently shows warning that input is visible)

2. **QR Code Support**: Generate QR code in TUI mode for easier URL access on mobile devices

3. **Auto-Detection**: Detect if display/X server is available and automatically choose GUI vs TUI mode

4. **Web-based Auth**: Add option to run a local web server with authentication form instead of relying on system browser

5. **Token Refresh UI**: Add UI feedback when tokens are being refreshed in the background

## Related Documentation

- [OAuth_Authentication_Process.md](./OAuth_Authentication_Process.md) - Detailed OAuth flow documentation
- [OAuth_Browser_Migration.md](./OAuth_Browser_Migration.md) - Browser implementation notes

## Files Modified/Created

### New Files
- `Src/Clients/TSClient/Auth/AuthHandler.h`
- `Src/Clients/TSClient/Auth/AuthHandler.cpp`
- `Src/Clients/TSClient/Auth/AuthHandlerGUI.h`
- `Src/Clients/TSClient/Auth/AuthHandlerGUI.cpp`
- `Src/Clients/TSClient/Auth/AuthHandlerHeadless.h`
- `Src/Clients/TSClient/Auth/AuthHandlerHeadless.cpp`

### Modified Files
- `Src/Clients/TSClient/Auth/AuthWindow.h` - Simplified to use AuthHandlerGUI
- `Src/Clients/TSClient/Auth/AuthWindow.cpp` - Refactored to delegate to AuthHandlerGUI
- `Src/Clients/TSClient/TSClient.h` - Added conditional compilation for auth handlers
- `Src/Clients/TSClient/TSClientRefreshToken.cpp` - Support both GUI and TUI auth
