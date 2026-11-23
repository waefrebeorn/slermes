#ifndef NOC_W8_SESSIONUSAG_C7F1BB_H
#define NOC_W8_SESSIONUSAG_C7F1BB_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Faithful C11 port of the pure helpers in gateway/session_usage.py. */

/* PoP: w8_sessionusag_c7f1bb_finite @ gateway/session_usage.py:_finite */
struct json_t *w8_sessionusag_c7f1bb_finite(const struct json_t *value);

/* PoP: w8_sessionusag_c7f1bb_carried_compression_count @ gateway/session_usage.py:carried_compression_count */
struct json_t *w8_sessionusag_c7f1bb_carried_compression_count(const struct json_t *live,
        const struct json_t *agent);

/* PoP: w8_sessionusag_c7f1bb_stored_stats @ gateway/session_usage.py:_stored_stats */
struct json_t *w8_sessionusag_c7f1bb_stored_stats(const struct json_t *row);

/* PoP: w8_sessionusag_c7f1bb_tui_usage @ gateway/session_usage.py:tui_usage */
struct json_t *w8_sessionusag_c7f1bb_tui_usage(const struct json_t *stats);

/* PoP: w8_sessionusag_c7f1bb_usage_frame @ gateway/session_usage.py:usage_frame */
struct json_t *w8_sessionusag_c7f1bb_usage_frame(const struct json_t *stats);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_SESSIONUSAG_C7F1BB_H */
