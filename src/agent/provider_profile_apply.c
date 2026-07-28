/* ================================================================
 *  provider_profile_apply.c (v649)
 *  Integration glue: apply a ProviderProfile's per-provider request quirks
 *  onto an already-built chat-completions request body.
 *
 *  Port of Python agent/transports/chat_completions.py
 *  ChatCompletionsTransport._build_kwargs_from_profile().
 *
 *  Called by provider_openai.c (openai_build_request_body) for every
 *  OpenAI-compatible provider. Providers without a registered profile are
 *  left untouched, so custom/unknown endpoints keep their existing path.
 * ================================================================ */

#include "provider_profile.h"
#include "provider.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_portal_tags.h"
#include <stdlib.h>
#include <string.h>

void apply_provider_profile(const provider_t *p, json_t *root)
{
    if (!p || !root) return;
    provider_profile_t *prof = provider_profile_get(p->name);
    if (!prof) return;

    /* ── fixed_temperature ─────────────────────────────────────────── */
    if (prof->fixed_temperature_mode == PROFILE_TEMP_OMIT) {
        json_obj_del(root, "temperature");
    } else if (prof->fixed_temperature_mode == PROFILE_TEMP_FIXED) {
        json_object_set(root, "temperature",
                        json_number(prof->fixed_temperature));
    }
    /* PROFILE_TEMP_DEFAULT: leave existing temperature (already set) */

    /* ── default_max_tokens cap when caller left it unset ──────────── */
    if (prof->default_max_tokens > 0) {
        json_t *mt = json_obj_get(root, "max_tokens");
        bool unset = (mt == NULL) ||
                     (mt->type == JSON_NUMBER && mt->num_val <= 0.0);
        if (unset)
            json_object_set(root, "max_tokens",
                            json_number((double)prof->default_max_tokens));
    }

    /* ── build context JSON consumed by the profile hooks ──────────── */
    json_t *ctx = json_object();
    json_object_set(ctx, "model", json_string(p->model));
    json_object_set(ctx, "base_url", json_string(p->base_url[0] ? p->base_url : ""));
    const char *sid = hermes_get_conversation_context_id();
    if (sid && sid[0]) json_object_set(ctx, "session_id", json_string(sid));
    if (p->config.openrouter_provider[0])
        json_object_set(ctx, "provider_preferences",
                        json_string(p->config.openrouter_provider));

    /* reasoning_config (serialized string for the hooks to json_parse) */
    if (p->config.reasoning_effort[0] || p->config.max_thinking_tokens > 0) {
        json_t *rc = json_object();
        json_object_set(rc, "enabled", json_bool(true));
        if (p->config.reasoning_effort[0])
            json_object_set(rc, "effort",
                            json_string(p->config.reasoning_effort));
        if (p->config.max_thinking_tokens > 0)
            json_object_set(rc, "max_thinking_tokens",
                            json_number((double)p->config.max_thinking_tokens));
        char *rc_str = json_serialize(rc);
        json_free(rc);
        if (rc_str) {
            json_object_set(ctx, "reasoning_config", json_string(rc_str));
            free(rc_str);
        }
    }
    bool supports_reasoning = p->config.reasoning_effort[0] != '\0';
    json_object_set(ctx, "supports_reasoning", json_bool(supports_reasoning));

    /* ── build_extra_body hook ──────────────────────────────────────── */
    if (prof->build_extra_body) {
        char *eb = prof->build_extra_body(prof, json_serialize(ctx));
        if (eb) {
            json_t *ebj = json_parse(eb, NULL);
            if (ebj && ebj->type == JSON_OBJECT) {
                for (size_t i = 0; i < ebj->c.count; i++) {
                    json_t *copy = json_copy(ebj->c.items[i]);
                    if (copy) json_object_set(root, ebj->c.keys[i], copy);
                }
            }
            json_free(ebj);
            free(eb);
        }
    }

    /* ── build_api_kwargs_extras hook ───────────────────────────────── */
    if (prof->build_api_kwargs_extras) {
        char *extra = NULL, *top = NULL;
        prof->build_api_kwargs_extras(prof, json_serialize(ctx), &extra, &top);
        char *blobs[2] = { extra, top };
        for (int b = 0; b < 2; b++) {
            if (!blobs[b]) continue;
            json_t *bj = json_parse(blobs[b], NULL);
            if (bj && bj->type == JSON_OBJECT) {
                for (size_t i = 0; i < bj->c.count; i++) {
                    json_t *copy = json_copy(bj->c.items[i]);
                    if (copy) json_object_set(root, bj->c.keys[i], copy);
                }
            }
            json_free(bj);
            free(blobs[b]);
        }
    }

    json_free(ctx);
}
