#ifndef NOC_W9_SESSIONINGR_2D59A9_H
#define NOC_W9_SESSIONINGR_2D59A9_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_sessioningr_2d59a9_row_turn_author @ gateway/session_ingress.py:row_turn_author
 *
 * Who wrote the admitted input, for memory attribution only.  The producer's
 * turn_author stamp (first of local_automation_v1 / api_turn_v1 / cli_turn_v1
 * that has one) wins; `fallback_author` is the injected forward_author(policy)
 * result used when none is stamped.  Caller frees. */
struct json_t *w9_sessioningr_2d59a9_row_turn_author(const struct json_t *row,
                                                     const struct json_t *fallback_author);

/* PoP: w9_sessioningr_2d59a9_pause_notice @ gateway/session_ingress.py:pause_notice
 *
 * The committed input stays queued behind a paused FIFO: the per-reason notice
 * string, or JSON null when already notified / no notice applies.  Caller frees. */
struct json_t *w9_sessioningr_2d59a9_pause_notice(const char *reason,
                                                  int pause_notified, int slack);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_SESSIONINGR_2D59A9_H */
