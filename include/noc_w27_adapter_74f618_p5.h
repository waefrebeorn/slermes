/*
 * noc_w27_adapter_74f618_p5.h - C surface for upstream
 * plugins/platforms/slack/adapter.py (gate w27_adapter_74f618_p5).
 *
 * Gate 5: the bounded tracking structures' eviction math (chronological ts
 * sort keys, oldest-first discard of sets and dicts) and the workspace-id
 * readers every thread key is built from. The containers themselves are
 * seams; what is ported is which entries survive.
 */
#ifndef NOC_W27_ADAPTER_74F618_P5_H
#define NOC_W27_ADAPTER_74F618_P5_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"ts": str} | {"pair": [team, ts]} -> [seconds, micros, str] */
json_t *w27_p5_timestamp_sort_key(const json_t *req);

/* req: {"entries": [str], "count": int, "mode": "identity"|"suffix"} ->
 *      {"removed": [...], "remaining": [...]}  (both sorted chronologically) */
json_t *w27_p5_discard_oldest(const json_t *req);

/* req: {"entries": [str], "cap": int, "mode": opt} -> [remaining sorted] */
json_t *w27_p5_evict_oldest(const json_t *req);

/* req: {"mapping": obj, "max_size": int} -> obj (oldest keys dropped) */
json_t *w27_p5_trim_dict_entries(const json_t *req);

/* req: {"metadata": obj|null} -> str */
json_t *w27_p5_metadata_team_id(const json_t *req);

/* req: {"team_id": str?, "event_id": str?} -> str */
json_t *w27_p5_workspace_event_id(const json_t *req);

/* req: {"team_id": str?, "message_id": str?} -> str|[team, id] */
json_t *w27_p5_workspace_message_marker(const json_t *req);

/* req: {"value": any} -> bool */
json_t *w27_p5_truthy_config(const json_t *req);

/* req: {"channel_id": str?, "ignored": [str]} -> bool */
json_t *w27_p5_is_ignored_channel(const json_t *req);

/* req: {"team_id": str?, "channel_id": str?, "thread_ts": str?} -> [..]|null */
json_t *w27_p5_workspace_thread_key(const json_t *req);

/* req: {"team_id": str?, "user_id": str?} -> [..]|null */
json_t *w27_p5_agent_view_context_key(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_74F618_P5_H */
