/*
 * LTTng UST tracepoint provider for L2Trader.
 *
 * This header defines all TRACEPOINT_EVENT macros for the `l2trader` provider.
 * It follows the canonical two-inclusion LTTng UST pattern:
 *   - First inclusion: declares the tracepoint descriptors
 *   - Second inclusion (via tracepoint-event.h): registers them
 *
 * Usage in application code — include LTTngTracepoints.h, not this file directly.
 *
 * BarCache is a 3-level cache. Tracepoints reflect each level clearly:
 *
 *   L1 (memory)   barcache_l1_hit / barcache_l1_miss
 *                   m_barCacheByTimeFrame QHash lookup in RAM.
 *                   A miss does NOT mean the data is absent — it only means
 *                   it is not yet loaded into memory. L2 (SQLite) is checked next.
 *
 *   L2 (SQLite)   barcache_l2_hit / barcache_l2_miss / barcache_l2_partial
 *                   Hit: DB returned ≥90% of expected bars — loaded into L1.
 *                   Miss: DB returned 0 bars — L3 (API) fetch is triggered.
 *                   Partial: DB has some bars but below the 90% threshold —
 *                            treated as a miss and triggers an L3 fetch.
 *
 *   L3 (API)      barcache_api_fetch_start / barcache_api_fetch_done
 *                   Databento historical bars request over the network.
 *                   Only triggered after both L1 and L2 miss.
 *
 *   Misc:         barcache_day_alloc, barcache_store_bar_live,
 *                 barcache_store_full_day, barcache_store_partial,
 *                 fillholes_run, fillholes_done,
 *                 chart_missing_bars_request, livebar_closed
 */

#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER l2trader

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "LTTng/l2trader_tp.h"

#if !defined(_L2TRADER_TP_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _L2TRADER_TP_H

#include <lttng/tracepoint.h>

/* ------------------------------------------------------------------
 * BarCache — API fetch lifecycle
 * ------------------------------------------------------------------ */

/** Fired just before a Databento API fetch is dispatched for a full day. */
TRACEPOINT_EVENT(l2trader,
                 barcache_api_fetch_start,
                 TP_ARGS(const char*, symbol, int, tf_seconds, const char*, date),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, tf_seconds, tf_seconds) ctf_string(date, date)))

/** Fired when historicalBarsReceived resolves; reports how many bars were returned. */
TRACEPOINT_EVENT(l2trader,
                 barcache_api_fetch_done,
                 TP_ARGS(const char*, symbol, int, tf_seconds, const char*, date, int, bars_count),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, tf_seconds, tf_seconds) ctf_string(date, date)
                               ctf_integer(int, bars_count, bars_count)))

/* ------------------------------------------------------------------
 * BarCache — day-vector allocation
 * ------------------------------------------------------------------ */

/** Fired every time a new QVector<Bar> slab is allocated for a full trading day. */
TRACEPOINT_EVENT(l2trader,
                 barcache_day_alloc,
                 TP_ARGS(const char*, symbol, int, tf_seconds, const char*, date, int, num_slots),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, tf_seconds, tf_seconds) ctf_string(date, date)
                               ctf_integer(int, num_slots, num_slots)))

/* ------------------------------------------------------------------
 * BarCache — live bar store
 * ------------------------------------------------------------------ */

/** Fired on every live/replay bar stored in the in-memory cache. */
TRACEPOINT_EVENT(l2trader,
                 barcache_store_bar_live,
                 TP_ARGS(const char*, symbol, int, tf_seconds, int, index),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, tf_seconds, tf_seconds)
                               ctf_integer(int, index, index)))

/* ------------------------------------------------------------------
 * DatabaseThread — L2 (SQLite) cache hit / miss / partial
 * ------------------------------------------------------------------ */

/** Fired when the database returns sufficient bars (≥90% of expected). Data is loaded into L1. */
TRACEPOINT_EVENT(l2trader,
                 barcache_l2_hit,
                 TP_ARGS(const char*, symbol, int, tf_seconds, const char*, date, int, bars_returned, int, expected),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, tf_seconds, tf_seconds) ctf_string(date, date)
                               ctf_integer(int, bars_returned, bars_returned) ctf_integer(int, expected, expected)))

/** Fired when the database has no bars at all for the requested day. L3 (API) fetch will follow. */
TRACEPOINT_EVENT(l2trader,
                 barcache_l2_miss,
                 TP_ARGS(const char*, symbol, int, tf_seconds, const char*, date),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, tf_seconds, tf_seconds) ctf_string(date, date)))

/** Fired when the database has some bars but below the 90% completeness threshold. L3 (API) fetch will follow. */
TRACEPOINT_EVENT(l2trader,
                 barcache_l2_partial,
                 TP_ARGS(const char*, symbol, int, tf_seconds, const char*, date, int, expected, int, got),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, tf_seconds, tf_seconds) ctf_string(date, date)
                               ctf_integer(int, expected, expected) ctf_integer(int, got, got)))

/* ------------------------------------------------------------------
 * BarCache — fillHolesOfReceivedRequest
 * ------------------------------------------------------------------ */

/**
 * Fired at entry of fillHolesOfReceivedRequest.
 * step_secs=0 would indicate the old integer-division bug has returned.
 */
TRACEPOINT_EVENT(l2trader,
                 fillholes_run,
                 TP_ARGS(const char*, symbol, int, tf_seconds, int, step_secs, int, bars_in, int, expected_slots),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, tf_seconds, tf_seconds)
                               ctf_integer(int, step_secs, step_secs) ctf_integer(int, bars_in, bars_in)
                                   ctf_integer(int, expected_slots, expected_slots)))

/**
 * Fired at exit of fillHolesOfReceivedRequest with the output counts.
 */
TRACEPOINT_EVENT(l2trader,
                 fillholes_done,
                 TP_ARGS(const char*, symbol, int, tf_seconds, int, bars_out, int, void_bars),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, tf_seconds, tf_seconds)
                               ctf_integer(int, bars_out, bars_out) ctf_integer(int, void_bars, void_bars)))

/* ------------------------------------------------------------------
 * BarCache — L1 (in-memory) cache hit / miss
 *
 * NOTE: A miss here only means the day is not yet in RAM.
 * The caller will next check L2 (SQLite). Only if L2 also misses
 * will an L3 (Databento API) fetch be triggered.
 * ------------------------------------------------------------------ */

/** Fired when getBarsFromCache finds the day in RAM and returns bars without any I/O. */
TRACEPOINT_EVENT(l2trader,
                 barcache_l1_hit,
                 TP_ARGS(const char*, symbol, int, tf_seconds, const char*, date, int, bars_returned),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, tf_seconds, tf_seconds) ctf_string(date, date)
                               ctf_integer(int, bars_returned, bars_returned)))

/** Fired when getBarsFromCache does not find the day in RAM. L2 (SQLite) will be checked next. */
TRACEPOINT_EVENT(l2trader,
                 barcache_l1_miss,
                 TP_ARGS(const char*, symbol, int, tf_seconds, const char*, date),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, tf_seconds, tf_seconds) ctf_string(date, date)))

/* ------------------------------------------------------------------
 * BarCache — storeBarsInCache full vs partial
 * ------------------------------------------------------------------ */

/** Fired when a complete day (bars == barsPerDay) is inserted into memory cache. */
TRACEPOINT_EVENT(l2trader,
                 barcache_store_full_day,
                 TP_ARGS(const char*, symbol, int, tf_seconds, const char*, date, int, bars_count),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, tf_seconds, tf_seconds) ctf_string(date, date)
                               ctf_integer(int, bars_count, bars_count)))

/** Fired when a partial day is slotted into the pre-allocated day vector. */
TRACEPOINT_EVENT(l2trader,
                 barcache_store_partial,
                 TP_ARGS(const char*, symbol, int, tf_seconds, const char*, date, int, bars_count, int, expected_slots),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, tf_seconds, tf_seconds) ctf_string(date, date)
                               ctf_integer(int, bars_count, bars_count)
                                   ctf_integer(int, expected_slots, expected_slots)))

/* ------------------------------------------------------------------
 * Chart — missing bars request
 * ------------------------------------------------------------------ */

/** Fired each time the chart fires requestMissingBars. */
TRACEPOINT_EVENT(l2trader,
                 chart_missing_bars_request,
                 TP_ARGS(const char*, symbol, int, tf_seconds, const char*, from, const char*, to),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, tf_seconds, tf_seconds) ctf_string(from, from)
                               ctf_string(to, to)))

/** Fired each time a bar interval closes and is emitted downstream. */
TRACEPOINT_EVENT(l2trader,
                 livebar_closed,
                 TP_ARGS(const char*, symbol, int, interval_seconds),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, interval_seconds, interval_seconds)))

/* ------------------------------------------------------------------
 * SymbolContext — actor-model drain loop
 *
 * These tracepoints instrument the thread-pool actor model used by
 * SymbolContext. They enable visual analysis of per-symbol throughput,
 * queue backpressure, and thread pool utilisation via TraceCompass.
 * ------------------------------------------------------------------ */

/** Fired when a Level2 or Trade event is pushed into a SymbolContext queue. */
TRACEPOINT_EVENT(l2trader,
                 symbolctx_enqueue,
                 TP_ARGS(const char*, symbol, const char*, event_type, int, queue_depth),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_string(event_type, event_type)
                               ctf_integer(int, queue_depth, queue_depth)))

/** Fired when a QRunnable is submitted to the global thread pool for a SymbolContext. */
TRACEPOINT_EVENT(l2trader, symbolctx_pool_submit, TP_ARGS(const char*, symbol), TP_FIELDS(ctf_string(symbol, symbol)))

/** Fired at the top of SymbolContext::drain() — a pool thread begins processing. */
TRACEPOINT_EVENT(l2trader, symbolctx_drain_start, TP_ARGS(const char*, symbol), TP_FIELDS(ctf_string(symbol, symbol)))

/** Fired at exit of SymbolContext::drain() — pool thread done, queue empty. */
TRACEPOINT_EVENT(l2trader,
                 symbolctx_drain_end,
                 TP_ARGS(const char*, symbol, int, items_processed),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, items_processed, items_processed)))

/** Fired before processLevel2() call inside the drain loop. */
TRACEPOINT_EVENT(l2trader,
                 symbolctx_process_level2,
                 TP_ARGS(const char*, symbol),
                 TP_FIELDS(ctf_string(symbol, symbol)))

/** Fired before processTrade() call inside the drain loop. */
TRACEPOINT_EVENT(l2trader, symbolctx_process_trade, TP_ARGS(const char*, symbol), TP_FIELDS(ctf_string(symbol, symbol)))

/** Fired in SymbolContext destructor while waiting for drain to finish. */
TRACEPOINT_EVENT(l2trader, symbolctx_shutdown_wait, TP_ARGS(const char*, symbol), TP_FIELDS(ctf_string(symbol, symbol)))

/* ------------------------------------------------------------------
 * DBClient — replay tick
 * ------------------------------------------------------------------ */

/** Fired on each replay timer tick with the number of events emitted in this batch. */
TRACEPOINT_EVENT(l2trader,
                 replay_tick,
                 TP_ARGS(int, events_emitted, long, current_epoch_ms),
                 TP_FIELDS(ctf_integer(int, events_emitted, events_emitted)
                               ctf_integer(long, current_epoch_ms, current_epoch_ms)))

/* ------------------------------------------------------------------
 * GUI — main thread data-path entry points
 *
 * These tracepoints fire in the GUIFrontend slots that receive data
 * from MainAlgo/TSClient via queued connections. They mark the moment
 * the GUI event loop picks up each event, enabling TraceCompass
 * correlation with the emitter-side tracepoints above.
 * ------------------------------------------------------------------ */

/** Fired when GUIFrontend::onReplayModeEntered() begins. */
TRACEPOINT_EVENT(l2trader, gui_replay_entered, TP_ARGS(), TP_FIELDS())

/** Fired when GUIFrontend::onReplayModeExited() begins. */
TRACEPOINT_EVENT(l2trader, gui_replay_exited, TP_ARGS(), TP_FIELDS())

/** Fired when GUIFrontend::onReplayTimeUpdated() receives a new replay timestamp. */
TRACEPOINT_EVENT(l2trader,
                 gui_replay_time_updated,
                 TP_ARGS(long, epoch_ms),
                 TP_FIELDS(ctf_integer(long, epoch_ms, epoch_ms)))

/** Fired when GUIFrontend dispatches a 1-minute bar to the chart (onCurrentHighlightedStockBarReceived). */
TRACEPOINT_EVENT(l2trader,
                 gui_bar_received,
                 TP_ARGS(const char*, symbol, int, tf_seconds),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, tf_seconds, tf_seconds)))

/** Fired when GUIFrontend dispatches a Level2 update to widgets (onCurrentHighlightedReceivedNewLevel2). */
TRACEPOINT_EVENT(l2trader, gui_level2_received, TP_ARGS(const char*, symbol), TP_FIELDS(ctf_string(symbol, symbol)))

/** Fired when GUIFrontend dispatches a trade to TimeAndSales (onCurrentHighlightedReceivedNewTrade). */
TRACEPOINT_EVENT(l2trader, gui_trade_received, TP_ARGS(const char*, symbol), TP_FIELDS(ctf_string(symbol, symbol)))

/** Fired when GUIFrontend receives an order update (onNewOrderReceived). */
TRACEPOINT_EVENT(l2trader,
                 gui_order_received,
                 TP_ARGS(const char*, symbol, int, status),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, status, status)))

/** Fired when GUIFrontend receives a position update (onNewPositionReceived). */
TRACEPOINT_EVENT(l2trader, gui_position_received, TP_ARGS(const char*, symbol), TP_FIELDS(ctf_string(symbol, symbol)))

/* ------------------------------------------------------------------
 * GUI — chart rendering
 * ------------------------------------------------------------------ */

/** Fired when StockPriceChart::addLiveBar() starts processing a bar into the chart. */
TRACEPOINT_EVENT(l2trader,
                 gui_chart_add_bar,
                 TP_ARGS(const char*, symbol, int, chart_index),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, chart_index, chart_index)))

/** Fired when historical backfill bars arrive (onRequestedMissingBarsReceived). */
TRACEPOINT_EVENT(l2trader,
                 gui_chart_backfill_received,
                 TP_ARGS(int, bars_count),
                 TP_FIELDS(ctf_integer(int, bars_count, bars_count)))

/** Fired when the replay time line is repositioned (updateReplayTimeLine / updateCurrentTimeLine). */
TRACEPOINT_EVENT(l2trader, gui_chart_timeline_update, TP_ARGS(), TP_FIELDS())

/* ------------------------------------------------------------------
 * GUI — widget updates
 * ------------------------------------------------------------------ */

/** Fired when Level2Widget::updateData() rebuilds the order book display. */
TRACEPOINT_EVENT(l2trader, gui_level2_widget_update, TP_ARGS(), TP_FIELDS())

/** Fired when TimeAndSalesWidget::onNewTrade() inserts a trade row. */
TRACEPOINT_EVENT(l2trader, gui_timesales_widget_update, TP_ARGS(), TP_FIELDS())

/** Fired when OrderWidget::updateOrder() processes an order. */
TRACEPOINT_EVENT(l2trader, gui_order_widget_update, TP_ARGS(), TP_FIELDS())

/** Fired when PositionWidget::updatePosition() processes a position. */
TRACEPOINT_EVENT(l2trader, gui_position_widget_update, TP_ARGS(), TP_FIELDS())

// ---------------------------------------------------------------------------
// Pull-based GUI refresh (30 Hz snapshot polling)
// ---------------------------------------------------------------------------

/** Fired on each 30 Hz tick of the display refresh timer in GUIFrontend. */
TRACEPOINT_EVENT(l2trader,
                 gui_pull_tick,
                 TP_ARGS(int, dirty_flags),
                 TP_FIELDS(ctf_integer(int, dirty_flags, dirty_flags)))

/** Fired when a DisplaySnapshot write-lock is acquired by a data handler. */
TRACEPOINT_EVENT(l2trader,
                 snapshot_write,
                 TP_ARGS(const char*, symbol, const char*, field),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_string(field, field)))

#endif /* _L2TRADER_TP_H */

#include <lttng/tracepoint-event.h>
