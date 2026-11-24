/*
 * noc_w26_gatewayrun_558360.h - C surface for upstream
 * hermes_cli/gateway_runtime_start.py (gate w26_gatewayrun_558360).
 *
 * The detached-gateway launch primitive: which supervisor markers a spawned
 * daemon must not inherit (they would make it identify as supervised and race
 * the unit), the profile pin that survives the strip, the OS-refusal text, and
 * the append-only stdio log the launch writes to.  The spawn itself, the
 * platform branches and the deadline stay with the caller; every function here
 * is a pure value -> value decision.
 */
#ifndef NOC_W26_GATEWAYRUN_558360_H
#define NOC_W26_GATEWAYRUN_558360_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of supervisor markers / daemon-stripped names. */
#define W26_GATEWAYRUN_558360_MARKERS 10
#define W26_GATEWAYRUN_558360_STRIPPED 40

/* strip_supervisor_markers: {"env": {...}} -> the env a daemon may inherit. */
json_t *w26_gatewayrun_558360_strip_markers(const json_t *env);

/* os_refusal: {"winerror": n|null, "errno": n|null, "strerror": s|null} ->
 * "WinError 5: Access is denied" / "errno 2: ..." / null when nothing is known. */
json_t *w26_gatewayrun_558360_os_refusal(const json_t *exc);

/* stdio_log_path: {"home": "..."} -> "<home>/logs/gateway-stdio.log". */
json_t *w26_gatewayrun_558360_stdio_log_path(const json_t *req);

/* The marker tables, the elevated-client remedy and the log-tail literals. */
json_t *w26_gatewayrun_558360_constants(void);

#ifdef __cplusplus
}
#endif
#endif
