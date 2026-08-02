/*
 * port_gateway_run_agent.c — Faithful C11 port of the GatewayRunner turn core
 * (gateway/run.py:GatewayRunner._run_agent_inner + _apply_session_model_override).
 *
 * The message→agent execution foundation. Reuses:
 *   - run_conversation()                          (agent core)
 *   - gateway_runner_* session-state helpers      (gateway_runner.c)
 *   - gw_load_reasoning_config / resolve          (port_gateway_run_deps.c)
 *   - build_native_content_parts                  (image_routing.c)
 *   - gw_wrap_current_message_with_observed_context (run_pure2.c)
 *   - gateway_normalize_empty_agent_response      (gateway_run_pure.c)
 *   - gw_collect_auto_append_media_tags           (run_pure2.c)
 *   - gw_sanitize_final_response                  (port_gateway_run.c)
 * around a single run_conversation() call, assembling the faithful result dict.
 */
#include "port_gateway_run_agent.h"
#include "gateway_run_pure.h"
#include "gateway_run_pure2.h"
#include "port_gateway_run_deps.h"
#include "hermes_agent.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Reused from other TUs (declared here to avoid dragging their full headers). */
extern char *run_conversation(agent_state_t *state, const char *user_message,
                              const char *system_message);
extern char *serialize_messages(const agent_state_t *state);
extern char *build_native_content_parts(const char *user_text,
                                        const char *image_paths_json,
                                        char **out_skipped_json);
extern void gw_sanitize_final_response(const char *platform, const char *text,
                                       char *out, size_t out_size);

/* PoP: gateway_runner_apply_session_model_override @ gateway/run.py:_apply_session_model_override */
/* PoP: gateway_runner_apply_session_model_override @ gateway/run.py:_apply_session_model_override */
void gateway_runner_apply_session_model_override(GatewayRunner *self,
                                                 const char *session_key,
                                                 char **io_model,
                                                 json_t *runtime_kwargs)
{
    if (!self || !session_key || !runtime_kwargs) return;
    json_t *overrides = gateway_runner_session_model_overrides(self);
    if (!overrides) return;
    json_t *override = json_obj_get(overrides, session_key);
    if (!override || override->type != JSON_OBJECT) return;

    json_t *m = json_obj_get(override, "model");
    if (m && m->type == JSON_STRING && io_model) {
        free(*io_model);
        *io_model = strdup(m->str_val);
    }

    static const char *keys[] = {"provider", "api_key", "base_url",
                                 "api_mode", "credential_pool"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        json_t *val = json_obj_get(override, keys[i]);
        if (val && val->type != JSON_NULL)
            json_set(runtime_kwargs, keys[i], json_copy(val));
    }

    /* Derive credential_pool from provider when api_key present but no pool. */
    json_t *api_key = json_obj_get(runtime_kwargs, "api_key");
    json_t *pool = json_obj_get(runtime_kwargs, "credential_pool");
    json_t *prov = json_obj_get(override, "provider");
    if (api_key && api_key->type == JSON_STRING && api_key->str_val[0] &&
        (!pool || pool->type == JSON_NULL) &&
        prov && prov->type == JSON_STRING && prov->str_val[0]) {
        char *cp = gw_credential_pool_for_provider(prov->str_val);
        if (cp) {
            json_set(runtime_kwargs, "credential_pool", json_string(cp));
            free(cp);
        }
    }
}

/* PoP: gateway_runner_run_agent_inner @ gateway/run.py:_run_agent_inner */
json_t *gateway_runner_run_agent_inner(GatewayRunner *self,
                                       agent_state_t *agent,
                                       const gw_turn_input_t *in)
{
    if (!agent || !in) return NULL;

    const char *session_key = in->session_key ? in->session_key : "";
    const char *platform = in->platform ? in->platform : "";

    /* ── 1. Apply /model session override ─────────────────────────────── */
    char *effective_model =
        strdup(agent->llm.model[0] ? agent->llm.model : "");
    json_t *runtime_kwargs = json_object();
    gateway_runner_apply_session_model_override(self, session_key,
                                                &effective_model,
                                                runtime_kwargs);
    if (effective_model && effective_model[0]) {
        snprintf(agent->llm.model, sizeof(agent->llm.model), "%s",
                 effective_model);
        json_t *prov = json_obj_get(runtime_kwargs, "provider");
        if (prov && prov->type == JSON_STRING && prov->str_val[0])
            snprintf(agent->llm.provider, sizeof(agent->llm.provider), "%s",
                     prov->str_val);
    }

    /* ── 2. Resolve session reasoning config (session > per-model > global) */
    json_t *reasoning =
        gateway_runner_resolve_session_reasoning_config(self, session_key,
                                                        agent->llm.model);
    /* (reasoning is applied to the agent by the caller/agent config; retained
     * in the result meta so delivery can honor show_reasoning.) */

    /* ── 3. Consume pending native image paths → multimodal wrap ──────── */
    char *run_message = NULL;   /* text, or JSON content-parts array string */
    bool run_message_is_parts = false;
    json_t *native_imgs =
        gateway_runner_consume_pending_native_image_paths(self, session_key);
    if (native_imgs && native_imgs->type == JSON_ARRAY &&
        native_imgs->c.count > 0) {
        char *imgs_json = json_serialize(native_imgs);
        char *skipped = NULL;
        char *parts = build_native_content_parts(in->message ? in->message : "",
                                                 imgs_json, &skipped);
        free(imgs_json);
        free(skipped);
        if (parts && strstr(parts, "\"image_url\"")) {
            run_message = parts;
            run_message_is_parts = true;
        } else {
            /* All images failed to read → fall back to plain text. */
            free(parts);
            run_message = strdup(in->message ? in->message : "");
        }
    } else {
        run_message = strdup(in->message ? in->message : "");
    }
    if (native_imgs) json_free(native_imgs);

    /* ── 4. Wrap with observed group context ──────────────────────────── */
    char *api_run_message = run_message;
    bool api_message_owned = false;
    if (in->observed_context && in->observed_context[0] && !run_message_is_parts) {
        char *wrapped = gw_wrap_current_message_with_observed_context(
            run_message, in->observed_context);
        if (wrapped) {
            api_run_message = wrapped;
            api_message_owned = true;
        }
    }

    /* ── 5. Run-generation staleness guard (pre-run) ──────────────────── */
    if (!gateway_runner_is_session_run_current(self, session_key,
                                               in->run_generation)) {
        if (api_message_owned) free(api_run_message);
        free(run_message);
        free(effective_model);
        json_free(runtime_kwargs);
        if (reasoning) json_free(reasoning);
        return NULL;   /* superseded turn — drop silently */
    }

    /* ── 6. Execute the turn ──────────────────────────────────────────── */
    /* run_conversation takes a plain text message; for multimodal parts the
     * agent core consumes the JSON content-parts string as its user message. */
    char *raw_response = run_conversation(agent, api_run_message,
                                          in->context_prompt);

    /* ── 5b. Run-generation staleness guard (post-run) ────────────────── */
    if (!gateway_runner_is_session_run_current(self, session_key,
                                               in->run_generation)) {
        free(raw_response);
        if (api_message_owned) free(api_run_message);
        free(run_message);
        free(effective_model);
        json_free(runtime_kwargs);
        if (reasoning) json_free(reasoning);
        return NULL;   /* superseded turn — drop silently */
    }

    /* ── 7. Assemble the result dict ──────────────────────────────────── */
    json_t *result = json_object();
    char *messages_json = serialize_messages(agent);
    json_t *messages = messages_json ? json_parse(messages_json, NULL)
                                     : json_array();
    if (!messages) messages = json_array();
    free(messages_json);

    const char *final_text = raw_response ? raw_response : "";

    /* ── 8a. Normalize empty responses ────────────────────────────────── */
    char *normalized = NULL;
    if (!final_text[0]) {
        /* Build a minimal agent_result the normalizer understands. */
        json_t *ar = json_object();
        json_set(ar, "final_response", json_string(""));
        json_set(ar, "messages", json_copy(messages));
        json_t *norm = gateway_normalize_empty_agent_response(ar);
        if (norm && norm->type == JSON_STRING && norm->str_val[0])
            normalized = strdup(norm->str_val);
        json_free(ar);
        if (norm) json_free(norm);
        if (normalized) final_text = normalized;
    }

    /* ── 8b. Sanitize provider errors for the platform ────────────────── */
    char sanitized[8192];
    gw_sanitize_final_response(platform, final_text, sanitized,
                               sizeof(sanitized));
    const char *deliver_text = sanitized[0] ? sanitized : final_text;

    /* ── 8c. Auto-append MEDIA: tags from this turn's tool results ─────── */
    char *final_with_media = strdup(deliver_text);
    if (!strstr(deliver_text, "MEDIA:")) {
        bool has_voice = false;
        char *msgs_str = json_serialize(messages);
        char *tags = gw_collect_auto_append_media_tags(msgs_str, 0, "[]",
                                                       &has_voice);
        free(msgs_str);
        if (tags && tags[0]) {
            size_t need = strlen(final_with_media) + strlen(tags) +
                          (has_voice ? 24 : 0) + 4;
            char *joined = malloc(need);
            if (joined) {
                if (has_voice)
                    snprintf(joined, need, "%s\n[[audio_as_voice]]\n%s",
                             final_with_media, tags);
                else
                    snprintf(joined, need, "%s\n%s", final_with_media, tags);
                free(final_with_media);
                final_with_media = joined;
            }
        }
        free(tags);
    }

    json_set(result, "final_response", json_string(final_with_media));
    json_set(result, "messages", messages);   /* result owns messages now */
    json_set(result, "api_calls", json_number((double)agent->iteration_count));
    json_set(result, "interrupted", json_bool(agent->interrupted));
    json_set(result, "input_tokens",
             json_number((double)agent->session_input_tokens));
    json_set(result, "output_tokens",
             json_number((double)agent->session_output_tokens));
    json_set(result, "model", json_string(agent->llm.model));
    json_set(result, "session_id",
             json_string(in->session_id ? in->session_id
                                         : agent->session_id));
    json_set(result, "run_generation", json_number((double)in->run_generation));
    if (reasoning) json_set(result, "reasoning", reasoning);

    /* ── 9. Consume one-shot sidecar notes → result meta ──────────────── */
    json_t *sidecar =
        gateway_runner_consume_pending_turn_sidecar_notes(self, session_key);
    if (sidecar && sidecar->type == JSON_ARRAY && sidecar->c.count > 0)
        json_set(result, "sidecar_notes", sidecar);
    else if (sidecar)
        json_free(sidecar);

    /* cleanup */
    free(final_with_media);
    free(normalized);
    free(raw_response);
    if (api_message_owned) free(api_run_message);
    free(run_message);
    free(effective_model);
    json_free(runtime_kwargs);

    return result;
}

const char *gw_turn_result_final_response(const json_t *result)
{
    if (!result) return "";
    json_t *fr = json_obj_get((json_t *)result, "final_response");
    return (fr && fr->type == JSON_STRING) ? fr->str_val : "";
}
