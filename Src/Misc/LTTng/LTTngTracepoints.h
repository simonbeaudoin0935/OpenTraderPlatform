#pragma once

/*
 * LTTngTracepoints.h — single include point for LTTng tracepoints.
 *
 * Include this header in any translation unit that needs to emit tracepoints.
 * When LTTNG_ENABLED is not defined (normal builds) every LTTnG_TP() call
 * compiles away to nothing — zero runtime cost.
 *
 * Example:
 *   #include "LTTng/LTTngTracepoints.h"
 *   ...
 *   LTTnG_TP(opentraderplatform, barcache_api_fetch_start,
 *          m_symbol.toUtf8().constData(), (int)tf,
 *          date.toString("yyyy-MM-dd").toUtf8().constData());
 */

#ifdef LTTNG_ENABLED
#include "LTTng/opentraderplatform_tp.h"
#define LTTnG_TP(provider, event, ...) tracepoint(provider, event, ##__VA_ARGS__)
#else
#define LTTnG_TP(provider, event, ...)                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
    } while (0)
#endif
