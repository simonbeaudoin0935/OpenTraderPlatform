#pragma once

#include <QString>
#include <QStringList>

#include "MainApp.h"

namespace LedgerPaths
{
    [[nodiscard]] QString ledgersRootDir();
    [[nodiscard]] QString liveLedgerDatabasePath(TradingMode p_mode);
    [[nodiscard]] QString replayLedgerDatabasePath(const QString& p_sessionId);
    [[nodiscard]] QString currentLedgerDatabasePath();
    [[nodiscard]] QStringList replayLedgerSessionIds();
} // namespace LedgerPaths
