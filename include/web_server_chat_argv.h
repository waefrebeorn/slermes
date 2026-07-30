/*
 * web_server_chat_argv.h — chat PTY argv/env assembly + session descendant
 * resolution (faithful C11 port of _resolve_chat_argv, _resolve_profile_dir,
 * and _session_latest_descendant from hermes_cli/web_server.py).
 */
#ifndef WEB_SERVER_CHAT_ARGV_H
#define WEB_SERVER_CHAT_ARGV_H

#include <stdbool.h>

#include "libjson/json.h"

/* _resolve_profile_dir: validate + resolve profile name.
 * Returns malloc'd dir path on success; NULL on failure with *status
 * (400 invalid name / 404 missing) and malloc'd *detail set. */
char *ws_chat_resolve_profile_dir(const char *name, int *status, char **detail);

/* _session_latest_descendant against a sessions sqlite DB at db_path.
 * Returns {"latest": id|null, "path": [ids...]} (caller json_free). */
json_t *ws_chat_session_latest_descendant(const char *db_path,
                                          const char *session_id);

/* resolve_session_id (hermes_state.py): exact id, else unique-prefix.
 * malloc'd full id or NULL. */
char *ws_chat_resolve_session_id(const char *db_path, const char *prefix);

/* _resolve_chat_argv env assembly (pure part): given the base env as a JSON
 * object {NAME: value}, apply the dashboard-chat env contract in Python
 * order. Mutates/returns a NEW env object. Inputs mirror the kwargs:
 * resume (already descendant-resolved), sidecar_url, profile_dir (NULL for
 * unscoped), active_session_file, gateway_ws_url (NULL to skip attach).
 * Flags say whether setdefault keys already existed. */
json_t *ws_chat_build_env(const json_t *base_env, const char *resume,
                          const char *sidecar_url, const char *profile_dir,
                          const char *active_session_file,
                          const char *gateway_ws_url);

#endif /* WEB_SERVER_CHAT_ARGV_H */
