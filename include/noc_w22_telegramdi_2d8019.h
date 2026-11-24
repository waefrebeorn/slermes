#ifndef NOC_W22_TELEGRAMDI_2D8019_H
#define NOC_W22_TELEGRAMDI_2D8019_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/telegram/telegram_dice.py (gate w22_telegramdi_2d8019).
 *
 * An inbound dice message carries no text and no media, so without its own
 * handler PTB matched no filter and the throw vanished. The adapter is a live
 * PTB object (a network seam) and every `_`-method below is one of its hooks;
 * the oracle runs the real coroutine against a recording adapter and this port
 * reproduces the same call transcript and event.
 *
 * Fixture contract (`in` keys):
 *   msg          the object `_effective_update_message` returns; null/absent
 *                (or any other falsy scalar) means no message. An object is
 *                always truthy -- it is an adapter message, not a dict.
 *   dice         the `dice` attribute the message exposes; null/absent = None.
 *   authorized   `_is_user_authorized_from_message(msg)` result (truthiness).
 *   proceed      `_should_process_message(msg)` result (truthiness).
 *   observe      `_should_observe_unmentioned_group_message(msg)` (truthiness).
 *   update_id    `update.update_id`; required once the event is built.
 *
 * Result:
 *   {"calls": [...], "event": <event>|null}
 *   {"raised": "AttributeError"}            -- dice without emoji/value, or no
 *                                              update_id, exactly where the live
 *                                              attribute access raises.
 *   calls is the adapter transcript in order:
 *     ["log_blocked_user"]                                  unauthorized
 *     []                                                    dice None / neither
 *                                                           processed nor observed
 *     ["build_message_event", "observe"]                    observed chatter
 *     ["build_message_event", "apply_attribution",
 *      "handle_message"]                                    handled turn
 *   event is {"text":..., "kind":"TEXT", "update_id":..., "attributed":bool}
 *   with text = "[The user sent a Telegram dice: {emoji} rolled {value}.]".
 *   `attributed` is true only on the handled path (the observed path passes the
 *   raw event to _observe_unmentioned_group_message). */
json_t *w22_telegramdi_2d8019_handle(const json_t *in);

/* PoP: w22_telegramdi_2d8019_event_text @ plugins/platforms/telegram/telegram_dice.py:handle_dice_message
 * The rendered `event.text` for one roll:
 * "[The user sent a Telegram dice: {emoji} rolled {value}.]".
 * emoji/value go through str(:) -- null -> "None", true -> "True", 6 -> "6". */
json_t *w22_telegramdi_2d8019_event_text(const json_t *emoji, const json_t *value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_TELEGRAMDI_2D8019_H */
