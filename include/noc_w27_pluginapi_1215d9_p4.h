/*
 * noc_w27_pluginapi_1215d9_p4.h - C11 surface for the board / profile /
 * orchestration projections of upstream plugins/kanban/dashboard/plugin_api.py
 * (gate w27_pluginapi_1215d9_p4, source lines 1301-1845).
 *
 * Boards CRUD, the profile roster and the orchestration knobs are all "read a
 * couple of seams, then shape a payload" handlers; the WebSocket cursor
 * helpers are pure parsing.  The filesystem stat, projects DB, profiles
 * registry and board list stay seams.  Every entry point takes a decoded
 * request object and returns the common envelope (see
 * noc_w27_pluginapi_1215d9_p1.h).
 */
#ifndef NOC_W27_PLUGINAPI_1215D9_P4_H
#define NOC_W27_PLUGINAPI_1215D9_P4_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The two orchestration settings that name a profile (validated as such). */
#define W27_P4_PROFILE_SETTING_COUNT 2

/* req {payload} -> the four display-metadata fields a board write takes. */
json_t *w27p4_board_display_kwargs(const json_t *req);

/* req {ref, projects, connect_error} -> [id, name, primary_path]; 400 on unknown. */
json_t *w27p4_resolve_project(const json_t *req);

/* req {board, git} -> "scratch" | "worktree" | "dir". */
json_t *w27p4_default_workspace_kind(const json_t *req);

/* req {meta, workspace_kind, project} -> meta plus the two derived keys. */
json_t *w27p4_annotate_board_meta(const json_t *req);

/* req {raw, mod, exists, exists_raises} -> stripped profile name; 400 if unknown. */
json_t *w27p4_validated_profile_name(const json_t *req);

/* req {config, active, exists, active_raises} -> the 8-key orchestration view. */
json_t *w27p4_orchestration_settings(const json_t *req);

/* req {query} -> the WS event cursor (0 allowed), or null when absent/garbage. */
json_t *w27p4_since_param(const json_t *req);

/* req {raw} -> normalised board slug for the WS handshake, or null. */
json_t *w27p4_ws_board(const json_t *req);

/* req {slug, boards} -> slug only when it names a live board. */
json_t *w27p4_ws_board_live(const json_t *req);

/* req {boards, current, projects, counts, kinds} -> annotated board list. */
json_t *w27p4_list_boards_annotate(const json_t *req);

/* req {profiles} -> the profile roster payload. */
json_t *w27p4_profile_roster(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
