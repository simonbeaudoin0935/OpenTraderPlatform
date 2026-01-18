#pragma once

#include <QDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QLoggingCategory>

#include "AuthHandler.h"

Q_DECLARE_LOGGING_CATEGORY(TSAuthWindowLog)

/**
 * @brief GUI-based authentication window for TradeStation OAuth
 * 
 * Provides a Qt dialog-based interface for OAuth authentication.
 * Opens the system browser automatically and shows a dialog with status.
 */
class AuthWindow : public QDialog
{
    Q_OBJECT

public:
    explicit AuthWindow(QWidget *parent = nullptr);
    ~AuthWindow() override;

    /**
     * @brief Start the authentication process and show the dialog
     */
    void startAuthenticationDialog();

signals:
    void authFinished(bool success, AuthToken token, QString reason);

private slots:
    void handleDialogFinished(int result);
    void onAuthHandlerFinished(bool success, AuthToken token, QString reason);

private:
    AuthHandler* m_authHandler = nullptr;

    // UI setup
    void setupUi();
};
