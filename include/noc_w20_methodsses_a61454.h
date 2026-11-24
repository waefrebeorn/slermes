#ifndef NOC_W20_METHODSSES_A61454_H
#define NOC_W20_METHODSSES_A61454_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of tui_gateway/methods_session_seed.py (gate w20_methodsses_a61454).
 *
 * The module persists a seeded session row the moment session.create returns
 * (the one exception to lazy rows) and compensates when the transcript or title
 * write fails afterwards. Its bodies are rebound onto server globals at install
 * time, so the only way to test them is the shape of their inputs and outputs:
 * this port takes the same arguments plus the handful of seams those bodies
 * read (the persistence handle, the message identity mapper) and answers what
 * the live function did to them.
 *
 * What stays upstream: _session_db(), _persistence_for()'s real handle,
 * _branch_title(), _is_remote_launch_cwd(), _current_profile_name(),
 * _session_default_route(), _session_auth_user_id() and the NewMessage builder
 * -- each needs the live room/session config, not just a mapping.
 */

/* PoP: w20_methodsses_a61454_branch_rows @ tui_gateway/methods_session_seed.py:_persist_branch
 * The transcript rows a branch child copies:
 *   {"role": msg.get("role", "user"), "content": msg.get("content"),
 *    **{f: msg.get(f) for f in copy_fields}, **message_identity(msg)}
 * ``identities`` is the seam for message_identity(msg), one entry per history
 * message (NULL entry reads as {}); later sources win, as the dict unpacking
 * does. Answer is the array of rows. */
json_t *w20_methodsses_a61454_branch_rows(const json_t *history, const json_t *copy_fields,
                                          const json_t *identities);

/* PoP: w20_methodsses_a61454_branch_persist @ tui_gateway/methods_session_seed.py:_persist_branch
 * The whole seeded-branch write against a scripted persistence handle.
 *   spec = {"new_key", "parent_key", "source", "cwd", "profile_name", "model",
 *           "user_id", "title", "title_source" ("user"|"derived"|"llm"),
 *           "copy_fields": [...], "history": [...], "identities": [...],
 *           "annotations": [ {field: value} per row ],
 *           "parent": "missing" | {"system_prompt": ...} | {"error": true},
 *           "fail": "none" | "append" | "title", "disk_full": bool,
 *           "compensate": bool}
 * Answer:
 *   {"session": {the NewSession fields this call fills}, "rows": [...after the
 *    outcome annotations...], "title": {"kind": ...} | null, "parent_read_failed":
 *    bool, "compensated": bool, "raised": <exception text> | null}
 * `raised` is the text the re-raise carries; the compensation delete is only
 * attempted for a non-disk-full failure, and a failing delete never masks it. */
json_t *w20_methodsses_a61454_branch_persist(const json_t *spec);

/* PoP: w20_methodsses_a61454_seed_title_source @ tui_gateway/methods_session_seed.py:_persist_branch
 * Whether the title write is `set_title` (title_source == "user") or
 * `set_auto_title(title, source=TitleSource(title_source))`. */
int w20_methodsses_a61454_seed_title_source(const char *title_source);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_METHODSSES_A61454_H */
