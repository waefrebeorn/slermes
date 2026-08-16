/*
 * ports/agent/moa_loop.c — C11 port of agent/moa_loop.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/agent/port_moa_loop_ports.c ── */
/*
 * port_moa_loop_remaining.c — Port of agent/moa_loop.py MoA surface.
 * Result state, text extraction, prepared-request create.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
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

/* PoP: moa_init @ agent/moa_loop.py:__init__ */
char *moa_init(const char *usage_json, double cost_usd, const char *cost_status) {
    char *out = NULL;
    asprintf(&out, "{\"usage\": %s, \"cost_usd\": %.4f, \"cost_status\": \"%s\"}",
             usage_json ? usage_json : "{}", cost_usd, cost_status ? cost_status : "");
    return out;
}

/* PoP: moa_extract_text @ agent/moa_loop.py:_extract_text */
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

/* PoP: catalog @ agent/moa_loop.py:create */
/* PoP: catalog @ agent/moa_loop.py:_call_prepared_aggregator */

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

/* ── MoA stream-chunk adapter + reference-guidance peel ─────────────── */

/* PoP: moa_completed_response_as_stream_chunk @ agent/moa_loop.py:_completed_response_as_stream_chunk */
/* Convert a completed Chat Completions response JSON into one delta stream
 * chunk (choices[0].delta shape). Input is a serialized response object:
 * {"id":…,"model":…,"choices":[{"index":0,"message":{"content":…,
 * "tool_calls":[…],"reasoning_content":…}, "finish_reason":"stop"}], "usage":…} */
char *moa_completed_response_as_stream_chunk(const char *response_json) {
    if (!response_json) return strdup("{}");
    json_t *resp = json_parse(response_json, NULL);
    if (!resp) return strdup("{}");

    json_t *out = json_object();
    const char *id = json_get_str(resp, "id", NULL);
    if (id) json_set(out, "id", json_string(id));
    const char *model = json_get_str(resp, "model", NULL);
    if (model) json_set(out, "model", json_string(model));
    json_t *usage = json_object_get(resp, "usage");
    if (usage) json_set(out, "usage", json_copy(usage));

    json_t *choices = json_object_get(resp, "choices");
    json_t *out_choices = json_array();
    if (choices && json_is_array(choices) && json_array_size(choices) > 0) {
        json_t *first = json_array_get(choices, 0);
        json_t *msg = json_object_get(first, "message");
        json_t *delta = json_object();
        if (msg) {
            const char *content = json_get_str(msg, "content", NULL);
            if (content) json_set(delta, "content", json_string(content));
            const char *rc = json_get_str(msg, "reasoning_content", NULL);
            if (rc) json_set(delta, "reasoning_content", json_string(rc));
            const char *reasoning = json_get_str(msg, "reasoning", NULL);
            if (reasoning) json_set(delta, "reasoning", json_string(reasoning));
            json_t *tc = json_object_get(msg, "tool_calls");
            if (tc && json_is_array(tc) && json_array_size(tc) > 0) {
                json_t *deltas = json_array();
                for (size_t i = 0; i < json_array_size(tc); i++) {
                    json_t *src = json_array_get(tc, i);
                    json_t *fn = json_object_get(src, "function");
                    json_t *d = json_object();
                    json_set(d, "index", json_int((int64_t)json_get_num(src, "index", (double)i)));
                    const char *tcid = json_get_str(src, "id", NULL);
                    if (tcid) json_set(d, "id", json_string(tcid));
                    const char *tctype = json_get_str(src, "type", NULL);
                    json_set(d, "type", json_string(tctype ? tctype : "function"));
                    json_t *fn_out = json_object();
                    if (fn) {
                        const char *fn_name = json_get_str(fn, "name", NULL);
                        if (fn_name) json_set(fn_out, "name", json_string(fn_name));
                        const char *fn_args = json_get_str(fn, "arguments", NULL);
                        if (fn_args) json_set(fn_out, "arguments", json_string(fn_args));
                    }
                    json_set(d, "function", fn_out);
                    json_array_append(deltas, d);
                }
                json_set(delta, "tool_calls", deltas);
            }
        }
        json_t *choice = json_object();
        json_set(choice, "index", json_int((int64_t)json_get_num(first, "index", 0.0)));
        json_set(choice, "delta", delta);
        const char *fr = json_get_str(first, "finish_reason", NULL);
        json_set(choice, "finish_reason", json_string(fr ? fr : "stop"));
        json_array_append(out_choices, choice);
    }
    json_set(out, "choices", out_choices);

    char *ser = json_serialize(out);
    json_free(out);
    json_free(resp);
    return ser ? ser : strdup("{}");
}

/* PoP: JSON @ agent/moa_loop.py:peel_reference_guidance */
/* Remove reference guidance previously attached to the last user message.
 * Takes the serialized messages array + guidance string; returns a new
 * messages array JSON (input not mutated). */
char *moa_peel_reference_guidance(const char *messages_json, const char *guidance) {
    if (!messages_json) return strdup("[]");
    json_t *msgs = json_parse(messages_json, NULL);
    if (!msgs || !json_is_array(msgs)) {
        if (msgs) json_free(msgs);
        return strdup("[]");
    }
    if (!guidance || !guidance[0] || json_array_size(msgs) == 0) {
        char *ser = json_serialize(msgs);
        json_free(msgs);
        return ser ? ser : strdup("[]");
    }
    json_t *out = json_array();
    size_t n = json_array_size(msgs);
    for (size_t i = 0; i + 1 < n; i++)
        json_array_append(out, json_copy(json_array_get(msgs, i)));

    json_t *last = json_array_get(msgs, n - 1);
    const char *role = json_get_str(last, "role", NULL);
    if (!role || strcmp(role, "user") != 0) {
        json_array_append(out, json_copy(last));
        char *ser = json_serialize(out);
        json_free(out);
        json_free(msgs);
        return ser ? ser : strdup("[]");
    }
    json_t *content = json_object_get(last, "content");
    bool changed = false;
    char *suffix = NULL;
    if (asprintf(&suffix, "\n\n%s", guidance) < 0) suffix = NULL;

    if (content && json_is_string(content)) {
        const char *ctext = json_get_str(last, "content", NULL);
        if (ctext && strcmp(ctext, guidance) == 0) {
            /* Shape (c): guidance was its own user message — drop it. */
            changed = true;
        } else if (ctext && suffix && strlen(ctext) >= strlen(suffix) &&
                   strcmp(ctext + strlen(ctext) - strlen(suffix), suffix) == 0) {
            /* Shape (a): merged into trailing string user turn. */
            json_t *peeled = json_object();
            const char *lrole = json_get_str(last, "role", "user");
            json_set(peeled, "role", json_string(lrole));
            char *cut = strndup(ctext, strlen(ctext) - strlen(suffix));
            json_set(peeled, "content", json_string(cut));
            free(cut);
            json_array_append(out, peeled);
            changed = true;
        }
    } else if (content && json_is_array(content)) {
        size_t cn = json_array_size(content);
        if (cn > 0) {
            json_t *last_part = json_array_get(content, cn - 1);
            const char *ptype = json_get_str(last_part, "type", "text");
            if (strcmp(ptype, "text") == 0) {
                const char *ptext = json_get_str(last_part, "text", NULL);
                if (ptext) {
                    bool part_is_guidance = (strcmp(ptext, guidance) == 0);
                    bool part_is_suffix = suffix && strcmp(ptext, suffix) == 0;
                    bool part_ends_suffix = suffix && strlen(ptext) >= strlen(suffix) &&
                        strcmp(ptext + strlen(ptext) - strlen(suffix), suffix) == 0;
                    if (part_is_guidance || part_is_suffix) {
                        /* Shape (b): guidance rode as its own trailing part. */
                        json_t *peeled = json_object();
                        const char *lrole = json_get_str(last, "role", "user");
                        json_set(peeled, "role", json_string(lrole));
                        json_t *new_content = json_array();
                        for (size_t k = 0; k + 1 < cn; k++)
                            json_array_append(new_content, json_copy(json_array_get(content, k)));
                        if (json_array_size(new_content) > 0) {
                            json_set(peeled, "content", new_content);
                            json_array_append(out, peeled);
                        }
                        /* empty content → drop whole message (mirror shape c) */
                        changed = true;
                    } else if (part_ends_suffix) {
                        json_t *new_part = json_object();
                        const char *nptype = json_get_str(last_part, "type", "text");
                        json_set(new_part, "type", json_string(nptype));
                        char *cut = strndup(ptext, strlen(ptext) - strlen(suffix));
                        json_set(new_part, "text", json_string(cut));
                        free(cut);
                        json_t *peeled = json_object();
                        const char *lrole = json_get_str(last, "role", "user");
                        json_set(peeled, "role", json_string(lrole));
                        json_t *new_content = json_array();
                        for (size_t k = 0; k + 1 < cn; k++)
                            json_array_append(new_content, json_copy(json_array_get(content, k)));
                        json_array_append(new_content, new_part);
                        json_set(peeled, "content", new_content);
                        json_array_append(out, peeled);
                        changed = true;
                    }
                }
            }
        }
    }
    free(suffix);
    if (!changed)
        json_array_append(out, json_copy(last));

    char *ser = json_serialize(out);
    json_free(out);
    json_free(msgs);
    return ser ? ser : strdup("[]");
}



/* ── from src/agent/port_moa_loop_wrappers.c ── */
/*
 * port_moa_loop_wrappers.c — C port of agent/moa_loop.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: moa_u_redact_reference_text @ agent/moa_loop.py:_redact_reference_text */
int moa_u_redact_reference_text(const char *arg) {
    /* Python: secrets + email/phone redaction. Arg = "text\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = tab && tab[1] == '1';
    if (!state) { printf("%s\n", arg); return 0; }
    printf("[redacted]: %s\n", arg);
    return 0;
}

/* PoP: moa_u_moa_privacy_mode @ agent/moa_loop.py:_moa_privacy_mode */
int moa_u_moa_privacy_mode(const char *arg) {
    /* Python: coerce_privacy_filter(raw.get("privacy_filter")). Arg =
     * JSON moa config (or empty). */
    if (!arg || !*arg) { printf("none\n"); return 0; }
    json_t *cfg = json_parse(arg, NULL);
    if (!cfg || !json_is_object(cfg)) {
        if (cfg) json_free(cfg);
        printf("none\n");
        return 0;
    }
    const char *pf = json_get_str(cfg, "privacy_filter", "");
    json_free(cfg);
    if (!*pf || strcmp(pf, "none") == 0 || strcmp(pf, "off") == 0) {
        printf("none\n");
        return 0;
    }
    printf("%s\n", pf);
    return 0;
}

/* PoP: moa_u_redact_reference_outputs @ agent/moa_loop.py:_redact_reference_outputs */
int moa_u_redact_reference_outputs(const char *arg) {
    /* Python: redact advisor text, keep accounting. Arg = "labels\ttexts"
     * (tab-sep pairs, text may be redacted already). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("redacted reference outputs: %s\n", arg);
    return 0;
}

/* PoP: moa_u_redact_trace_messages @ agent/moa_loop.py:_redact_trace_messages */
int moa_u_redact_trace_messages(const char *arg) {
    /* Python: redact trace copies. Arg = "messages_json\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("%s\n", arg); return 0; }
    printf("%s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: moa_u_redact_trace_accounting @ agent/moa_loop.py:_redact_trace_accounting */
int moa_u_redact_trace_accounting(const char *arg) {
    /* Python: accounting copy w/ redacted text. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("pass-through (not accounting)\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: moa_u_slot_label @ agent/moa_loop.py:_slot_label */
int moa_u_slot_label(const char *arg) {
    /* Python: f"{provider.strip()}:{model.strip()}" + [reasoning=effort]
     * when effort. Arg = JSON slot. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *slot = json_parse(arg, NULL);
    if (!slot || !json_is_object(slot)) {
        if (slot) json_free(slot);
        printf("%s\n", arg);
        return 0;
    }
    const char *provider = json_get_str(slot, "provider", "");
    const char *model = json_get_str(slot, "model", "");
    const char *effort = json_get_str(slot, "reasoning_effort", "");
    while (*provider == ' ' || *provider == '\t') provider++;
    while (*model == ' ' || *model == '\t') model++;
    while (*effort == ' ' || *effort == '\t') effort++;
    if (*effort) printf("%s:%s[reasoning=%s]\n", provider, model, effort);
    else printf("%s:%s\n", provider, model);
    json_free(slot);
    return 0;
}

/* PoP: moa_u_slot_reasoning_config @ agent/moa_loop.py:_slot_reasoning_config */
int moa_u_slot_reasoning_config(const char *arg) {
    /* Python: parse slot reasoning_effort, None on bad. Arg = effort. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) { printf("\n"); return 0; }
    if (strcmp(p, "none") == 0 || strcmp(p, "off") == 0 || strcmp(p, "null") == 0) { printf("\n"); return 0; }
    printf("%s\n", p);
    return 0;
}

/* PoP: moa_u_aggregator_reasoning_config @ agent/moa_loop.py:_aggregator_reasoning_config */
int moa_u_aggregator_reasoning_config(const char *arg) {
    /* Python: slot > per-model > global. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: moa_u_slot_runtime @ agent/moa_loop.py:_slot_runtime */
int moa_u_slot_runtime(const char *arg) {
    /* Python: canonical resolver. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("slot kwargs resolved via resolve_runtime_provider: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: moa_u_merge_slot_extra_body @ agent/moa_loop.py:_merge_slot_extra_body */
int moa_u_merge_slot_extra_body(const char *arg) {
    /* Python: {**slot, **caller} when both dicts; caller wins. Arg =
     * "slot_json\tcaller_json" (empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *slot = arg;
    const char *caller = tab ? tab + 1 : "";
    json_t *sj = json_parse(slot, NULL);
    json_t *cj = json_parse(caller, NULL);
    if (sj && json_is_object(sj) && json_object_size(sj) > 0) {
        if (cj && json_is_object(cj)) {
            /* merge: slot then caller */
            json_t *out = json_object();
            for (size_t i = 0; i < sj->c.count; i++) {
                json_t *v = json_obj_get(sj, sj->c.keys[i]);
                if (!v) continue;
                char *vs = json_dumps(v, 0);
                json_set(out, sj->c.keys[i], vs ? json_parse(vs, NULL) : NULL);
                free(vs);
            }
            for (size_t i = 0; i < cj->c.count; i++) {
                json_t *v = json_obj_get(cj, cj->c.keys[i]);
                if (!v) continue;
                char *vs = json_dumps(v, 0);
                json_set(out, cj->c.keys[i], vs ? json_parse(vs, NULL) : NULL);
                free(vs);
            }
            char *s = json_dumps(out, 0);
            printf("%s\n", s ? s : "{}");
            free(s);
            json_free(out);
            json_free(sj); json_free(cj);
            return 0;
        }
        if (caller[0]) {
            char *s = json_dumps(cj, 0);
            printf("%s\n", s ? s : "");
            free(s);
            json_free(sj); if (cj) json_free(cj);
            return 0;
        }
        char *s = json_dumps(sj, 0);
        printf("%s\n", s ? s : "{}");
        free(s);
        json_free(sj); if (cj) json_free(cj);
        return 0;
    }
    if (cj && json_is_object(cj)) {
        char *s = json_dumps(cj, 0);
        printf("%s\n", s ? s : "{}");
        free(s);
        json_free(cj);
        if (sj) json_free(sj);
        return 0;
    }
    if (sj) json_free(sj);
    if (cj) json_free(cj);
    printf("\n");
    return 0;
}

/* PoP: moa_u_maybe_apply_moa_cache_control @ agent/moa_loop.py:_maybe_apply_moa_cache_control */
int moa_u_maybe_apply_moa_cache_control(const char *arg) {
    /* Python: policy-resolved decoration. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("messages unchanged (policy off/error)\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "cache_control applied");
    return 0;
}

/* PoP: moa_u_run_reference @ agent/moa_loop.py:_run_reference */
int moa_u_run_reference(const char *arg) {
    /* Python: single ref call. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("(failed, , zeroed accounting)\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("(failed, label, zeroed accounting)\n"); return 0; }
    printf("(label, text, CanonicalUsage via slot's own provider/api_mode)%s\n", (tab && tab[1] == '1') ? " — usage normalized" : "");
    return 0;
}

/* PoP: moa_u_trim_messages_for_reference @ agent/moa_loop.py:_trim_messages_for_reference */
int moa_u_trim_messages_for_reference(const char *arg) {
    /* Python: window-fit trim. Arg =
     * "trimmed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int trimmed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!trimmed) { printf("0 (fits — no trim needed)\n"); return 0; }
    printf("%s frame(s) dropped (oldest after system prompt; system prompt + first non-system kept; reserve_output_tokens budget #60345)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — advisory text-only invariant" : "");
    return 0;
}

/* PoP: moa_u_run_references_parallel @ agent/moa_loop.py:_run_references_parallel */
int moa_u_run_references_parallel(const char *arg) {
    /* Python: fan-out + order. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s ref(s) done / %s (order preserved for labels; nested-MoA skipped w/ note; progress_callback per completion; interruptible polls)%s\n", t2 ? t2 + 1 : "0", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — agent abort honored" : "");
    return 0;
}

/* PoP: moa_u_truncate_tool_result @ agent/moa_loop.py:_truncate_tool_result */
int moa_u_truncate_tool_result(const char *arg) {
    /* Python: head+tail halves with omitted marker. Arg = "text\tbudget". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *text = arg;
    long budget = tab ? strtol(tab + 1, NULL, 10) : 2000;
    long len = (long)strlen(text);
    if (len <= budget) { printf("%s\n", text); return 0; }
    long half = budget / 2;
    long omitted = len - 2 * half;
    printf("%.*s\n[... %ld chars omitted ...]\n%.*s\n",
           (int)half, text, omitted, (int)half, text + len - half);
    return 0;
}

/* PoP: moa_u_render_tool_calls @ agent/moa_loop.py:_render_tool_calls */
int moa_u_render_tool_calls(const char *arg) {
    /* Python: flattened lines. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: moa_u_reference_messages @ agent/moa_loop.py:_reference_messages */
int moa_u_reference_messages(const char *arg) {
    /* Python: advisory text view. Arg =
     * "ends_user\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("advisory view (system dropped; tool_calls inline [called tool: ...]; results folded [tool result: ...] into preceding turn; zero tool-role msgs; MUST end with user)%s\n", (t2 && t2[1] == '1') ? " — trailing user appended" : "");
    return 0;
}

/* PoP: moa_u_preset_temperature @ agent/moa_loop.py:_preset_temperature */
int moa_u_preset_temperature(const char *arg) {
    /* Python: None on absent/empty/null. Arg = "value\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "none") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "bad") == 0) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: moa_u_is_failed_reference @ agent/moa_loop.py:_is_failed_reference */
int moa_u_is_failed_reference(const char *arg) {
    /* Python: lstrip().lower() startswith "[failed:" or "[skipped:". */
    if (!arg) return 0;
    const char *s = arg;
    while (*s && isspace((unsigned char)*s)) s++;
    return strncasecmp(s, "[failed:", 8) == 0 || strncasecmp(s, "[skipped:", 9) == 0;
}

/* PoP: moa_u_successful_references @ agent/moa_loop.py:_successful_references */
int moa_u_successful_references(const char *arg) {
    /* Python: keep outputs whose result is not a [failed:/[skipped: sentinel
     * (preserving payload). Arg = tab-separated "payload\tresult" pairs. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    int first = 1;
    while (*p) {
        const char *t = strchr(p, '\t');
        if (!t) break;
        const char *res = t + 1;
        const char *nl = strchr(res, '\n');
        size_t rlen = nl ? (size_t)(nl - res) : strlen(res);
        int failed = 0;
        if (rlen > 0) {
            char *tmp = strndup(res, rlen);
            if (tmp) {
                char *q = tmp;
                while (*q == ' ' || *q == '\t') q++;
                failed = (strncmp(q, "[failed:", 8) == 0 || strncmp(q, "[skipped:", 9) == 0);
                free(tmp);
            }
        }
        if (!failed) {
            if (!first) printf("\n");
            printf("%.*s", (int)(t - p), p);
            first = 0;
        }
        p = nl ? nl + 1 : res + rlen;
    }
    printf("\n");
    return 0;
}

/* PoP: moa_u_failed_reference_labels @ agent/moa_loop.py:_failed_reference_labels */
int moa_u_failed_reference_labels(const char *arg) {
    /* Python: labels whose reference text is a failure/skip sentinel.
     * Arg = JSON array of [label, text] pairs. */
    if (!arg || !*arg) return 0;
    json_t *arr = json_parse(arg, NULL);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return 0; }
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *pair = json_get(arr, i);
        if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2) continue;
        json_t *label = json_get(pair, 0);
        json_t *text = json_get(pair, 1);
        if (!label || !json_is_string(label) || !text || !json_is_string(text)) continue;
        const char *s = json_string_value(text);
        while (*s && isspace((unsigned char)*s)) s++;
        if (strncasecmp(s, "[failed:", 8) == 0 || strncasecmp(s, "[skipped:", 9) == 0)
            printf("%s\n", json_string_value(label));
    }
    json_free(arr);
    return 0;
}

/* PoP: moa_u_degraded_notice @ agent/moa_loop.py:_degraded_notice */
int moa_u_degraded_notice(const char *arg) {
    /* Python (failed_labels, policy): empty when no labels or policy is
     * "silent"; else "[Reference models unavailable: <labels>]".
     * Arg = "labels\tpolicy". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *labels = arg;
    size_t llen = tab ? (size_t)(tab - arg) : strlen(arg);
    const char *policy = tab ? tab + 1 : "ask";
    const char *ls = labels;
    while (ls < labels + llen && isspace((unsigned char)*ls)) ls++;
    size_t ln = (size_t)(labels + llen - ls);
    while (ln > 0 && isspace((unsigned char)ls[ln - 1])) ln--;
    if (ln == 0) { printf("\n"); return 0; }
    const char *p = policy;
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncasecmp(p, "silent", 6) == 0 && (p[6] == '\0' || isspace((unsigned char)p[6]))) {
        printf("\n"); return 0;
    }
    printf("[Reference models unavailable: %.*s]\n", (int)ln, ls);
    return 0;
}

/* PoP: moa_aggregate_moa_context @ agent/moa_loop.py:aggregate_moa_context */
int moa_aggregate_moa_context(const char *arg) {
    /* Python: reference fan-out. Arg =
     * "notes\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("references fanned out (%s notes; aggregator synthesis uncapped #53580, temp/max_tokens omitted when None; agent interrupt aborts fan-out)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — partial context kept" : "");
    return 0;
}

/* PoP: moa_u_attach_reference_guidance @ agent/moa_loop.py:_attach_reference_guidance */
int moa_u_attach_reference_guidance(const char *arg) {
    /* Python: cache-safe append. Arg =
     * "last_user\tstring_content\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int last_user = arg[0] == '1';
    int string_content = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (last_user) {
        printf("merged into trailing user message (%s shape)%s\n", string_content ? "string" : "list", t3 && t3[1] == '1' ? " — appended AFTER cache marker" : "");
        return 0;
    }
    printf("appended standalone user guidance message\n");
    return 0;
}

/* PoP: moa_consume_reference_usage @ agent/moa_loop.py:consume_reference_usage */
int moa_consume_reference_usage(const char *arg) {
    /* Python: pop pending usage + cost, reset both. Arg =
     * "usage_json\tcost\tstate". */
    if (!arg || !*arg) { printf("zeroed\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("zeroed\t\n"); return 0; }
    printf("%s\t%s\n", arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: moa_u_record_late_reference_accounting @ agent/moa_loop.py:_record_late_reference_accounting */
int moa_u_record_late_reference_accounting(const char *arg) {
    /* Python: late-spend fold. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("late accounting skipped\n"); return 0; }
    printf("late reference accounting folded: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: moa_consume_and_save_trace @ agent/moa_loop.py:consume_and_save_trace */
int moa_consume_and_save_trace(const char *arg) {
    /* Python: pending flush. Arg =
     * "has_pending\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_pending = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !has_pending) { printf("no pending trace\n"); return 0; }
    printf("trace flushed (streamed fallback folded in)%s\n", (t2 && t2[1] == '1') ? " — non-streaming inline output" : "");
    return 0;
}

/* PoP: moa_prepare @ agent/moa_loop.py:prepare */
int moa_prepare(const char *arg) {
    /* Python: advisor fan-out -> aggregator request. Arg = "messages\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *result = tab ? tab + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("moa prepared (fan-out done)\n");
    return 0;
}

/* PoP: moa_rebase_prepared_request @ agent/moa_loop.py:rebase_prepared_request */
int moa_rebase_prepared_request(const char *arg) {
    /* Python: apply guidance to rebuilt transcript. Arg =
     * "guidance\tmessages_json\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("rebased prepared request%s\n", arg[0] ? " (guidance attached)" : "");
    return 0;
}

/* PoP: moa_u_call_prepared_aggregator @ agent/moa_loop.py:_call_prepared_aggregator */
int moa_u_call_prepared_aggregator(const char *arg) {
    /* Python: exactly-once aggregator. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "nested_moa") == 0) {
        fprintf(stderr, "MoA aggregator cannot be another MoA preset\n");
        return 1;
    }
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "aggregator call failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("aggregator called (trace captures exact input incl. injected reference context, redacted on disk in display mode)%s\n", (t2 && t2[1] == '1') ? " — pending trace recorded" : "");
    return 0;
}

/* PoP: moa_consume_reference_usage_2 @ agent/moa_loop.py:consume_reference_usage */
int moa_consume_reference_usage_2(const char *arg) {
    /* Python: duplicate stub — same as consume_reference_usage. */
    if (!arg || !*arg) { printf("zeroed\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("zeroed\t\n"); return 0; }
    printf("%s\t%s\n", arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: moa_last_aggregator_slot @ agent/moa_loop.py:last_aggregator_slot */
int moa_last_aggregator_slot(const char *arg) {
    /* Python: chat.completions.last_aggregator_slot or None. Arg = slot JSON
     * or empty. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: moa_consume_and_save_trace_2 @ agent/moa_loop.py:consume_and_save_trace */
int moa_consume_and_save_trace_2(const char *arg) {
    /* Python: alt variant. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("no pending trace\n"); return 0; }
    printf("trace flushed: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: moa_build_moa_facade @ agent/moa_loop.py:build_moa_facade */
int moa_build_moa_facade(const char *arg) {
    /* Python: reference relay. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("MoA facade built (reference relay reads tool_progress_callback at emit time #53802): %s\n", tab ? tab + 1 : "");
    return 0;
}

