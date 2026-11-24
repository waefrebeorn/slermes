/*
 * noc_w27_adapter_42b735_p2.h - C surface for slice 2 of upstream
 * plugins/platforms/whatsapp/adapter.py (gate w27_adapter_42b735_p2):
 * bridge lifecycle + endpoint addressing.
 *
 * _bridge_url, the bridge.pid line parser behind _read_bridge_pidfile, the
 * _bridge_died fatal-error classification and _is_connected.  Each symbol
 * takes the JSON-lines request object and returns the Python-equivalent JSON
 * value of the same-named upstream function applied to the keys named after
 * its parameters.  See adapter.c for the PoP map; process spawning, pidfiles
 * and the gateway env lookup stay in the caller.
 */
#ifndef NOC_W27_ADAPTER_42B735_P2_H
#define NOC_W27_ADAPTER_42B735_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req.port, req.path -> "http://127.0.0.1:<port>/<path>". */
json_t *w27_adapter_42b735_p2_bridge_url(const json_t *req);

/* req.text (the whole bridge.pid file) -> [pid, start_or_null, port_or_null];
 * [null, null, null] when line 1 is not an int. */
json_t *w27_adapter_42b735_p2_pidfile_parse(const json_t *req);

/* req.rc (the polled exit code, null = no process), req.has_process,
 * req.code, req.detail, req.log ->
 * {"code","detail","retryable"} as handed to _set_fatal_error, with
 * PASSKEY_REQUIRED_EXIT (78) promoted to whatsapp_passkey_required. */
json_t *w27_adapter_42b735_p2_bridge_died(const json_t *req);

/* req.config ({enabled, extra} or null), req.env (WHATSAPP_ENABLED) -> bool. */
json_t *w27_adapter_42b735_p2_is_connected(const json_t *req);

/* Dispatch by request "func"; returns "UNKNOWN_FUNC" when absent. */
json_t *w27_adapter_42b735_p2_dispatch(const char *func, const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_42B735_P2_H */
