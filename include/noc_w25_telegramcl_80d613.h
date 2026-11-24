/*
 * noc_w25_telegramcl_80d613.h - C surface for upstream
 * plugins/platforms/telegram/telegram_clarify_binding.py
 * (gate w25_telegramcl_80d613).
 *
 * The clarify-button binding decision: normalize a Telegram id, build the
 * pending prompt identity from a Bot API send result, decide whether a
 * callback tap came from the message the prompt was rendered on (fail closed,
 * answering the denial on a mismatch), and format the resolved audit line.
 * The Bot API objects and the logger are seams.
 */
#ifndef NOC_W25_TELEGRAMCL_80D613_H
#define NOC_W25_TELEGRAMCL_80D613_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"value": <any>} -> str(value), or null for None. */
json_t *w25_telegramcl_80d613_ident(const json_t *req);

/* {"session_key": <str>, "msg": {"chat_id","message_id","message_thread_id"}}
 * -> {"session_key","chat_id","message_id","thread_id"} (all ids normalized). */
json_t *w25_telegramcl_80d613_from_sent(const json_t *req);

/* {"prompt": {...}, "query": {"id","from_id","message":{...}}, "denial": <str>}
 * -> {"matched": <bool>, "answered": <bool>, "denial": <str|null>}. */
json_t *w25_telegramcl_80d613_tap_matches(const json_t *req);

/* {"clarify_id","choice","caller_id","query_id","chat","message"} -> the
 * "Telegram clarify button resolved (...)" audit line. */
json_t *w25_telegramcl_80d613_audit_line(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
