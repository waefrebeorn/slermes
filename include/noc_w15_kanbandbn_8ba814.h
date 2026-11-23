#ifndef NOC_W15_KANBANDDBN_8BA814_H
#define NOC_W15_KANBANDDBN_8BA814_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes hermes_cli/kanban_db_notify.py (gate w15_kanbandbn_8ba814).
 * All results are freshly allocated json_t values freed by the caller. */

/* PoP: w15_kanbandbn_8ba814_sub_key @ hermes_cli/kanban_db_notify.py:_sub_key */
struct json_t *w15_kanbandbn_8ba814_sub_key(const struct json_t *task_id,
                                            const struct json_t *platform,
                                            const struct json_t *chat_id,
                                            const struct json_t *thread_id);

/* PoP: w15_kanbandbn_8ba814_encode_notify_delivery_metadata @ hermes_cli/kanban_db_notify.py:_encode_notify_delivery_metadata */
struct json_t *w15_kanbandbn_8ba814_encode_notify_delivery_metadata(const struct json_t *metadata);

/* PoP: w15_kanbandbn_8ba814_decode_notify_delivery_metadata @ hermes_cli/kanban_db_notify.py:_decode_notify_delivery_metadata */
struct json_t *w15_kanbandbn_8ba814_decode_notify_delivery_metadata(const struct json_t *raw);

/* PoP: w15_kanbandbn_8ba814_notify_profile_filter @ hermes_cli/kanban_db_notify.py:_notify_profile_filter */
struct json_t *w15_kanbandbn_8ba814_notify_profile_filter(const struct json_t *notifier_profiles,
                                                          int include_unowned);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_KANBANDDBN_8BA814_H */
