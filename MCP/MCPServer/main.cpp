#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("l2trader-mcp-server");
    app.setApplicationVersion("0.1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("OpenClaw-facing MCP bridge for L2Trader.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    QTextStream err(stderr);
    err << "l2trader-mcp-server scaffold: MCP stdio transport and the platform bridge\n"
           "have not been implemented yet.\n";
    err.flush();
    return 1;
}
