/*
 * noc_w25_telegramli_06412b.h - C surface for upstream
 * plugins/platforms/telegram/telegram_live_location.py (gate w25_telegramli_06412b).
 *
 * The value-only decision of a Telegram live-location refresh: the great-circle
 * distance the "has the user actually moved" test is made on, the LRU table of
 * shares this process has seen (keyed by chat id + message id, bounded), and the
 * redundant/pass decision itself.  PTB dispatch, the handler queue and the
 * clock are seams: the timestamps arrive as fixed values.
 */
#ifndef NOC_W25_TELEGRAMLI_06412B_H
#define NOC_W25_TELEGRAMLI_06412B_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Memory bound only: one entry per live message still being edited. */
#define W25_TELEGRAMLI_06412B_MAX_TRACKED_SHARES 256
/* Movement that makes a refresh a new turn again. */
#define W25_TELEGRAMLI_06412B_MIN_METERS 75.0
/* ...or this long since the last position that did. */
#define W25_TELEGRAMLI_06412B_MIN_SECONDS 900.0

/* Earth radius the haversine below is measured on. */
#define W25_TELEGRAMLI_06412B_EARTH_RADIUS_M 6371000.0

/* {"lat1","lon1","lat2","lon2"} -> metres between the two points. */
json_t *w25_telegramli_06412b_meters_between(const json_t *req);

/* {"shares": [[chat, message, ts|null, lat, lon], ...],   (LRU order, oldest first)
 *  "msg": {"chat_id", "message_id", "edit_date"|null, "date"|null},
 *  "location": {"live_period": <any|null>},
 *  "lat": num, "lon": num}
 * -> {"redundant": bool, "shares": [[chat, message, ts|null, lat, lon], ...]}
 *
 * A stop edit drops the share and reports redundant; a new message never does;
 * a first edit of a share this process never saw start becomes the reference
 * silently; a move past MIN_METERS or a gap past MIN_SECONDS passes and becomes
 * the reference. */
json_t *w25_telegramli_06412b_is_redundant(const json_t *req);

/* LIVE_REFRESH_MIN_METERS / LIVE_REFRESH_MIN_SECONDS / _MAX_TRACKED_SHARES. */
json_t *w25_telegramli_06412b_constants(void);

#ifdef __cplusplus
}
#endif
#endif
