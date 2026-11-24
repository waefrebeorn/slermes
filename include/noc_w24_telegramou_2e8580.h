/*
 * noc_w24_telegramou_2e8580.h - C surface for upstream
 * plugins/platforms/telegram/telegram_outbound.py (gate w24_telegramou_2e8580).
 *
 * The send policy both Telegram lanes share: the visually-empty guard over
 * invisible code points, the push-notification mode resolution, the Bot API
 * kwargs and the metadata dict for one send.  The bot object, the adapter and
 * the process environment stay in the caller; every function here is a pure
 * function of the values handed to it.
 */
#ifndef W24_TELEGRAMOU_2E8580_H
#define W24_TELEGRAMOU_2E8580_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of code points str.translate deletes before strip(). */
#define W24_TELEGRAMOU_2E8580_INVISIBLE_COUNT 22

/* text -> bool: no visible glyph (None/falsy, empty, whitespace or invisible
 * code points only).  A truthy non-text value is the live AttributeError seam
 * and answers null. */
json_t *w24_telegramou_2e8580_is_visually_empty(const json_t *text);

/* (mode, notify) -> {} unless mode == "important" and notify is falsy, in
 * which case {"disable_notification": true}. */
json_t *w24_telegramou_2e8580_notification_kwargs(const json_t *mode, const json_t *notify);

/* (thread_id, notify) -> {"thread_id": str(thread_id)} when not None, plus
 * {"notify": true} when notify is truthy. */
json_t *w24_telegramou_2e8580_metadata(const json_t *thread_id, const json_t *notify);

/* (env, cfg) -> "all" | "important" from the HERMES_TELEGRAM_NOTIFICATIONS
 * value, else the config value, else "important"; unknown values answer
 * "important" as the live logger.warning path does. */
json_t *w24_telegramou_2e8580_resolve_mode(const json_t *env, const json_t *cfg);

/* _NOTIFICATION_MODES plus the deleted code points, as an object. */
json_t *w24_telegramou_2e8580_constants(void);

#ifdef __cplusplus
}
#endif
#endif
