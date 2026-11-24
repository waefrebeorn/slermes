#ifndef NOC_W19_TELEGRAMME_82FDEB_H
#define NOC_W19_TELEGRAMME_82FDEB_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_telegramme_82fdeb_flood_wait_seconds @ plugins/platforms/telegram/telegram_media_flood.py:flood_wait_seconds
 * Seconds Telegram asked this call to wait, or null for any other error. `has_retry_after`
 * is 0 when the exception carries no `retry_after` (getattr -> None); a `retry_after`
 * object with a numeric `total_seconds` is a timedelta and is unwrapped first; otherwise
 * the message text is scanned for "retry in|after <n>". */
struct json_t *w19_telegramme_82fdeb_flood_wait_seconds(int has_retry_after,
                                                        struct json_t *retry_after,
                                                        const char *text);

/* PoP: w19_telegramme_82fdeb_known_window_message @ plugins/platforms/telegram/telegram_media_flood.py:_KnownFloodWindow
 * The exception text for a request refused locally because the chat's flood window is open. */
const char *w19_telegramme_82fdeb_known_window_message(double wait);

/* PoP: w19_telegramme_82fdeb_recover_action @ plugins/platforms/telegram/telegram_media_flood.py:TelegramMediaFloodMixin._media_flood_recover
 * Which branch of flood recovery a refused media send takes:
 * {"action":"none"|"sleep_none"|"defer"|"inline_retry","cooldown":bool,"sleep":seconds}. */
struct json_t *w19_telegramme_82fdeb_recover_action(int wait_present, double wait, int stage,
                                                    int known_window, double inline_cap);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_TELEGRAMME_82FDEB_H */
