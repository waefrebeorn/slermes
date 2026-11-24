#ifndef NOC_W17_RUNHOUSEKE_16C95D_H
#define NOC_W17_RUNHOUSEKE_16C95D_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_runhouseke_16c95d_clear_local_auto_prune_notice @ gateway/run_housekeeping_disk.py:clear_local_auto_prune_notice
 * fixture: platform (source.platform), chat_id, has_authority, has_live.
 * Returns JSON null when nothing is published, else
 * {"chat_id":str,"key":str,"event_type":"notification.clear"}. */
struct json_t *w17_runhouseke_16c95d_clear_local_auto_prune_notice(const struct json_t *fixture);

/* PoP: w17_runhouseke_16c95d_maybe_post_auto_prune_prompt @ gateway/run_housekeeping_disk.py:maybe_post_auto_prune_prompt
 * fixture drives the seams. Returns {"scheduled":[...],"snoozed":[...]}. */
struct json_t *w17_runhouseke_16c95d_maybe_post_auto_prune_prompt(const struct json_t *fixture);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_RUNHOUSEKE_16C95D_H */
