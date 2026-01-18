#include "AuthHandlerHeadless.h"

#include <QFile>
#include <iostream>

Q_LOGGING_CATEGORY(TSAuthHandlerHeadlessLog, "TSClient.authhandler.headless")

AuthHandlerHeadless::AuthHandlerHeadless(QObject *parent)
    : AuthHandler(parent)
    , m_stdin(stdin, QIODevice::ReadOnly)
    , m_stdout(stdout, QIODevice::WriteOnly)
    , m_stderr(stderr, QIODevice::WriteOnly)
{
    qCDebug(TSAuthHandlerHeadlessLog) << "Initializing Headless Auth Handler";
}

QString AuthHandlerHeadless::readLine(const QString& prompt, bool isPassword)
{
    m_stdout << prompt;
    m_stdout.flush();
    
    QString input;
    if (isPassword) {
        // For password input, we can't easily mask input in a cross-platform way
        // without additional dependencies, so we'll just warn the user
        m_stdout << "(Note: input will be visible)\n";
        m_stdout.flush();
    }
    
    input = m_stdin.readLine().trimmed();
    return input;
}

bool AuthHandlerHeadless::promptForCredentials()
{
    m_stdout << "\n=== TradeStation API Setup ===\n\n";
    m_stdout.flush();
    
    QString newClientId = readLine("Enter your Client ID: ", false);
    
    if (newClientId.isEmpty()) {
        qCDebug(TSAuthHandlerHeadlessLog) << "User provided empty Client ID";
        return false;
    }

    QString newClientSecret = readLine("Enter your Client Secret: ", true);

    if (newClientSecret.isEmpty()) {
        qCDebug(TSAuthHandlerHeadlessLog) << "User provided empty Client Secret";
        return false;
    }

    // Create a new ClientToken with the provided credentials
    m_clientToken = ClientToken(newClientId, newClientSecret);
    
    if (m_clientToken.isValid()) {
        // Ask user if they want to save credentials
        QString saveChoice = readLine("\nWould you like to save these credentials for future use? (yes/no) [no]: ", false);
        
        if (saveChoice.toLower() == "yes" || saveChoice.toLower() == "y") {
            if (ClientToken::storeToSettings(m_clientToken)) {
                qCDebug(TSAuthHandlerHeadlessLog) << "Credentials saved successfully";
                m_stdout << "Credentials saved successfully.\n";
                m_stdout.flush();
                return true;
            } else {
                m_stderr << "Warning: Failed to save credentials securely.\n";
                m_stderr.flush();
            }
        } else {
            qCDebug(TSAuthHandlerHeadlessLog) << "User chose not to save credentials";
            ClientToken::clearSettings();
            m_stdout << "Credentials will not be saved.\n";
            m_stdout.flush();
            return true;
        }
    } else {
        m_stderr << "Error: The provided credentials appear to be invalid.\n";
        m_stderr << "Please check your TradeStation API credentials and try again.\n";
        m_stderr.flush();
    }

    return false;
}

void AuthHandlerHeadless::displayAuthorizationUrl(const QString& authUrl)
{
    qCDebug(TSAuthHandlerHeadlessLog) << "Displaying authorization URL to user";
    
    m_stdout << "\n";
    m_stdout << "================================================================================\n";
    m_stdout << "                   TradeStation Authentication Required\n";
    m_stdout << "================================================================================\n";
    m_stdout << "\n";
    m_stdout << "To complete the authentication process, please follow these steps:\n";
    m_stdout << "\n";
    m_stdout << "1. Copy the URL below and paste it into your web browser\n";
    m_stdout << "2. Log in to your TradeStation account\n";
    m_stdout << "3. Authorize the application\n";
    m_stdout << "4. Wait for the authentication to complete automatically\n";
    m_stdout << "\n";
    m_stdout << "Authorization URL:\n";
    m_stdout << "--------------------------------------------------------------------------------\n";
    m_stdout << authUrl << "\n";
    m_stdout << "--------------------------------------------------------------------------------\n";
    m_stdout << "\n";
    m_stdout << "Waiting for authentication callback...\n";
    m_stdout << "(The application will continue once you complete the login in your browser)\n";
    m_stdout << "\n";
    m_stdout.flush();
}

void AuthHandlerHeadless::showError(const QString& title, const QString& message)
{
    m_stderr << "\n";
    m_stderr << "ERROR: " << title << "\n";
    m_stderr << message << "\n";
    m_stderr << "\n";
    m_stderr.flush();
}
