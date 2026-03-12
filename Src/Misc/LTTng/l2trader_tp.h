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
 * Tracepoint groups:
 *   barcache_*        — BarCache fetch/store lifecycle
 *   db_*              — DatabaseThread cache miss events
 *   livebar_*         — LiveBarAccumulator bar completion
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
 * DatabaseThread — completeness miss
 * ------------------------------------------------------------------ */

/** Fired when the database has bars for a day but below the 90% completeness threshold. */
TRACEPOINT_EVENT(l2trader,
                 db_completeness_miss,
                 TP_ARGS(const char*, symbol, int, tf_seconds, const char*, date, int, expected, int, got),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, tf_seconds, tf_seconds) ctf_string(date, date)
                               ctf_integer(int, expected, expected) ctf_integer(int, got, got)))

/* ------------------------------------------------------------------
 * LiveBarAccumulator — bar closed
 * ------------------------------------------------------------------ */

/** Fired each time a bar interval closes and is emitted downstream. */
TRACEPOINT_EVENT(l2trader,
                 livebar_closed,
                 TP_ARGS(const char*, symbol, int, interval_seconds),
                 TP_FIELDS(ctf_string(symbol, symbol) ctf_integer(int, interval_seconds, interval_seconds)))

#endif /* _L2TRADER_TP_H */

#include <lttng/tracepoint-event.h>
