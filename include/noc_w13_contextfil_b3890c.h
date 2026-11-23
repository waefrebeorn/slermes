#ifndef NOC_W13_CONTEXTFIL_B3890C_H
#define NOC_W13_CONTEXTFIL_B3890C_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C11 port of the pure helpers of agent/context_file_sources.py
 * (gate w13_contextfil_b3890c). Filesystem discovery (list_context_file_sources)
 * is environment-bound and is NOT ported; the pure projection helpers are. */

/* agent.model_metadata.estimate_tokens_rough(text). */
long long w13_contextfil_b3890c_estimate_tokens_rough(const char *text);

/* _entry(label, path, content, status) -> {label,path,chars,est_tokens,loaded,status}. */
struct json_t *w13_contextfil_b3890c_entry(const char *label, const char *path,
        const char *content, const char *status);

/* _loaded_status with the injection-scan result injected as `scan_hit`.
 * rendered_len / max_chars are 64-bit ints. Returns a heap string. */
char *w13_contextfil_b3890c_loaded_status(int scan_hit, long long rendered_len,
        long long max_chars, int user_authored);

/* render_context_file_lines(sources) -> JSON array of strings. */
struct json_t *w13_contextfil_b3890c_render_context_file_lines(const struct json_t *sources);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W13_CONTEXTFIL_B3890C_H */
