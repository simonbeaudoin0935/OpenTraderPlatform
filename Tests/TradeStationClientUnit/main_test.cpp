#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDebug>
#include "AuthWindow.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr) : QMainWindow(parent)
    {
        setWindowTitle("TradeStation Application");
        resize(400, 300);

        // Create central widget and layout
        QWidget *centralWidget = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(centralWidget);
        setCentralWidget(centralWidget);

        // Create login button
        loginButton = new QPushButton("Login to TradeStation", this);
        connect(loginButton, &QPushButton::clicked, this, &MainWindow::handleLogin);
        layout->addWidget(loginButton);

        // Add some spacing
        layout->addStretch();
    }

private slots:
    void handleLogin()
    {
        // Create dialog with this as parent - Qt will handle memory management
        AuthWindow *authDialog = new AuthWindow(this);
        connect(authDialog, &AuthWindow::authenticationCompleted, this, &MainWindow::handleAuthSuccess);
        connect(authDialog, &AuthWindow::authenticationFailed, this, &MainWindow::handleAuthFailure);
        authDialog->exec(); // Show dialog modally
    }

    void handleAuthSuccess()
    {
        // Handle successful authentication
        qDebug() << "********************Authentication successful!";
        if (loginButton) {
            loginButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; }");
            loginButton->setText("Authentication Successful!");
        }
    }

    void handleAuthFailure(const QString &error)
    {
        // Handle authentication failure
        qDebug() << "********************Authentication failed:" << error;
        if (loginButton) {
            loginButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; }");
            loginButton->setText("Login Failed: " + error);
        }
    }

private:
    QPushButton *loginButton;  // Store the login button pointer
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
