/*
 * noc_w27_adapter_74f618_p7.h - C surface for upstream
 * plugins/platforms/slack/adapter.py (gate w27_adapter_74f618_p7).
 *
 * Gate 7: thread_ts resolution, the reactions opt-in shapes, users.info
 * parsing, the first file-share pick and the identity prompt line.
 */
#ifndef NOC_W27_ADAPTER_74F618_P7_H
#define NOC_W27_ADAPTER_74F618_P7_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"reply_to": str|null, "metadata": obj|null, "reply_in_thread": bool?}
 *   -> str|null */
json_t *w27_p7_resolve_thread_ts(const json_t *req);

/* req: {"extra": obj|null, "env": str|null} -> bool */
json_t *w27_p7_reactions_enabled(const json_t *req);

/* req: {"extra": obj|null, "key": str, "default": bool?} -> bool */
json_t *w27_p7_extra_flag(const json_t *req);

/* req: {"payload": obj, "user_id": str} -> [display_name, is_bot] */
json_t *w27_p7_parse_users_info(const json_t *req);

/* req: {"raw": any} -> [str]|null (sorted when a set) */
json_t *w27_p7_reaction_triggers(const json_t *req);

/* req: {"raw": any} -> [channel, thread] */
json_t *w27_p7_reaction_trigger_target(const json_t *req);

/* req: {"file_obj": obj|null, "channel_id": str} -> obj */
json_t *w27_p7_first_file_share(const json_t *req);

/* req: {"event": obj, "action": str, "thread_ts": str, "team_id": str?,
 *       "target_channel": str?, "target_thread": str?} -> obj */
json_t *w27_p7_synthetic_reaction_event(const json_t *req);

/* req: {"name": str} -> str */
json_t *w27_p7_build_identity_prompt(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_74F618_P7_H */
