#ifndef NOC_W17_RUNRESTORE_0ACD81_H
#define NOC_W17_RUNRESTORE_0ACD81_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_runrestore_0acd81_resume_pending_candidates @ gateway/run_restored_delivery.py:resume_pending_candidates
 * fixture drives the runner seams. Returns a new JSON array of session_key
 * strings for the selected pending entries, or JSON null on enumeration failure
 * or a tripped restart-loop guard. */
struct json_t *w17_runrestore_0acd81_resume_pending_candidates(const struct json_t *fixture);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_RUNRESTORE_0ACD81_H */
