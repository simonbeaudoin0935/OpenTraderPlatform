#include "AuthHandlerGUI.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QLineEdit>

Q_LOGGING_CATEGORY(TSAuthHandlerGUILog, "TSClient.authhandler.gui")

AuthHandlerGUI::AuthHandlerGUI(QWidget *parent)
    : AuthHandler(parent)
    , m_parentWidget(parent)
{
    qCDebug(TSAuthHandlerGUILog) << "Initializing GUI Auth Handler";
}

bool AuthHandlerGUI::promptForCredentials()
{
    bool ok;
    QInputDialog dialog(m_parentWidget);
    dialog.setWindowTitle("TradeStation API Setup");
    dialog.setLabelText("Enter your Client ID:");
    dialog.setTextEchoMode(QLineEdit::Normal);
    dialog.setMinimumWidth(400);
    ok = dialog.exec();
    QString newClientId = dialog.textValue();

    if (!ok || newClientId.isEmpty()) {
        qCDebug(TSAuthHandlerGUILog) << "User cancelled Client ID input";
        return false;
    }

    dialog.setLabelText("Enter your Client Secret:");
    dialog.setTextEchoMode(QLineEdit::Password);
    dialog.setTextValue("");
    ok = dialog.exec();
    QString newClientSecret = dialog.textValue();

    if (!ok || newClientSecret.isEmpty()) {
        qCDebug(TSAuthHandlerGUILog) << "User cancelled Client Secret input";
        return false;
    }

    // Create a new ClientToken with the provided credentials
    m_clientToken = ClientToken(newClientId, newClientSecret);
    
    if (m_clientToken.isValid()) {
        // Ask user if they want to save credentials
        QMessageBox::StandardButton saveChoice = QMessageBox::question(
            m_parentWidget,
            "Save Credentials",
            "Would you like to save these credentials for future use?\n\n"
            "If you choose not to save them, you'll need to enter them each time you start the application.",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No  // Default to No for security
        );

        if (saveChoice == QMessageBox::Yes) {
            if (ClientToken::storeToSettings(m_clientToken)) {
                qCDebug(TSAuthHandlerGUILog) << "Credentials saved successfully";
                return true;
            }
        } else {
            qCDebug(TSAuthHandlerGUILog) << "User chose not to save credentials";
            ClientToken::clearSettings();
            return true;
        }
    } else {
        QMessageBox::warning(m_parentWidget, "Invalid Credentials",
                           "The provided credentials appear to be invalid. "
                           "Please check your TradeStation API credentials and try again.");
    }

    return false;
}

void AuthHandlerGUI::displayAuthorizationUrl(const QString& authUrl)
{
    qCDebug(TSAuthHandlerGUILog) << "Opening URL in system browser...";

    // Open the URL in the system's default web browser
    if (!QDesktopServices::openUrl(QUrl(authUrl))) {
        qCWarning(TSAuthHandlerGUILog) << "Failed to open system browser";
        QMessageBox::warning(m_parentWidget, "Browser Error", 
            "Failed to open your default web browser. Please copy the URL manually:\n\n" + authUrl);
    } else {
        qCDebug(TSAuthHandlerGUILog) << "Successfully opened URL in system browser";
    }
}

void AuthHandlerGUI::showError(const QString& title, const QString& message)
{
    QMessageBox::warning(m_parentWidget, title, message);
}
