#pragma once

#include <QString>

namespace RiskStateStoreQueries
{
    const QString CREATE_RISK_STATE_TABLE = "CREATE TABLE IF NOT EXISTS risk_state ("
                                            "account_id TEXT PRIMARY KEY, "
                                            "risk_day TEXT NOT NULL, "
                                            "drawdown_basis INTEGER NOT NULL, "
                                            "basis_peak REAL NOT NULL DEFAULT 0, "
                                            "basis_baseline REAL NOT NULL DEFAULT 0, "
                                            "current_metric REAL NOT NULL DEFAULT 0, "
                                            "trading_locked INTEGER NOT NULL DEFAULT 0, "
                                            "lock_reason TEXT, "
                                            "entry_trades_count INTEGER NOT NULL DEFAULT 0, "
                                            "open_positions_count INTEGER NOT NULL DEFAULT 0, "
                                            "cooldown_until TEXT, "
                                            "last_equity REAL NOT NULL DEFAULT 0, "
                                            "last_todays_pnl REAL NOT NULL DEFAULT 0, "
                                            "last_realized_pnl REAL NOT NULL DEFAULT 0, "
                                            "updated_at TEXT NOT NULL"
                                            ")";

    const QString CREATE_RISK_EVENTS_TABLE = "CREATE TABLE IF NOT EXISTS risk_events ("
                                             "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                             "timestamp TEXT NOT NULL, "
                                             "account_id TEXT NOT NULL, "
                                             "event_type TEXT NOT NULL, "
                                             "reason_code TEXT, "
                                             "message TEXT, "
                                             "data_json TEXT"
                                             ")";

    const QString CREATE_RISK_CONFIG_SNAPSHOT_TABLE = "CREATE TABLE IF NOT EXISTS risk_config_snapshot ("
                                                      "account_id TEXT PRIMARY KEY, "
                                                      "updated_at TEXT NOT NULL, "
                                                      "json_data TEXT NOT NULL"
                                                      ")";

    const QString SELECT_RISK_STATE_FOR_ACCOUNT = "SELECT risk_day, drawdown_basis, basis_peak, basis_baseline, "
                                                  "current_metric, trading_locked, lock_reason, entry_trades_count, "
                                                  "open_positions_count, cooldown_until, last_equity, "
                                                  "last_todays_pnl, last_realized_pnl "
                                                  "FROM risk_state WHERE account_id = ?";

    const QString UPSERT_RISK_STATE = "INSERT OR REPLACE INTO risk_state ("
                                      "account_id, risk_day, drawdown_basis, basis_peak, basis_baseline, "
                                      "current_metric, trading_locked, lock_reason, entry_trades_count, "
                                      "open_positions_count, cooldown_until, last_equity, last_todays_pnl, "
                                      "last_realized_pnl, updated_at"
                                      ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    const QString INSERT_RISK_EVENT = "INSERT INTO risk_events ("
                                      "timestamp, account_id, event_type, reason_code, message, data_json"
                                      ") VALUES (?, ?, ?, ?, ?, ?)";

    const QString UPSERT_RISK_CONFIG_SNAPSHOT =
        "INSERT OR REPLACE INTO risk_config_snapshot (account_id, updated_at, json_data) VALUES (?, ?, ?)";
} // namespace RiskStateStoreQueries
