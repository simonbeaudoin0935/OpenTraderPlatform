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

#endif /* _L2TRADER_TP_H */

#include <lttng/tracepoint-event.h>
