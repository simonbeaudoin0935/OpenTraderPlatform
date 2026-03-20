#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("l2trader-ctl");
    app.setApplicationVersion("0.1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("User-space CLI for the future L2Trader platform control socket.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("command", "Control command to send to the platform.");
    parser.process(app);

    QTextStream err(stderr);
    err << "l2trader-ctl scaffold: the platform control socket is not implemented yet.\n";
    err.flush();
    return 1;
}
