#ifndef NOC_W21_TELEGRAMTY_A78C8C_H
#define NOC_W21_TELEGRAMTY_A78C8C_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/telegram/telegram_typing_gate.py (gate
 * w21_telegramty_a78c8c).
 *
 * `typing_may_fire` is a per-chat admission decision: a typing call goes out
 * only when the chat is out of its cooldown, has no flood window pending, its
 * shared send+edit slot is free, the >= 2 s per-chat interval has elapsed, and
 * the shared draft/typing budget still has room. The adapter is a live object,
 * so the whole adapter surface is carried as one state object and handed back
 * mutated -- the same shape the live `adapter.__dict__` dicts hold:
 *
 *   state = {
 *     "cooldown_until": {"<chat>": float},   # _telegram_typing_cooldown_until
 *     "last_sent":      {"<chat>": float},   # _telegram_typing_last_sent
 *     "ai_calls":       {"<chat>": [float]}, # _telegram_ai_action_calls (deques)
 *     "flood_remaining": float|null,         # _send_flood_cooldown_remaining(chat_id)
 *     "slot_remaining":  float               # _chat_outbound_slot_remaining(chat_id)
 *   }
 *
 * `now` is `asyncio.get_running_loop().time()`: the clock is a seam, injected by
 * the caller (the oracle feeds the same value to the live module). `chat_id` is
 * the decoded value; pass it as a string or an integral number -- a JSON float
 * that is integral reads back as an int here, while Python distinguishes
 * ``123`` (int) from ``123.0`` (float) when stringifying it.
 *
 * Both functions answer {"result": bool, "state": {<the four dicts above>}},
 * with every expired entry already dropped exactly where the live code drops it.
 */

/* PoP: w21_telegramty_a78c8c_typing_in_cooldown @ plugins/platforms/telegram/telegram_typing_gate.py:_typing_in_cooldown
 * True while now < cooldown_until[str(chat_id)]; an expired entry is popped. */
json_t *w21_telegramty_a78c8c_typing_in_cooldown(const json_t *state,
                                                 const json_t *chat_id, double now);

/* PoP: w21_telegramty_a78c8c_typing_may_fire @ plugins/platforms/telegram/telegram_typing_gate.py:typing_may_fire
 * The admission decision, reserving the chat's typing tick when it says True. */
json_t *w21_telegramty_a78c8c_typing_may_fire(const json_t *state,
                                              const json_t *chat_id, double now);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_TELEGRAMTY_A78C8C_H */
