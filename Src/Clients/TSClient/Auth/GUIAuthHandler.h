#pragma once

#include <QDialog>
#include <QLoggingCategory>
#include <QTimer>

#include "AuthHandler.h"

Q_DECLARE_LOGGING_CATEGORY(TSGUIAuthHandlerLog)

// GUI-based authentication handler using Qt dialogs
class GUIAuthHandler : public QDialog
{
    Q_OBJECT

  public:
    explicit GUIAuthHandler(QWidget* parent = nullptr);
    ~GUIAuthHandler() override;

  signals:
    void authFinished(bool success, AuthToken token, QString reason);

  private slots:
    void handleDialogFinished(int result);
    void handleAuthHandlerFinished(bool success, AuthToken token, QString reason);

  private:
    // GUI-specific authentication handler implementation
    class Impl : public AuthHandler
    {
      public:
        explicit Impl(GUIAuthHandler* window);

      protected:
        bool promptForCredentials(QString& clientId, QString& clientSecret) override;
        void showAuthUrl(const QString& authUrl) override;
        void showError(const QString& title, const QString& message) override;

      private:
        GUIAuthHandler* m_window;
    };

    Impl* m_authHandler = nullptr;
    bool m_authReported = false;

    // UI setup
    void setupUi();
};
