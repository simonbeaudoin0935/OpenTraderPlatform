#pragma once

#include "AuthHandler.h"
#include <iostream>

// TUI authentication handler for console mode
// Uses stdin/stdout for user interaction instead of GUI dialogs
class TUIAuthHandler : public AuthHandler
{
    Q_OBJECT

  public:
    explicit TUIAuthHandler(QObject* parent = nullptr);
    ~TUIAuthHandler() override = default;

  protected:
    // Override virtual methods for console-based interaction
    bool promptForCredentials(QString& clientId, QString& clientSecret) override;
    void showAuthUrl(const QString& authUrl) override;
    void showError(const QString& title, const QString& message) override;
    void showServerError(const QString& errorMsg) override;

  private:
    // Helper method to read input from stdin with optional masking
    QString readLineFromStdin(bool hideInput = false);
};
