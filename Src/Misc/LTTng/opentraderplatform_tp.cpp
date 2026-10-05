/*
 * LTTng UST probe registration unit.
 *
 * This translation unit must be compiled exactly once to register the
 * opentraderplatform tracepoint provider with the LTTng daemon.  The two defines
 * below must appear before the provider header is included:
 *
 *   TRACEPOINT_CREATE_PROBES  — emits the probe implementation code
 *   TRACEPOINT_DEFINE         — emits the provider registration table
 *
 * All tracepoint calls in other TUs are no-ops unless liblttng-ust.so is
 * loaded (the provider auto-connects at dlopen time).
 */

#ifdef LTTNG_ENABLED

// clang-format off
#define TRACEPOINT_CREATE_PROBES
#define TRACEPOINT_DEFINE
#include "LTTng/opentraderplatform_tp.h"
// clang-format on

#endif // LTTNG_ENABLED
