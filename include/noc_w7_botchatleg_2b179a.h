#ifndef NOC_W7_BOTCHATLEG_2B179A_H
#define NOC_W7_BOTCHATLEG_2B179A_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: botchat_legacy_malformed @ cron/bot_chat_legacy.py:_malformed
 * Why a queued legacy Bot Chat record can never be handed off (empty string
 * when its required fields are usable). */
json_t *botchat_legacy_malformed(const json_t *record);

/* PoP: botchat_legacy_defer_record @ cron/bot_chat_legacy.py:defer
 * The record defer() parks under its delivery id: only the `id`/`name` job
 * keys survive, status is "queued", home is the string form, and `sequence`
 * is the injected time.time_ns() value (decimal string). */
json_t *botchat_legacy_defer_record(const char *key, const json_t *job,
                                    const char *content, const char *home,
                                    int for_failure, const char *sequence);

#ifdef __cplusplus
}
#endif
#endif
