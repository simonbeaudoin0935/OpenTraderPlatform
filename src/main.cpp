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

int main(int argc, char *argv[])
{
    qDebug() << "Qt version:" << QT_VERSION_STR;

    APPLICATION app(argc, argv);

    QCoreApplication::setApplicationName("L2Trader");
    QCoreApplication::setApplicationVersion("1.0");
    app.setWindowIcon(QIcon(":/Icons/L2T.png"));

    parseArguments(app.arguments());

    MainApp mainApp;

    mainApp.start();

    return app.exec();
}
