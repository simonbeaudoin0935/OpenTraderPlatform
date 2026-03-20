#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QTextStream>

#include <iostream>

#include "Core/PlatformControlProtocol.h"
#include "PlatformControlClient.h"

namespace
{
    inline constexpr auto kMCPProtocolVersion = "2025-06-18";

    [[nodiscard]] QJsonObject makeJsonRpcResponse(const QJsonValue& p_id, const QJsonObject& p_result)
    {
        QJsonObject response;
        response["jsonrpc"] = "2.0";
        response["id"] = p_id;
        response["result"] = p_result;
        return response;
    }

    [[nodiscard]] QJsonObject makeJsonRpcError(const QJsonValue& p_id, int p_code, const QString& p_message)
    {
        QJsonObject response;
        response["jsonrpc"] = "2.0";
        response["id"] = p_id;

        QJsonObject error;
        error["code"] = p_code;
        error["message"] = p_message;
        response["error"] = error;
        return response;
    }

    [[nodiscard]] QJsonObject
    makeTextContentResult(const QString& p_text, const bool p_isError, const QJsonObject& p_structuredContent = {})
    {
        QJsonObject content;
        content["type"] = "text";
        content["text"] = p_text;

        QJsonArray contents;
        contents.append(content);

        QJsonObject result;
        result["content"] = contents;
        result["isError"] = p_isError;
        if (!p_structuredContent.isEmpty())
        {
            result["structuredContent"] = p_structuredContent;
        }
        return result;
    }

    [[nodiscard]] QByteArray compactJsonLine(const QJsonObject& p_payload)
    {
        return PlatformControlProtocol::serializeMessage(p_payload);
    }

    [[nodiscard]] QJsonObject emptyObjectSchema()
    {
        return QJsonObject{{"type", "object"}, {"properties", QJsonObject{}}};
    }

    [[nodiscard]] QJsonObject replayArgsSchema()
    {
        QJsonObject properties;
        properties["date"] = QJsonObject{{"type", "string"}, {"description", "Replay date in YYYY-MM-DD format."}};
        properties["startTime"] =
            QJsonObject{{"type", "string"}, {"description", "Replay start time in HH:MM[:SS] format."}};
        properties["speed"] = QJsonObject{
            {"type", "string"},
            {"enum", PlatformControlProtocol::supportedReplaySpeedsJson()},
            {"description", "Replay speed."},
        };

        return QJsonObject{
            {"type", "object"},
            {"properties", properties},
            {"required", QJsonArray{"date", "startTime", "speed"}},
        };
    }

    [[nodiscard]] QJsonObject replayStartArgsSchema()
    {
        QJsonObject properties;
        properties["date"] = QJsonObject{{"type", "string"}, {"description", "Replay date in YYYY-MM-DD format."}};
        properties["startTime"] =
            QJsonObject{{"type", "string"}, {"description", "Replay start time in HH:MM[:SS] format."}};
        properties["speed"] = QJsonObject{
            {"type", "string"},
            {"enum", PlatformControlProtocol::supportedReplaySpeedsJson()},
            {"description", "Replay speed. Omit to reuse the platform's configured replay speed."},
        };

        return QJsonObject{
            {"type", "object"},
            {"properties", properties},
        };
    }

    [[nodiscard]] QJsonObject replaySpeedSchema()
    {
        QJsonObject properties;
        properties["speed"] = QJsonObject{
            {"type", "string"},
            {"enum", PlatformControlProtocol::supportedReplaySpeedsJson()},
            {"description", "Replay speed."},
        };

        return QJsonObject{
            {"type", "object"},
            {"properties", properties},
            {"required", QJsonArray{"speed"}},
        };
    }

    [[nodiscard]] QJsonObject optionalSymbolSchema()
    {
        return QJsonObject{
            {"type", "string"},
            {"description",
             "Optional ticker symbol. If omitted, L2Trader uses the currently displayed symbol. Arbitrary symbols are auto-activated for polling."},
        };
    }

    [[nodiscard]] QJsonObject symbolPollingSchema()
    {
        return QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{{"symbol", optionalSymbolSchema()}}},
        };
    }

    [[nodiscard]] QJsonObject tradesSnapshotSchema()
    {
        return QJsonObject{
            {"type", "object"},
            {"properties",
             QJsonObject{
                 {"symbol", optionalSymbolSchema()},
                 {"maxCount",
                  QJsonObject{
                      {"type", "integer"},
                      {"minimum", 1},
                      {"maximum", PlatformControlConstants::MAX_TRADES_SNAPSHOT_MAX_COUNT},
                      {"description",
                       QString("Maximum number of recent trades to return. Defaults to %1.")
                           .arg(PlatformControlConstants::DEFAULT_TRADES_SNAPSHOT_MAX_COUNT)},
                  }},
             }},
        };
    }

    [[nodiscard]] QJsonObject barsSchema()
    {
        return QJsonObject{
            {"type", "object"},
            {"properties",
             QJsonObject{
                 {"symbol", optionalSymbolSchema()},
                 {"date", QJsonObject{{"type", "string"}, {"description", "Trading day in YYYY-MM-DD format."}}},
                 {"startTime", QJsonObject{{"type", "string"}, {"description", "Range start in HH:MM[:SS] format."}}},
                 {"endTime", QJsonObject{{"type", "string"}, {"description", "Range end in HH:MM[:SS] format."}}},
                 {"timeFrame",
                  QJsonObject{
                      {"type", "string"},
                      {"enum", PlatformControlProtocol::supportedBarTimeFramesJson()},
                      {"description", "Bar timeframe. Defaults to 1m."},
                  }},
             }},
            {"required", QJsonArray{"date", "startTime", "endTime"}},
        };
    }

    [[nodiscard]] QJsonArray buildToolList()
    {
        QJsonArray tools;
        tools.append(QJsonObject{
            {"name", PlatformControlProtocol::kCommandStatus},
            {"title", "Get platform status"},
            {"description", "Read current L2Trader platform status, replay state, and control socket details."},
            {"inputSchema", emptyObjectSchema()},
        });
        tools.append(QJsonObject{
            {"name", PlatformControlProtocol::kCommandGetDisplayedSymbol},
            {"title", "Get displayed symbol"},
            {"description", "Read the symbol currently displayed in the L2Trader frontend."},
            {"inputSchema", emptyObjectSchema()},
        });
        tools.append(QJsonObject{
            {"name", PlatformControlProtocol::kCommandGetLevel2},
            {"title", "Get level 2 snapshot"},
            {"description", "Poll the latest 10-level order-book snapshot for the displayed or requested symbol."},
            {"inputSchema", symbolPollingSchema()},
        });
        tools.append(QJsonObject{
            {"name", PlatformControlProtocol::kCommandGetTradesSnapshot},
            {"title", "Get recent trades"},
            {"description", "Poll the recent trade snapshot for the displayed or requested symbol."},
            {"inputSchema", tradesSnapshotSchema()},
        });
        tools.append(QJsonObject{
            {"name", PlatformControlProtocol::kCommandGetBars},
            {"title", "Get historical bars"},
            {"description",
             "Fetch historical bars for the displayed or requested symbol over a single trading-day range."},
            {"inputSchema", barsSchema()},
        });
        tools.append(QJsonObject{
            {"name", PlatformControlProtocol::kCommandGetActivityMetrics},
            {"title", "Get activity metrics"},
            {"description", "Read current trade/L2 activity metrics for the displayed or requested symbol."},
            {"inputSchema", symbolPollingSchema()},
        });
        tools.append(QJsonObject{
            {"name", PlatformControlProtocol::kCommandEnterReplay},
            {"title", "Enter replay mode"},
            {"description", "Switch L2Trader into replay mode and preload the chart in a paused state."},
            {"inputSchema", replayArgsSchema()},
        });
        tools.append(QJsonObject{
            {"name", PlatformControlProtocol::kCommandStartReplay},
            {"title", "Start replay playback"},
            {"description",
             "Start replay playback after replay mode is entered. Omitted date/time/speed fields reuse the platform's current replay configuration."},
            {"inputSchema", replayStartArgsSchema()},
        });
        tools.append(QJsonObject{
            {"name", PlatformControlProtocol::kCommandPauseReplay},
            {"title", "Pause replay playback"},
            {"description", "Pause replay playback while remaining in replay mode."},
            {"inputSchema", emptyObjectSchema()},
        });
        tools.append(QJsonObject{
            {"name", PlatformControlProtocol::kCommandResumeReplay},
            {"title", "Resume replay playback"},
            {"description", "Resume replay playback from the current paused position."},
            {"inputSchema", emptyObjectSchema()},
        });
        tools.append(QJsonObject{
            {"name", PlatformControlProtocol::kCommandSetReplaySpeed},
            {"title", "Set replay speed"},
            {"description", "Update replay speed while already in replay mode."},
            {"inputSchema", replaySpeedSchema()},
        });
        tools.append(QJsonObject{
            {"name", PlatformControlProtocol::kCommandPreloadReplay},
            {"title", "Preload replay chart"},
            {"description", "Reload replay data for a different date/time while staying in replay mode."},
            {"inputSchema", replayArgsSchema()},
        });
        tools.append(QJsonObject{
            {"name", PlatformControlProtocol::kCommandExitReplay},
            {"title", "Exit replay mode"},
            {"description", "Leave replay mode and resume live data mode."},
            {"inputSchema", emptyObjectSchema()},
        });
        tools.append(QJsonObject{
            {"name", PlatformControlProtocol::kCommandSetTradingMode},
            {"title", "Set trading mode"},
            {"description", "Persist the requested TradeStation mode (sim or live). A platform restart is required."},
            {"inputSchema",
             QJsonObject{
                 {"type", "object"},
                 {"properties",
                  QJsonObject{
                      {"mode",
                       QJsonObject{
                           {"type", "string"},
                           {"enum", QJsonArray{"sim", "live"}},
                           {"description", "Trading mode to persist."},
                       }},
                  }},
                 {"required", QJsonArray{"mode"}},
             }},
        });
        return tools;
    }
} // namespace

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
    err << "l2trader-mcp-server ready - exposing L2Trader control tools over MCP stdio\n";
    err.flush();

    PlatformControlClient controlClient;
    bool initializeSeen = false;

    const QSet<QString> supportedTools = {
        PlatformControlProtocol::kCommandStatus,
        PlatformControlProtocol::kCommandGetDisplayedSymbol,
        PlatformControlProtocol::kCommandGetLevel2,
        PlatformControlProtocol::kCommandGetTradesSnapshot,
        PlatformControlProtocol::kCommandGetBars,
        PlatformControlProtocol::kCommandGetActivityMetrics,
        PlatformControlProtocol::kCommandEnterReplay,
        PlatformControlProtocol::kCommandStartReplay,
        PlatformControlProtocol::kCommandPauseReplay,
        PlatformControlProtocol::kCommandResumeReplay,
        PlatformControlProtocol::kCommandSetReplaySpeed,
        PlatformControlProtocol::kCommandPreloadReplay,
        PlatformControlProtocol::kCommandExitReplay,
        PlatformControlProtocol::kCommandSetTradingMode,
    };

    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line.empty())
        {
            continue;
        }

        const auto parsedMessage = PlatformControlProtocol::parseMessage(QByteArray::fromStdString(line));
        if (!parsedMessage.has_value())
        {
            std::cout << compactJsonLine(makeJsonRpcError(QJsonValue(), -32700, parsedMessage.error())).constData()
                      << std::flush;
            continue;
        }

        const QJsonObject message = parsedMessage.value();
        const QString method = message.value("method").toString();
        const QJsonValue id = message.value("id");
        if (message.value("jsonrpc").toString() != "2.0" || method.isEmpty())
        {
            std::cout << compactJsonLine(makeJsonRpcError(id, -32600, "Invalid JSON-RPC request")).constData()
                      << std::flush;
            continue;
        }

        if (method == "notifications/initialized")
        {
            initializeSeen = true;
            continue;
        }

        if (method == "initialize")
        {
            initializeSeen = true;
            const QJsonObject result{
                {"protocolVersion", kMCPProtocolVersion},
                {"capabilities", QJsonObject{{"tools", QJsonObject{{"listChanged", false}}}}},
                {"serverInfo",
                 QJsonObject{
                     {"name", "l2trader-mcp-server"},
                     {"title", "L2Trader MCP Server"},
                     {"version", app.applicationVersion()},
                 }},
                {"instructions",
                 "This MCP server exposes L2Trader platform-control tools plus poll-style market-data tools over the "
                 "platform control socket. Order-flow tools will be added in later phases."},
            };
            std::cout << compactJsonLine(makeJsonRpcResponse(id, result)).constData() << std::flush;
            continue;
        }

        if (method == "ping")
        {
            std::cout << compactJsonLine(makeJsonRpcResponse(id, QJsonObject{})).constData() << std::flush;
            continue;
        }

        if (!initializeSeen)
        {
            std::cout << compactJsonLine(makeJsonRpcError(id, -32000, "Server not initialized")).constData()
                      << std::flush;
            continue;
        }

        if (method == "tools/list")
        {
            std::cout << compactJsonLine(makeJsonRpcResponse(id, QJsonObject{{"tools", buildToolList()}})).constData()
                      << std::flush;
            continue;
        }

        if (method == "tools/call")
        {
            const QJsonObject params = message.value("params").toObject();
            const QString toolName = params.value("name").toString();
            const QJsonObject arguments = params.value("arguments").toObject();

            if (!supportedTools.contains(toolName))
            {
                std::cout << compactJsonLine(makeJsonRpcError(id, -32602, QString("Unknown tool: %1").arg(toolName)))
                                 .constData()
                          << std::flush;
                continue;
            }

            const auto controlResponse =
                controlClient.sendCommand(PlatformControlProtocol::makeRequest(toolName, arguments));
            if (!controlResponse.has_value())
            {
                const QJsonObject result = makeTextContentResult(controlResponse.error(), true);
                std::cout << compactJsonLine(makeJsonRpcResponse(id, result)).constData() << std::flush;
                continue;
            }

            const QByteArray controlJson = QJsonDocument(controlResponse.value()).toJson(QJsonDocument::Compact);
            const bool isError = !controlResponse->value("ok").toBool(false);
            const QJsonObject result =
                makeTextContentResult(QString::fromUtf8(controlJson), isError, controlResponse.value());
            std::cout << compactJsonLine(makeJsonRpcResponse(id, result)).constData() << std::flush;
            continue;
        }

        std::cout
            << compactJsonLine(makeJsonRpcError(id, -32601, QString("Unknown method: %1").arg(method))).constData()
            << std::flush;
    }

    return 0;
}
