#ifdef GUI_ENABLED
#include <QApplication>
#include <QIcon>
#define APPLICATION QApplication
#else
#include <QCoreApplication>
#define APPLICATION QCoreApplication
#endif

#include "Misc/ArgumentParser.h"
#include "Core/MainApp.h"

#include <QtGlobal>
#include <QDateTime>

#include <iostream>

// ANSI color codes
#define RESET_COLOR "\033[0m"
#define RED_COLOR "\033[31m"
#define GREEN_COLOR "\033[32m"
#define YELLOW_COLOR "\033[33m"
#define BLUE_COLOR "\033[34m"
#define MAGENTA_COLOR "\033[35m"
#define CYAN_COLOR "\033[36m"
#define WHITE_COLOR "\033[37m"
#define GRAY_COLOR "\033[90m"

void coloredMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString colorCode;
    QString typeText;

    switch (type) {
    case QtDebugMsg:
        colorCode = CYAN_COLOR;
        typeText = "DEBG";
        break;
    case QtInfoMsg:
        colorCode = GREEN_COLOR;
        typeText = "INFO";
        break;
    case QtWarningMsg:
        colorCode = YELLOW_COLOR;
        typeText = "WARN";
        break;
    case QtCriticalMsg:
        colorCode = RED_COLOR;
        typeText = "CRIT";
        break;
    case QtFatalMsg:
        colorCode = MAGENTA_COLOR;
        typeText = "FATAL";
        break;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString category = context.category ? QString(context.category) : "default";

    QString formattedMsg = QString("%1[%2] %3 %4:%5 %6%7")
                               .arg(colorCode)
                               .arg(timestamp)
                               .arg(typeText)
                               .arg(category)
                               .arg(RESET_COLOR)
                               .arg(msg)
                               .arg(RESET_COLOR);

    std::cout << formattedMsg.toStdString() << std::endl;
    std::cout.flush();
}

int main(int argc, char *argv[])
{
    // Install custom colored message handler instead of simple pattern
    qInstallMessageHandler(coloredMessageOutput);

    qInfo() << "Qt version:" << QT_VERSION_STR;

    APPLICATION app(argc, argv);

    QCoreApplication::setApplicationName("L2Trader");
    QCoreApplication::setApplicationVersion("1.0");
    app.setWindowIcon(QIcon(":/Icons/L2T.png"));

    parseArguments(app.arguments());

    MainApp mainApp;

    mainApp.start();

    return app.exec();
}
