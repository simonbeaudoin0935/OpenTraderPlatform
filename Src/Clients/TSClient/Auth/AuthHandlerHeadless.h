#pragma once

#include "AuthHandler.h"
#include <QTextStream>

Q_DECLARE_LOGGING_CATEGORY(TSAuthHandlerHeadlessLog)

/**
 * @brief Headless authentication handler for TradeStation OAuth
 * 
 * Uses command-line prompts for user interaction and displays
 * authorization URL as text for manual copy/paste into browser.
 * Suitable for TUI (terminal UI) mode without GUI dependencies.
 */
class AuthHandlerHeadless : public AuthHandler
{
    Q_OBJECT

public:
    explicit AuthHandlerHeadless(QObject *parent = nullptr);
    ~AuthHandlerHeadless() override = default;

protected:
    bool promptForCredentials() override;
    void displayAuthorizationUrl(const QString& authUrl) override;
    void showError(const QString& title, const QString& message) override;

private:
    QTextStream m_stdin;
    QTextStream m_stdout;
    QTextStream m_stderr;
    
    QString readLine(const QString& prompt, bool isPassword = false);
};
