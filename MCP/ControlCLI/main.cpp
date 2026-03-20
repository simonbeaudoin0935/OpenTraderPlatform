#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include "Core/PlatformControlProtocol.h"
#include "PlatformControlClient.h"

namespace
{
    [[nodiscard]] bool requireSingleCommand(const QStringList& p_positionals)
    {
        return p_positionals.size() == 1;
    }
} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("l2trader-ctl");
    app.setApplicationVersion("0.1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("User-space CLI for the L2Trader platform control socket.");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption socketOption(QStringLiteral("socket"),
                                    QStringLiteral("Override the platform control socket path."),
                                    QStringLiteral("path"),
                                    PlatformControlProtocol::socketPath());
    QCommandLineOption dateOption(QStringLiteral("date"),
                                  QStringLiteral("Replay date in YYYY-MM-DD format."),
                                  QStringLiteral("date"));
    QCommandLineOption startTimeOption(QStringLiteral("start-time"),
                                       QStringLiteral("Replay start time in HH:MM[:SS] format."),
                                       QStringLiteral("time"));
    QCommandLineOption speedOption(QStringLiteral("speed"),
                                   QStringLiteral("Replay speed (for example 1x, 10x, max)."),
                                   QStringLiteral("speed"));
    QCommandLineOption modeOption(QStringLiteral("mode"),
                                  QStringLiteral("Trading mode (sim or live)."),
                                  QStringLiteral("mode"));
    QCommandLineOption compactOption(QStringLiteral("compact"),
                                     QStringLiteral("Print compact JSON instead of pretty JSON."));

    parser.addOption(socketOption);
    parser.addOption(dateOption);
    parser.addOption(startTimeOption);
    parser.addOption(speedOption);
    parser.addOption(modeOption);
    parser.addOption(compactOption);
    parser.addPositionalArgument(
        "command",
        "One of: status, enter-replay, start-replay, pause-replay, resume-replay, set-replay-speed, preload-replay, "
        "exit-replay, set-trading-mode");
    parser.process(app);

    QTextStream err(stderr);
    const QStringList positionalArguments = parser.positionalArguments();
    if (!requireSingleCommand(positionalArguments))
    {
        err << "Expected exactly one command. Run with --help for usage.\n";
        err.flush();
        return 2;
    }

    const QString command = positionalArguments.constFirst().trimmed();
    const QStringList supportedCommands = {
        PlatformControlProtocol::kCommandStatus,
        PlatformControlProtocol::kCommandEnterReplay,
        PlatformControlProtocol::kCommandStartReplay,
        PlatformControlProtocol::kCommandPauseReplay,
        PlatformControlProtocol::kCommandResumeReplay,
        PlatformControlProtocol::kCommandSetReplaySpeed,
        PlatformControlProtocol::kCommandPreloadReplay,
        PlatformControlProtocol::kCommandExitReplay,
        PlatformControlProtocol::kCommandSetTradingMode,
    };

    if (!supportedCommands.contains(command))
    {
        err << "Unsupported command '" << command << "'. Supported commands: " << supportedCommands.join(", ") << "\n";
        err.flush();
        return 2;
    }

    QJsonObject arguments;
    if (parser.isSet(dateOption))
    {
        arguments["date"] = parser.value(dateOption);
    }
    if (parser.isSet(startTimeOption))
    {
        arguments["startTime"] = parser.value(startTimeOption);
    }
    if (parser.isSet(speedOption))
    {
        arguments["speed"] = parser.value(speedOption);
    }
    if (parser.isSet(modeOption))
    {
        arguments["mode"] = parser.value(modeOption);
    }

    PlatformControlClient client({.socketPath = parser.value(socketOption)});
    const auto response = client.sendCommand(PlatformControlProtocol::makeRequest(command, arguments));
    if (!response.has_value())
    {
        err << response.error() << "\n";
        err.flush();
        return 3;
    }

    QTextStream out(stdout);
    const auto jsonFormat = parser.isSet(compactOption) ? QJsonDocument::Compact : QJsonDocument::Indented;
    out << QJsonDocument(response.value()).toJson(jsonFormat);
    out.flush();

    return response->value("ok").toBool(false) ? 0 : 1;
}
