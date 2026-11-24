/*
 * noc_w21_telegramme_52d000.h -- public C API for the port of
 *   plugins/platforms/telegram/telegram_media_budget.py (wave 21 batch 5).
 *
 * Size-scaled budgets for one Telegram media upload. The on-disk sizes of the
 * payloads arrive already resolved (the live `_on_disk_size` does os.stat /
 * os.fstat, which is the filesystem seam); everything the module decides on top
 * of them is here: which keyword values are media, the seconds the transfer
 * takes at MIN_MEDIA_UPLOAD_RATE, the widened read/write timeouts and the
 * 1.5x wall-clock deadline capped at MEDIA_UPLOAD_DEADLINE_MAX.
 */
#ifndef NOC_W21_TELEGRAMME_52D000_H
#define NOC_W21_TELEGRAMME_52D000_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* telegram_media_budget.MEDIA_READ_TIMEOUT / MEDIA_WRITE_TIMEOUT */
#define W21_TELEGRAMME_52D000_MEDIA_READ_TIMEOUT 60.0
#define W21_TELEGRAMME_52D000_MEDIA_WRITE_TIMEOUT 60.0
/* gateway.platforms.helpers.MIN_MEDIA_UPLOAD_RATE (B/s) / MEDIA_UPLOAD_DEADLINE_MAX */
#define W21_TELEGRAMME_52D000_MIN_UPLOAD_RATE 131072.0
#define W21_TELEGRAMME_52D000_DEADLINE_MAX 3600.0

/* PoP: w21_telegramme_52d000_media_sources @ plugins/platforms/telegram/telegram_media_budget.py:media_sources
 * `[media_kwargs.get(key) for key in _MEDIA_KEYS]` -- the nine Bot API media
 * keys in order, None (JSON null) where the call omits them. Returns a fresh
 * array (never NULL). */
json_t *w21_telegramme_52d000_media_sources(const json_t *media_kwargs);

/* PoP: w21_telegramme_52d000_upload_seconds @ gateway/platforms/helpers.py:media_upload_seconds
 * `sum(_on_disk_size(s) for s in sources) / MIN_MEDIA_UPLOAD_RATE`. The sources
 * are the resolved on-disk byte counts (numbers; null/non-numbers count 0, as
 * a URL or platform file id does). Returns a fresh number node. */
json_t *w21_telegramme_52d000_upload_seconds(const json_t *sizes);

/* PoP: w21_telegramme_52d000_scaled_deadline @ gateway/platforms/helpers.py:scaled_upload_deadline
 * `max(deadline, min(MEDIA_UPLOAD_DEADLINE_MAX, deadline + 1.5 * upload_seconds))`
 * for a positive upload time; the deadline itself otherwise. */
json_t *w21_telegramme_52d000_scaled_deadline(double deadline, double upload_seconds);

/* PoP: w21_telegramme_52d000_scaled_budgets @ plugins/platforms/telegram/telegram_media_budget.py:scaled_budgets
 * `{"timeouts": {...}, "deadline": <float>}`: the per-request timeout kwargs
 * (read_timeout always, write_timeout only once the payload has an on-disk
 * size) and the wall-clock deadline. */
json_t *w21_telegramme_52d000_scaled_budgets(const json_t *sizes, double deadline,
                                             double read_timeout);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_TELEGRAMME_52D000_H */
