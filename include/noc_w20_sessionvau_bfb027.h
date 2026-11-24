/*
 * noc_w20_sessionvau_bfb027.h -- public C API for the port of upstream Hermes
 *   gateway/session_vault_code_card.py (gate w20_sessionvau_bfb027).
 *
 * Ported: the _CodeRequest settle-once state machine (submit / close / cancel /
 * wait), the adapter detection of _code_card_adapter, the author-id derivation
 * `str(getattr(getattr(ctx, "source", None), "user_id", "") or "")`, the
 * install-or-passthrough decision of code_card_scope, and the 1 s wait slice.
 * The contextmanager body (unlock.set_code_prompt_callback, gateway.run imports,
 * turn._schedule) needs a live turn and stays upstream.
 */
#ifndef NOC_W20_SESSIONVAU_BFB027_H
#define NOC_W20_SESSIONVAU_BFB027_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Replay a sequence of _CodeRequest operations. `ops` is an array of
 *   {"op":"submit","code":" 123456 "} | {"op":"close"} | {"op":"cancel"}
 *   | {"op":"wait","timeout":0.0,"cancelled":false} | {"op":"state"}
 * Returns one record per op:
 *   submit -> {"kind":"submit","accepted":bool}
 *   close/cancel -> {"kind":"void"}
 *   wait -> {"kind":"wait","code":"<settled value>","settled":bool}
 *   state -> {"kind":"state","settled":bool,"value":"<pending value>"} */
json_t *w20_sessionvau_bfb027_code_request(const json_t *ops);

/* `str(getattr(getattr(ctx, "source", None), "user_id", "") or "")` for the
 * scalars a source record can carry. `source` is the source object or NULL. */
char *w20_sessionvau_bfb027_user_id(const json_t *source);   /* caller frees */

/* `_code_card_adapter(turn)` + the scope's install decision. `turn` is
 * {"has_adapter":bool,"adapter_kind":"with"|"none"|"without","source":{...}}.
 * Returns {"adapter": bool, "user_id": "...", "gate": "install"|"passthrough"}. */
json_t *w20_sessionvau_bfb027_scope_gate(const json_t *turn);

/* the heartbeat slice of _CodeRequest.wait: 1 s, floored at 0, capped at 1. */
double w20_sessionvau_bfb027_wait_slice(double remaining);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W20_SESSIONVAU_BFB027_H */
