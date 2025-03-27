#ifdef GUI_ENABLED
#include <QApplication>
#include "../GUI/guifrontend.h"
#define FRONTEND GuiFrontend
#define APPLICATION QApplication
#else
#include <QCoreApplication>
#include "../Core/terminalfrontend.h"
#define FRONTEND TerminalFrontend
#define APPLICATION QCoreApplication
#endif

#include "argumentparser.h"
#include "../Core/mainapp.h"


int main(int argc, char *argv[])
{
    APPLICATION app(argc, argv);

    QCoreApplication::setApplicationName("TradeStation Algo");
    QCoreApplication::setApplicationVersion("1.0");

    parseArguments(app.arguments());

    MainApp mainApp(new FRONTEND());

    return app.exec();
}
