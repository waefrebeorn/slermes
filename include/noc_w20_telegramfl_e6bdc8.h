#ifndef NOC_W20_TELEGRAMFL_E6BDC8_H
#define NOC_W20_TELEGRAMFL_E6BDC8_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w20_telegramfl_e6bdc8_bot_id @ plugins/platforms/telegram/telegram_flood_cooldown.py:_bot_id
 * The numeric bot id prefix of `token` (before the first ':'), or null when the
 * head is not all digits. */
struct json_t *w20_telegramfl_e6bdc8_bot_id(const char *token);

/* PoP: w20_telegramfl_e6bdc8_store_name @ plugins/platforms/telegram/telegram_flood_cooldown.py:_store_path
 * The persisted-window file NAME for this token, or null when the adapter has
 * no receipt dir or no bot id (the live helper returns the joined Path). */
struct json_t *w20_telegramfl_e6bdc8_store_name(const char *token, int has_receipt_dir);

/* PoP: w20_telegramfl_e6bdc8_seed_windows @ plugins/platforms/telegram/telegram_flood_cooldown.py:_windows
 * The loop-clock deadline map a boot seeds from the persisted `{"until":{...}}`
 * document: entries that already expired, are not numbers, or exceed the
 * 300 s cap are dropped/clamped. `saved_text` is the raw file body (null = no
 * file); `preset` is the adapter's in-memory map; `loaded` its load-once flag.
 * A token with no numeric bot id (or an adapter with no receipt dir) has no
 * store path, so nothing is ever loaded. */
struct json_t *w20_telegramfl_e6bdc8_seed_windows(const char *token, int has_receipt_dir,
                                                  const char *saved_text,
                                                  const struct json_t *preset,
                                                  int loaded, double wall_now, double loop_now);

/* PoP: w20_telegramfl_e6bdc8_record @ plugins/platforms/telegram/telegram_flood_cooldown.py:_record_send_flood_cooldown
 * A 429 `retry_after=wait` arms the chat's window: `{"key","deadline","error",
 * "retry_after","success","until"}` where error is the fail-closed
 * `flood_control:<wait>` SendResult text. */
struct json_t *w20_telegramfl_e6bdc8_record(const struct json_t *chat, double wait,
                                            double loop_now, const struct json_t *preset);

/* PoP: w20_telegramfl_e6bdc8_remaining @ plugins/platforms/telegram/telegram_flood_cooldown.py:_send_flood_cooldown_remaining
 * `{"remaining": seconds|null, "until": map}` -- an expired (or absent) window
 * is dropped and reads null. */
struct json_t *w20_telegramfl_e6bdc8_remaining(const struct json_t *until, const struct json_t *chat,
                                               double loop_now);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W20_TELEGRAMFL_E6BDC8_H */
