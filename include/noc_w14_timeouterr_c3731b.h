#ifndef NOC_W14_TIMEOERR_C3731B_H
#define NOC_W14_TIMEOERR_C3731B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_timeouterr_c3731b_timeout_phase @ agent/timeout_error_summary.py:_timeout_phase */
/* nodes: JSON array of {"type": <exception class name>, "cause": <index|null>}. */
const char *w14_timeouterr_c3731b_timeout_phase(const struct json_t *nodes);

/* PoP: w14_timeouterr_c3731b_summarize @ agent/timeout_error_summary.py:summarize_timeout_error */
/* Returns a malloc'd UTF-8 summary or NULL; caller frees. */
char *w14_timeouterr_c3731b_summarize(const struct json_t *nodes, const struct json_t *provider);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_TIMEOERR_C3731B_H */
