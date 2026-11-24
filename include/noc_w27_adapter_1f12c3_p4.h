/*
 * noc_w27_adapter_1f12c3_p4.h - C surface for upstream
 * plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p4).
 *
 * missed-message-backfill policy (adapter.py:2370-2730).
 * PORTED (pure): the missed-message-backfill enable switch, the clamped
 * number reader and its four call sites, the downtime-notice recogniser and the
 * message channel/thread/parent id triple.
 * SEAM: os.getenv (value is the parameter), the adapter config extra dict and
 * the discord.py message/channel objects (only their ids are read).
 */
#ifndef NOC_W27_ADAPTER_1F12C3_P4_H
#define NOC_W27_ADAPTER_1F12C3_P4_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_1f12c3_p4_missed_message_backfill_enabled(const json_t *extra, const json_t *env);
json_t *w27_adapter_1f12c3_p4_missed_message_backfill_number(const json_t *extra, const json_t *key, const json_t *dflt, const json_t *cast, const json_t *lo, const json_t *hi, const json_t *env);
json_t *w27_adapter_1f12c3_p4_missed_message_backfill_window_seconds(const json_t *extra, const json_t *env);
json_t *w27_adapter_1f12c3_p4_missed_message_backfill_limit(const json_t *extra, const json_t *env);
json_t *w27_adapter_1f12c3_p4_missed_message_backfill_max_dispatches(const json_t *extra, const json_t *env);
json_t *w27_adapter_1f12c3_p4_missed_message_backfill_max_attempts(const json_t *extra, const json_t *env);
json_t *w27_adapter_1f12c3_p4_is_down_notice_content(const json_t *content);
json_t *w27_adapter_1f12c3_p4_message_channel_ids(const json_t *channel);

#ifdef __cplusplus
}
#endif
#endif

