/*
 * noc_w27_adapter_1f12c3_p1.h - C surface for upstream
 * plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p1).
 *
 * module-level pure helpers (adapter.py:44-660).
 * PORTED (pure): link/attachment shaping, UTF-16 field truncation, snowflake
 * decoding, the non-conversational classifiers, pasted-id cleaning, the ready
 * timeout and env-flag readers.
 * SEAM: os.getenv (the env value is the parameter) and the t() catalog.
 */
#ifndef NOC_W27_ADAPTER_1F12C3_P1_H
#define NOC_W27_ADAPTER_1F12C3_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_1f12c3_p1_image_ext_from_content_type(const json_t *content_type);
json_t *w27_adapter_1f12c3_p1_format_discord_markdown_link(const json_t *label, const json_t *url);
json_t *w27_adapter_1f12c3_p1_is_discord_transport_error(const json_t *type_name, const json_t *text);
json_t *w27_adapter_1f12c3_p1_truncate_discord_component_text(const json_t *text, const json_t *limit);
json_t *w27_adapter_1f12c3_p1_needs_server_members_intent(const json_t *allowed_user_ids, const json_t *allowed_role_ids);
json_t *w27_adapter_1f12c3_p1_format_privileged_intents_guidance(const json_t *needs_members);
json_t *w27_adapter_1f12c3_p1_discord_snowflake_time(const json_t *snowflake);
json_t *w27_adapter_1f12c3_p1_metadata_marks_nonconversational(const json_t *metadata);
json_t *w27_adapter_1f12c3_p1_prompt_target_id(const json_t *chat_id, const json_t *metadata);
json_t *w27_adapter_1f12c3_p1_looks_like_nonconversational_history_message(const json_t *content);
json_t *w27_adapter_1f12c3_p1_clean_discord_id(const json_t *entry);
json_t *w27_adapter_1f12c3_p1_discord_ready_timeout_seconds(const json_t *env);
json_t *w27_adapter_1f12c3_p1_env_bool(const json_t *env, const json_t *dflt);

#ifdef __cplusplus
}
#endif
#endif

