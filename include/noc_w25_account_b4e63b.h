/*
 * noc_w25_account_b4e63b.h - C surface for upstream tools/connectors/account.py
 * (gate w25_account_b4e63b).
 *
 * The value-only core of the account connect flow: the reconciliation rule that
 * reuses one already-open operation for every requested connector (and refuses
 * when the request spans two), and the Target/session plan a fresh operation is
 * opened with.  The thread, the context copy, the live registry
 * (``live.find_target`` / ``live.open``) and ``threading.Event.wait`` are seams:
 * the open-operation lookup is a fixed input per connector name.
 */
#ifndef NOC_W25_ACCOUNT_B4E63B_H
#define NOC_W25_ACCOUNT_B4E63B_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* How long wait_for_prepare waits on the operation's done event. */
#define W25_ACCOUNT_B4E63B_PREPARE_WAIT_SECONDS 31.0
/* The operation's session key is this prefix + uuid4().hex. */
#define W25_ACCOUNT_B4E63B_SESSION_KEY_PREFIX "account:"
/* Targets an account operation opens are all of this contract kind. */
#define W25_ACCOUNT_B4E63B_TARGET_KIND "connector"

/* {"names": [str, ...], "alias": <str|null>, "repair_id": <str|null>,
 *  "profile_home": <str|null>,
 *  "open": {<name>: <op id>|null, ...}}   -- live.find_target's answer per name
 * -> {"found": <op id>|null}
 * or {"error": "ValueError",
 *     "message": "requested connectors do not share one open operation"}
 *
 * The first non-None match is the operation; every other requested name must
 * resolve to that same operation (a name that resolves to nothing at all is a
 * mismatch too, exactly as the live ``is not`` walk treats it). */
json_t *w25_account_b4e63b_matching_open_operation(const json_t *req);

/* {"names": [str, ...], "action": str, "alias": <str|null>,
 *  "repair_id": <str|null>} ->
 * {"targets": [{"name","kind","action","alias","repair_id"}, ...],
 *  "session_key_prefix": "account:"}   -- ConnectionOperation(session_key=
 *  f"account:{uuid4().hex}") built from Target(name, "connector", action, ...) */
json_t *w25_account_b4e63b_plan(const json_t *req);

/* _PREPARE_WAIT_SECONDS, the thread name, the session-key prefix, the target
 * kind and the uuid hex width, as an object. */
json_t *w25_account_b4e63b_constants(void);

#ifdef __cplusplus
}
#endif
#endif
