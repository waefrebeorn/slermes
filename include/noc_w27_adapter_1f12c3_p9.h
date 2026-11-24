/*
 * noc_w27_adapter_1f12c3_p9.h - C surface for upstream
 * plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p9).
 *
 * gate env / csv lists (adapter.py:4883-4920).
 * PORTED (pure): the per-profile gate env reader with its snapshot branch, the
 * env-then-extra gate reader and the CSV/JSON-list allowlist parser with its
 * three channel-set call sites.
 * SEAM: os.getenv (the value is the parameter), the connect-time env snapshot
 * dict and the adapter config extra dict.
 */
#ifndef NOC_W27_ADAPTER_1F12C3_P9_H
#define NOC_W27_ADAPTER_1F12C3_P9_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_1f12c3_p9_gate_env(const json_t *snapshot, const json_t *name, const json_t *dflt, const json_t *env);
json_t *w27_adapter_1f12c3_p9_gate_raw(const json_t *env_val, const json_t *extra, const json_t *extra_key);
json_t *w27_adapter_1f12c3_p9_gate_csv_set(const json_t *raw);
json_t *w27_adapter_1f12c3_p9_get_allowed_channels(const json_t *env_val, const json_t *extra);
json_t *w27_adapter_1f12c3_p9_get_ignored_channels(const json_t *env_val, const json_t *extra);
json_t *w27_adapter_1f12c3_p9_get_no_thread_channels(const json_t *env_val, const json_t *extra);

#ifdef __cplusplus
}
#endif
#endif

