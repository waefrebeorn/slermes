#ifndef NOC_W22_TELEGRAMRE_8BC0FE_H
#define NOC_W22_TELEGRAMRE_8BC0FE_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/telegram/telegram_resolved_prompt.py
 * (gate w22_telegramre_8bc0fe).
 *
 * `edit_keeping_prompt` is an async adapter call: it reads the prompt's own
 * rendering (`query.message.text_html`), drops the "answer within N minutes"
 * deadline line the adapter renders, appends the decision and edits the
 * message. The live Telegram client, the query object and the adapter are the
 * transport seam; what the function *decides* is pure and ported here:
 *
 *   - escape:      `html.escape(decision)` exactly as it is appended.
 *   - strip:       `body.replace(adapter._ea_deadline_line(), "")` -- every
 *                  occurrence, and an empty deadline line is a no-op.
 *   - plan:        which edit path the function takes (append + HTML edit,
 *                  the decision-only quiet edit after `BadRequest`, the plain
 *                  return after `NetworkError`, an unexpected raise), together
 *                  with the exact text handed to `edit_message_text`.
 *
 * The composed text is `f"{body}\n\n\xe2\x80\x94 {html.escape(decision)}"` (UTF-8 em dash
 * and space), and `edit_message_text` is called with parse_mode="HTML" and
 * reply_markup=None.
 */

/* PoP: w22_telegramre_8bc0fe_escape @ plugins/platforms/telegram/telegram_resolved_prompt.py:edit_keeping_prompt
 * `html.escape(text)` with quote=True: & < > " ' become entities. */
json_t *w22_telegramre_8bc0fe_escape(const char *text);

/* PoP: w22_telegramre_8bc0fe_strip @ plugins/platforms/telegram/telegram_resolved_prompt.py:edit_keeping_prompt
 * `body.replace(deadline, "")`: every non-overlapping occurrence, left to
 * right; an empty (or absent) deadline line leaves the body unchanged. */
json_t *w22_telegramre_8bc0fe_strip(const char *body, const char *deadline);

/* PoP: w22_telegramre_8bc0fe_plan @ plugins/platforms/telegram/telegram_resolved_prompt.py:edit_keeping_prompt
 * body: the raw `text_html` value (any JSON type; a non-string or an empty
 * string means "no readable body"). error: "" for a clean edit, "BadRequest",
 * "NetworkError", or any other exception name.
 *
 * -> {"path": "quiet_no_body" | "edit" | "quiet_after_badrequest" |
 *            "return_after_network" | "raise",
 *     "text": <text passed to edit_message_text> | null,
 *     "parse_mode": "HTML" | null,
 *     "quiet_decision": <decision passed to _edit_md_quiet> | null}
 */
json_t *w22_telegramre_8bc0fe_plan(const json_t *body, const char *deadline,
                                   const char *decision, const char *error);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_TELEGRAMRE_8BC0FE_H */
