/*
 * noc_w25_kanbannoti_2979d2.h - C surface for upstream
 * hermes_cli/kanban_notify_thread.py (gate w25_kanbannoti_2979d2).
 *
 * Which thread a "kanban notify-subscribe/unsubscribe" without --thread-id
 * targets: the caller's own session thread when the target is the caller's own
 * chat, else this task's existing subscription thread for that chat, else the
 * root lane.  The DB read and the session env are seams: the subscription rows
 * and the session identity are fixed inputs.
 */
#ifndef NOC_W25_KANBANNOTI_2979D2_H
#define NOC_W25_KANBANNOTI_2979D2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"subs": [...], "platform": <str>, "chat_id": <any>} ->
 * [<str|null>, ...] thread ids of this task's subscriptions to that chat. */
json_t *w25_kanbannoti_2979d2_chat_threads(const json_t *req);

/* {"thread_id"?, "task_id", "platform", "chat_id", "subs": [...],
 *  "session": {"platform","chat_id","thread_id"}} -> <str|null>. */
json_t *w25_kanbannoti_2979d2_unsubscribe_thread(const json_t *req);

/* Same inputs plus "chat_type" -> {"thread_id": <str|null>, "notice": <str|null>}
 * where notice is the exact stderr text (no trailing newline). */
json_t *w25_kanbannoti_2979d2_subscribe_thread(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
