/*
 * noc_w26_telegramcl_ca0267.h - C surface for upstream
 * plugins/platforms/telegram/telegram_clarify_multi.py
 * (gate w26_telegramcl_ca0267).
 *
 * The multi-select clarify card as values: the toggle keyboard it renders, the
 * token -> action decision a ``t<idx>`` / ``go`` tap resolves to, the labels a
 * Send submits, and the acknowledgement body a resolved entry edits in.  The
 * Telegram client, the pending-clarify registry and the i18n lookup are seams;
 * the caller injects what they returned.
 */
#ifndef NOC_W26_TELEGRAMCL_CA0267_H
#define NOC_W26_TELEGRAMCL_CA0267_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Telegram caps callback_data at 64 bytes. */
#define W26_TELEGRAMCL_CA0267_CALLBACK_MAX 64
/* resolve_clarify_choice shows at most this many code points in the ack. */
#define W26_TELEGRAMCL_CA0267_ACK_MAX_CHARS 60

/* req: {"clarify_id": str, "count": int, "selected": [int]} ->
 * {"rows": [[{"text": ..., "callback_data": ...}, ...], ...]}. */
json_t *w26_telegramcl_ca0267_keyboard(const json_t *req);

/* req: {"entry": {"multi_select": bool, "choices": [...]}|null} -> bool:
 * is this pending entry a multi-select that has choices. */
json_t *w26_telegramcl_ca0267_pending_multi(const json_t *req);

/* req: {"token": str, "entry": {...}|null, "selected": [int]} ->
 * {"handled": bool, "expired": bool, "answers": [toast key, ...],
 *  "toggle": {"idx": int, "selected": [int]}|null,
 *  "resolve": {"resolved_text": str, "shown": str}|null}. */
json_t *w26_telegramcl_ca0267_token_action(const json_t *req);

/* req: {"clarify_id", "resolved_text", "shown": str|null, "resolved": bool,
 *       "message_text": str|null, "user": str} ->
 * {"answer": str|null, "html": str|null, "expired": bool,
 *  "logged": {"clarify_id","shown","user","resolved_text"}|null}. */
json_t *w26_telegramcl_ca0267_resolve_ack(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
