#include "TUIAuthHandler.h"

#include <QTextStream>
#include <QCoreApplication>
#include <iostream>

#ifdef Q_OS_UNIX
#include <termios.h>
#include <unistd.h>
#endif

TUIAuthHandler::TUIAuthHandler(QObject* parent) : AuthHandler(parent)
{
    qCDebug(TSAuthHandlerLog) << "Initializing TUIAuthHandler for TUI mode";
}

bool TUIAuthHandler::promptForCredentials(QString& clientId, QString& clientSecret)
{
    std::cout << "\n=== TradeStation API Credentials Setup ===\n" << std::endl;
    std::cout << "Please enter your TradeStation API credentials." << std::endl;
    std::cout << "If you don't have them, you can obtain them from the TradeStation developer portal." << std::endl;
    std::cout << std::endl;

    // Prompt for Client ID
    std::cout << "Enter your Client ID: " << std::flush;
    clientId = readLineFromStdin(false);

    if (clientId.trimmed().isEmpty())
    {
        std::cerr << "Error: Client ID cannot be empty." << std::endl;
        return false;
    }

    // Prompt for Client Secret
    std::cout << "Enter your Client Secret (input will be hidden): " << std::flush;
    clientSecret = readLineFromStdin(true);
    std::cout << std::endl; // Add newline after hidden input

    if (clientSecret.trimmed().isEmpty())
    {
        std::cerr << "Error: Client Secret cannot be empty." << std::endl;
        return false;
    }

    // Ask if user wants to save credentials
    std::cout << "\nWould you like to save these credentials for future use? (y/N): " << std::flush;
    QString saveChoice = readLineFromStdin(false);

    if (saveChoice.trimmed().toLower() == "y" || saveChoice.trimmed().toLower() == "yes")
    {
        std::cout << "Credentials will be saved." << std::endl;
        // Note: Actual saving is handled by the base class
    }
    else
    {
        std::cout << "Credentials will not be saved. You'll need to enter them again next time." << std::endl;
        ClientToken::clearSettings();
    }

    return true;
}

void TUIAuthHandler::showAuthUrl(const QString& authUrl)
{
    std::cout << "\n=== TradeStation Authentication Required ===\n" << std::endl;
    std::cout << "Please complete the authentication in your web browser." << std::endl;
    std::cout << "Copy and paste the following URL into your browser:\n" << std::endl;
    std::cout << authUrl.toStdString() << std::endl;
    std::cout << "\nWaiting for authentication callback..." << std::endl;
    std::cout << "(Keep this application running until authentication is complete)\n" << std::endl;
}

void TUIAuthHandler::showError(const QString& title, const QString& message)
{
    std::cerr << "\n*** ERROR: " << title.toStdString() << " ***" << std::endl;
    std::cerr << message.toStdString() << std::endl;
    std::cerr << std::endl;
}

void TUIAuthHandler::showServerError(const QString& errorMsg)
{
    std::cerr << "\n*** SERVER ERROR ***" << std::endl;
    std::cerr << errorMsg.toStdString() << std::endl;
    std::cerr << std::endl;
}

QString TUIAuthHandler::readLineFromStdin(bool hideInput)
{
#ifdef Q_OS_UNIX
    if (hideInput)
    {
        // Disable echo for password input on Unix-like systems
        struct termios oldSettings, newSettings;
        tcgetattr(STDIN_FILENO, &oldSettings);
        newSettings = oldSettings;
        newSettings.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);

        // Read input
        QTextStream stream(stdin);
        QString input = stream.readLine();

        // Restore echo
        tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);

        return input;
    }
#else
    Q_UNUSED(hideInput)
    // On non-Unix systems, we can't hide input easily
    // Just show a warning and read normally
    if (hideInput)
    {
        std::cout << "(Warning: input will be visible) ";
    }
#endif

    // Normal input reading
    QTextStream stream(stdin);
    return stream.readLine();
}
