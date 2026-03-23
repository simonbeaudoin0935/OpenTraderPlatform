#include "LedgerPaths.h"

#include <QDir>
#include <QFileInfoList>
#include <QRegularExpression>

#include "Assume.h"
#include "Settings.h"
#include "TSClient.h"

namespace
{
    [[nodiscard]] QString ensureDirectory(const QString& p_path)
    {
        QDir().mkpath(p_path);
        return p_path;
    }
} // namespace

namespace LedgerPaths
{
    QString ledgersRootDir()
    {
        return ensureDirectory(getDataLocation() + "/Ledgers");
    }

    QString liveLedgerDatabasePath(const TradingMode p_mode)
    {
        const QString modeDir = (p_mode == TradingMode::Sim) ? "Simulation" : "Live";
        return ensureDirectory(ledgersRootDir() + "/" + modeDir) + "/Ledger.db";
    }

    QString replayLedgerDatabasePath(const QString& p_sessionId)
    {
        ASSUME_TRUE(!p_sessionId.isEmpty());
        return ensureDirectory(ledgersRootDir() + "/Replay") + "/Ledger_" + p_sessionId + ".db";
    }

    QString currentLedgerDatabasePath()
    {
        switch (MainApp::getDataSourceMode())
        {
        case DataSourceMode::Review:
            return replayLedgerDatabasePath(MainApp::getReviewSessionId());

        case DataSourceMode::Replay:
        {
            TSClient* client = TSClient::getInstance();
            ASSUME_DIFF(client, nullptr);
            const QString sessionId = client->getReplaySessionTimestamp();
            ASSUME_TRUE(!sessionId.isEmpty());
            return replayLedgerDatabasePath(sessionId);
        }

        case DataSourceMode::Live:
        default:
            return liveLedgerDatabasePath(MainApp::getTradingMode());
        }
    }

    QStringList replayLedgerSessionIds()
    {
        QStringList sessionIds;
        QDir replayDir(ledgersRootDir() + "/Replay");
        if (!replayDir.exists())
        {
            return sessionIds;
        }

        const QFileInfoList entries =
            replayDir.entryInfoList({"Ledger_*.db"}, QDir::Files, QDir::Name | QDir::Reversed);
        const QRegularExpression pattern("^Ledger_(.+)\\.db$");
        for (const QFileInfo& entry: entries)
        {
            const QRegularExpressionMatch match = pattern.match(entry.fileName());
            if (!match.hasMatch())
            {
                continue;
            }
            sessionIds.append(match.captured(1));
        }

        return sessionIds;
    }
} // namespace LedgerPaths
