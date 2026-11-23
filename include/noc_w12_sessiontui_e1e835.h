#ifndef NOC_W12_SESSIONTUI_E1E835_H
#define NOC_W12_SESSIONTUI_E1E835_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_sessiontui_e1e835_tui_read @ gateway/session_tui_reads.py:tui_read
 * Returns 0 = None (keep the gateway handler), 1 = _status, 2 = _context. */
int w12_sessiontui_e1e835_tui_read(const char *policy_source, const char *name);

/* PoP: w12_sessiontui_e1e835_saved_route @ gateway/session_tui_reads.py:_saved_route
 * Fills *out with {"provider": <str|null>, "base_url": <str|null>, "model": <str|null>}
 * from row.model_config and row.model. Caller frees *out. */
void w12_sessiontui_e1e835_saved_route(const struct json_t *row, struct json_t **out);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_SESSIONTUI_E1E835_H */
