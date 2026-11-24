/*
 * noc_w25_callbackan_bbb650.h - C surface for upstream
 * plugins/platforms/telegram/callback_answer.py (gate w25_callbackan_bbb650).
 *
 * Callback-query answers that survive a tap on an expired Telegram card.  What
 * is ported is the value-only decision: which ``answerCallbackQuery`` call a
 * given (text, show_alert) becomes, whether a raised error is the "query
 * expired" refusal (matched on its wording, narrowed to PTB's BadRequest when
 * the library is importable), and whether a refused tap falls back to a normal
 * chat note in the card's forum topic.  Awaiting the query and the adapter's
 * send stay a seam: the raised error and the send's outcome are fixed inputs.
 */
#ifndef NOC_W25_CALLBACKAN_BBB650_H
#define NOC_W25_CALLBACKAN_BBB650_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Telegram truncates answerCallbackQuery text at 200 chars. */
#define W25_CALLBACKAN_BBB650_TOAST_LIMIT 200
#define W25_CALLBACKAN_BBB650_STALE_PHRASE_COUNT 3

/* {"message": str, "exc_type": str, "badrequest_importable": bool} ->
 * true when the error is Telegram refusing an answer because the query expired.
 * The wording is matched first; when PTB is importable the error must also be a
 * BadRequest (str(exc).lower() must carry no capitalised phrase). */
json_t *w25_callbackan_bbb650_is_stale_query_error(const json_t *req);

/* {"text": <str|null>, "show_alert": bool,
 *  "fault": {"message": str, "exc_type": str, "badrequest_importable": bool}|null}
 * -> {"calls": [{"text": ...}|{"text": ..., "show_alert": true}],
 *     "result": true|false|null, "raised": bool}
 * text null -> query.answer(); show_alert -> answer(text=..., show_alert=True);
 * a stale fault -> result false, any other fault -> raised. */
json_t *w25_callbackan_bbb650_answer_callback(const json_t *req);

/* {"text": str, "chat_id": <any|null>, "message_thread_id": <any|null>,
 *  "is_topic_message": <any>, "fault": <as above>|null, "send_fault": bool}
 * -> {"calls": [...], "sent": null|{"chat_id": str, "text": str,
 *     "parse_mode": null, "thread_id": str|null, "metadata": null},
 *     "raised": bool}
 * A refused (expired) answer is replaced by a normal message via the adapter's
 * topic-aware control send, in the card's topic when it is a topic message. */
json_t *w25_callbackan_bbb650_answer_or_note(const json_t *req);

/* {"requester": <str|null>, "refusal": str} -> {"calls": [...], "result": bool}
 * ``refusal`` is the localized "only X can answer" text (gateway.approval_actor
 * is a seam); it is sliced to TOAST_LIMIT and sent as an alert.  No requester
 * means the tap was not refused, so nothing is answered. */
json_t *w25_callbackan_bbb650_refuse_tap(const json_t *req);

/* TOAST_LIMIT and the stale-query wording, as an object. */
json_t *w25_callbackan_bbb650_constants(void);

#ifdef __cplusplus
}
#endif
#endif
