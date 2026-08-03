/*
 * port_moa_loop_remaining.c — Port of agent/moa_loop.py MoA surface.
 * Result state, text extraction, prepared-request create.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "json.h"
#include "hermes_agent.h"
#include "port_provider_registry.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ agent/moa_loop.py:__init__ */
char *moa_init(const char *usage_json, double cost_usd, const char *cost_status) {
    char *out = NULL;
    asprintf(&out, "{\"usage\": %s, \"cost_usd\": %.4f, \"cost_status\": \"%s\"}",
             usage_json ? usage_json : "{}", cost_usd, cost_status ? cost_status : "");
    return out;
}

/* PoP: _extract_text @ agent/moa_loop.py:_extract_text */
char *moa_extract_text(const char *response_json) {
    /* Python: assistant text from response. */
    if (!response_json) return strdup("");
    const char *p = strstr(response_json, "\"content\"");
    if (!p) return strdup("");
    const char *colon = strchr(p, ':');
    if (!colon) return strdup("");
    const char *v = colon + 1;
    while (*v == ' ' || *v == '"') v++;
    const char *e = v;
    while (*e && *e != '"') e++;
    if (e == v) return strdup("");
    return strndup(v, (size_t)(e - v));
}

/* PoP: create @ agent/moa_loop.py:create */
/* PoP: _call_prepared_aggregator @ agent/moa_loop.py:_call_prepared_aggregator */

/* Resolve a provider slot's base_url + api_key from the environment, using
 * the provider catalog (port_provider_registry.h): the provider's declared
 * base_url_env_var and api_key_env_vars, with host-derived fallback. */
void llm_resolve_provider_env(llm_config_t *cfg) {
    if (!cfg || !cfg->provider[0]) return;
    const catalog_provider_t *pc = provider_registry_get(cfg->provider);
    if (pc) {
        if (!cfg->base_url[0] && pc->base_url_env_var) {
            const char *v = getenv(pc->base_url_env_var);
            if (v && *v) snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", v);
        }
        if (!cfg->api_key[0] && pc->api_key_env_vars) {
            for (int i = 0; pc->api_key_env_vars[i]; i++) {
                const char *v = getenv(pc->api_key_env_vars[i]);
                if (v && *v) {
                    snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", v);
                    break;
                }
            }
        }
    }
    /* Host-derived fallback: <VENDOR>_API_KEY from the base URL host. */
    if (!cfg->api_key[0]) {
        char *hk = provider_host_derived_api_key(cfg->base_url);
        if (hk && *hk) {
            snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", hk);
            free(hk);
        }
    }
}

char *moa_create(const char *kwargs_json) {
    /* Python: prepared-request aware create. The kwargs JSON carries either
     * {"_moa_prepared_request": {messages, aggregator, aggregator_temperature}}
     * (the prepared-request path — send the aggregator request exactly once)
     * or plain {"messages": [...], "model": ..., "temperature": ...} which we
     * resolve through the active MoA preset. */
    if (!kwargs_json) return NULL;

    char *err = NULL;
    json_t *kwargs = json_parse(kwargs_json, &err);
    if (!kwargs) {
        fprintf(stderr, "moa_create: bad kwargs JSON: %s\n", err ? err : "parse error");
        free(err);
        return strdup("{}");
    }

    /* Prepared request path. */
    json_t *prepared = json_obj_get(kwargs, "_moa_prepared_request");
    if (prepared && prepared->type == JSON_OBJECT) {
        json_t *agg = json_obj_get(prepared, "aggregator");
        json_t *msgs = json_obj_get(prepared, "messages");
        if (agg && agg->type == JSON_OBJECT && msgs && msgs->type == JSON_ARRAY) {
            const char *provider = json_get_str(agg, "provider", "");
            const char *model = json_get_str(agg, "model", "");
            if (provider[0] && model[0] && strcmp(provider, "moa") != 0) {
                double temp = json_get_num(prepared, "aggregator_temperature", 0.0);

                /* Build the aggregator llm_config. */
                llm_config_t agg_cfg;
                memset(&agg_cfg, 0, sizeof(agg_cfg));
                snprintf(agg_cfg.provider, sizeof(agg_cfg.provider), "%s", provider);
                snprintf(agg_cfg.model, sizeof(agg_cfg.model), "%s", model);
                if (temp > 0.0) agg_cfg.temperature = (float)temp;

                /* Resolve provider base_url + api_key from the environment. */
                llm_resolve_provider_env(&agg_cfg);

                /* Convert the JSON messages to message_t array. */
                size_t n = json_len(msgs);
                message_t **arr = calloc(n ? n : 1, sizeof(message_t *));
                if (!arr) { json_free(kwargs); return strdup("{}"); }
                size_t used = 0;
                for (size_t i = 0; i < n; i++) {
                    json_t *m = json_get(msgs, i);
                    if (!m || m->type != JSON_OBJECT) continue;
                    const char *role = json_get_str(m, "role", "user");
                    const char *content = json_get_str(m, "content", "");
                    message_role_t mr = MSG_USER;
                    if (strcmp(role, "system") == 0) mr = MSG_SYSTEM;
                    else if (strcmp(role, "assistant") == 0) mr = MSG_ASSISTANT;
                    arr[used] = message_new(mr, content);
                    if (arr[used]) used++;
                }
                const message_t **carr = (const message_t **)arr;

                llm_response_t *resp = NULL;
                if (used > 0)
                    resp = llm_chat_completion(&agg_cfg, carr, used, NULL);

                for (size_t i = 0; i < used; i++) message_free(arr[i]);
                free(arr);

                if (resp) {
                    /* Emit a response-shaped JSON with content + model. */
                    json_t *out = json_object();
                    if (resp->content)
                        json_set(out, "content", json_string(resp->content));
                    json_set(out, "model", json_string(model));
                    json_set(out, "moa_aggregator", json_bool(true));
                    char *ser = json_serialize(out);
                    json_free(out);
                    llm_response_free(resp);
                    json_free(kwargs);
                    return ser ? ser : strdup("{}");
                }
            }
        }
        json_free(kwargs);
        return strdup("{}");
    }

    /* Non-prepared path: run the preset fan-out + aggregation. */
    {
        json_t *msgs = json_obj_get(kwargs, "messages");
        if (msgs && msgs->type == JSON_ARRAY) {
            /* Load the merged config (config_py_load_config_readonly is the
             * canonical read-only accessor), extract the moa section, resolve
             * the active preset, run each enabled reference model, then
             * synthesize via the aggregator — mirroring
             * aggregate_moa_context(). */
            extern json_t *config_py_load_config_readonly(void);
            extern json_t *moa_resolve_preset(const json_t *config, const char *name);
            json_t *cfg = config_py_load_config_readonly();
            if (cfg) {
                /* moa_resolve_preset expects the moa SECTION (presets +
                 * default_preset), not the whole config document. */
                json_t *moa_sec = json_obj_get(cfg, "moa");
                json_t *preset = moa_resolve_preset(moa_sec, NULL);
                if (preset) {
                    json_t *refs = json_obj_get(preset, "reference_models");
                    json_t *agg = json_obj_get(preset, "aggregator");
                    if (agg && agg->type == JSON_OBJECT) {
                        const char *agg_provider = json_get_str(agg, "provider", "");
                        const char *agg_model = json_get_str(agg, "model", "");
                        if (agg_provider[0] && agg_model[0] &&
                            strcmp(agg_provider, "moa") != 0) {
                            /* 1. Reference fan-out. */
                            size_t nrefs = (refs && refs->type == JSON_ARRAY)
                                ? json_len(refs) : 0;
                            char advice[16384] = "";
                            size_t apos = 0;
                            for (size_t ri = 0; ri < nrefs && apos < sizeof(advice) - 1; ri++) {
                                json_t *slot = json_get(refs, ri);
                                if (!slot || slot->type != JSON_OBJECT) continue;
                                if (json_get_bool(slot, "enabled", true) == false) continue;
                                const char *rp = json_get_str(slot, "provider", "");
                                const char *rm = json_get_str(slot, "model", "");
                                if (!rp[0] || !rm[0] || strcmp(rp, "moa") == 0) continue;

                                /* Advisor call with the reference system prompt. */
                                llm_config_t ref_cfg;
                                memset(&ref_cfg, 0, sizeof(ref_cfg));
                                snprintf(ref_cfg.provider, sizeof(ref_cfg.provider), "%s", rp);
                                snprintf(ref_cfg.model, sizeof(ref_cfg.model), "%s", rm);
                                double rt = json_get_num(slot, "temperature", 0.0);
                                if (rt > 0.0) ref_cfg.temperature = (float)rt;
                                llm_resolve_provider_env(&ref_cfg);

                                /* Advisor messages: system reference prompt +
                                 * the conversation's user turns. */
                                size_t n = json_len(msgs);
                                message_t **arr = calloc(n + 2, sizeof(message_t *));
                                if (!arr) break;
                                size_t used = 0;
                                arr[used++] = message_new(MSG_SYSTEM,
                                    "You are a reference advisor in a Mixture of Agents (MoA) "
                                    "process. You are NOT the acting agent and you do NOT execute "
                                    "anything: you cannot call tools, run commands, browse, or access "
                                    "files, repositories, or URLs. Analyze the conversation context and "
                                    "provide concise, high-signal advice for the aggregator model, "
                                    "which holds the actual capabilities. Never claim you performed "
                                    "an action.");
                                for (size_t i = 0; i < n && used < n + 1; i++) {
                                    json_t *m = json_get(msgs, i);
                                    if (!m || m->type != JSON_OBJECT) continue;
                                    const char *role = json_get_str(m, "role", "user");
                                    const char *content = json_get_str(m, "content", "");
                                    if (strcmp(role, "tool") == 0) continue; /* advisors see no tools */
                                    message_role_t mr = MSG_USER;
                                    if (strcmp(role, "system") == 0) mr = MSG_SYSTEM;
                                    else if (strcmp(role, "assistant") == 0) mr = MSG_ASSISTANT;
                                    arr[used] = message_new(mr, content);
                                    if (arr[used]) used++;
                                }
                                const message_t **carr = (const message_t **)arr;
                                llm_response_t *resp = NULL;
                                if (used > 1)
                                    resp = llm_chat_completion(&ref_cfg, carr, used, NULL);
                                for (size_t i = 0; i < used; i++) message_free(arr[i]);
                                free(arr);

                                if (resp && resp->content) {
                                    int w = snprintf(advice + apos, sizeof(advice) - apos,
                                        "[advisor %s/%s]\n%s\n\n",
                                        rp, rm, resp->content);
                                    if (w > 0) apos += (size_t)w;
                                    if (apos >= sizeof(advice)) apos = sizeof(advice) - 1;
                                }
                                llm_response_free(resp);
                            }

                            /* 2. Aggregator synthesis. */
                            llm_config_t agg_cfg;
                            memset(&agg_cfg, 0, sizeof(agg_cfg));
                            snprintf(agg_cfg.provider, sizeof(agg_cfg.provider), "%s", agg_provider);
                            snprintf(agg_cfg.model, sizeof(agg_cfg.model), "%s", agg_model);
                            double at = json_get_num(preset, "aggregator_temperature", 0.0);
                            if (at > 0.0) agg_cfg.temperature = (float)at;
                            llm_resolve_provider_env(&agg_cfg);

                            size_t n = json_len(msgs);
                            message_t **arr = calloc(n + 2, sizeof(message_t *));
                            if (arr) {
                                size_t used = 0;
                                char guidance[17408];
                                snprintf(guidance, sizeof(guidance),
                                    "You are the aggregator in a Mixture of Agents (MoA) process. "
                                    "Reference advisors have analyzed the conversation; their "
                                    "independent advice is below. Synthesize the best final answer, "
                                    "integrating the strongest points and resolving conflicts.\n\n"
                                    "%s", advice);
                                arr[used++] = message_new(MSG_SYSTEM, guidance);
                                for (size_t i = 0; i < n && used < n + 1; i++) {
                                    json_t *m = json_get(msgs, i);
                                    if (!m || m->type != JSON_OBJECT) continue;
                                    const char *role = json_get_str(m, "role", "user");
                                    const char *content = json_get_str(m, "content", "");
                                    message_role_t mr = MSG_USER;
                                    if (strcmp(role, "system") == 0) mr = MSG_SYSTEM;
                                    else if (strcmp(role, "assistant") == 0) mr = MSG_ASSISTANT;
                                    arr[used] = message_new(mr, content);
                                    if (arr[used]) used++;
                                }
                                const message_t **carr = (const message_t **)arr;
                                llm_response_t *resp = NULL;
                                if (used > 1)
                                    resp = llm_chat_completion(&agg_cfg, carr, used, NULL);
                                for (size_t i = 0; i < used; i++) message_free(arr[i]);
                                free(arr);
                                if (resp) {
                                    json_t *out = json_object();
                                    if (resp->content)
                                        json_set(out, "content", json_string(resp->content));
                                    json_set(out, "model", json_string(agg_model));
                                    json_set(out, "moa_aggregator", json_bool(true));
                                    char *ser = json_serialize(out);
                                    json_free(out);
                                    llm_response_free(resp);
                                    json_free(preset);
                                    json_free(cfg);
                                    json_free(kwargs);
                                    return ser ? ser : strdup("{}");
                                }
                            }
                        }
                    }
                    json_free(preset);
                }
                json_free(cfg);
            }
        }
    }

    json_free(kwargs);
    return strdup("{}");
}
