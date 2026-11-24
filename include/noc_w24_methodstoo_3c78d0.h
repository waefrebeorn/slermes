/*
 * noc_w24_methodstoo_3c78d0.h - C surface for upstream
 * tui_gateway/methods_tools_plugin_commands.py (gate w24_methodstoo_3c78d0).
 *
 * The plugin slash-command RPC path as pure session-shaping decisions: the
 * HERMES_SESSION_* binding key/cwd a plugin handler reads, the context dict
 * handed to invoke_plugin_command, and the profile-scope ladder that decides
 * whether a dispatch runs, returns None, or re-raises.  The plugin registry,
 * the secret scope, the clarify asker and the handler call itself are seams:
 * each is a flag or a fixed answer here.
 */
#ifndef W24_METHODSTOO_3C78D0_H
#define W24_METHODSTOO_3C78D0_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"session": <any>, "arg": str, "platform": str} ->
 * {"key": str, "cwd": str,
 *  "context": {"source": null, "session_key": str, "platform": str,
 *              "session_id": str},
 *  "result": str}
 *
 * ``key`` is str(session["session_key"] or "") for a truthy session else "",
 * ``cwd`` is str(session["cwd"] or "") the same way, and ``session_id`` is
 * getattr(session["agent"], "session_id", None) or key -- which for every
 * json-representable agent is key.  ``result`` is what the (seamed) handler
 * call plus resolve_plugin_command_result return. */
json_t *w24_methodstoo_3c78d0_run_command(const json_t *args);

/* {"name": str} -> null: the registry lookup raises in every unbound or
 * unregistered context, which the caller turns into "no handler". */
json_t *w24_methodstoo_3c78d0_handler_lookup(const json_t *args);

/* {"name","arg","session","platform","absent","bind_fails","registry",
 *  "handler_present"}
 * -> {"action": "none"|"raise"|"run", "result": <str|null>}
 *
 * The _dispatch_plugin_in_profile_scope ladder: an absent command returns None
 * before the scope; a failing bind returns None for a registry command and
 * re-raises otherwise; a missing handler returns None; else the command runs. */
json_t *w24_methodstoo_3c78d0_dispatch(const json_t *args);

#ifdef __cplusplus
}
#endif
#endif
