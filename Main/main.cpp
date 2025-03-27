#ifdef GUI_ENABLED
#include <QApplication>
#include "../GUI/guifrontend.h"
#else
#include <QCoreApplication>
#include "../Core/terminalfrontend.h"
#endif

#include "argumentparser.h"
#include "../Core/mainapp.h"


#ifdef GUI_ENABLED
#define FRONTEND GuiFrontend
#define APPLICATION QApplication
#else
#define FRONTEND TerminalFrontend
#define APPLICATION QCoreApplication
#endif


int main(int argc, char *argv[])
{
    APPLICATION app(argc, argv);

    QCoreApplication::setApplicationName("TradeStation Algo");
    QCoreApplication::setApplicationVersion("1.0");

    parseArguments(app.arguments());

    MainApp mainApp(new FRONTEND());

    return app.exec();
}
