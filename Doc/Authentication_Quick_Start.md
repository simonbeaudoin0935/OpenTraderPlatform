# Quick Start: Headless Authentication

## Building

### GUI Mode (Default)
```bash
mkdir build && cd build
cmake .. -DENABLE_GUI=ON
make
```

### TUI Mode (Headless)
```bash
mkdir build && cd build
cmake .. -DENABLE_GUI=OFF
make
```

## First-Time Authentication

### GUI Mode
1. Run the application:
   ```bash
   ./src/L2Trader --criterias=../Example_Config/selection_criteria.ini
   ```
2. Authentication dialog appears automatically
3. Browser opens with TradeStation login
4. Complete login in browser
5. Dialog closes automatically
6. Application continues

### TUI Mode
1. Run the application:
   ```bash
   ./src/L2Trader --criterias=../Example_Config/selection_criteria.ini
   ```
2. Console prompts appear:
   ```
   === TradeStation API Setup ===
   
   Enter your Client ID: <paste your Client ID>
   Enter your Client Secret: (Warning: input will be visible on screen)
   <paste your Client Secret>
   
   Would you like to save these credentials for future use? (yes/no) [no]: yes
   ```
3. Authorization URL is displayed:
   ```
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
   ```
4. Copy the URL and paste into your browser
5. Complete login in browser
6. Application detects callback automatically
7. Application continues

## Troubleshooting

### "Failed to find available port"
The HTTP callback server couldn't bind to ports 8080-8089. Check:
- Are other applications using these ports?
- Do you have permission to bind to these ports?
- Try closing other applications and restarting

### "Unable to obtain valid TradeStation API credentials"
Your Client ID or Client Secret is invalid or empty. Check:
- Are you entering the correct credentials from TradeStation?
- Did you copy the entire ID/Secret without extra spaces?

### "State mismatch - possible security issue"
CSRF validation failed. This could indicate:
- A timing issue (try again)
- A security attack (verify your network is secure)
- Multiple authentication attempts running simultaneously (close other instances)

### Browser doesn't open (GUI mode)
The system couldn't open your default browser. The dialog should display an error with the URL to copy manually.

### Application hangs waiting for callback
The OAuth callback wasn't received. Check:
- Did you complete the login in the browser?
- Is the callback URL accessible (http://localhost:PORT/callback)?
- Are you behind a firewall or proxy blocking localhost?

## Credential Storage

### Location
- **Linux**: `~/.local/share/keyrings/` (GNOME Keyring) or `~/.config/L2Trader/SecureStorage.ini` (fallback)
- **macOS**: `~/Library/Keychains/login.keychain-db`
- **Windows**: Windows Credential Manager

### Clearing Saved Credentials
```bash
# Using QKeychain (recommended)
# Linux:
secret-tool clear service TradeStation

# Fallback storage:
rm ~/.config/L2Trader/SecureStorage.ini
rm ~/.config/L2Trader/settings.ini
```

## Security Notes

### TUI Mode Password Visibility
⚠️ **Warning**: In TUI mode, the Client Secret is visible when typed due to terminal limitations. For maximum security:
- Ensure no one is looking over your shoulder
- Clear terminal history after authentication
- Consider using GUI mode for initial setup
- Future versions may implement platform-specific password masking

### Credential Persistence
When prompted "Would you like to save these credentials?":
- **Yes**: Credentials stored in OS keychain (recommended for personal use)
- **No**: Credentials only in memory (recommended for shared systems)

### Token Refresh
- Access tokens expire after 20 minutes
- Automatic refresh happens 5 seconds before expiration
- Refresh tokens are preserved and reused
- No user interaction needed for token refresh

## Advanced Usage

### Force Re-authentication
To force a new authentication (useful for testing or account changes):
```bash
# Clear stored tokens
rm ~/.config/L2Trader/SecureStorage.ini
rm ~/.config/L2Trader/settings.ini

# Run application
./src/L2Trader --criterias=../Example_Config/selection_criteria.ini
```

### Custom Port Range
If you need to use different ports (requires code modification):
1. Edit `Src/Clients/TSClient/Auth/AuthHandler.h`
2. Change `DEFAULT_PORT` and `MAX_PORT_ATTEMPTS` constants
3. Rebuild application

### Debugging Authentication
Enable debug logging:
```bash
export QT_LOGGING_RULES="TSClient.authhandler*=true;TSClient.authwindow=true"
./src/L2Trader --criterias=../Example_Config/selection_criteria.ini
```

This will show detailed logs of:
- HTTP server startup
- State generation
- Token exchange
- Credential loading/saving

## For Developers

### Testing Authentication Flow
```cpp
// In your test code
#ifdef GUI_ENABLED
    AuthWindow* auth = new AuthWindow(nullptr);
    QObject::connect(auth, &AuthWindow::authFinished,
        [](bool success, AuthToken token, QString reason) {
            qDebug() << "Auth result:" << success << reason;
        });
    auth->startAuthenticationDialog();
#else
    AuthHandlerHeadless* auth = new AuthHandlerHeadless(nullptr);
    QObject::connect(auth, &AuthHandler::authFinished,
        [](bool success, AuthToken token, QString reason) {
            qDebug() << "Auth result:" << success << reason;
        });
    auth->startAuthentication();
#endif
```

### Implementing Custom Handler
To implement a custom authentication handler:
```cpp
class MyAuthHandler : public AuthHandler {
    Q_OBJECT
protected:
    bool promptForCredentials() override {
        // Your credential prompt implementation
    }
    
    void displayAuthorizationUrl(const QString& authUrl) override {
        // Your URL display implementation
    }
    
    void showError(const QString& title, const QString& message) override {
        // Your error display implementation
    }
};
```

## See Also

- [Headless_Authentication.md](./Headless_Authentication.md) - Full architecture documentation
- [OAuth_Authentication_Process.md](./OAuth_Authentication_Process.md) - OAuth flow details
- [README.md](../README.md) - General application documentation
