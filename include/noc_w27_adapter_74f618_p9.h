/*
 * noc_w27_adapter_74f618_p9.h - C surface for upstream
 * plugins/platforms/slack/adapter.py (gate w27_adapter_74f618_p9).
 *
 * Gate 9: which workspace an event belongs to (never a foreign team), the
 * active-channel context shapes, assistant-thread metadata and the suggested
 * prompt rows.
 */
#ifndef NOC_W27_ADAPTER_74F618_P9_H
#define NOC_W27_ADAPTER_74F618_P9_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"event": obj, "body": obj|null, "installs": [str]|null} -> str
 *   (installs absent/null = no authenticated set) */
json_t *w27_p9_event_team_id(const json_t *req);

/* req: {"team_id": str?, "installs": [str]} -> str */
json_t *w27_p9_team_in_install_set(const json_t *req);

/* req: {"author_team": str?, "resolved_team": str?, "installs": [str]?} -> bool */
json_t *w27_p9_is_foreign_author(const json_t *req);

/* req: {"context": any} -> str */
json_t *w27_p9_context_channel_id(const json_t *req);

/* req: {"event": obj, "body": obj|null, "installs": [str]|null} -> obj */
json_t *w27_p9_extract_assistant_meta(const json_t *req);

/* req: {"event": obj, "body": obj|null, "installs": [str]|null} -> obj */
json_t *w27_p9_agent_view_event_fields(const json_t *req);

/* req: {"raw": any} -> [title, [{title, message}...]] */
json_t *w27_p9_suggested_prompts(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_74F618_P9_H */
