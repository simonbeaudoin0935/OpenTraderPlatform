#ifdef GUI_ENABLED
#include <QApplication>
#include <QIcon>
#include "GUI/GuiFrontend.h"
#define FRONTEND GuiFrontend
#define APPLICATION QApplication
#else
#include <QCoreApplication>
#include "Core/TerminalFrontend.h"
#define FRONTEND TerminalFrontend
#define APPLICATION QCoreApplication
#endif

#include "Misc/ArgumentParser.h"
#include "Core/MainApp.h"

int main(int argc, char *argv[])
{
    APPLICATION app(argc, argv);

    QCoreApplication::setApplicationName("L2Trader");
    QCoreApplication::setApplicationVersion("1.0");
    app.setWindowIcon(QIcon(":/Icons/L2T.png"));

    parseArguments(app.arguments());

    MainApp mainApp(new FRONTEND());

    mainApp.start();

    return app.exec();
}
