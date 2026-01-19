#include <QVBoxLayout>
#include <QDebug>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QDesktopServices>

#include "GUIAuthHandler.h"

Q_LOGGING_CATEGORY(TSGUIAuthHandlerLog, "TSClient.guiauthhandler")

GUIAuthHandler::GUIAuthHandler(QWidget* parent) : QDialog(parent)
{
    qCDebug(TSGUIAuthHandlerLog) << "Initializing TradeStation Auth Window";

    // Set dialog properties
    setWindowTitle("TradeStation Authentication");
    setModal(true);
    setMinimumSize(500, 200);
    setAttribute(Qt::WA_DeleteOnClose); // Ensure dialog is deleted when closed

    // Connect dialog finished signal
    connect(this, &QDialog::finished, this, &GUIAuthHandler::handleDialogFinished);

    // Setup UI
    setupUi();

    // Create and start authentication handler
    m_authHandler = new Impl(this);
    connect(m_authHandler, &AuthHandler::authFinished, this, &GUIAuthHandler::handleAuthHandlerFinished);

    // Start authentication process
    if (!m_authHandler->startAuthentication())
    {
        qCDebug(TSGUIAuthHandlerLog) << "Failed to start authentication process";
        QTimer::singleShot(0, this, &QDialog::reject);
    }
}

GUIAuthHandler::~GUIAuthHandler() = default;

void GUIAuthHandler::handleDialogFinished(int result)
{
    Q_UNUSED(result)
    // Cleanup is handled automatically by Qt's parent-child relationships
}

void GUIAuthHandler::handleAuthHandlerFinished(bool success, AuthToken token, QString reason)
{
    // Forward the signal and close the dialog
    emit authFinished(success, token, reason);

    if (success)
    {
        accept();
    }
    else
    {
        reject();
    }
}

void GUIAuthHandler::setupUi()
{
    auto* layout = new QVBoxLayout(this);

    // Create status label with instructions
    auto* statusLabel = new QLabel(this);
    statusLabel->setWordWrap(true);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setText("<h2>TradeStation Authentication</h2>"
                         "<p>Your default web browser should open automatically with the TradeStation login page.</p>"
                         "<p><b>Please complete the authentication in your browser.</b></p>"
                         "<p>This window will close automatically once authentication is complete.</p>"
                         "<p><i>Note: Keep this window open while you complete the authentication.</i></p>");
    statusLabel->setMargin(20);
    layout->addWidget(statusLabel);

    // Add dialog buttons
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    setLayout(layout);
}

// Impl implementation
GUIAuthHandler::Impl::Impl(GUIAuthHandler* window) : AuthHandler(window), m_window(window) {}

bool GUIAuthHandler::Impl::promptForCredentials(QString& clientId, QString& clientSecret)
{
    bool ok;
    QInputDialog dialog(m_window);
    dialog.setWindowTitle("TradeStation API Setup");
    dialog.setLabelText("Enter your Client ID:");
    dialog.setTextEchoMode(QLineEdit::Normal);
    dialog.setMinimumWidth(400);
    ok = dialog.exec();
    clientId = dialog.textValue();

    if (!ok || clientId.isEmpty())
    {
        qCDebug(TSGUIAuthHandlerLog) << "User cancelled Client ID input";
        return false;
    }

    dialog.setLabelText("Enter your Client Secret:");
    dialog.setTextEchoMode(QLineEdit::Password);
    dialog.setTextValue(""); // Clear previous input
    ok = dialog.exec();
    clientSecret = dialog.textValue();

    if (!ok || clientSecret.isEmpty())
    {
        qCDebug(TSGUIAuthHandlerLog) << "User cancelled Client Secret input";
        return false;
    }

    // Ask user if they want to save credentials
    QMessageBox::StandardButton saveChoice = QMessageBox::question(
        m_window,
        "Save Credentials",
        "Would you like to save these credentials for future use?\n\n"
        "If you choose not to save them, you'll need to enter them each time you start the application.",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No // Default to No for security
    );

    if (saveChoice != QMessageBox::Yes)
    {
        qCDebug(TSGUIAuthHandlerLog) << "User chose not to save credentials";
        ClientToken::clearSettings(); // Clear any existing credentials
    }

    return true;
}

void GUIAuthHandler::Impl::showAuthUrl(const QString& authUrl)
{
    qCDebug(TSGUIAuthHandlerLog) << "Opening URL in system browser...";

    // Open the URL in the system's default web browser
    if (!QDesktopServices::openUrl(QUrl(authUrl)))
    {
        qCWarning(TSGUIAuthHandlerLog) << "Failed to open system browser";
        QMessageBox::warning(m_window,
                             "Browser Error",
                             "Failed to open your default web browser. Please copy the URL manually:\n\n" + authUrl);
    }
    else
    {
        qCDebug(TSGUIAuthHandlerLog) << "Successfully opened URL in system browser";
    }
}

void GUIAuthHandler::Impl::showError(const QString& title, const QString& message)
{
    QMessageBox::warning(m_window, title, message);
}
