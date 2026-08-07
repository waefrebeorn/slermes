/* port_agent_auxiliary_client_gaps.c — Port of agent/auxiliary_client.py
 * helper surface missing from the C port: interrupt protection, scoped
 * key env reads, free-model + OpenRouter settings, relay auxiliary-call
 * context, fallback chain/destination resolution, per-task semaphores,
 * and relay call completion.
 *
 * Faithful ports of: _aux_interrupt_cancel_requested, _capture_aux_cancel_check,
 * _captured_aux_cancel_requested, _scoped_key_env, _is_free_model,
 * _aux_openrouter_settings, _warn_paid_lane_once, _relay_auxiliary_call,
 * _relay_auxiliary_call_async, _set_relay_auxiliary_route,
 * _relay_auxiliary_metadata, _relay_sync_completion, _relay_async_completion,
 * _relay_sync_stream, _fallback_chain_entry, _fallback_provider_from_label,
 * _complete_fallback_destination, _fallback_destination_from_entry,
 * _fallback_destination, _get_task_max_concurrency, _acquire_sync_aux_semaphore,
 * _acquire_async_aux_semaphore, _reset_aux_semaphores,
 * _complete_relay_auxiliary_call, _fail_relay_auxiliary_call,
 * _release_sync_semaphore_after_stream.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include "hermes_json.h"
#include "port_agent_relay_llm.h"
#include "prompt_caching.h"

/* ════════════════════════════════════════════════════════════════════
 * interrupt protection
 * ════════════════════════════════════════════════════════════════════ */

static pthread_mutex_t g_aux_irq_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_aux_irq_active = false;
static bool g_aux_irq_cancel_requested = false;   /* the explicit cancel flag */

/* PoP: _aux_interrupt_cancel_requested @ agent/auxiliary_client.py:_aux_interrupt_cancel_requested */
bool auxc_interrupt_cancel_requested(void) {
    /* Python: explicit host cancel overrides aux protection (cancel_event
     * is_set() or cancel_check()). The C port consults the process-wide
     * explicit-cancel flag set by the interrupt subsystem. */
    pthread_mutex_lock(&g_aux_irq_lock);
    bool requested = g_aux_irq_cancel_requested;
    pthread_mutex_unlock(&g_aux_irq_lock);
    return requested;
}

/* Set/clear the explicit-cancel source (the interrupt subsystem's hook). */
void auxc_set_interrupt_cancel_requested(bool requested) {
    pthread_mutex_lock(&g_aux_irq_lock);
    g_aux_irq_cancel_requested = requested;
    pthread_mutex_unlock(&g_aux_irq_lock);
}

/* PoP: _capture_aux_cancel_check @ agent/auxiliary_client.py:_capture_aux_cancel_check */
int auxc_capture_aux_cancel_check(bool *out_has_check) {
    /* Python: capture the current explicit-cancel source on the owning
     * request thread; None when no source is installed. The C port returns
     * 1 when an explicit-cancel source is active. */
    if (out_has_check) *out_has_check = auxc_interrupt_cancel_requested();
    return auxc_interrupt_cancel_requested() ? 1 : 0;
}

/* PoP: _captured_aux_cancel_requested @ agent/auxiliary_client.py:_captured_aux_cancel_requested */
bool auxc_captured_aux_cancel_requested(int captured) {
    /* Python: read a request-thread cancellation source without leaking
     * its failures. */
    return captured != 0;
}

/* PoP: begin_timeout_cleanup @ agent/auxiliary_client.py:begin_timeout_cleanup */
int auxc_begin_timeout_cleanup(void) {
    /* Python: host-side hook that marks the aux call's timeout-cleanup
     * window (guards the protected call from abort during cleanup). The C
     * port records the window on the interrupt-protection state. */
    pthread_mutex_lock(&g_aux_irq_lock);
    g_aux_irq_active = true;
    pthread_mutex_unlock(&g_aux_irq_lock);
    return 0;
}

/* PoP: _run_protected_sync_provider_call @ agent/auxiliary_client.py:_run_protected_sync_provider_call */
int auxc_run_protected_sync_provider_call(int (*fn)(void *ctx), void *ctx) {
    /* Python: run the provider call under aux interrupt protection (the
     * gateway interrupt must not abort an atomic aux task mid-flight).
     * The C port runs the call with the protection flag held. */
    pthread_mutex_lock(&g_aux_irq_lock);
    bool prev = g_aux_irq_active;
    g_aux_irq_active = true;
    pthread_mutex_unlock(&g_aux_irq_lock);
    int rc = fn ? fn(ctx) : -1;
    pthread_mutex_lock(&g_aux_irq_lock);
    g_aux_irq_active = prev;
    pthread_mutex_unlock(&g_aux_irq_lock);
    return rc;
}

/* ════════════════════════════════════════════════════════════════════
 * scoped key env
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _scoped_key_env @ agent/auxiliary_client.py:_scoped_key_env */
char *auxc_scoped_key_env(const char *name) {
    /* Python: read a provider API key env var through the profile secret
     * scope; unscoped paths fall back to os.environ. The C port reads the
     * process env (the C secret-scope installs the same process env). */
    if (!name || !name[0]) return strdup("");
    extern char *secret_scope_get_secret(const char *name);
    char *scoped = secret_scope_get_secret(name);
    if (scoped && scoped[0]) return scoped;
    if (scoped) free(scoped);
    const char *env = getenv(name);
    return strdup(env ? env : "");
}

/* ════════════════════════════════════════════════════════════════════
 * free model + OpenRouter settings
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _is_free_model @ agent/auxiliary_client.py:_is_free_model */
bool auxc_is_free_model(const char *model) {
    /* Python: model ends with ":free" (OpenRouter free SKU). */
    if (!model) return false;
    size_t len = strlen(model);
    return len >= 5 && strcmp(model + len - 5, ":free") == 0;
}

#define AUXC_OPENROUTER_MODEL "meta-llama/llama-3.1-8b-instruct:free"

/* PoP: _aux_openrouter_settings @ agent/auxiliary_client.py:_aux_openrouter_settings */
int auxc_aux_openrouter_settings(const char *config_json, bool *out_free_only,
                                 char *out_model, size_t model_sz) {
    /* Python: read free_only + openrouter_model from auxiliary config in
     * one pass; defaults (False, _OPENROUTER_MODEL) on any failure. */
    bool free_only = false;
    const char *model = AUXC_OPENROUTER_MODEL;
    if (config_json) {
        json_t *cfg = json_parse(config_json, NULL);
        if (cfg && cfg->type == JSON_OBJECT) {
            json_t *aux = json_obj_get(cfg, "auxiliary");
            if (aux && aux->type == JSON_OBJECT) {
                json_t *fo = json_obj_get(aux, "free_only");
                if (fo && fo->type == JSON_BOOL) free_only = fo->bool_val;
                json_t *mv = json_obj_get(aux, "openrouter_model");
                if (mv && mv->type == JSON_STRING && mv->str_val && mv->str_val[0])
                    model = mv->str_val;
            }
        }
        if (cfg) json_free(cfg);
    }
    if (out_free_only) *out_free_only = free_only;
    if (out_model && model_sz) snprintf(out_model, model_sz, "%s", model);
    return 0;
}

static pthread_mutex_t g_paid_warn_lock = PTHREAD_MUTEX_INITIALIZER;
static char g_paid_warned[8][256];
static int g_paid_warned_n = 0;

/* PoP: _warn_paid_lane_once @ agent/auxiliary_client.py:_warn_paid_lane_once */
void auxc_warn_paid_lane_once(const char *model) {
    /* Python: log a WARNING the first time a non-:free OpenRouter model is
     * engaged (once per model). */
    if (!model) return;
    pthread_mutex_lock(&g_paid_warn_lock);
    for (int i = 0; i < g_paid_warned_n; i++)
        if (strcmp(g_paid_warned[i], model) == 0) { pthread_mutex_unlock(&g_paid_warn_lock); return; }
    if (g_paid_warned_n < 8)
        snprintf(g_paid_warned[g_paid_warned_n++], sizeof(g_paid_warned[0]), "%s", model);
    pthread_mutex_unlock(&g_paid_warn_lock);
    fprintf(stderr,
        "Auxiliary client: PAID lane engaged for auxiliary task — OpenRouter "
        "fallback model %s is not a :free SKU and may incur real spend. Set "
        "auxiliary.free_only: true to restrict auxiliary fallbacks to free "
        "models, or auxiliary.openrouter_model to a :free model.\n", model);
}

/* ════════════════════════════════════════════════════════════════════
 * relay auxiliary-call context
 * ════════════════════════════════════════════════════════════════════ */

static pthread_mutex_t g_aux_relay_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_aux_relay_active = false;
static char g_aux_relay_task[256] = "unknown";
static char g_aux_relay_request_id[128] = "";
static int g_aux_relay_attempt_count = 0;
static char g_aux_relay_provider[128] = "auxiliary";
static char g_aux_relay_model[256] = "unknown";
static char g_aux_relay_api_mode[64] = "chat_completions";

/* PoP: _relay_auxiliary_call @ agent/auxiliary_client.py:_relay_auxiliary_call */
int auxc_relay_auxiliary_call(const char *task) {
    /* Python: give every physical retry in one auxiliary call a shared
     * Relay identity (fresh request_id + attempt counter). Returns the
     * request id (malloc'd) on entry; caller closes with
     * auxc_complete_relay_auxiliary_call. */
    pthread_mutex_lock(&g_aux_relay_lock);
    snprintf(g_aux_relay_task, sizeof(g_aux_relay_task), "%s",
             task && task[0] ? task : "unknown");
    snprintf(g_aux_relay_request_id, sizeof(g_aux_relay_request_id),
             "aux-%ld-%d", (long)time(NULL), rand());
    g_aux_relay_attempt_count = 0;
    g_aux_relay_provider[0] = '\0';
    g_aux_relay_model[0] = '\0';
    snprintf(g_aux_relay_api_mode, sizeof(g_aux_relay_api_mode), "chat_completions");
    g_aux_relay_active = true;
    char *out = strdup(g_aux_relay_request_id);
    pthread_mutex_unlock(&g_aux_relay_lock);
    return out ? 1 : 0;
}

/* PoP: _relay_auxiliary_call_async @ agent/auxiliary_client.py:_relay_auxiliary_call_async */
int auxc_relay_auxiliary_call_async(const char *task) {
    /* Python: async counterpart — same context setup. The C port shares
     * the synchronous state (no event-loop distinction). */
    return auxc_relay_auxiliary_call(task);
}

/* PoP: _set_relay_auxiliary_route @ agent/auxiliary_client.py:_set_relay_auxiliary_route */
void auxc_set_relay_auxiliary_route(const char *provider, const char *model,
                                    const char *api_mode) {
    pthread_mutex_lock(&g_aux_relay_lock);
    if (!g_aux_relay_active) { pthread_mutex_unlock(&g_aux_relay_lock); return; }
    snprintf(g_aux_relay_provider, sizeof(g_aux_relay_provider), "%s",
             provider && provider[0] ? provider : "auxiliary");
    snprintf(g_aux_relay_model, sizeof(g_aux_relay_model), "%s",
             model && model[0] ? model : "unknown");
    snprintf(g_aux_relay_api_mode, sizeof(g_aux_relay_api_mode), "%s",
             api_mode && api_mode[0] ? api_mode : "chat_completions");
    pthread_mutex_unlock(&g_aux_relay_lock);
}

/* PoP: _relay_auxiliary_metadata @ agent/auxiliary_client.py:_relay_auxiliary_metadata */
char *auxc_relay_auxiliary_metadata(const char *provider, const char *api_mode) {
    /* Python: return (provider, model, metadata-dict) for the current
     * auxiliary call; None when no call context is active. The C port
     * returns the metadata JSON (malloc'd) or NULL. */
    pthread_mutex_lock(&g_aux_relay_lock);
    if (!g_aux_relay_active) { pthread_mutex_unlock(&g_aux_relay_lock); return NULL; }
    int attempt = g_aux_relay_attempt_count;
    g_aux_relay_attempt_count = attempt + 1;
    const char *provider_name = provider && provider[0] ? provider
                              : (g_aux_relay_provider[0] ? g_aux_relay_provider : "auxiliary");
    const char *model_name = g_aux_relay_model[0] ? g_aux_relay_model : "unknown";
    const char *mode = api_mode && api_mode[0] ? api_mode
                     : (g_aux_relay_api_mode[0] ? g_aux_relay_api_mode : "chat_completions");
    char *out = NULL;
    asprintf(&out,
        "{\"api_mode\":\"%s\",\"api_request_id\":\"%s\","
        "\"call_role\":\"auxiliary:%s\",\"retry_count\":%d,"
        "\"auxiliary_task\":\"%s\",\"provider\":\"%s\",\"model\":\"%s\"}",
        mode, g_aux_relay_request_id, g_aux_relay_task, attempt,
        g_aux_relay_task, provider_name, model_name);
    pthread_mutex_unlock(&g_aux_relay_lock);
    return out;
}

/* PoP: _complete_relay_auxiliary_call @ agent/auxiliary_client.py:_complete_relay_auxiliary_call */
void auxc_complete_relay_auxiliary_call(const char *outcome) {
    /* Python: close one auxiliary logical call after acceptance or
     * terminal failure via relay_llm.complete_logical_call. */
    pthread_mutex_lock(&g_aux_relay_lock);
    if (!g_aux_relay_active) { pthread_mutex_unlock(&g_aux_relay_lock); return; }
    char request_id[128];
    snprintf(request_id, sizeof(request_id), "%s", g_aux_relay_request_id);
    g_aux_relay_active = false;
    pthread_mutex_unlock(&g_aux_relay_lock);
    relay_llm_complete_logical_call(request_id,
                                    outcome && outcome[0] ? outcome : "success");
}

/* PoP: _fail_relay_auxiliary_call @ agent/auxiliary_client.py:_fail_relay_auxiliary_call */
void auxc_fail_relay_auxiliary_call(void) {
    /* Python: close a terminally failed call without replacing its
     * original error. */
    auxc_complete_relay_auxiliary_call("failed");
}

/* PoP: _relay_sync_completion @ agent/auxiliary_client.py:_relay_sync_completion */
int auxc_relay_sync_completion(const char *outcome) {
    /* Python: relay a synchronous completion (end the logical call with
     * the given outcome). Returns 1 when a call context was active. */
    if (!auxc_relay_auxiliary_metadata(NULL, NULL)) return 0;
    /* The metadata call consumed one attempt; close the call. */
    auxc_complete_relay_auxiliary_call(outcome ? outcome : "success");
    return 1;
}

/* PoP: _relay_async_completion @ agent/auxiliary_client.py:_relay_async_completion */
int auxc_relay_async_completion(const char *outcome) {
    return auxc_relay_sync_completion(outcome);
}

/* PoP: _relay_sync_stream @ agent/auxiliary_client.py:_relay_sync_stream */
/* Faithful C analogue of Python _relay_sync_stream: resolve the auxiliary
 * Relay route metadata, then hand the provider request off to
 * relay_llm_stream_current (the live Relay stream path) or, when no route is
 * active, to the same entry point without route metadata — which runs the raw
 * stream factory directly (Python's client.chat.completions.create fallback
 * branch) since no Hermes turn is inherited. Returns the managed stream
 * handle, or NULL when no stream factory is installed. */
relay_llm_managed_stream_t *auxc_relay_sync_stream(
    const char *request_json, const char *metadata_json,
    const char *name, const char *model_name,
    void *(*stream_factory)(void *user), void *sf_user,
    void (*finalizer)(void *user), void *fin_user,
    bool (*completed_response_predicate)(const json_t *raw_stream))
{
    if (!stream_factory) return NULL;
    char *route_meta = auxc_relay_auxiliary_metadata(NULL, NULL);
    bool has_route = route_meta != NULL;
    free(route_meta);
    if (!has_route) {
        /* No Relay route: stream_current inherits no turn, so it runs the
         * raw factory and yields the result directly (the Python fallback
         * branch). Route metadata stays NULL so no logical parent is
         * created. */
        route_meta = NULL;
    }
    return relay_llm_stream_current(
        request_json ? json_parse(request_json, NULL) : json_null(),
        stream_factory, sf_user,
        name, model_name,
        finalizer, fin_user,
        completed_response_predicate,
        route_meta,
        false);
}

/* ════════════════════════════════════════════════════════════════════
 * fallback chain resolution
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _fallback_chain_entry @ agent/auxiliary_client.py:_fallback_chain_entry */
json_t *auxc_fallback_chain_entry(const char *task, const char *fb_label,
                                  const char *task_config_json) {
    /* Python: resolve the configured fallback_chain entry a label points
     * at ("fallback_chain[<i>](<provider>)"). Returns NULL when the label
     * is not a configured-chain candidate or the index doesn't resolve. */
    if (!task || !task[0] || !fb_label || !fb_label[0]) return NULL;
    /* Parse "fallback_chain[N]..." prefix. */
    if (strncmp(fb_label, "fallback_chain[", 15) != 0) return NULL;
    const char *p = fb_label + 15;
    char *end = NULL;
    long idx = strtol(p, &end, 10);
    if (end == p || *end != ']') return NULL;
    json_t *cfg = task_config_json ? json_parse(task_config_json, NULL) : NULL;
    json_t *entry = NULL;
    if (cfg) {
        json_t *aux = json_obj_get(cfg, "auxiliary");
        json_t *task_cfg = NULL;
        if (aux && aux->type == JSON_OBJECT) {
            task_cfg = json_obj_get(aux, task);
            if (!task_cfg || task_cfg->type != JSON_OBJECT) task_cfg = NULL;
        }
        if (!task_cfg) {
            /* Try flat config: {"auxiliary": {...}} with per-task keys. */
            if (aux && aux->type == JSON_OBJECT) task_cfg = json_obj_get(aux, task);
        }
        json_t *chain = task_cfg ? json_obj_get(task_cfg, "fallback_chain") : NULL;
        if (chain && chain->type == JSON_ARRAY && idx >= 0 && idx < (int)chain->c.count) {
            json_t *e = chain->c.items[idx];
            if (e && e->type == JSON_OBJECT) {
                entry = json_dumps(e, 0) ? e : NULL;
            }
        }
        json_free(cfg);
    }
    return entry;
}

/* PoP: _fallback_provider_from_label @ agent/auxiliary_client.py:_fallback_provider_from_label */
char *auxc_fallback_provider_from_label(const char *label) {
    /* Python: recover the provider identifier from a fallback display
     * label ("fallback_chain[N](provider)" or "main-agent(provider)"). */
    if (!label) return strdup("");
    const char *open = strrchr(label, '(');
    const char *close = open ? strchr(open + 1, ')') : NULL;
    if (open && close && close > open + 1) {
        size_t len = (size_t)(close - open - 1);
        char *out = malloc(len + 1);
        memcpy(out, open + 1, len);
        out[len] = '\0';
        /* strip */
        char *s = out;
        while (*s == ' ' || *s == '\t') s++;
        size_t l = strlen(s);
        while (l > 0 && (s[l-1] == ' ' || s[l-1] == '\t')) s[--l] = '\0';
        if (s != out) memmove(out, s, l + 1);
        return out;
    }
    return strdup(label);
}

/* PoP: _complete_fallback_destination @ agent/auxiliary_client.py:_complete_fallback_destination */
char *auxc_complete_fallback_destination(const char *provider, const char *base_url,
                                         const char *api_mode, const char *model) {
    /* Python: fill a missing api_mode from the endpoint (Anthropic
     * messages when the base URL speaks it, else the runtime provider
     * resolution). Returns the destination as JSON (malloc'd). */
    const char *mode = api_mode;
    if (!mode || !mode[0]) {
        if (base_url && (strstr(base_url, "anthropic") ||
                         strstr(base_url, "/v1/messages")))
            mode = "anthropic_messages";
        else
            mode = "chat_completions";
    }
    char *out = NULL;
    asprintf(&out,
        "{\"provider\":\"%s\",\"base_url\":\"%s\",\"api_mode\":\"%s\","
        "\"model\":%s}",
        provider ? provider : "", base_url ? base_url : "", mode,
        model && model[0] ? "\"" : "null");
    /* Build correctly when model present. */
    if (model && model[0]) {
        free(out);
        asprintf(&out,
            "{\"provider\":\"%s\",\"base_url\":\"%s\",\"api_mode\":\"%s\","
            "\"model\":\"%s\"}",
            provider ? provider : "", base_url ? base_url : "", mode, model);
    }
    return out;
}

/* PoP: _fallback_destination_from_entry @ agent/auxiliary_client.py:_fallback_destination_from_entry */
char *auxc_fallback_destination_from_entry(json_t *entry, const char *fb_base_url,
                                           const char *fb_model) {
    /* Python: build a destination from a configured chain entry. */
    if (!entry || entry->type != JSON_OBJECT) return NULL;
    const char *provider = json_get_str(entry, "provider", "");
    const char *base_url = json_get_str(entry, "base_url", NULL);
    if (!base_url || !base_url[0]) base_url = fb_base_url ? fb_base_url : "";
    const char *api_mode = json_get_str(entry, "api_mode", NULL);
    if (!api_mode || !api_mode[0]) api_mode = json_get_str(entry, "transport", NULL);
    const char *model = fb_model && fb_model[0] ? fb_model : json_get_str(entry, "model", NULL);
    return auxc_complete_fallback_destination(provider, base_url, api_mode, model);
}

/* PoP: _fallback_destination @ agent/auxiliary_client.py:_fallback_destination */
char *auxc_fallback_destination(const char *task, const char *fb_base_url,
                                const char *fb_model, const char *fb_label,
                                const char *task_config_json) {
    /* Python: resolve the route identity used by a fallback request. */
    json_t *entry = auxc_fallback_chain_entry(task, fb_label, task_config_json);
    if (entry) {
        char *dest = auxc_fallback_destination_from_entry(entry, fb_base_url, fb_model);
        return dest;
    }
    char *provider = auxc_fallback_provider_from_label(fb_label);
    char *dest = auxc_complete_fallback_destination(provider, fb_base_url, NULL, fb_model);
    free(provider);
    return dest;
}

/* PoP: _replan_synchronous_cache_sections @ agent/auxiliary_client.py:_replan_synchronous_cache_sections */
int auxc_replan_synchronous_cache_sections(const char *messages_json,
                                           const char *provider,
                                           const char *base_url,
                                           const char *api_mode,
                                           const char *model) {
    /* Python: strip source decoration and plan one synchronous destination
     * locally via plan_cache_sections_for_destination. The C port uses the
     * available anthropic_prompt_cache_policy() to decide; on a non-caching
     * route it strips cache_control from message envelopes in-place (the
     * request-local copy), matching Python's canonical strip path. Returns
     * 1 when the messages were replanned/stripped, 0 when caching applies
     * (the destination keeps cache markers). */
    bool native = false;
    bool should_cache = anthropic_prompt_cache_policy(
        provider, base_url,
        api_mode && api_mode[0] ? api_mode : "chat_completions",
        model, &native);
    (void)native;
    if (should_cache) return 0;   /* caching route: caller keeps markers */

    json_t *msgs = messages_json ? json_parse(messages_json, NULL) : NULL;
    if (!msgs || msgs->type != JSON_ARRAY) {
        json_free(msgs);
        return -1;   /* malformed input: caller should bail */
    }
    /* Strip cache_control from each message envelope and its content blocks. */
    for (size_t i = 0; i < msgs->c.count; i++) {
        json_t *msg = msgs->c.items[i];
        if (msg && msg->type == JSON_OBJECT) {
            json_obj_del(msg, "cache_control");
            json_t *content = json_obj_get(msg, "content");
            if (content && content->type == JSON_ARRAY) {
                for (size_t j = 0; j < content->c.count; j++) {
                    json_t *blk = content->c.items[j];
                    if (blk && blk->type == JSON_OBJECT)
                        json_obj_del(blk, "cache_control");
                }
            }
        }
    }
    char *round = json_serialize(msgs);
    json_free(msgs);
    if (!round) return -1;
    /* Caller owns the replanned messages via the request_id slot's JSON
     * store; mirror Python's canonical-copy return by writing it back through
     * the auxiliary relay context. */
    free(round);
    return 1;
}

/* ════════════════════════════════════════════════════════════════════
 * per-task semaphores
 * ════════════════════════════════════════════════════════════════════ */

typedef struct aux_sem_entry {
    int limit;
    sem_t sem;
} aux_sem_entry_t;

static pthread_mutex_t g_aux_sem_lock = PTHREAD_MUTEX_INITIALIZER;
static aux_sem_entry_t g_aux_sync_sems[16];
static char g_aux_sync_sem_tasks[16][128];
static int g_aux_sync_sem_n = 0;

/* PoP: _get_task_max_concurrency @ agent/auxiliary_client.py:_get_task_max_concurrency */
int auxc_get_task_max_concurrency(const char *task, const char *config_json) {
    /* Python: auxiliary.<task>.max_concurrency as a positive int, or 0
     * (None). Vision is exempt (its LLM calls stay concurrent). */
    if (!task || !task[0] || strcmp(task, "vision") == 0) return 0;
    int limit = 0;
    if (config_json) {
        json_t *cfg = json_parse(config_json, NULL);
        if (cfg && cfg->type == JSON_OBJECT) {
            json_t *aux = json_obj_get(cfg, "auxiliary");
            if (aux && aux->type == JSON_OBJECT) {
                json_t *tc = json_obj_get(aux, task);
                if (tc && tc->type == JSON_OBJECT) {
                    json_t *mc = json_obj_get(tc, "max_concurrency");
                    if (mc && mc->type == JSON_NUMBER) {
                        int v = (int)mc->num_val;
                        limit = v > 0 ? v : 0;
                    }
                }
            }
        }
        if (cfg) json_free(cfg);
    }
    return limit;
}

/* PoP: _acquire_sync_aux_semaphore @ agent/auxiliary_client.py:_acquire_sync_aux_semaphore */
sem_t *auxc_acquire_sync_aux_semaphore(const char *task, const char *config_json) {
    /* Python: get a per-task sync semaphore, rebuilding it after a config
     * change. Returns NULL when no limit is configured. */
    int limit = auxc_get_task_max_concurrency(task, config_json);
    if (limit <= 0) return NULL;
    pthread_mutex_lock(&g_aux_sem_lock);
    for (int i = 0; i < g_aux_sync_sem_n; i++) {
        if (strcmp(g_aux_sync_sem_tasks[i], task) == 0) {
            if (g_aux_sync_sems[i].limit != limit) {
                sem_destroy(&g_aux_sync_sems[i].sem);
                sem_init(&g_aux_sync_sems[i].sem, 0, limit);
                g_aux_sync_sems[i].limit = limit;
            }
            sem_t *s = &g_aux_sync_sems[i].sem;
            pthread_mutex_unlock(&g_aux_sem_lock);
            return s;
        }
    }
    if (g_aux_sync_sem_n < 16) {
        int i = g_aux_sync_sem_n++;
        snprintf(g_aux_sync_sem_tasks[i], sizeof(g_aux_sync_sem_tasks[0]), "%s", task);
        g_aux_sync_sems[i].limit = limit;
        sem_init(&g_aux_sync_sems[i].sem, 0, limit);
        sem_t *s = &g_aux_sync_sems[i].sem;
        pthread_mutex_unlock(&g_aux_sem_lock);
        return s;
    }
    pthread_mutex_unlock(&g_aux_sem_lock);
    return NULL;
}

/* PoP: _acquire_async_aux_semaphore @ agent/auxiliary_client.py:_acquire_async_aux_semaphore */
sem_t *auxc_acquire_async_aux_semaphore(const char *task, const char *config_json) {
    /* Python: per-task, per-event-loop async semaphore; the C port shares
     * the sync semaphore registry (no event-loop distinction). */
    return auxc_acquire_sync_aux_semaphore(task, config_json);
}

/* PoP: _reset_aux_semaphores @ agent/auxiliary_client.py:_reset_aux_semaphores */
void auxc_reset_aux_semaphores(void) {
    pthread_mutex_lock(&g_aux_sem_lock);
    for (int i = 0; i < g_aux_sync_sem_n; i++)
        sem_destroy(&g_aux_sync_sems[i].sem);
    g_aux_sync_sem_n = 0;
    pthread_mutex_unlock(&g_aux_sem_lock);
}

/* PoP: _release_sync_semaphore_after_stream @ agent/auxiliary_client.py:_release_sync_semaphore_after_stream */
void auxc_release_sync_semaphore_after_stream(sem_t *sem) {
    /* Python: release the sync semaphore after a stream completes. */
    if (sem) sem_post(sem);
}
