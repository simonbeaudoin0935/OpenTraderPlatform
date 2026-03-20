#pragma once

#include "Clients/DBClient/PlaybackTypes.h"
#include "Misc/CONSTANTS.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcessEnvironment>
#include <QStringList>

#include <expected>

namespace PlatformControlProtocol
{
    inline constexpr int kProtocolVersion = 1;
    inline constexpr qint64 kDefaultTimeoutMs = 30000;
    inline constexpr qsizetype kMaxMessageBytes = 1024 * 1024;

    inline constexpr auto kSocketFileName = "platform-control.sock";

    inline constexpr auto kCommandStatus = "status";
    inline constexpr auto kCommandEnterReplay = "enter-replay";
    inline constexpr auto kCommandStartReplay = "start-replay";
    inline constexpr auto kCommandPauseReplay = "pause-replay";
    inline constexpr auto kCommandResumeReplay = "resume-replay";
    inline constexpr auto kCommandSetReplaySpeed = "set-replay-speed";
    inline constexpr auto kCommandPreloadReplay = "preload-replay";
    inline constexpr auto kCommandExitReplay = "exit-replay";
    inline constexpr auto kCommandSetTradingMode = "set-trading-mode";
    inline constexpr auto kCommandGetDisplayedSymbol = "get-displayed-symbol";
    inline constexpr auto kCommandGetLevel2 = "get-level2";
    inline constexpr auto kCommandGetTradesSnapshot = "get-trades-snapshot";
    inline constexpr auto kCommandGetBars = "get-bars";
    inline constexpr auto kCommandGetActivityMetrics = "get-activity-metrics";

    inline QString stateRootPath()
    {
        QString xdgStateHome = QProcessEnvironment::systemEnvironment().value("XDG_STATE_HOME");
        if (xdgStateHome.isEmpty())
        {
            xdgStateHome = QDir::homePath() + "/.local/state";
        }

        return xdgStateHome + "/L2Trader";
    }

    inline QString socketPath()
    {
        return stateRootPath() + "/" + kSocketFileName;
    }

    inline QByteArray serializeMessage(const QJsonObject& p_payload)
    {
        QByteArray encoded = QJsonDocument(p_payload).toJson(QJsonDocument::Compact);
        encoded.append('\n');
        return encoded;
    }

    inline std::expected<QJsonObject, QString> parseMessage(const QByteArray& p_payload)
    {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(p_payload, &parseError);
        if (parseError.error != QJsonParseError::NoError)
        {
            return std::unexpected(QString("Invalid JSON payload: %1").arg(parseError.errorString()));
        }

        if (!document.isObject())
        {
            return std::unexpected("JSON payload must be an object");
        }

        return document.object();
    }

    inline QStringList supportedReplaySpeeds()
    {
        return {
            "0.01x",
            "0.1x",
            "0.5x",
            "1x",
            "2x",
            "5x",
            "10x",
            "50x",
            "100x",
            "max",
        };
    }

    inline QJsonArray supportedReplaySpeedsJson()
    {
        QJsonArray values;
        for (const QString& speed: supportedReplaySpeeds())
        {
            values.append(speed);
        }
        return values;
    }

    inline QStringList supportedBarTimeFrames()
    {
        return {
            "10s",
            "1m",
            "5m",
            "15m",
            "30m",
            "1h",
            "4h",
            "1d",
            "1w",
            "1M",
        };
    }

    inline QJsonArray supportedBarTimeFramesJson()
    {
        QJsonArray values;
        for (const QString& timeFrame: supportedBarTimeFrames())
        {
            values.append(timeFrame);
        }
        return values;
    }

    inline QString replaySpeedToString(const Playback::Speed p_speed)
    {
        switch (p_speed)
        {
        case Playback::Speed::SuperSlow:
            return "0.01x";
        case Playback::Speed::VerySlow:
            return "0.1x";
        case Playback::Speed::Half:
            return "0.5x";
        case Playback::Speed::Normal:
            return "1x";
        case Playback::Speed::Double:
            return "2x";
        case Playback::Speed::Fast5x:
            return "5x";
        case Playback::Speed::Fast10x:
            return "10x";
        case Playback::Speed::Fast50x:
            return "50x";
        case Playback::Speed::Fast100x:
            return "100x";
        case Playback::Speed::AsFastAsPossible:
            return "max";
        }

        return QString::number(static_cast<int>(p_speed));
    }

    inline std::expected<Playback::Speed, QString> replaySpeedFromString(QString p_value)
    {
        p_value = p_value.trimmed().toLower();
        if (p_value.isEmpty())
        {
            return std::unexpected("Replay speed must not be empty");
        }

        if (p_value == "max" || p_value == "as-fast-as-possible" || p_value == "as_fast_as_possible")
        {
            return Playback::Speed::AsFastAsPossible;
        }

        bool isNumeric = !p_value.isEmpty() && (p_value.front().isDigit() || p_value.front() == '-');
        for (const QChar ch: p_value)
        {
            if (!ch.isDigit() && ch != '-')
            {
                isNumeric = false;
                break;
            }
        }
        if (isNumeric)
        {
            bool ok = false;
            const int numericValue = p_value.toInt(&ok);
            if (ok)
            {
                switch (numericValue)
                {
                case static_cast<int>(Playback::Speed::SuperSlow):
                    return Playback::Speed::SuperSlow;
                case static_cast<int>(Playback::Speed::VerySlow):
                    return Playback::Speed::VerySlow;
                case static_cast<int>(Playback::Speed::Half):
                    return Playback::Speed::Half;
                case static_cast<int>(Playback::Speed::Normal):
                    return Playback::Speed::Normal;
                case static_cast<int>(Playback::Speed::Double):
                    return Playback::Speed::Double;
                case static_cast<int>(Playback::Speed::Fast5x):
                    return Playback::Speed::Fast5x;
                case static_cast<int>(Playback::Speed::Fast10x):
                    return Playback::Speed::Fast10x;
                case static_cast<int>(Playback::Speed::Fast50x):
                    return Playback::Speed::Fast50x;
                case static_cast<int>(Playback::Speed::Fast100x):
                    return Playback::Speed::Fast100x;
                case static_cast<int>(Playback::Speed::AsFastAsPossible):
                    return Playback::Speed::AsFastAsPossible;
                default:
                    break;
                }
            }
        }

        if (p_value.endsWith('x'))
        {
            if (p_value == "0.01x")
                return Playback::Speed::SuperSlow;
            if (p_value == "0.1x")
                return Playback::Speed::VerySlow;
            if (p_value == "0.5x")
                return Playback::Speed::Half;
            if (p_value == "1x")
                return Playback::Speed::Normal;
            if (p_value == "2x")
                return Playback::Speed::Double;
            if (p_value == "5x")
                return Playback::Speed::Fast5x;
            if (p_value == "10x")
                return Playback::Speed::Fast10x;
            if (p_value == "50x")
                return Playback::Speed::Fast50x;
            if (p_value == "100x")
                return Playback::Speed::Fast100x;
        }

        return std::unexpected(QString("Unsupported replay speed '%1'. Supported values: %2")
                                   .arg(p_value, supportedReplaySpeeds().join(", ")));
    }

    inline QJsonObject makeRequest(const QString& p_command, const QJsonObject& p_arguments = {})
    {
        QJsonObject request;
        request["protocolVersion"] = kProtocolVersion;
        request["command"] = p_command;
        request["arguments"] = p_arguments;
        return request;
    }
} // namespace PlatformControlProtocol
