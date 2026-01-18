#include "AuthWindow.h"
#include "AuthHandlerGUI.h"

#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QTimer>

Q_LOGGING_CATEGORY(TSAuthWindowLog, "TSClient.authwindow")

AuthWindow::AuthWindow(QWidget *parent) : QDialog(parent)
{
    qCDebug(TSAuthWindowLog) << "Initializing TradeStation Auth Window";

    // Set dialog properties
    setWindowTitle("TradeStation Authentication");
    setModal(true);
    setMinimumSize(500, 200);
    setAttribute(Qt::WA_DeleteOnClose);
    
    // Connect dialog finished signal
    connect(this, &QDialog::finished, this, &AuthWindow::handleDialogFinished);
    
    // Create GUI auth handler
    m_authHandler = new AuthHandlerGUI(this);
    connect(m_authHandler, &AuthHandler::authFinished, this, &AuthWindow::onAuthHandlerFinished);
    
    // Setup UI
    setupUi();
}

AuthWindow::~AuthWindow() = default;

void AuthWindow::startAuthenticationDialog()
{
    // Start the authentication process
    m_authHandler->startAuthentication();
    
    // Show the dialog
    show();
}

void AuthWindow::handleDialogFinished(int result)
{
    Q_UNUSED(result);
    // Dialog finished signal is handled, but the actual auth result
    // comes through onAuthHandlerFinished
}

void AuthWindow::onAuthHandlerFinished(bool success, AuthToken token, QString reason)
{
    // Forward the auth result
    emit authFinished(success, token, reason);
    
    // Close the dialog
    if (success) {
        accept();
    } else {
        reject();
    }
}

void AuthWindow::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    
    // Create status label with instructions
    auto *statusLabel = new QLabel(this);
    statusLabel->setWordWrap(true);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setText(
        "<h2>TradeStation Authentication</h2>"
        "<p>Your default web browser should open automatically with the TradeStation login page.</p>"
        "<p><b>Please complete the authentication in your browser.</b></p>"
        "<p>This window will close automatically once authentication is complete.</p>"
        "<p><i>Note: Keep this window open while you complete the authentication.</i></p>"
    );
    statusLabel->setMargin(20);
    layout->addWidget(statusLabel);
    
    // Add dialog buttons
    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Cancel,
        Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    setLayout(layout);
}
