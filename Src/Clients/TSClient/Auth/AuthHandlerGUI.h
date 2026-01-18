#pragma once

#include "AuthHandler.h"
#include <QWidget>

Q_DECLARE_LOGGING_CATEGORY(TSAuthHandlerGUILog)

/**
 * @brief GUI-based authentication handler for TradeStation OAuth
 * 
 * Uses Qt dialogs for user interaction and opens authorization URL
 * in the system's default web browser.
 */
class AuthHandlerGUI : public AuthHandler
{
    Q_OBJECT

public:
    explicit AuthHandlerGUI(QWidget *parent = nullptr);
    ~AuthHandlerGUI() override = default;

protected:
    bool promptForCredentials() override;
    void displayAuthorizationUrl(const QString& authUrl) override;
    void showError(const QString& title, const QString& message) override;

private:
    QWidget* m_parentWidget;
};
