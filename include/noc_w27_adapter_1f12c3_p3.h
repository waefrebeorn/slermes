/*
 * noc_w27_adapter_1f12c3_p3.h - C surface for upstream
 * plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p3).
 *
 * connect classification and command-sync math (adapter.py:1595-2300).
 * PORTED (pure): the startup-exception classifier (_classify_connect_exception),
 * the disconnect flush-budget math, the command-sync state key / skip reason /
 * mutation interval, the retry-after extractor and the rate-limit +
 * unknown-interaction predicates.
 * SEAM: discord.py exception classes (type name is the parameter), os.getenv, the
 * sync state file (its parsed entry is the parameter) and time.time (shimmed).
 */
#ifndef NOC_W27_ADAPTER_1F12C3_P3_H
#define NOC_W27_ADAPTER_1F12C3_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_1f12c3_p3_classify_connect_exception(const json_t *type_name, const json_t *msg, const json_t *shown, const json_t *allowed_user_ids, const json_t *allowed_role_ids);
json_t *w27_adapter_1f12c3_p3_text_batch_flush_deadline_seconds(const json_t *env);
json_t *w27_adapter_1f12c3_p3_command_sync_state_key(const json_t *app_id);
json_t *w27_adapter_1f12c3_p3_command_sync_skip_reason(const json_t *entry, const json_t *fingerprint, const json_t *now);
json_t *w27_adapter_1f12c3_p3_command_sync_mutation_interval_seconds();
json_t *w27_adapter_1f12c3_p3_extract_discord_retry_after(const json_t *retry_after, const json_t *headers);
json_t *w27_adapter_1f12c3_p3_is_discord_rate_limit(const json_t *type_name, const json_t *retry_after);
json_t *w27_adapter_1f12c3_p3_is_discord_unknown_interaction(const json_t *code, const json_t *data_code, const json_t *status, const json_t *msg);

#ifdef __cplusplus
}
#endif
#endif

