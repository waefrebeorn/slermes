/*
 * noc_w25_telegramin_d02e02.h - C surface for upstream
 * plugins/platforms/telegram/telegram_ingress.py (gate w25_telegramin_d02e02).
 *
 * Inbound Telegram shapes the core handlers miss: a shared contact card
 * (rendered as a text turn) and the photo-burst / album buffers a reset
 * boundary must clear so a stale turn cannot surface after ``/stop``.  The PTB
 * update, the adapter, its flush tasks and ``handle_message`` are seams; the
 * values decide the rendered text and the drop count.
 */
#ifndef NOC_W25_TELEGRAMIN_D02E02_H
#define NOC_W25_TELEGRAMIN_D02E02_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* contact: {"first_name","last_name","phone_number","user_id","vcard"} (absent
 * keys are None) -> the agent-facing text block, fields only when truthy. */
json_t *w25_telegramin_d02e02_contact_text(const json_t *contact);

/* pending: [{"key": <any>, "session": <any>, "task_done": bool|null}], with
 * task_done null meaning no flush task -> how many buffered events of
 * ``session_key`` were dropped. */
json_t *w25_telegramin_d02e02_drop_matching(const json_t *pending,
                                            const json_t *session_key);

/* session_key, photo_batches, media_group_events -> the total dropped across
 * both buffers (_drop_matching over each, summed). */
json_t *w25_telegramin_d02e02_discard_media_buffers(const json_t *session_key,
                                                    const json_t *photo_batches,
                                                    const json_t *media_group_events);

/* The contact card's literal labels and separator. */
json_t *w25_telegramin_d02e02_constants(void);

#ifdef __cplusplus
}
#endif
#endif
