/*
 * noc_w27_adapter_1f12c3_p5.h - C surface for upstream
 * plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p5).
 *
 * slash-command payload canonicalisation (adapter.py:2956-3400).
 * PORTED (pure): the command-sync policy resolver, the permission normaliser,
 * the (recursive) app-command option/payload canonicaliser, the patch payload,
 * the existing-command payload merge and the two Discord error-text predicates.
 * SEAM: os.getenv (value is the parameter), discord.py command objects (their
 * payload/attributes are the parameter).
 */
#ifndef NOC_W27_ADAPTER_1F12C3_P5_H
#define NOC_W27_ADAPTER_1F12C3_P5_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_1f12c3_p5_get_discord_command_sync_policy(const json_t *env);
json_t *w27_adapter_1f12c3_p5_normalize_permissions(const json_t *value);
json_t *w27_adapter_1f12c3_p5_canonicalize_app_command_option(const json_t *payload);
json_t *w27_adapter_1f12c3_p5_canonicalize_app_command_payload(const json_t *payload);
json_t *w27_adapter_1f12c3_p5_patchable_app_command_payload(const json_t *payload);
json_t *w27_adapter_1f12c3_p5_existing_command_to_payload(const json_t *command);
json_t *w27_adapter_1f12c3_p5_is_reply_reference_rejected(const json_t *text);
json_t *w27_adapter_1f12c3_p5_is_length_overflow_error(const json_t *text);

#ifdef __cplusplus
}
#endif
#endif

