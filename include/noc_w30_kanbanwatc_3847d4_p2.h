/*
 * noc_w30_kanbanwatc_3847d4_p2.h - C surface for upstream
 * gateway/kanban_watchers_notifier.py (gate w30_kanbanwatc_3847d4_p2).
 *
 * Pure "which subscriptions this gateway serves, what the wake says, and what
 * happens to a subscription after a send" core: the collector's profile /
 * platform pre-filter, ``_platform_names``, ``_adapter_for_subscription``
 * (route authorization), ``_chat_type_of``, ``_wake_scope_id``, the two
 * warn-once dedupe decisions, ``build_wake_text`` + the diagnostic/non-diagnostic
 * wake split, the send-failure counter step, and the post-delivery tail.
 *
 * Seams: the DB claim/cursor writes, the adapter registry and its
 * ``scope_id_for_chat``, ``profile_routes[].matches()`` (the caller passes each
 * route's two match answers), ``_multiplex_profile_homes`` (served list),
 * ``session_owned_by_profile`` (one boolean), the logger, asyncio, and the
 * session store.
 */
#ifndef NOC_W30_KANBANWATC_3847D4_P2_H
#define NOC_W30_KANBANWATC_3847D4_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"mapping": [<str> | {"value": <str>}, ...]} -> sorted lower-cased [<str>...]. */
json_t *w30_kanbanwatc_3847d4_p2_platform_names(const json_t *req);

/* {"notifier_profile": <str|null>, "profile_adapters": {<name>: [<platform>...]},
 *  "adapters": [...], "multiplex": <bool>,
 *  "routes": [{"profile", "enabled", "platform"}]} ->
 * {"notifier_profiles": [...], "active_platforms": [...]}. */
json_t *w30_kanbanwatc_3847d4_p2_collect_scope(const json_t *req);

/* {"sub": {...}, "metadata": {...}|null} -> <str>. */
json_t *w30_kanbanwatc_3847d4_p2_chat_type_of(const json_t *req);

/* {"sub": {...}, "resolver": {"present": <bool>, "value": <any>}|null} -> <str|null>. */
json_t *w30_kanbanwatc_3847d4_p2_wake_scope_id(const json_t *req);

/* {"sub": {...}, "platform": <str>, "warned": [[...], ...]} ->
 * {"warn": <bool>, "key": [...]|null}. */
json_t *w30_kanbanwatc_3847d4_p2_anchorless_warn(const json_t *req);

/* {"sub": {...}, "platform": <str>, "warned": [[...], ...]} -> {"warn", "key"}. */
json_t *w30_kanbanwatc_3847d4_p2_unroutable_warn(const json_t *req);

/* {"platform": <str>, "owner_profile": <str|null>, "notifier_profile": <str|null>,
 *  "primary_profile": <str>, "multiplex": <bool>,
 *  "adapter": "absent"|"primary"|"other", "has_primary": <bool>,
 *  "own_platforms": [...], "served": [...],
 *  "routes": [{"profile", "plain": <bool>, "relaxed": <bool>}],
 *  "sub": {...}, "api_server_owned": <bool>} ->
 * {"adapter": "absent"|"primary"|"other",
 *  "warn": {"key": [...], "profile": <str>, "route_profile": <str>}|null}. */
json_t *w30_kanbanwatc_3847d4_p2_resolve_adapter(const json_t *req);

/* {"events": [...], "sub": {...}, "task": {...}|null, "board": <str|null>,
 *  "split": <bool>, "is_push_adapter": <bool>, "multiplex": <bool>} ->
 * {"groups": [{"synth", "diagnostic", "kinds": [...]}...],
 *  "synth": <str>, "session_key": <str>, "wake_kinds": [...],
 *  "wake_diagnostic": <bool|null>, "served_profile": <str|null>}. */
json_t *w30_kanbanwatc_3847d4_p2_wake_plan(const json_t *req);

/* {"count": <int>} -> {"fails": <int>, "action": "unsub"|"rewind",
 *  "count_after": <int|null>}. */
json_t *w30_kanbanwatc_3847d4_p2_failure_step(const json_t *req);

/* {"task": {"status": <str|null>}|null, "is_push": <bool>} ->
 * {"advance": <bool>, "clear_failures": <bool>, "unsub": <bool>}. */
json_t *w30_kanbanwatc_3847d4_p2_after_delivery(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
