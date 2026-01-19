#pragma once

#include <QDialog>
#include <QLoggingCategory>
#include <QTimer>

#include "AuthHandler.h"

Q_DECLARE_LOGGING_CATEGORY(TSAuthWindowLog)

// GUI-based authentication handler using Qt dialogs
class AuthWindow : public QDialog
{
    Q_OBJECT

  public:
    explicit AuthWindow(QWidget* parent = nullptr);
    ~AuthWindow() override;

  signals:
    void authFinished(bool success, AuthToken token, QString reason);

  private slots:
    void handleDialogFinished(int result);
    void handleAuthHandlerFinished(bool success, AuthToken token, QString reason);

  private:
    // GUI-specific authentication handler
    class GUIAuthHandler : public AuthHandler
    {
      public:
        explicit GUIAuthHandler(AuthWindow* window);

      protected:
        bool promptForCredentials(QString& clientId, QString& clientSecret) override;
        void showAuthUrl(const QString& authUrl) override;
        void showError(const QString& title, const QString& message) override;

      private:
        AuthWindow* m_window;
    };

    GUIAuthHandler* m_authHandler = nullptr;

    // UI setup
    void setupUi();
};
