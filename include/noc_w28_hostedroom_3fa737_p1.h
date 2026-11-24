/*
 * noc_w28_hostedroom_3fa737_p1.h - C surface for the value-shaping layer of
 * upstream Hermes tui_gateway/hosted_room_driver.py (gate
 * w28_hostedroom_3fa737_p1).
 *
 * What a hosted room turn is called, which session it resumes, how much text a
 * terminal receipt may carry, and which receipt a history row proves.  Every
 * store/transport call stays with the caller; these functions are pure over the
 * values they are handed.
 */
#ifndef NOC_W28_HOSTEDROOM_3FA737_P1_H
#define NOC_W28_HOSTEDROOM_3FA737_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* room_session_title: "Group: <room_id>" -- the hidden room transcript title. */
json_t *w28_hostedroom_3fa737_p1_room_session_title(const char *room_id);

/* _member_id: payload.target_member_id, else payload.target_profile, else "". */
json_t *w28_hostedroom_3fa737_p1_member_id(const json_t *task);

/* _session_id: {"ok": bool, "value": str|null, "error": str|null}. */
json_t *w28_hostedroom_3fa737_p1_session_id(const json_t *session);

/* _truncate_utf8: {"text": str, "truncated": bool} cut to max_bytes, notice included. */
json_t *w28_hostedroom_3fa737_p1_truncate_utf8(const char *text, long long max_bytes);

/* _bounded_terminal_result: {message_id, text[, error][, truncated]} with the
 * 64 KiB text bound and the 4 KiB error bound. */
json_t *w28_hostedroom_3fa737_p1_bounded_result(const json_t *receipt);

/* _receipt_from: {status, settlement_id, result} with the settled/failed
 * coercion and the "reply:<task>:<gen>" settlement fallback. */
json_t *w28_hostedroom_3fa737_p1_receipt_from(const json_t *receipt, const char *task_id,
                                              long long generation);

/* discarded_receipt: the lost attempt's receipt when the Discard committed a
 * retained answer, else NULL. */
json_t *w28_hostedroom_3fa737_p1_discarded_receipt(const json_t *result, const json_t *task);

/* _find_terminal_receipt: newest assistant history row of this exact
 * (task_id, generation) whose status is settled/failed, as a receipt, or NULL. */
json_t *w28_hostedroom_3fa737_p1_find_terminal_receipt(const json_t *history, const char *task_id,
                                                       long long generation);

/* _session_kw: {profile, session_id, source: "bot_room"}. */
json_t *w28_hostedroom_3fa737_p1_session_kw(const char *profile, const char *session_id);

/* _fences: {expected_execution_generation, expected_cancel_generation}. */
json_t *w28_hostedroom_3fa737_p1_fences(const json_t *task);

#ifdef __cplusplus
}
#endif
#endif
