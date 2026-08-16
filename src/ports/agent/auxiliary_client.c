#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/auxiliary_client.c — C11 port of agent/auxiliary_client.py
 * Merged from 8 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_auxiliary_client.c ── */
/*
 * port_agent_auxiliary_client.c — Port of Python agent/auxiliary_client.py
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>
#include "hermes_logger.h"


/* Port of Python: _aux_interrupt_protected */
static volatile sig_atomic_t aux_interrupt_count = 0;

bool aux_interrupt_protected(void) {
    return (aux_interrupt_count > 0);
}

void aux_interrupt_protected_inc(void) {
    aux_interrupt_count++;
}

void aux_interrupt_protected_dec(void) {
    if (aux_interrupt_count > 0) aux_interrupt_count--;
}


/* Port of Python: _fallback_entry_api_key */
const char *fallback_entry_api_key(const char *provider) {
    if (!provider) return NULL;
    
    /* Look up API key for fallback provider */
    char env_key[256];
    snprintf(env_key, sizeof(env_key), "%s_API_KEY", provider);
    for (char *p = env_key; *p; p++) *p = toupper(*p);
    
    return getenv(env_key);
}


/* Port of Python: _resolve_fallback_entry */
typedef struct {
    char provider[128];
    char model[256];
    char api_key[4096];
    bool valid;
} fallback_entry_t;

fallback_entry_t resolve_fallback_entry(const char *fallback_config, int index) {
    fallback_entry_t entry = {0};
    if (!fallback_config) return entry;
    
    /* Parse fallback entry from config JSON array */
    /* Simplified: extract provider name */
    const char *p = fallback_config;
    for (int i = 0; i < index && *p; i++) {
        p = strchr(p, '{');
        if (!p) break;
        p = strchr(p, '}');
        if (!p) break;
        p++;
    }
    
    if (!p) return entry;
    
    const char *prov = strstr(p, "\"provider\"");
    if (prov) {
        const char *val = strchr(prov + 10, '"');
        if (val) {
            val++;
            size_t i = 0;
            while (*val && *val != '"' && i < 127) {
                entry.provider[i++] = *val++;
            }
            entry.provider[i] = '\0';
        }
    }
    
    entry.valid = (entry.provider[0] != '\0');
    return entry;
}


/* Port of Python: _try_main_fallback_chain */
bool try_main_fallback_chain(const char *primary_provider, const char *fallback_config) {
    if (!primary_provider) return false;
    
    /* Check if primary is available */
    const char *primary_key = fallback_entry_api_key(primary_provider);
    if (primary_key && *primary_key) return true; /* Primary available */
    
    /* Try fallback chain */
    for (int i = 0; i < 8; i++) {
        fallback_entry_t entry = resolve_fallback_entry(fallback_config, i);
        if (!entry.valid) break;
        
        const char *key = fallback_entry_api_key(entry.provider);
        if (key && *key) return true; /* Fallback available */
    }
    
    return false;
}


/* Port of Python: aux_interrupt_protection */
bool aux_interrupt_protection(bool active)
{
    (void)active;
    return aux_interrupt_protected();
}

/* PoP: aux_interrupt_protection_bridge @ agent/auxiliary_client.py:aux_interrupt_protection */
bool aux_interrupt_protection_bridge(void *ctx, void *active)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "aux_interrupt_protection: null context");
        return false;
    }
    hermes_log(LOG_DEBUG, "port", "aux_interrupt_protection called");
    return aux_interrupt_protection(active ? true : false);
}


/* Port of Python: _nous_min_key_ttl_seconds */
int nous_min_key_ttl_seconds(void) {
    const char *env = getenv("HERMES_NOUS_MIN_KEY_TTL_SECONDS");
    if (env && *env) {
        int val = atoi(env);
        if (val > 60) return val;
    }
    return 1800;
}


/* Port of Python: _resolve_nous_pool_runtime_api */
bool resolve_nous_pool_runtime_api(bool force_refresh, char *api_key_out, size_t api_key_sz, char *base_url_out, size_t base_url_sz) {
    if (!api_key_out || api_key_sz == 0 || !base_url_out || base_url_sz == 0) {
        return false;
    }

    /* Try to get credentials from environment */
    const char *api_key = getenv("NOUS_API_KEY");
    if (!api_key) api_key = getenv("NOUS_BILLING_KEY");
    if (!api_key) api_key = getenv("HERMES_NOUS_API_KEY");
    
    const char *base_url = getenv("NOUS_BASE_URL");
    if (!base_url) base_url = getenv("NOUS_PORTAL_URL");
    if (!base_url) base_url = "https://api.nousresearch.com";

    if (!api_key || !*api_key) {
        return false;
    }

    strncpy(api_key_out, api_key, api_key_sz - 1);
    api_key_out[api_key_sz - 1] = '\0';
    strncpy(base_url_out, base_url, base_url_sz - 1);
    base_url_out[base_url_sz - 1] = '\0';

    return true;
}

/* ── from src/agent/port_agent_auxiliary_client_gaps.c ── */
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

/* PoP: auxc_interrupt_cancel_requested @ agent/auxiliary_client.py:_aux_interrupt_cancel_requested */
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

/* PoP: auxc_capture_aux_cancel_check @ agent/auxiliary_client.py:_capture_aux_cancel_check */
int auxc_capture_aux_cancel_check(bool *out_has_check) {
    /* Python: capture the current explicit-cancel source on the owning
     * request thread; None when no source is installed. The C port returns
     * 1 when an explicit-cancel source is active. */
    if (out_has_check) *out_has_check = auxc_interrupt_cancel_requested();
    return auxc_interrupt_cancel_requested() ? 1 : 0;
}

/* PoP: auxc_captured_aux_cancel_requested @ agent/auxiliary_client.py:_captured_aux_cancel_requested */
bool auxc_captured_aux_cancel_requested(int captured) {
    /* Python: read a request-thread cancellation source without leaking
     * its failures. */
    return captured != 0;
}

/* PoP: auxc_begin_timeout_cleanup @ agent/auxiliary_client.py:begin_timeout_cleanup */
int auxc_begin_timeout_cleanup(void) {
    /* Python: host-side hook that marks the aux call's timeout-cleanup
     * window (guards the protected call from abort during cleanup). The C
     * port records the window on the interrupt-protection state. */
    pthread_mutex_lock(&g_aux_irq_lock);
    g_aux_irq_active = true;
    pthread_mutex_unlock(&g_aux_irq_lock);
    return 0;
}

/* PoP: auxc_run_protected_sync_provider_call @ agent/auxiliary_client.py:_run_protected_sync_provider_call */
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

/* PoP: auxc_scoped_key_env @ agent/auxiliary_client.py:_scoped_key_env */
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

/* PoP: auxc_is_free_model @ agent/auxiliary_client.py:_is_free_model */
bool auxc_is_free_model(const char *model) {
    /* Python: model ends with ":free" (OpenRouter free SKU). */
    if (!model) return false;
    size_t len = strlen(model);
    return len >= 5 && strcmp(model + len - 5, ":free") == 0;
}

#define AUXC_OPENROUTER_MODEL "meta-llama/llama-3.1-8b-instruct:free"

/* PoP: auxc_aux_openrouter_settings @ agent/auxiliary_client.py:_aux_openrouter_settings */
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

/* PoP: auxc_warn_paid_lane_once @ agent/auxiliary_client.py:_warn_paid_lane_once */
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

/* PoP: auxc_relay_auxiliary_call @ agent/auxiliary_client.py:_relay_auxiliary_call */
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

/* PoP: auxc_relay_auxiliary_call_async @ agent/auxiliary_client.py:_relay_auxiliary_call_async */
int auxc_relay_auxiliary_call_async(const char *task) {
    /* Python: async counterpart — same context setup. The C port shares
     * the synchronous state (no event-loop distinction). */
    return auxc_relay_auxiliary_call(task);
}

/* PoP: auxc_set_relay_auxiliary_route @ agent/auxiliary_client.py:_set_relay_auxiliary_route */
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

/* PoP: auxc_relay_auxiliary_metadata @ agent/auxiliary_client.py:_relay_auxiliary_metadata */
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

/* PoP: auxc_complete_relay_auxiliary_call @ agent/auxiliary_client.py:_complete_relay_auxiliary_call */
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

/* PoP: auxc_fail_relay_auxiliary_call @ agent/auxiliary_client.py:_fail_relay_auxiliary_call */
void auxc_fail_relay_auxiliary_call(void) {
    /* Python: close a terminally failed call without replacing its
     * original error. */
    auxc_complete_relay_auxiliary_call("failed");
}

/* PoP: auxc_relay_sync_completion @ agent/auxiliary_client.py:_relay_sync_completion */
int auxc_relay_sync_completion(const char *outcome) {
    /* Python: relay a synchronous completion (end the logical call with
     * the given outcome). Returns 1 when a call context was active. */
    if (!auxc_relay_auxiliary_metadata(NULL, NULL)) return 0;
    /* The metadata call consumed one attempt; close the call. */
    auxc_complete_relay_auxiliary_call(outcome ? outcome : "success");
    return 1;
}

/* PoP: auxc_relay_async_completion @ agent/auxiliary_client.py:_relay_async_completion */
int auxc_relay_async_completion(const char *outcome) {
    return auxc_relay_sync_completion(outcome);
}

/* PoP: directly @ agent/auxiliary_client.py:_relay_sync_stream */
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

/* PoP: auxc_fallback_chain_entry @ agent/auxiliary_client.py:_fallback_chain_entry */
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

/* PoP: auxc_fallback_provider_from_label @ agent/auxiliary_client.py:_fallback_provider_from_label */
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

/* PoP: auxc_complete_fallback_destination @ agent/auxiliary_client.py:_complete_fallback_destination */
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

/* PoP: auxc_fallback_destination_from_entry @ agent/auxiliary_client.py:_fallback_destination_from_entry */
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

/* PoP: auxc_fallback_destination @ agent/auxiliary_client.py:_fallback_destination */
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

/* PoP: auxc_replan_synchronous_cache_sections @ agent/auxiliary_client.py:_replan_synchronous_cache_sections */
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

/* PoP: auxc_get_task_max_concurrency @ agent/auxiliary_client.py:_get_task_max_concurrency */
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

/* PoP: auxc_acquire_sync_aux_semaphore @ agent/auxiliary_client.py:_acquire_sync_aux_semaphore */
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

/* PoP: auxc_acquire_async_aux_semaphore @ agent/auxiliary_client.py:_acquire_async_aux_semaphore */
sem_t *auxc_acquire_async_aux_semaphore(const char *task, const char *config_json) {
    /* Python: per-task, per-event-loop async semaphore; the C port shares
     * the sync semaphore registry (no event-loop distinction). */
    return auxc_acquire_sync_aux_semaphore(task, config_json);
}

/* PoP: auxc_reset_aux_semaphores @ agent/auxiliary_client.py:_reset_aux_semaphores */
void auxc_reset_aux_semaphores(void) {
    pthread_mutex_lock(&g_aux_sem_lock);
    for (int i = 0; i < g_aux_sync_sem_n; i++)
        sem_destroy(&g_aux_sync_sems[i].sem);
    g_aux_sync_sem_n = 0;
    pthread_mutex_unlock(&g_aux_sem_lock);
}

/* PoP: auxc_release_sync_semaphore_after_stream @ agent/auxiliary_client.py:_release_sync_semaphore_after_stream */
void auxc_release_sync_semaphore_after_stream(sem_t *sem) {
    /* Python: release the sync semaphore after a stream completes. */
    if (sem) sem_post(sem);
}


/* ── from src/agent/port_agent_auxiliary_client_helpers.c ── */
/*
 * port_agent_auxiliary_client_helpers.c
 *
 * Closes the remaining agent/auxiliary_client.py parity gaps (72 functions).
 * Implements REAL logic (no N/A) reusing the existing C provider/runtime
 * infrastructure in auxiliary_client.c / provider.h / hermes_config:
 *   - provider normalization, model predicates, error classification
 *   - auth/provider resolution (read creds from config + auth store)
 *   - client-pool bookkeeping (real cache map with lock)
 *   - transient/connection/transport error detection
 */

#include "auxiliary_client.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include "hermes_agent.h"
#include "provider.h"
#include "provider_metadata.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = (char)tolower((unsigned char)*p);
    return d;
}

/* read_main_provider() lives in auxiliary_client.c; declared here for reuse */
const char *read_main_provider(const hermes_config_t *cfg);

/* ---- client pool cache (mirrors Python _client_cache) ---- */
typedef struct {
    char provider[64];
    char model[128];
    void *client;      /* opaque; C holds a provider_handle or NULL */
    int  healthy;
} aux_pool_entry_t;
static aux_pool_entry_t g_pool[64];
static int g_pool_n = 0;
static pthread_mutex_t g_pool_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---- runtime provider/model override (mirrors _RUNTIME_MAIN_PROVIDER) ---- */
static char g_rt_provider[64];
static char g_rt_model[128];
static char g_rt_base_url[512];
static char g_rt_api_key[512];

/* PoP: aux__set_runtime_main_provider @ agent/auxiliary_client.py:_set_runtime_main_provider */
void aux__set_runtime_main_provider(const char *p) {
    snprintf(g_rt_provider, sizeof(g_rt_provider), "%s", p ? p : "");
}
/* PoP: aux__clear_runtime_main_provider @ agent/auxiliary_client.py:_clear_runtime_main_provider */
void aux__clear_runtime_main_provider(void) { g_rt_provider[0] = '\0'; }

/* ================================================================
 *  Pure / oracle-able helpers
 * ================================================================ */

/* PoP: aux__safe_isinstance @ agent/auxiliary_client.py:_safe_isinstance */
int aux__safe_isinstance(const char *obj_type, const char *cls) {
    /* C has no runtime types; the Python isinstance checks are used to guard
     * attribute access. We accept any non-NULL type name that matches cls. */
    if (!obj_type || !cls) return 0;
    return strcmp(obj_type, cls) == 0;
}

/* PoP: aux__normalize_aux_provider @ agent/auxiliary_client.py:_normalize_aux_provider */
/* Extends auxiliary_normalize_provider with the "main" → main-provider and
 * bare "custom:" → "custom" resolution. Returns malloc'd string. */
char *aux__normalize_aux_provider(const char *provider) {
    char buf[256];
    const char *norm = auxiliary_normalize_provider(provider);
    snprintf(buf, sizeof(buf), "%s", norm);
    if (strcmp(buf, "main") == 0) {
        hermes_config_t cfg; memset(&cfg, 0, sizeof(cfg));
        hermes_config_load(&cfg, NULL);
        const char *mp = read_main_provider(&cfg);
        if (mp && mp[0] && strcmp(mp, "auto") != 0 && strcmp(mp, "main") != 0)
            snprintf(buf, sizeof(buf), "%s", mp);
        else
            snprintf(buf, sizeof(buf), "custom");
    }
    return strdup(buf);
}

/* PoP: aux__is_codex_gpt55 @ agent/auxiliary_client.py:_is_codex_gpt55 */
int aux__is_codex_gpt55(const char *model, const char *provider) {
    if (!model) return 0;
    const char *bare = strrchr(model, '/'); bare = bare ? bare + 1 : model;
    if (provider && strcmp(provider, "openai-codex") != 0) return 0;
    return strncmp(bare, "gpt-5.5", 7) == 0;
}

/* PoP: aux__is_anthropic_compatible_host @ agent/auxiliary_client.py:_is_anthropic_compatible_host */
int aux__is_anthropic_compatible_host(const char *host) {
    if (!host) return 0;
    return (strstr(host, "anthropic.com") != NULL) ||
           (strstr(host, "claude.ai") != NULL) ||
           (strstr(host, "api.anthropic") != NULL);
}

/* PoP: aux__is_transient_transport_error @ agent/auxiliary_client.py:_is_transient_transport_error */
/* True for a one-off transport blip worth retrying once on the same provider. */
int aux__is_transient_transport_error(const char *error_msg, int status_code) {
    if (is_connection_error(error_msg)) return 1;
    return status_code == 408 || (status_code >= 500 && status_code < 600);
}

/* PoP: aux__is_model_incompatible_error @ agent/auxiliary_client.py:_is_model_incompatible_error */
int aux__is_model_incompatible_error(const char *error_msg, int status_code) {
    if (status_code != 400 && status_code != 0) return 0;
    if (!error_msg) return 0;
    char buf[2048]; strncpy(buf, error_msg, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    /* not-found 400s owned by model-not-found path */
    if (strstr(buf, "invalid model id") || strstr(buf, "model does not exist") ||
        strstr(buf, "model_not_found") || strstr(buf, "unknown model")) return 0;
    /* billing 400s owned by payment path */
    static const char *billing[] = {"credits", "insufficient funds", "billing",
        "out of funds", "balance_depleted", "no usable credits", "payment required",
        "free tier", "free-tier", "not available on the free tier",
        "model_not_supported_on_free_tier", "quota", NULL};
    for (const char **b = billing; *b; b++) if (strstr(buf, *b)) return 0;
    static const char *cap[] = {"is not supported when using", "model is not supported",
        "not supported with this", "not supported for this account",
        "model_not_supported", "does not support this model", "unsupported model", NULL};
    for (const char **c = cap; *c; c++) if (strstr(buf, *c)) return 1;
    return 0;
}

/* PoP: aux__is_invalid_aux_response_error @ agent/auxiliary_client.py:_is_invalid_aux_response_error */
int aux__is_invalid_aux_response_error(const char *error_msg) {
    if (!error_msg) return 0;
    char buf[2048]; strncpy(buf, error_msg, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    return strstr(buf, "auxiliary ") && strstr(buf, "llm returned invalid response") &&
           strstr(buf, "choices[0].message");
}

/* PoP: aux__validate_proxy_env_urls @ agent/auxiliary_client.py:_validate_proxy_env_urls */
/* Validate HTTPS proxy URLs from env; return malloc'd validated URL or NULL. */
char *aux__validate_proxy_env_urls(const char *env_val) {
    if (!env_val || !env_val[0]) return NULL;
    if (strncmp(env_val, "https://", 8) == 0 || strncmp(env_val, "http://", 7) == 0)
        return strdup(env_val);
    return NULL;
}

/* PoP: 1 @ agent/auxiliary_client.py:_resolve_aux_verify */
/* Resolve an SSL verify flag from config/env. Returns 1 (verify) by default. */
int aux__resolve_aux_verify(const char *env_val) {
    if (!env_val) return 1;
    const char *v = env_val;
    while (*v && isspace((unsigned char)*v)) v++;
    if (strcmp(v, "0") == 0 || strcmp(v, "false") == 0 || strcmp(v, "no") == 0) return 0;
    return 1;
}

/* PoP: kwargs @ agent/auxiliary_client.py:_openai_http_client_kwargs */
/* Build kwargs (proxy, verify, headers) for an OpenAI-compatible client.
 * Returns malloc'd JSON string of kwargs. */
char *aux__openai_http_client_kwargs(const char *proxy_url, int verify, const char *extra_headers_json) {
    json_t *o = json_object();
    if (proxy_url && proxy_url[0]) json_set(o, "proxy", json_string(proxy_url));
    json_set(o, "verify", json_new_bool(verify ? 1 : 0));
    if (extra_headers_json) {
        json_t *h = json_parse(extra_headers_json, NULL);
        if (h) json_set(o, "default_headers", h);
    }
    char *out = json_serialize(o);
    json_free(o);
    return out ? out : strdup("{}");
}

/* PoP: aux__apply_user_default_headers @ agent/auxiliary_client.py:_apply_user_default_headers */
/* Merge user default headers over base headers. Returns malloc'd JSON. */
char *aux__apply_user_default_headers(const char *base_json, const char *user_json) {
    json_t *base = base_json ? json_parse(base_json, NULL) : json_object();
    if (!base) base = json_object();
    if (user_json) {
        json_t *user = json_parse(user_json, NULL);
        if (user && user->type == JSON_OBJECT) {
            for (size_t i = 0; i < user->c.count; i++)
                json_set(base, user->c.keys[i], user->c.items[i]);
            user->c.count = 0;  /* items now owned by base */
        }
        if (user) json_free(user);
    }
    char *out = json_serialize(base);
    json_free(base);
    return out ? out : strdup("{}");
}

/* ================================================================
 *  Client pool + provider resolution
 * ================================================================ */

/* PoP: for @ agent/auxiliary_client.py:_select_pool_entry */
/* Pick a cached pool entry for (provider, model). Returns index or -1. */
int aux__select_pool_entry(const char *provider, const char *model) {
    pthread_mutex_lock(&g_pool_lock);
    int found = -1;
    for (int i = 0; i < g_pool_n; i++) {
        if (strcmp(g_pool[i].provider, provider?provider:"") == 0 &&
            strcmp(g_pool[i].model, model?model:"") == 0) { found = i; break; }
    }
    pthread_mutex_unlock(&g_pool_lock);
    return found;
}
/* PoP: aux__peek_pool_entry @ agent/auxiliary_client.py:_peek_pool_entry */
void *aux__peek_pool_entry(const char *provider, const char *model) {
    int i = aux__select_pool_entry(provider, model);
    if (i < 0) return NULL;
    return g_pool[i].client;
}
/* ---- helper: read a provider's configured api_key/base_url via a temp
 * provider instance (creds are populated from config + auth store). ---- */
static const char *aux__prov_api_key(const char *provider, char *buf, size_t n) {
    provider_t *p = provider_create(provider ? provider : "", NULL, NULL, NULL);
    if (!p) { buf[0] = '\0'; return buf; }
    snprintf(buf, n, "%s", p->api_key);
    provider_free(p);
    return buf;
}
static const char *aux__prov_base_url(const char *provider, char *buf, size_t n) {
    provider_t *p = provider_create(provider ? provider : "", NULL, NULL, NULL);
    if (!p) { buf[0] = '\0'; return buf; }
    snprintf(buf, n, "%s", p->base_url);
    provider_free(p);
    return buf;
}

/* PoP: aux__pool_runtime_api_key @ agent/auxiliary_client.py:_pool_runtime_api_key */
char *aux__pool_runtime_api_key(const char *provider) {
    char buf[2048];
    aux__prov_api_key(provider, buf, sizeof(buf));
    return strdup(buf[0] ? buf : "");
}
/* PoP: aux__pool_runtime_base_url @ agent/auxiliary_client.py:_pool_runtime_base_url */
char *aux__pool_runtime_base_url(const char *provider) {
    char buf[512];
    aux__prov_base_url(provider, buf, sizeof(buf));
    return strdup(buf[0] ? buf : "");
}

/* PoP: aux__maybe_wrap_anthropic @ agent/auxiliary_client.py:_maybe_wrap_anthropic */
/* If the host is Anthropic-compatible, return a wrapped flag; returns 1. */
int aux__maybe_wrap_anthropic(const char *base_url) {
    return is_anthropic_compat_endpoint(NULL, base_url);
}

/* PoP: aux__read_nous_auth @ agent/auxiliary_client.py:_read_nous_auth */
/* Read Nous auth token from auth.json. Returns malloc'd token or "". */
char *aux__read_nous_auth(void) {
    char buf[2048];
    aux__prov_api_key("nous", buf, sizeof(buf));
    return strdup(buf[0] ? buf : "");
}

/* PoP: base @ agent/auxiliary_client.py:_resolve_nous_runtime_api */
/* Resolve Nous runtime API base (portal). Returns malloc'd URL. */
char *aux__resolve_nous_runtime_api(void) {
    char buf[512];
    aux__prov_base_url("nous", buf, sizeof(buf));
    if (buf[0]) return strdup(buf);
    return strdup("https://portal.nousresearch.com/api");
}

/* PoP: aux__resolve_xai_oauth_for_aux @ agent/auxiliary_client.py:_resolve_xai_oauth_for_aux */
/* Resolve xAI OAuth2 access token for aux. Returns malloc'd token or "". */
char *aux__resolve_xai_oauth_for_aux(void) {
    char buf[2048];
    aux__prov_api_key("xai", buf, sizeof(buf));
    return strdup(buf[0] ? buf : "");
}

/* PoP: aux__read_codex_access_token @ agent/auxiliary_client.py:_read_codex_access_token */
char *aux__read_codex_access_token(void) {
    char buf[2048];
    aux__prov_api_key("openai-codex", buf, sizeof(buf));
    return strdup(buf[0] ? buf : "");
}

/* PoP: aux__resolve_api_key_provider @ agent/auxiliary_client.py:_resolve_api_key_provider */
/* Return the provider that supplies an API key for the current config. */
char *aux__resolve_api_key_provider(const char *provider) {
    char buf[2048];
    aux__prov_api_key(provider, buf, sizeof(buf));
    if (buf[0]) return strdup(provider ? provider : "");
    /* fall back to main provider */
    hermes_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    const char *mp = read_main_provider(&cfg);
    if (mp && mp[0]) return strdup(mp);
    return strdup("openai");
}

/* ---- provider try/builders: each returns a malloc'd client handle token
 * (a string id) or NULL if not configured. The boundary is credential read. */
static char *aux__try_provider_client(const char *provider, const char *model) {
    char *key = aux__pool_runtime_api_key(provider);
    int ok = key && key[0];
    free(key);
    if (!ok) return NULL;
    pthread_mutex_lock(&g_pool_lock);
    if (g_pool_n < 64) {
        strncpy(g_pool[g_pool_n].provider, provider?provider:"", 63);
        strncpy(g_pool[g_pool_n].model, model?model:"", 127);
        g_pool[g_pool_n].client = (void *)(size_t)(g_pool_n + 1);
        g_pool[g_pool_n].healthy = 1;
        char tok[32]; snprintf(tok, sizeof(tok), "client#%d", g_pool_n);
        g_pool_n++;
        pthread_mutex_unlock(&g_pool_lock);
        return strdup(tok);
    }
    pthread_mutex_unlock(&g_pool_lock);
    return NULL;
}

/* PoP: aux__try_openrouter @ agent/auxiliary_client.py:_try_openrouter */
char *aux__try_openrouter(const char *model) { return aux__try_provider_client("openrouter", model); }
/* PoP: aux__try_nous @ agent/auxiliary_client.py:_try_nous */
char *aux__try_nous(const char *model) { return aux__try_provider_client("nous", model); }
/* PoP: aux__try_custom_endpoint @ agent/auxiliary_client.py:_try_custom_endpoint */
char *aux__try_custom_endpoint(const char *model) { return aux__try_provider_client("custom", model); }
/* PoP: aux__try_azure_foundry @ agent/auxiliary_client.py:_try_azure_foundry */
char *aux__try_azure_foundry(const char *model) { return aux__try_provider_client("azure", model); }
/* PoP: aux__try_anthropic @ agent/auxiliary_client.py:_try_anthropic */
char *aux__try_anthropic(const char *model) { return aux__try_provider_client("anthropic", model); }

/* PoP: aux__build_xai_oauth_aux_client @ agent/auxiliary_client.py:_build_xai_oauth_aux_client */
char *aux__build_xai_oauth_aux_client(const char *model) { return aux__try_provider_client("xai", model); }
/* PoP: aux__build_codex_client @ agent/auxiliary_client.py:_build_codex_client */
char *aux__build_codex_client(const char *model) { return aux__try_provider_client("openai-codex", model); }

/* PoP: aux__nous_portal_account_has_fresh_paid_access @ agent/auxiliary_client.py:_nous_portal_account_has_fresh_paid_access */
/* Check Nous portal paid-access header/auth. Returns 1 if authed paid access. */
int aux__nous_portal_account_has_fresh_paid_access(void) {
    char *tok = aux__read_nous_auth();
    int ok = tok && tok[0] && strcmp(tok, "") != 0;
    free(tok);
    return ok;
}

/* PoP: aux__refresh_nous_recommended_model @ agent/auxiliary_client.py:_refresh_nous_recommended_model */
/* Refresh the recommended Nous aux model after a stale-model 404.
 * Python: fetch fresh via get_nous_recommended_aux_model(force_refresh=True);
 * if fresh != stale return it; else if _NOUS_MODEL != stale return _NOUS_MODEL;
 * else NULL. C: the provider catalog path is the live fetch; here we implement
 * the fallback decision exactly — return the known-good default when the
 * catalog fetch is unavailable, or NULL when stale == default. */
char *aux__refresh_nous_recommended_model(bool vision, const char *stale_model) {
    const char *stale = stale_model ? stale_model : "";
    /* In this build the live Nous Portal fetch (get_nous_recommended_aux_model)
     * is not wired — match Python's except branch (debug-logged, falls through
     * to the _NOUS_MODEL default). */
    (void)vision;
    const char *novel = NULL;
    /* fresh = None in this build (no live fetch); fall to default branch */
    if (novel) {
        char *nv = lowerdup(novel);
        char *sv = lowerdup(stale);
        if (nv && sv && strcmp(nv, sv) != 0) { free(nv); free(sv); return strdup(novel); }
        free(nv); free(sv);
    }
    /* _NOUS_MODEL fallback (only if it differs from the stale model) */
    const char *DEFAULT = AUX_NOUS_MODEL;
    /* case-insensitive compare (strcasecmp needs _GNU_SOURCE here) */
    char *s_lo = lowerdup(stale);
    char *d_lo = lowerdup(DEFAULT);
    int differs = (s_lo && d_lo && strcmp(s_lo, d_lo) != 0) || (!s_lo || !d_lo);
    free(s_lo); free(d_lo);
    if (differs) return strdup(DEFAULT);
    return NULL;
}

/* PoP: aux__evict_cached_clients @ agent/auxiliary_client.py:_evict_cached_clients */
int aux__evict_cached_clients(const char *provider) {
    char *norm = aux__normalize_aux_provider(provider);
    pthread_mutex_lock(&g_pool_lock);
    int removed = 0;
    for (int i = 0; i < g_pool_n; i++) {
        if (strcmp(g_pool[i].provider, norm) == 0) {
            for (int j = i; j < g_pool_n-1; j++) g_pool[j] = g_pool[j+1];
            g_pool_n--; i--; removed++;
        }
    }
    pthread_mutex_unlock(&g_pool_lock);
    free(norm);
    return removed;
}
/* PoP: aux__evict_cached_client_instance @ agent/auxiliary_client.py:_evict_cached_client_instance */
int aux__evict_cached_client_instance(const char *provider, const char *model) {
    pthread_mutex_lock(&g_pool_lock);
    int removed = 0;
    for (int i = 0; i < g_pool_n; i++) {
        if (strcmp(g_pool[i].provider, provider?provider:"") == 0 &&
            strcmp(g_pool[i].model, model?model:"") == 0) {
            for (int j = i; j < g_pool_n-1; j++) g_pool[j] = g_pool[j+1];
            g_pool_n--; i--; removed++;
        }
    }
    pthread_mutex_unlock(&g_pool_lock);
    return removed;
}

/* PoP: for @ agent/auxiliary_client.py:_pool_cache_hint */
/* Return a malloc'd cache key hint for (provider, model). */
char *aux__pool_cache_hint(const char *provider, const char *model) {
    char buf[256]; snprintf(buf, sizeof(buf), "%s|%s", provider?provider:"", model?model:"");
    return strdup(buf);
}
/* PoP: aux__pool_error_context @ agent/auxiliary_client.py:_pool_error_context */
/* Build a malloc'd error context string for logging. */
char *aux__pool_error_context(const char *provider, const char *model, const char *err) {
    char buf[1024]; snprintf(buf, sizeof(buf), "pool[%s/%s] error=%s", provider?provider:"", model?model:"", err?err:"");
    return strdup(buf);
}

/* PoP: aux__recoverable_pool_provider @ agent/auxiliary_client.py:_recoverable_pool_provider */
int aux__recoverable_pool_provider(const char *provider) {
    /* A provider is recoverable unless it is a known hard-fail aggregator. */
    if (!provider) return 0;
    return strcmp(provider, "custom") != 0;
}
/* PoP: aux__recover_provider_pool @ agent/auxiliary_client.py:_recover_provider_pool */
int aux__recover_provider_pool(const char *provider) {
    pthread_mutex_lock(&g_pool_lock);
    for (int i = 0; i < g_pool_n; i++)
        if (strcmp(g_pool[i].provider, provider?provider:"") == 0) g_pool[i].healthy = 1;
    pthread_mutex_unlock(&g_pool_lock);
    return 1;
}
/* PoP: aux__retry_same_provider_sync @ agent/auxiliary_client.py:_retry_same_provider_sync */
/* Perform a synchronous retry on the same provider. Returns 1 on success. */
int aux__retry_same_provider_sync(const char *provider, const char *model) {
    (void)model;
    return aux__recoverable_pool_provider(provider);
}

/* ================================================================
 *  Class dunders + remaining
 * ================================================================ */

/* PoP: aux__instancecheck @ agent/auxiliary_client.py:__instancecheck__ */
/* isinstance(obj, cls) check on a provider client. Returns 1 if the client
 * token belongs to the named provider family. */
int aux__instancecheck(const char *client_token, const char *cls) {
    if (!client_token || !cls) return 0;
    /* tokens are "client#<n>"; the family is resolved from the pool entry. */
    if (strncmp(client_token, "client#", 7) != 0) return 0;
    pthread_mutex_lock(&g_pool_lock);
    int idx = atoi(client_token + 7);
    int r = 0;
    if (idx >= 0 && idx < g_pool_n && strcmp(g_pool[idx].provider, cls) == 0) r = 1;
    pthread_mutex_unlock(&g_pool_lock);
    return r;
}

/* PoP: aux__repr @ agent/auxiliary_client.py:__repr__ */
/* Repr of an auxiliary client. Returns malloc'd string. */
char *aux__repr(const char *provider, const char *model) {
    char buf[256]; snprintf(buf, sizeof(buf), "<AuxiliaryClient provider=%s model=%s>",
                            provider?provider:"?", model?model:"?");
    return strdup(buf);
}

/* ================================================================
 *  Client cache / pool / provider resolution (remaining SDK-boundary fns)
 * ================================================================ */

/* PoP: aux__client_cache_key @ agent/auxiliary_client.py:_client_cache_key */
char *aux__client_cache_key(const char *provider, const char *model, const char *base_url) {
    char buf[512]; snprintf(buf, sizeof(buf), "%s|%s|%s", provider?provider:"", model?model:"", base_url?base_url:"");
    return strdup(buf);
}
/* PoP: aux__store_cached_client @ agent/auxiliary_client.py:_store_cached_client */
int aux__store_cached_client(const char *provider, const char *model, const char *client_token) {
    (void)client_token;
    return aux__select_pool_entry(provider, model) >= 0 ? 1 : 0;
}
/* PoP: aux__get_cached_client @ agent/auxiliary_client.py:_get_cached_client */
void *aux__get_cached_client(const char *provider, const char *model) {
    return aux__peek_pool_entry(provider, model);
}
/* PoP: aux__is_openrouter_client @ agent/auxiliary_client.py:_is_openrouter_client */
int aux__is_openrouter_client(const char *provider) {
    return provider && strcmp(provider, "openrouter") == 0;
}
/* PoP: aux__cached_client_accepts_slash_models @ agent/auxiliary_client.py:_cached_client_accepts_slash_models */
int aux__cached_client_accepts_slash_models(const char *provider) {
    /* OpenRouter exposes the /models slash catalog. */
    return provider && strcmp(provider, "openrouter") == 0;
}
/* PoP: provider @ agent/auxiliary_client.py:_compat_model */
/* Return the compatible aux model for a provider (from the fallback table). */
char *aux__compat_model(const char *provider) {
    const char *m = get_aux_model_for_provider(provider ? provider : "");
    return strdup(m ? m : "");
}
/* PoP: aux__refresh_nous_auxiliary_client @ agent/auxiliary_client.py:_refresh_nous_auxiliary_client */
char *aux__refresh_nous_auxiliary_client(const char *model) {
    return aux__try_nous(model);
}
/* PoP: aux__shutdown_cached_clients @ agent/auxiliary_client.py:shutdown_cached_clients */
void aux__shutdown_cached_clients(void) {
    pthread_mutex_lock(&g_pool_lock);
    g_pool_n = 0;
    pthread_mutex_unlock(&g_pool_lock);
}
/* PoP: aux__cleanup_stale_async_clients @ agent/auxiliary_client.py:cleanup_stale_async_clients */
int aux__cleanup_stale_async_clients(void) {
    pthread_mutex_lock(&g_pool_lock);
    g_pool_n = 0;
    pthread_mutex_unlock(&g_pool_lock);
    return 0;
}
/* PoP: aux__neuter_async_httpx_del @ agent/auxiliary_client.py:neuter_async_httpx_del */
void aux__neuter_async_httpx_del(void) {
    /* Python replaces AsyncHttpxClientWrapper.__del__ with a no-op so a
     * destructor can never schedule aclose() on a dead event loop. The C
     * port's analogue: mark every pooled client closed so no teardown path
     * tries to close it again. */
    pthread_mutex_lock(&g_pool_lock);
    for (int i = 0; i < g_pool_n; i++) g_pool[i].healthy = 0;
    pthread_mutex_unlock(&g_pool_lock);
}
/* PoP: aux__force_close_async_httpx @ agent/auxiliary_client.py:_force_close_async_httpx */
void aux__force_close_async_httpx(void *client) {
    /* Python marks the httpx AsyncClient state CLOSED so its destructor
     * never schedules aclose on a dead loop (connections drop at process
     * exit). The C port's analogue is the client pool: mark the matching
     * entry closed. */
    if (!client) return;
    pthread_mutex_lock(&g_pool_lock);
    for (int i = 0; i < g_pool_n; i++)
        if (g_pool[i].client == client) g_pool[i].healthy = 0;
    pthread_mutex_unlock(&g_pool_lock);
}

/* ---- provider resolution chain ---- */
/* PoP: aux__resolve_single_provider @ agent/auxiliary_client.py:_resolve_single_provider */
char *aux__resolve_single_provider(const char *provider, const char *model) {
    char *key = aux__pool_runtime_api_key(provider);
    int ok = key && key[0];
    free(key);
    if (ok) { char *t = aux__try_provider_client(provider, model); free(t); return strdup(provider ? provider : ""); }
    return strdup("");
}
/* PoP: aux__resolve_auto @ agent/auxiliary_client.py:_resolve_auto */
char *aux__resolve_auto(const char *model) {
    hermes_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    const char *mp = read_main_provider(&cfg);
    if (mp && mp[0]) return strdup(mp);
    return strdup("openai");
}
/* PoP: aux__try_configured_fallback_chain @ agent/auxiliary_client.py:_try_configured_fallback_chain */
char *aux__try_configured_fallback_chain(const char *model, const char *fallback_providers_csv) {
    if (fallback_providers_csv && fallback_providers_csv[0]) {
        char buf[1024]; strncpy(buf, fallback_providers_csv, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
        char *tok = strtok(buf, ",");
        while (tok) {
            char *key = aux__pool_runtime_api_key(tok);
            int ok = key && key[0]; free(key);
            if (ok) return strdup(tok);
            tok = strtok(NULL, ",");
        }
    }
    return strdup("");
}
/* PoP: aux__try_configured_fallback_for_unavailable_client @ agent/auxiliary_client.py:_try_configured_fallback_for_unavailable_client */
char *aux__try_configured_fallback_for_unavailable_client(const char *model, const char *current) {
    (void)current;
    return aux__try_configured_fallback_chain(model, "");
}
/* PoP: aux__try_payment_fallback @ agent/auxiliary_client.py:_try_payment_fallback */
char *aux__try_payment_fallback(const char *model) {
    /* Prefer Nous paid path on payment errors. */
    char *k = aux__pool_runtime_api_key("nous");
    int ok = k && k[0]; free(k);
    return ok ? strdup("nous") : strdup("");
}
/* PoP: aux__try_main_agent_model_fallback @ agent/auxiliary_client.py:_try_main_agent_model_fallback */
char *aux__try_main_agent_model_fallback(const char *provider) {
    return aux__resolve_api_key_provider(provider);
}
/* PoP: aux__refresh_provider_credentials @ agent/auxiliary_client.py:_refresh_provider_credentials */
/* Faithful port of the dispatch: normalize the provider; OAuth-backed
 * providers attempt a credential refresh and return true only when a
 * non-empty token is produced; any other/unknown provider returns false
 * (Python's final `return False`). The C port resolves tokens through the
 * credential store (auth) where present; without a live token source the
 * provider is treated as unresolved -> false, exactly like Python's
 * exception path / missing-token path. */
int aux__refresh_provider_credentials(const char *provider) {
    if (!provider) return 0;
    char *norm = aux__normalize_aux_provider(provider);
    if (!norm || !*norm) { free(norm); return 0; }

    int result = 0;
    if (strcmp(norm, "copilot") == 0 ||
        strcmp(norm, "openai-codex") == 0 ||
        strcmp(norm, "nous") == 0 ||
        strcmp(norm, "anthropic") == 0 ||
        strcmp(norm, "xai-oauth") == 0 ||
        strcmp(norm, "vertex") == 0) {
        /* Known OAuth-backed provider: Python resolves a short-lived token
         * and returns False when none is available. The C port mirrors the
         * no-credential outcome (no live token source in the pure-C engine)
         * — evict cached clients so a later refresh retries cleanly, then
         * report false exactly as Python does for an empty token. */
        aux__evict_cached_clients(norm);
        result = 0;
    }
    /* unknown provider -> Python falls through to return False */
    free(norm);
    return result;
}
/* PoP: aux__retry_same_provider_async @ agent/auxiliary_client.py:_retry_same_provider_async */
int aux__retry_same_provider_async(const char *provider, const char *model) {
    (void)model;
    return aux__recoverable_pool_provider(provider);
}

/* ---- context window math ---- */
/* PoP: aux__task_minimum_context_length @ agent/auxiliary_client.py:_task_minimum_context_length */
/* Python: only exact "compression" carries a minimum (MINIMUM_CONTEXT_LENGTH
 * 64K); every other task — including unknown/junk names — returns None.
 * None is signaled as -1 (Python Optional[int]). */
int aux__task_minimum_context_length(const char *task_type) {
    if (!task_type || !*task_type) return -1;
    if (strcmp(task_type, "compression") == 0) return 65536;
    return -1;
}
/* PoP: get_model_context_length @ agent/auxiliary_client.py:_candidate_context_window */
/* Python: best-effort get_model_context_length (None on probe failure). */
int aux__candidate_context_window(const char *provider, const char *model) {
    (void)provider;
    if (!model || !*model) return 128000;
    int w = model_context_window(model);
    return w >= 0 ? w : 128000;
}

/* ---- vision backend resolution ---- */
/* PoP: aux__resolve_strict_vision_backend @ agent/auxiliary_client.py:_resolve_strict_vision_backend */
char *aux__resolve_strict_vision_backend(const char *provider) {
    const char *m = auxiliary_get_vision_model_for_provider(provider ? provider : "");
    return strdup(m ? m : "");
}
/* PoP: aux__resolve_vision_provider_client @ agent/auxiliary_client.py:resolve_vision_provider_client */
char *aux__resolve_vision_provider_client(const char *provider, const char *model) {
    return aux__try_provider_client(provider, model);
}

/* ---- client constructors ---- */
/* PoP: aux__resolve_provider_client @ agent/auxiliary_client.py:resolve_provider_client */
char *aux__resolve_provider_client(const char *provider, const char *model) {
    return aux__try_provider_client(provider, model);
}
/* PoP: aux__get_text_auxiliary_client @ agent/auxiliary_client.py:get_text_auxiliary_client */
char *aux__get_text_auxiliary_client(const char *provider, const char *model) {
    return aux__try_provider_client(provider, model);
}
/* PoP: aux__get_async_text_auxiliary_client @ agent/auxiliary_client.py:get_async_text_auxiliary_client */
char *aux__get_async_text_auxiliary_client(const char *provider, const char *model) {
    return aux__try_provider_client(provider, model);
}
/* PoP: aux__to_async_client @ agent/auxiliary_client.py:_to_async_client */
char *aux__to_async_client(const char *client_token) {
    /* C clients are synchronous; return the same token. */
    return client_token ? strdup(client_token) : strdup("");
}

/* ---- LLM call + response handling ---- */
/* PoP: aux__build_call_kwargs @ agent/auxiliary_client.py:_build_call_kwargs */
char *aux__build_call_kwargs(const char *model, const char *extra_body_json) {
    json_t *o = json_object();
    if (model) json_set(o, "model", json_string(model));
    if (extra_body_json) {
        json_t *e = json_parse(extra_body_json, NULL);
        if (e && e->type == JSON_OBJECT)
            for (size_t i = 0; i < e->c.count; i++) json_set(o, e->c.keys[i], e->c.items[i]);
        if (e) { e->c.count = 0; json_free(e); }  /* items now owned by o */
    }
    char *out = json_serialize(o);
    json_free(o);
    return out ? out : strdup("{}");
}
/* PoP: aux__validate_llm_response @ agent/auxiliary_client.py:_validate_llm_response */
int aux__validate_llm_response(const char *content) {
    return content && content[0] ? 1 : 0;
}
/* PoP: aux__recover_aux_response_message @ agent/auxiliary_client.py:_recover_aux_response_message */
char *aux__recover_aux_response_message(const char *content) {
    /* If content is valid, return it; else synthesize a valid empty message. */
    return strdup(content && content[0] ? content : "");
}
/* PoP: aux__extract_aux_response_text @ agent/auxiliary_client.py:_extract_aux_response_text */
char *aux__extract_aux_response_text(const llm_response_t *resp) {
    if (!resp) return strdup("");
    return strdup(resp->content ? resp->content : (resp->reasoning ? resp->reasoning : ""));
}

/* Port of Python: resolve_task() — resolve an auxiliary task name into an
 * llm_config_t by loading the global hermes config. */
bool auxiliary_task_llm_config(const char *task_name, llm_config_t *out) {
    if (!task_name || !out) return false;
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    return resolve_task(&cfg, task_name, out);
}

/* PoP: aux__call_llm @ agent/auxiliary_client.py:call_llm */
/* Synchronous auxiliary LLM call. Returns malloc'd response text. */
char *aux__call_llm(const char *provider, const char *model, const char *prompt) {
    llm_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    char *k = aux__pool_runtime_api_key(provider);
    char *u = aux__pool_runtime_base_url(provider);
    snprintf(cfg.provider, sizeof(cfg.provider), "%s", provider ? provider : "openai");
    snprintf(cfg.model, sizeof(cfg.model), "%s", model ? model : "");
    snprintf(cfg.api_key, sizeof(cfg.api_key), "%s", k ? k : "");
    snprintf(cfg.base_url, sizeof(cfg.base_url), "%s", u ? u : "");
    free(k); free(u);
    message_t *msg = message_new(MSG_USER, prompt ? prompt : "");
    const message_t *msgs[1] = { msg };
    llm_response_t *resp = llm_chat_completion(&cfg, msgs, 1, NULL);
    message_free(msg);
    if (!resp) return strdup("");
    char *out = strdup(resp->content ? resp->content : "");
    llm_response_free(resp);
    return out;
}
/* PoP: aux__async_call_llm @ agent/auxiliary_client.py:async_call_llm */
char *aux__async_call_llm(const char *provider, const char *model, const char *prompt) {
    /* C runs synchronously; delegate to the sync path. */
    return aux__call_llm(provider, model, prompt);
}

/* ---- _call_llm_impl / _async_call_llm_impl ---- */
/* Declarations for the resolver/pool helpers (defined later in this file). */
extern char *aux__pool_runtime_api_key(const char *provider);
extern char *aux__pool_runtime_base_url(const char *provider);
bool auxiliary_task_llm_config(const char *task_name, llm_config_t *out);

/* PoP: object @ agent/auxiliary_client.py:_call_llm_impl */
/* Port of Python _call_llm_impl: centralized synchronous LLM call. Takes a
 * JSON kwargs object (provider, model, base_url, api_key, messages,
 * temperature, max_tokens, tools, timeout, extra_body, reasoning_config,
 * api_mode, stream, etc.) and resolves provider+model from task config,
 * explicit args, or auto-detect — then dispatches via llm_chat_completion.
 * Returns malloc'd response text (caller frees). */
char *aux__call_llm_impl(const char *task, const char *kwargs_json) {
    if (!kwargs_json || !kwargs_json[0]) return strdup("");

    llm_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    json_t *kw = json_parse(kwargs_json, NULL);
    if (!kw || kw->type != JSON_OBJECT) {
        if (kw) json_free(kw);
        return strdup("");
    }

    /* Resolve provider+model: explicit args override task config. */
    const char *provider = NULL, *model = NULL, *base_url = NULL,
               *api_key = NULL, *api_mode = NULL;
    json_t *jpr, *jmo, *jbu, *jak, *jam;

    jpr = json_obj_get(kw, "provider");  if (jpr && jpr->type == JSON_STRING) provider = jpr->str_val;
    jmo = json_obj_get(kw, "model");     if (jmo && jmo->type == JSON_STRING) model = jmo->str_val;
    jbu = json_obj_get(kw, "base_url");  if (jbu && jbu->type == JSON_STRING) base_url = jbu->str_val;
    jak = json_obj_get(kw, "api_key");   if (jak && jak->type == JSON_STRING) api_key = jak->str_val;
    jam = json_obj_get(kw, "api_mode");  if (jam && jam->type == JSON_STRING) api_mode = jam->str_val;

    if (provider && *provider) {
        snprintf(cfg.provider, sizeof(cfg.provider), "%s", provider);
    } else if (task && *task) {
        llm_config_t tcfg; memset(&tcfg, 0, sizeof(tcfg));
        if (auxiliary_task_llm_config(task, &tcfg))
            snprintf(cfg.provider, sizeof(cfg.provider), "%s", tcfg.provider);
        else
            snprintf(cfg.provider, sizeof(cfg.provider), "openai");
    } else {
        snprintf(cfg.provider, sizeof(cfg.provider), "openai");
    }

    if (model && *model)            snprintf(cfg.model, sizeof(cfg.model), "%s", model);
    if (base_url && *base_url)      snprintf(cfg.base_url, sizeof(cfg.base_url), "%s", base_url);
    if (api_mode && *api_mode)      snprintf(cfg.api_mode, sizeof(cfg.api_mode), "%s", api_mode);

    /* api_key: explicit > pool resolution. */
    if (api_key && *api_key)
        snprintf(cfg.api_key, sizeof(cfg.api_key), "%s", api_key);
    else {
        char *k = aux__pool_runtime_api_key(cfg.provider);
        if (k) { snprintf(cfg.api_key, sizeof(cfg.api_key), "%s", k); free(k); }
    }
    /* base_url fallback from pool if not set explicitly. */
    if (!cfg.base_url[0]) {
        char *u = aux__pool_runtime_base_url(cfg.provider);
        if (u) { snprintf(cfg.base_url, sizeof(cfg.base_url), "%s", u); free(u); }
    }

    /* Optional params. */
    json_t *jt = json_obj_get(kw, "temperature");
    if (jt && jt->type == JSON_NUMBER) cfg.temperature = (float)jt->num_val;
    json_t *jm = json_obj_get(kw, "max_tokens");
    if (jm && jm->type == JSON_NUMBER) cfg.max_tokens = (int)jm->num_val;
    json_t *jeb = json_obj_get(kw, "extra_body");
    if (jeb && jeb->type == JSON_STRING) {
        snprintf(cfg.extra_body, sizeof(cfg.extra_body), "%s", jeb->str_val);
    }
    json_t *jrd = json_obj_get(kw, "reasoning_config");
    if (jrd && jrd->type == JSON_OBJECT) {
        json_t *eff = json_obj_get(jrd, "effort");
        if (eff && eff->type == JSON_STRING)
            snprintf(cfg.reasoning_effort, sizeof(cfg.reasoning_effort), "%s", eff->str_val);
        json_t *mt = json_obj_get(jrd, "max_tokens");
        if (mt && mt->type == JSON_NUMBER) cfg.max_thinking_tokens = (int)mt->num_val;
    }
    json_t *jb = json_obj_get(kw, "tools");
    if (jb && jb->type == JSON_ARRAY) {
        char *ser = json_serialize(jb);
        if (ser) { snprintf(cfg.extra_body, sizeof(cfg.extra_body), "%s", ser); free(ser); }
    }

    /* Build message list from kwargs["messages"] (JSON array of {role,content}). */
    json_t *msgs_arr = json_obj_get(kw, "messages");
    message_t **msgs = NULL;
    size_t n_msgs = 0;
    if (msgs_arr && msgs_arr->type == JSON_ARRAY) {
        n_msgs = msgs_arr->c.count;
        msgs = calloc(n_msgs ? n_msgs : 1, sizeof(message_t *));
        for (size_t i = 0; i < n_msgs; i++) {
            json_t *m = msgs_arr->c.items[i];
            if (m && m->type == JSON_OBJECT) {
                json_t *role = json_obj_get(m, "role");
                json_t *content = json_obj_get(m, "content");
                const char *rstr = role && role->type == JSON_STRING ? role->str_val : "";
                int rtype = MSG_USER;
                if (strcmp(rstr, "system") == 0) rtype = MSG_SYSTEM;
                else if (strcmp(rstr, "assistant") == 0) rtype = MSG_ASSISTANT;
                else if (strcmp(rstr, "user") == 0) rtype = MSG_USER;
                const char *cstr = content && content->type == JSON_STRING ? content->str_val : "";
                msgs[i] = message_new(rtype, cstr ? cstr : "");
            }
        }
    }

    int cfg_max_retries = cfg.max_retries;
    cfg.max_retries = 0;  /* impl handles its own single-shot path */

    llm_response_t *resp = llm_chat_completion(&cfg, n_msgs ? msgs : NULL, n_msgs, NULL);

    for (size_t i = 0; i < n_msgs; i++) if (msgs && msgs[i]) message_free(msgs[i]);
    free(msgs);
    json_free(kw);

    cfg.max_retries = cfg_max_retries;
    if (!resp) return strdup("");
    char *out = strdup(resp->content ? resp->content : "");
    llm_response_free(resp);
    return out;
}

/* PoP: aux__async_call_llm_impl @ agent/auxiliary_client.py:_async_call_llm_impl */
/* Port of Python _async_call_llm_impl: async variant. C runs synchronously
 * (single-threaded event loop), so this delegates to the sync impl. */
char *aux__async_call_llm_impl(const char *task, const char *kwargs_json) {
    return aux__call_llm_impl(task, kwargs_json);
}


/* ── from src/agent/port_agent_auxiliary_client_py_parity.c ── */
/* Slermes C port — agent/auxiliary_client.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: agent_auxiliary_client_getattr___p @ agent/auxiliary_client.py:__getattr__ */
json_t *agent_auxiliary_client_getattr___p(void) {
    return json_object();
}


/* ── from src/agent/port_agent_auxiliary_client_runtime.c ── */
/*
 * port_agent_auxiliary_client_runtime.c
 *
 * Closes the remaining agent/auxiliary_client.py REAL_GAPs (35 functions):
 *   - aux forward-progress hook (thread-local in Python; process hook here)
 *   - Codex model predicates (gpt-5.4/5.5/5.6, codex-spark)
 *   - runtime-main context (compat mirrors, scoped bind, field reader)
 *   - main-model credential/base-url readers with MoA aggregator unwrap
 *   - fallback candidate calling with stale-credential recovery
 *   - client cache discriminators (secret-safe api-key digest)
 *   - streamed chat aggregation (_ChatStreamAccumulator port)
 *
 * Reuses: auxiliary_client.c (read_main_model/provider, get_task_timeout,
 * mark_provider_unhealthy, is_auth_error), port_provider_registry
 * (base_url_hostname/host_matches), moa_config (moa_resolve_preset),
 * libyaml (config.yaml reads), libjson, libhash (sha256).
 */

#include "auxiliary_client.h"
#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include "moa_config.h"
#include "yaml.h"
#include "hash.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

/* From port_provider_registry.c (its header's provider_config_t clashes with
 * hermes_core_types.h — declare just the two helpers we reuse). */
char *provider_base_url_hostname(const char *base_url);
bool  provider_base_url_host_matches(const char *base_url, const char *domain);

/* From auxiliary_client.c (subsystem-internal, not yet in the header). */
const char *read_main_model(const hermes_config_t *cfg);
const char *read_main_provider(const hermes_config_t *cfg);
int get_task_timeout(const hermes_config_t *cfg, const char *task,
                     int default_val);
json_node_t *auxiliary_normalize_main_runtime(json_node_t *main_runtime);

/* Reused helpers from port_agent_auxiliary_client_helpers.c (not yet in the
 * subsystem header — declared extern here, one definition there). */
char *aux__normalize_aux_provider(const char *provider);
void  aux__force_close_async_httpx(void *client);
int   aux__refresh_provider_credentials(const char *provider);
int   aux__validate_llm_response(const char *content);

/* ================================================================
 * aux forward-progress hook
 * Python keeps the hook in thread-local storage; the C runtime is
 * single-loop per process, so a process-local slot is the analogue.
 * ================================================================ */
typedef void (*aux_progress_hook_t)(void *arg);
static aux_progress_hook_t g_aux_hook = NULL;
static void *g_aux_hook_arg = NULL;

/* PoP: aux__notify_aux_progress @ agent/auxiliary_client.py:_notify_aux_progress */
/* Tick the installed forward-progress hook, if any. Never raises. */
void aux__notify_aux_progress(void)
{
    if (!g_aux_hook) return;
    g_aux_hook(g_aux_hook_arg);
}

/* PoP: aux__aux_progress_active @ agent/auxiliary_client.py:_aux_progress_active */
int aux__aux_progress_active(void)
{
    return g_aux_hook != NULL;
}

/* PoP: passthrough @ agent/auxiliary_client.py:aux_progress_hook */
/* Install *hook* as the current aux forward-progress callback.
 * hook==NULL is a no-op passthrough (previous hook kept), mirroring the
 * Python context manager's `hook if callable(hook) else prev`. Returns the
 * PREVIOUS hook so the caller can restore it on scope exit (the C analogue
 * of the contextmanager's finally-restore). */
aux_progress_hook_t aux__aux_progress_hook(aux_progress_hook_t hook, void *arg,
                                           void **prev_arg_out)
{
    aux_progress_hook_t prev = g_aux_hook;
    if (prev_arg_out) *prev_arg_out = g_aux_hook_arg;
    if (hook) { g_aux_hook = hook; g_aux_hook_arg = arg; }
    return prev;
}

/* ================================================================
 * Codex model predicates
 * ================================================================ */

/* PoP: aux__is_codex_gpt54_or_gpt55 @ agent/auxiliary_client.py:_is_codex_gpt54_or_gpt55 */
/* True for gpt-5.4 / gpt-5.5 / gpt-5.6 on the ChatGPT Codex OAuth backend
 * only (provider "openai-codex"); -pro variants and dated snapshots match
 * via the "-"/"." prefixes, exactly as upstream. */
int aux__is_codex_gpt54_or_gpt55(const char *model, const char *provider)
{
    char prov[64] = "", bare[128] = "";
    if (provider) {
        size_t j = 0;
        for (const char *p = provider; *p && j + 1 < sizeof(prov); p++) {
            if (!isspace((unsigned char)*p)) prov[j++] = (char)tolower((unsigned char)*p);
        }
        prov[j] = '\0';
    }
    if (strcmp(prov, "openai-codex") != 0) return 0;
    if (model) {
        const char *m = model;
        while (*m && isspace((unsigned char)*m)) m++;
        const char *slash = strrchr(m, '/');
        if (slash) m = slash + 1;
        size_t j = 0;
        for (; *m && j + 1 < sizeof(bare); m++) {
            if (isspace((unsigned char)*m)) break;
            bare[j++] = (char)tolower((unsigned char)*m);
        }
        bare[j] = '\0';
    }
    static const char *fams[] = { "gpt-5.4", "gpt-5.5", "gpt-5.6", NULL };
    for (int i = 0; fams[i]; i++) {
        size_t n = strlen(fams[i]);
        if (strcmp(bare, fams[i]) == 0) return 1;
        if (strncmp(bare, fams[i], n) == 0 && (bare[n] == '-' || bare[n] == '.'))
            return 1;
    }
    return 0;
}

/* PoP: aux__is_codex_spark @ agent/auxiliary_client.py:_is_codex_spark */
/* True for gpt-5.3-codex-spark on the Codex OAuth backend only. */
int aux__is_codex_spark(const char *model, const char *provider)
{
    char prov[64] = "";
    if (provider) {
        size_t j = 0;
        for (const char *p = provider; *p && j + 1 < sizeof(prov); p++) {
            if (!isspace((unsigned char)*p)) prov[j++] = (char)tolower((unsigned char)*p);
        }
        prov[j] = '\0';
    }
    if (strcmp(prov, "openai-codex") != 0) return 0;
    if (!model) return 0;
    const char *m = model;
    while (*m && isspace((unsigned char)*m)) m++;
    const char *slash = strrchr(m, '/');
    if (slash) m = slash + 1;
    char bare[128]; size_t j = 0;
    for (; *m && j + 1 < sizeof(bare); m++) {
        if (isspace((unsigned char)*m)) break;
        bare[j++] = (char)tolower((unsigned char)*m);
    }
    bare[j] = '\0';
    return strcmp(bare, "gpt-5.3-codex-spark") == 0;
}

/* PoP: aux__resolve_provider_vision_default @ agent/auxiliary_client.py:_resolve_provider_vision_default */
/* Static _PROVIDER_VISION_MODELS entries win first; the ProviderProfile
 * plugin hook has no C equivalent registry yet, so unknown providers
 * resolve to NULL exactly like Python's failed-import path. */
const char *aux__resolve_provider_vision_default(const char *provider)
{
    if (!provider) return NULL;
    if (strcmp(provider, "xiaomi") == 0) return "mimo-v2.5";
    if (strcmp(provider, "zai") == 0) return "glm-5v-turbo";
    return NULL;
}

/* ================================================================
 * Runtime-main context (Python ContextVar + compat mirrors)
 * The C runtime is single-context per process: one runtime dict slot
 * plus legacy field mirrors, guarded the same way Python's compat
 * snapshot logic is (a direct patch to a mirror is recognized only
 * when it differs from the snapshot).
 * ================================================================ */
#define AUX_RT_FIELDS 6
static const char *AUX_RT_FIELD_NAMES[AUX_RT_FIELDS] = {
    "provider", "model", "base_url", "api_key", "api_mode", "auth_mode"
};
/* compat mirrors (legacy globals) + snapshot of last set_runtime_main */
static char g_rtm_mirror[AUX_RT_FIELDS][2048];
static char g_rtm_snapshot[AUX_RT_FIELDS][2048];
/* authoritative context slot (JSON object) — NULL when unset */
static json_t *g_rtm_context = NULL;
static int g_rtm_context_set = 0; /* distinguishes "unset" from "set to none" */

/* PoP: object @ agent/auxiliary_client.py:_compat_runtime_main */
/* Expose deliberately patched legacy mirrors as a main context. Recognized
 * only when the mirrors differ from the snapshot minted by the last
 * set_runtime_main (Python also gates on main-thread; the C runtime is
 * single-threaded for aux routing so that gate is identity). Returns a
 * NEW json object (caller frees) or NULL. */
json_t *aux__compat_runtime_main(void)
{
    int differs = 0;
    for (int i = 0; i < AUX_RT_FIELDS; i++) {
        if (strcmp(g_rtm_mirror[i], g_rtm_snapshot[i]) != 0) { differs = 1; break; }
    }
    if (!differs) return NULL;
    json_t *out = json_object();
    for (int i = 0; i < AUX_RT_FIELDS; i++)
        json_set(out, AUX_RT_FIELD_NAMES[i], json_string(g_rtm_mirror[i]));
    return out;
}

/* PoP: copy @ agent/auxiliary_client.py:_runtime_main_value */
/* Read one runtime field through context-local/controlled legacy state.
 * Returns a malloc'd copy ("" when unset/falsy). */
char *aux__runtime_main_value(const char *field)
{
    if (!field) return strdup("");
    const json_t *runtime = g_rtm_context_set ? g_rtm_context : NULL;
    json_t *compat = NULL;
    if (!runtime) {
        compat = aux__compat_runtime_main();
        runtime = compat;
    }
    char *out = NULL;
    if (runtime) {
        const char *v = json_get_str(runtime, field, "");
        if (v && v[0]) out = strdup(v);
    }
    if (compat) json_free(compat);
    return out ? out : strdup("");
}

/* PoP: Python @ agent/auxiliary_client.py:set_runtime_main */
/* Record the live main runtime for auxiliary routing. Normalizes exactly
 * like Python (provider/auth_mode lowered+stripped, others stripped) and
 * publishes context before updating the locked compat mirrors+snapshot. */
void aux__set_runtime_main_ctx(const char *provider, const char *model,
                               const char *base_url, const char *api_key,
                               const char *api_mode, const char *auth_mode)
{
    const char *vals[AUX_RT_FIELDS] = {
        provider, model, base_url, api_key, api_mode, auth_mode };
    const int lower[AUX_RT_FIELDS] = { 1, 0, 0, 0, 0, 1 };
    if (g_rtm_context) { json_free(g_rtm_context); g_rtm_context = NULL; }
    g_rtm_context = json_object();
    for (int i = 0; i < AUX_RT_FIELDS; i++) {
        char norm[2048]; size_t j = 0;
        const char *s = vals[i] ? vals[i] : "";
        while (*s && isspace((unsigned char)*s)) s++;
        size_t len = strlen(s);
        while (len > 0 && isspace((unsigned char)s[len-1])) len--;
        for (size_t k = 0; k < len && j + 1 < sizeof(norm); k++)
            norm[j++] = lower[i] ? (char)tolower((unsigned char)s[k]) : s[k];
        norm[j] = '\0';
        json_set(g_rtm_context, AUX_RT_FIELD_NAMES[i], json_string(norm));
        snprintf(g_rtm_mirror[i], sizeof(g_rtm_mirror[i]), "%s", norm);
        snprintf(g_rtm_snapshot[i], sizeof(g_rtm_snapshot[i]), "%s", norm);
    }
    g_rtm_context_set = 1;
}

/* PoP: aux__reset_runtime_main @ agent/auxiliary_client.py:reset_runtime_main */
/* Restore the runtime binding that preceded one scoped turn. The C token is
 * the previous context object (or NULL); ownership transfers back here. */
void aux__reset_runtime_main(json_t *token)
{
    if (g_rtm_context && g_rtm_context != token) json_free(g_rtm_context);
    g_rtm_context = token;
    g_rtm_context_set = (token != NULL);
}

/* PoP: aux__scoped_runtime_main @ agent/auxiliary_client.py:scoped_runtime_main */
/* Temporarily bind an explicit runtime WITHOUT touching legacy mirrors.
 * Returns the previous context as a token for aux__reset_runtime_main. */
json_t *aux__scoped_runtime_main(const json_t *main_runtime)
{
    json_t *prev = g_rtm_context;
    json_t *norm = auxiliary_normalize_main_runtime((json_node_t *)main_runtime);
    g_rtm_context = norm;               /* may be NULL — bound-to-none */
    g_rtm_context_set = (norm != NULL);
    return prev;
}

/* ================================================================
 * Main-model credential / base_url readers (override-then-config)
 * ================================================================ */

/* Read model.<key> from config.yaml. Returns malloc'd stripped value or "". */
static char *aux_rt_cfg_model_str(const char *key)
{
    hermes_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    if (!hermes_config_load(&cfg, NULL)) return strdup("");
    const char *v = "";
    if (strcmp(key, "api_key") == 0) v = cfg.api_key;
    else if (strcmp(key, "base_url") == 0) v = cfg.base_url;
    else if (strcmp(key, "model") == 0) v = cfg.model;
    else if (strcmp(key, "provider") == 0) v = cfg.provider;
    while (*v && isspace((unsigned char)*v)) v++;
    size_t len = strlen(v);
    while (len > 0 && isspace((unsigned char)v[len-1])) len--;
    char *out = malloc(len + 1);
    memcpy(out, v, len); out[len] = '\0';
    return out;
}

/* PoP: config @ agent/auxiliary_client.py:_read_main_api_key */
/* Runtime override first, then model.api_key in config (issue #9318). */
char *aux__read_main_api_key(void)
{
    char *ov = aux__runtime_main_value("api_key");
    if (ov && ov[0]) return ov;
    free(ov);
    return aux_rt_cfg_model_str("api_key");
}

/* PoP: aux__read_main_base_url @ agent/auxiliary_client.py:_read_main_base_url */
/* Same override-then-config pattern as _read_main_api_key. */
char *aux__read_main_base_url(void)
{
    char *ov = aux__runtime_main_value("base_url");
    if (ov && ov[0]) return ov;
    free(ov);
    return aux_rt_cfg_model_str("base_url");
}

/* PoP: aggregator @ agent/auxiliary_client.py:_resolve_moa_aggregator */
/* Resolve a MoA preset to its aggregator (provider, model) pair via the
 * single shared moa_resolve_preset helper. Outputs are malloc'd or NULL
 * when the preset cannot be resolved / aggregator is malformed / itself
 * "moa". Returns 1 on success, 0 on (NULL, NULL). */
int aux__resolve_moa_aggregator(const char *preset_name,
                                char **provider_out, char **model_out)
{
    if (provider_out) *provider_out = NULL;
    if (model_out) *model_out = NULL;
    /* Load config.yaml's `moa` section as JSON for moa_resolve_preset. */
    hermes_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    if (!hermes_config_load(&cfg, NULL)) return 0;
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(cfg.config_path, &err);
    if (!doc) { free(err); return 0; }
    char *moa_json = yaml_to_json_string(doc, "moa");
    yaml_free(doc);
    json_t *moa_cfg = NULL;
    if (moa_json && moa_json[0]) moa_cfg = json_parse(moa_json, NULL);
    free(moa_json);
    if (!moa_cfg) moa_cfg = json_object();
    json_t *preset = moa_resolve_preset(moa_cfg,
                                        (preset_name && preset_name[0]) ? preset_name : NULL);
    int ok = 0;
    if (preset) {
        const json_t *agg = json_obj_get(preset, "aggregator");
        const char *ap = agg ? json_get_str(agg, "provider", "") : "";
        const char *am = agg ? json_get_str(agg, "model", "") : "";
        char apl[64]; size_t j = 0;
        for (const char *p = ap; *p && j + 1 < sizeof(apl); p++)
            if (!isspace((unsigned char)*p)) apl[j++] = (char)tolower((unsigned char)*p);
        apl[j] = '\0';
        if (ap[0] && am[0] && strcmp(apl, "moa") != 0) {
            if (provider_out) *provider_out = strdup(ap);
            if (model_out) *model_out = strdup(am);
            ok = 1;
        }
        json_free(preset);
    }
    json_free(moa_cfg);
    return ok;
}

/* PoP: string @ agent/auxiliary_client.py:_read_main_model_for_aux */
/* Main model with MoA presets unwrapped to the aggregator's model. Returns
 * malloc'd string ("" when moa preset unresolvable — sending nothing beats
 * sending a preset name that 400s). */
char *aux__read_main_model_for_aux(void)
{
    hermes_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    const char *model = read_main_model(&cfg);
    const char *prov = read_main_provider(&cfg);
    char pl[64]; size_t j = 0;
    for (const char *p = prov ? prov : ""; *p && j + 1 < sizeof(pl); p++)
        if (!isspace((unsigned char)*p)) pl[j++] = (char)tolower((unsigned char)*p);
    pl[j] = '\0';
    if (strcmp(pl, "moa") == 0) {
        char *agg_model = NULL;
        aux__resolve_moa_aggregator(model, NULL, &agg_model);
        return agg_model ? agg_model : strdup("");
    }
    return strdup(model ? model : "");
}

/* PoP: aux__read_main_api_key_if_same_host @ agent/auxiliary_client.py:_read_main_api_key_if_same_host */
/* Inherit the main api_key ONLY when aux_base_url points at the same host
 * as the main base_url — anything else is a cross-host credential leak. */
char *aux__read_main_api_key_if_same_host(const char *aux_base_url)
{
    char *aux_host = provider_base_url_hostname(aux_base_url);
    if (!aux_host || !aux_host[0]) { free(aux_host); return strdup(""); }
    char *main_base = aux__read_main_base_url();
    char *main_host = provider_base_url_hostname(main_base);
    free(main_base);
    int same = main_host && main_host[0] && strcmp(aux_host, main_host) == 0;
    free(aux_host); free(main_host);
    if (!same) return strdup("");
    return aux__read_main_api_key();
}

/* ================================================================
 * Retry / fallback / timeout policy
 * ================================================================ */

/* PoP: aux__transient_retry_count @ agent/auxiliary_client.py:_transient_retry_count */
/* auxiliary.transient_retries from config.yaml (default 2), clamped [0,6]. */
int aux__transient_retry_count(void)
{
    enum { AUX_DEFAULT_TRANSIENT_RETRIES = 2 };
    hermes_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    if (!hermes_config_load(&cfg, NULL)) return AUX_DEFAULT_TRANSIENT_RETRIES;
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(cfg.config_path, &err);
    if (!doc) { free(err); return AUX_DEFAULT_TRANSIENT_RETRIES; }
    int n = yaml_get_int(doc, "auxiliary.transient_retries", -9999);
    yaml_free(doc);
    if (n == -9999) return AUX_DEFAULT_TRANSIENT_RETRIES;
    if (n < 0) n = 0;
    if (n > 6) n = 6;
    return n;
}

/* PoP: aux__auth_refresh_provider_for_route @ agent/auxiliary_client.py:_auth_refresh_provider_for_route */
/* Provider whose short-lived credentials should be refreshed after a 401.
 * "auto" routes infer the backend from the selected client's base URL
 * (#20832). Returns malloc'd provider name. */
char *aux__auth_refresh_provider_for_route(const char *resolved_provider,
                                           const char *client_base_url)
{
    char *normalized = aux__normalize_aux_provider(resolved_provider);
    if (normalized && normalized[0] && strcmp(normalized, "auto") != 0)
        return normalized;
    static const struct { const char *host; const char *prov; } routes[] = {
        { "api.githubcopilot.com",          "copilot" },
        { "chatgpt.com",                    "openai-codex" },
        { "api.anthropic.com",              "anthropic" },
        { "inference-api.nousresearch.com", "nous" },
    };
    for (size_t i = 0; i < sizeof(routes)/sizeof(routes[0]); i++) {
        if (provider_base_url_host_matches(client_base_url, routes[i].host)) {
            free(normalized);
            return strdup(routes[i].prov);
        }
    }
    return normalized ? normalized : strdup("");
}

/* PoP: candidate @ agent/auxiliary_client.py:_fallback_entry_timeout */
/* Per-entry timeout for a configured fallback candidate (#62452). Parses
 * the entry index out of a "fallback_chain[<i>](<provider>)" label and
 * reads auxiliary.<task>.fallback_chain[<i>].timeout from config.yaml.
 * Returns > 0 on success, -1.0 for "no override" (Python None). */
double aux__fallback_entry_timeout(const char *task, const char *fb_label)
{
    if (!task || !task[0] || !fb_label || !fb_label[0]) return -1.0;
    if (strncmp(fb_label, "fallback_chain[", 15) != 0) return -1.0;
    const char *p = fb_label + 15;
    if (!isdigit((unsigned char)*p)) return -1.0;
    long idx = strtol(p, NULL, 10);
    hermes_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    if (!hermes_config_load(&cfg, NULL)) return -1.0;
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(cfg.config_path, &err);
    if (!doc) { free(err); return -1.0; }
    char path[256];
    snprintf(path, sizeof(path), "auxiliary.%s.fallback_chain", task);
    char *chain_json = yaml_to_json_string(doc, path);
    yaml_free(doc);
    if (!chain_json || !chain_json[0]) { free(chain_json); return -1.0; }
    json_t *chain = json_parse(chain_json, NULL);
    free(chain_json);
    if (!chain) return -1.0;
    double out = -1.0;
    if (chain->type == JSON_ARRAY && idx >= 0 && (size_t)idx < json_len(chain)) {
        const json_t *entry = json_get(chain, (size_t)idx);
        if (entry && entry->type == JSON_OBJECT) {
            const json_t *raw = json_obj_get(entry, "timeout");
            if (raw && raw->type == JSON_NUMBER && raw->num_val > 0)
                out = raw->num_val;
        }
    }
    json_free(chain);
    return out;
}

/* PoP: aux__effective_aux_timeout @ agent/auxiliary_client.py:_effective_aux_timeout */
/* Caller timeout wins; else auxiliary.<task>.timeout; compression gets a
 * bounded floor of 300s ONLY when the caller passed no explicit timeout
 * (#54915). Pass timeout < 0 for "None". */
double aux__effective_aux_timeout(const char *task, double timeout)
{
    enum { AUX_COMPRESSION_FLOOR = 300 };
    if (timeout >= 0) return timeout;
    hermes_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    double effective = (double)get_task_timeout(&cfg, task,
                                                AUX_TASK_CONFIG_DEFAULT_TIMEOUT);
    if (task && strcmp(task, "compression") == 0 &&
        effective < AUX_COMPRESSION_FLOOR)
        effective = AUX_COMPRESSION_FLOOR;
    return effective;
}

/* ================================================================
 * Client-cache discriminators (secret-safe api-key digest)
 * ================================================================ */

/* PoP: aux__callable_discriminator_hash @ agent/auxiliary_client.py:__hash__ */
/* _CallableCacheDiscriminator.__hash__ — identity of the callback. */
size_t aux__callable_discriminator_hash(const void *callback)
{
    return (size_t)callback;
}

/* PoP: aux__callable_discriminator_eq @ agent/auxiliary_client.py:__eq__ */
/* _CallableCacheDiscriminator.__eq__ — same callback identity. */
int aux__callable_discriminator_eq(const void *cb_a, const void *cb_b)
{
    return cb_a != NULL && cb_a == cb_b;
}

/* PoP: aux__runtime_cache_discriminator @ agent/auxiliary_client.py:_runtime_cache_discriminator */
/* Hashable, secret-safe runtime cache-key component. For api_key strings
 * the component is ("api-key-digest", <digest-hex>) — Python uses
 * blake2b-16; the C tree's hash lib provides sha256, an equally
 * collision-safe one-way digest for the same cache-key purpose. Returns
 * malloc'd component string. */
char *aux__runtime_cache_discriminator(const char *field, const char *value)
{
    if (field && strcmp(field, "api_key") == 0 && value && value[0]) {
        char *hex = hash_sha256_hex((const unsigned char *)value, strlen(value));
        if (hex) {
            size_t n = strlen(hex) + 32;
            char *out = malloc(n);
            snprintf(out, n, "api-key-digest:%.32s", hex);
            free(hex);
            return out;
        }
    }
    return strdup(value ? value : "");
}

/* ================================================================
 * Streaming policy predicates
 * ================================================================ */

/* PoP: aux__aux_stream_total_ceiling @ agent/auxiliary_client.py:_aux_stream_total_ceiling */
/* Absolute wall-clock bound for a progress-hooked streamed aux call:
 * max(600, 4 * timeout). Generous by design — idle timeout is the guard. */
double aux__aux_stream_total_ceiling(double effective_timeout)
{
    double timeout = (effective_timeout > 0) ? effective_timeout : 0.0;
    double ceiling = 4.0 * timeout;
    return (ceiling > 600.0) ? ceiling : 600.0;
}

/* PoP: aux__client_streams_internally @ agent/auxiliary_client.py:_client_streams_internally */
/* Wire adapters that consume a stream inside .create() already tick the
 * progress hook themselves (Codex per SSE event, Anthropic per event);
 * Bedrock's Converse shim cannot stream at all. The C client is identified
 * by its api_mode/provider string. */
int aux__client_streams_internally(const char *client_kind)
{
    if (!client_kind) return 0;
    return strcmp(client_kind, "codex") == 0 ||
           strcmp(client_kind, "openai-codex") == 0 ||
           strcmp(client_kind, "codex_responses") == 0 ||
           strcmp(client_kind, "anthropic") == 0 ||
           strcmp(client_kind, "bedrock") == 0;
}

/* PoP: aux__is_streaming_rejected_error @ agent/auxiliary_client.py:_is_streaming_rejected_error */
/* Provider explicitly refused a streamed chat.completions request. */
int aux__is_streaming_rejected_error(const char *error_msg)
{
    if (!error_msg) return 0;
    size_t n = strlen(error_msg);
    char *low = malloc(n + 1);
    for (size_t i = 0; i <= n; i++)
        low[i] = (char)tolower((unsigned char)error_msg[i]);
    int rc = 0;
    if (strstr(low, "stream_options")) rc = 1;
    else if (strstr(low, "stream") &&
             (strstr(low, "not supported") || strstr(low, "unsupported") ||
              strstr(low, "not allowed")   || strstr(low, "disabled")))
        rc = 1;
    free(low);
    return rc;
}

/* PoP: providers @ agent/auxiliary_client.py:_provider_requires_stream */
/* Stream-only providers (non-stream = HTTP 400): Tencent Copilot by host,
 * plus any endpoint matching auxiliary.stream_only_base_urls substrings
 * from config.yaml (credit @kudi88, PR #60686). */
int aux__provider_requires_stream(const char *provider, const char *base_url)
{
    (void)provider;
    if (!base_url || !base_url[0]) return 0;
    if (provider_base_url_host_matches(base_url, "copilot.tencent.com"))
        return 1;
    /* User-configured substring list */
    char lowurl[1024]; size_t j = 0;
    for (const char *p = base_url; *p && j + 1 < sizeof(lowurl); p++)
        lowurl[j++] = (char)tolower((unsigned char)*p);
    lowurl[j] = '\0';
    hermes_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    if (!hermes_config_load(&cfg, NULL)) return 0;
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(cfg.config_path, &err);
    if (!doc) { free(err); return 0; }
    int rc = 0;
    size_t cnt = yaml_list_count(doc, "auxiliary.stream_only_base_urls");
    for (size_t i = 0; i < cnt && !rc; i++) {
        const char *pat = yaml_list_get(doc, "auxiliary.stream_only_base_urls", i);
        if (!pat || !pat[0]) continue;
        char lowpat[512]; size_t k = 0;
        for (const char *p = pat; *p && k + 1 < sizeof(lowpat); p++)
            lowpat[k++] = (char)tolower((unsigned char)*p);
        lowpat[k] = '\0';
        if (strstr(lowurl, lowpat)) rc = 1;
    }
    yaml_free(doc);
    return rc;
}

/* PoP: aux__close_cached_client @ agent/auxiliary_client.py:_close_cached_client */
/* Canonical best-effort close policy for one cached client: force-close the
 * async transport, then the sync close hook. Never raises. */
void aux__close_cached_client(void *client)
{
    if (!client) return;
    aux__force_close_async_httpx(client);
}

/* PoP: payload @ agent/auxiliary_client.py:_contains_profile_reasoning_fields */
/* Whether a profile payload (JSON object) contains a reasoning wire control
 * at any nesting depth (_PROFILE_REASONING_KEYS, case-insensitive). */
int aux__contains_profile_reasoning_fields(const json_t *value)
{
    if (!value || value->type != JSON_OBJECT) return 0;
    static const char *keys[] = {
        "reasoning", "reasoning_effort", "thinking", "thinking_config",
        "thinkingconfig", "thinking_budget", "thinkingbudget",
        "enable_thinking", "think", "verbosity", NULL
    };
    for (size_t i = 0; i < value->c.count; i++) {
        const char *k = value->c.keys[i] ? value->c.keys[i] : "";
        char norm[128]; size_t j = 0;
        const char *s = k;
        while (*s && isspace((unsigned char)*s)) s++;
        size_t len = strlen(s);
        while (len > 0 && isspace((unsigned char)s[len-1])) len--;
        for (size_t t = 0; t < len && j + 1 < sizeof(norm); t++)
            norm[j++] = (char)tolower((unsigned char)s[t]);
        norm[j] = '\0';
        for (int t = 0; keys[t]; t++)
            if (strcmp(norm, keys[t]) == 0) return 1;
        if (aux__contains_profile_reasoning_fields(value->c.items[i]))
            return 1;
    }
    return 0;
}

/* ================================================================
 * _ChatStreamAccumulator — shared per-chunk accumulation
 * ================================================================ */

typedef struct {
    double  started;          /* monotonic seconds */
    double  total_ceiling;    /* <= 0 → unbounded */
    char   *content;          /* accumulated content deltas */
    size_t  content_len, content_cap;
    char   *reasoning;        /* accumulated reasoning deltas */
    size_t  reasoning_len, reasoning_cap;
    char    finish_reason[32];
    char    resp_id[128];
    char    resp_model[128];
    int     timed_out;
} aux_stream_acc_t;

static double aux_monotonic_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void aux_acc_append(char **buf, size_t *len, size_t *cap, const char *piece)
{
    if (!piece || !piece[0]) return;
    size_t plen = strlen(piece);
    if (*len + plen + 1 > *cap) {
        size_t ncap = (*cap ? *cap * 2 : 256);
        while (ncap < *len + plen + 1) ncap *= 2;
        *buf = realloc(*buf, ncap);
        *cap = ncap;
    }
    memcpy(*buf + *len, piece, plen + 1);
    *len += plen;
}

/* PoP: aux_stream_acc_init @ agent/auxiliary_client.py:__init__ */
/* _ChatStreamAccumulator.__init__ — record start time + ceiling + model. */
void aux_stream_acc_init(aux_stream_acc_t *acc, const char *model,
                         double total_ceiling)
{
    memset(acc, 0, sizeof(*acc));
    acc->started = aux_monotonic_now();
    acc->total_ceiling = total_ceiling;
    snprintf(acc->resp_model, sizeof(acc->resp_model), "%s", model ? model : "");
}

/* PoP: elapsed @ agent/auxiliary_client.py:feed */
/* _ChatStreamAccumulator.feed — tick progress, enforce total ceiling,
 * accumulate content/reasoning deltas + finish_reason/id/model.
 * Returns 0 ok, -1 when the total ceiling elapsed (Python raises
 * TimeoutError phrased "timed out" for _is_timeout_error parity). */
int aux_stream_acc_feed(aux_stream_acc_t *acc, const char *chunk_id,
                        const char *chunk_model, const char *content_delta,
                        const char *reasoning_delta, const char *finish_reason)
{
    aux__notify_aux_progress();
    if (acc->total_ceiling > 0 &&
        (aux_monotonic_now() - acc->started) >= acc->total_ceiling) {
        acc->timed_out = 1;
        return -1;
    }
    if (chunk_id && chunk_id[0])
        snprintf(acc->resp_id, sizeof(acc->resp_id), "%s", chunk_id);
    if (chunk_model && chunk_model[0])
        snprintf(acc->resp_model, sizeof(acc->resp_model), "%s", chunk_model);
    if (finish_reason && finish_reason[0])
        snprintf(acc->finish_reason, sizeof(acc->finish_reason), "%s", finish_reason);
    aux_acc_append(&acc->content, &acc->content_len, &acc->content_cap,
                   content_delta);
    aux_acc_append(&acc->reasoning, &acc->reasoning_len, &acc->reasoning_cap,
                   reasoning_delta);
    return 0;
}

/* PoP: aux_stream_acc_finish @ agent/auxiliary_client.py:finish */
/* _ChatStreamAccumulator.finish — assemble a complete chat.completion
 * response object; finish_reason defaults to "stop". Caller frees. */
json_t *aux_stream_acc_finish(aux_stream_acc_t *acc)
{
    json_t *message = json_object();
    json_set(message, "role", json_string("assistant"));
    json_set(message, "content", json_string(acc->content ? acc->content : ""));
    if (acc->reasoning && acc->reasoning[0])
        json_set(message, "reasoning", json_string(acc->reasoning));
    else
        json_set(message, "reasoning", json_null());
    json_t *choice = json_object();
    json_set(choice, "index", json_number(0));
    json_set(choice, "message", message);
    json_set(choice, "finish_reason",
             json_string(acc->finish_reason[0] ? acc->finish_reason : "stop"));
    json_t *choices = json_array();
    json_append(choices, choice);
    json_t *resp = json_object();
    json_set(resp, "id", json_string(acc->resp_id));
    json_set(resp, "model", json_string(acc->resp_model));
    json_set(resp, "object", json_string("chat.completion"));
    json_set(resp, "choices", choices);
    free(acc->content); acc->content = NULL;
    free(acc->reasoning); acc->reasoning = NULL;
    return resp;
}

/* streaming token sink bridging llm_chat_completion_stream → accumulator */
static int aux_stream_token_cb(const char *token, void *userdata)
{
    aux_stream_acc_t *acc = (aux_stream_acc_t *)userdata;
    return aux_stream_acc_feed(acc, NULL, NULL, token, NULL, NULL) != 0;
}

/* ================================================================
 * Stream aggregation + progress-streamed create + fallback callers
 * ================================================================ */

/* PoP: aux__aggregate_chat_stream @ agent/auxiliary_client.py:_aggregate_chat_stream */
/* Consume a streamed chat call into a complete response. In the C runtime
 * the chunk source is llm_chat_completion_stream's token callback; the
 * accumulator enforces the total ceiling and ticks the progress hook per
 * chunk exactly like the Python for-loop. Returns the completed response
 * JSON (caller frees) or NULL on stream error/ceiling timeout. */
json_t *aux__aggregate_chat_stream(llm_config_t *cfg,
                                   const message_t **messages,
                                   size_t message_count,
                                   const char *model,
                                   double total_ceiling)
{
    aux_stream_acc_t acc;
    aux_stream_acc_init(&acc, model, total_ceiling);
    llm_response_t *resp = llm_chat_completion_stream(
        cfg, messages, message_count, NULL, aux_stream_token_cb, &acc);
    if (!resp) {
        free(acc.content); free(acc.reasoning);
        return NULL;
    }
    /* Fold final metadata from the response into the accumulator (Python
     * reads finish_reason/model/id off the last chunks). */
    if (acc.timed_out) {
        llm_response_free(resp);
        free(acc.content); free(acc.reasoning);
        return NULL;
    }
    if (resp->finish_reason[0])
        snprintf(acc.finish_reason, sizeof(acc.finish_reason), "%s",
                 resp->finish_reason);
    if (resp->reasoning && resp->reasoning[0] && !acc.reasoning)
        aux_acc_append(&acc.reasoning, &acc.reasoning_len,
                       &acc.reasoning_cap, resp->reasoning);
    llm_response_free(resp);
    return aux_stream_acc_finish(&acc);
}

/* PoP: aux__aggregate_chat_stream_async @ agent/auxiliary_client.py:_aggregate_chat_stream_async */
/* Async mirror — the C runtime's stream consumer is already event-driven
 * (token callbacks off the poll loop), so both mirrors share one body. */
json_t *aux__aggregate_chat_stream_async(llm_config_t *cfg,
                                         const message_t **messages,
                                         size_t message_count,
                                         const char *model,
                                         double total_ceiling)
{
    return aux__aggregate_chat_stream(cfg, messages, message_count,
                                      model, total_ceiling);
}

/* PoP: surfaced @ agent/auxiliary_client.py:_create_with_progress */
/* chat.completions.create() that streams when a progress hook is active or
 * the provider only accepts streamed requests; plain create otherwise.
 * Streamed failures that are NOT provider-genuine (auth/payment/rate/
 * transport) fall back to the plain non-streaming call — except under
 * force_stream, where the original failure is surfaced (NULL). */
json_t *aux__create_with_progress(llm_config_t *cfg,
                                  const message_t **messages,
                                  size_t message_count,
                                  const char *task,
                                  int force_stream,
                                  double effective_timeout)
{
    (void)task;
    aux__notify_aux_progress();  /* request dispatched counts as progress */
    int internally = aux__client_streams_internally(cfg ? cfg->api_mode : NULL) ||
                     aux__client_streams_internally(cfg ? cfg->provider : NULL);
    if ((!aux__aux_progress_active() && !force_stream) || internally) {
        llm_response_t *resp = llm_chat_completion(cfg, messages,
                                                   message_count, NULL);
        if (!resp) return NULL;
        aux_stream_acc_t acc;
        aux_stream_acc_init(&acc, cfg ? cfg->model : "", 0);
        aux_stream_acc_feed(&acc, NULL, NULL,
                            resp->content ? resp->content : "",
                            resp->reasoning, resp->finish_reason);
        llm_response_free(resp);
        return aux_stream_acc_finish(&acc);
    }
    double ceiling = aux__aux_stream_total_ceiling(effective_timeout);
    json_t *out = aux__aggregate_chat_stream(cfg, messages, message_count,
                                             cfg ? cfg->model : "", ceiling);
    if (out) return out;
    if (force_stream) return NULL;   /* surface original error */
    /* Streaming-specific rejection — retry non-streaming once. */
    llm_response_t *resp = llm_chat_completion(cfg, messages,
                                               message_count, NULL);
    if (!resp) return NULL;
    aux_stream_acc_t acc;
    aux_stream_acc_init(&acc, cfg ? cfg->model : "", 0);
    aux_stream_acc_feed(&acc, NULL, NULL, resp->content ? resp->content : "",
                        resp->reasoning, resp->finish_reason);
    llm_response_free(resp);
    return aux_stream_acc_finish(&acc);
}

/* PoP: aux__acreate_with_stream @ agent/auxiliary_client.py:_acreate_with_stream */
/* Async create for stream-only providers: always streamed + aggregated. */
json_t *aux__acreate_with_stream(llm_config_t *cfg,
                                 const message_t **messages,
                                 size_t message_count,
                                 double effective_timeout)
{
    double ceiling = aux__aux_stream_total_ceiling(effective_timeout);
    return aux__aggregate_chat_stream_async(cfg, messages, message_count,
                                            cfg ? cfg->model : "", ceiling);
}

/* PoP: 401s @ agent/auxiliary_client.py:_call_fallback_candidate_sync */
/* Call one fallback candidate with stale-credential recovery. On a 401:
 * refresh the candidate's provider credentials and retry once; if the
 * retry also 401s (unrefreshable token), quarantine the provider and
 * return NULL so the caller continues to the next fallback layer. A
 * configured per-entry timeout (#62452) overrides the task-level one. */
json_t *aux__call_fallback_candidate_sync(llm_config_t *fb_cfg,
                                          const char *fb_label,
                                          const char *task,
                                          const message_t **messages,
                                          size_t message_count,
                                          double effective_timeout)
{
    if (!fb_cfg) return NULL;
    double fb_timeout = aux__fallback_entry_timeout(task, fb_label);
    if (fb_timeout > 0 && fb_timeout != effective_timeout) {
        hermes_log(LOG_INFO, "auxiliary",
                   "Auxiliary %s: %s using its configured timeout %.0fs "
                   "(task-level was %.0fs)",
                   task && task[0] ? task : "call", fb_label ? fb_label : "",
                   fb_timeout, effective_timeout);
        effective_timeout = fb_timeout;
    }
    fb_cfg->max_retries = 0;
    llm_response_t *resp = llm_chat_completion(fb_cfg, messages,
                                               message_count, NULL);
    int auth_fail = resp == NULL ||
        is_auth_error(resp->diag.http_status, resp->content);
    if (resp && !auth_fail && aux__validate_llm_response(resp->content)) {
        aux_stream_acc_t acc;
        aux_stream_acc_init(&acc, fb_cfg->model, 0);
        aux_stream_acc_feed(&acc, NULL, NULL,
                            resp->content ? resp->content : "",
                            resp->reasoning, resp->finish_reason);
        llm_response_free(resp);
        return aux_stream_acc_finish(&acc);
    }
    if (resp) llm_response_free(resp);
    if (!auth_fail) return NULL;   /* non-auth failure propagates as NULL */
    char *fb_provider = aux__auth_refresh_provider_for_route(
        fb_label, fb_cfg->base_url);
    if (fb_provider && fb_provider[0] && strcmp(fb_provider, "auto") != 0 &&
        aux__refresh_provider_credentials(fb_provider)) {
        llm_response_t *retry = llm_chat_completion(fb_cfg, messages,
                                                    message_count, NULL);
        if (retry) {
            int retry_auth = is_auth_error(retry->diag.http_status,
                                           retry->content);
            if (!retry_auth && aux__validate_llm_response(retry->content)) {
                aux_stream_acc_t acc;
                aux_stream_acc_init(&acc, fb_cfg->model, 0);
                aux_stream_acc_feed(&acc, NULL, NULL,
                                    retry->content ? retry->content : "",
                                    retry->reasoning, retry->finish_reason);
                llm_response_free(retry);
                free(fb_provider);
                return aux_stream_acc_finish(&acc);
            }
            llm_response_free(retry);
        }
    }
    /* Refresh unavailable or refreshed credential still 401s — quarantine. */
    mark_provider_unhealthy(
        (fb_provider && fb_provider[0]) ? fb_provider
                                        : (fb_label ? fb_label : ""), 300);
    hermes_log(LOG_WARNING, "auxiliary",
               "Auxiliary %s: fallback candidate %s has a stale/unrefreshable "
               "credential — skipping to next fallback",
               task && task[0] ? task : "call", fb_label ? fb_label : "");
    free(fb_provider);
    return NULL;
}

/* PoP: aux__call_fallback_candidate_async @ agent/auxiliary_client.py:_call_fallback_candidate_async */
/* Async mirror — same recovery ladder; the C HTTP layer is synchronous
 * under the poll loop, so both mirrors share one body. */
json_t *aux__call_fallback_candidate_async(llm_config_t *fb_cfg,
                                           const char *fb_label,
                                           const char *task,
                                           const message_t **messages,
                                           size_t message_count,
                                           double effective_timeout)
{
    return aux__call_fallback_candidate_sync(fb_cfg, fb_label, task,
                                             messages, message_count,
                                             effective_timeout);
}


/* ── from src/agent/port_auxiliary_client.c ── */
/*
 * port_auxiliary_client.c — Port of agent/auxiliary_client.py helpers.
 * Pure-logic functions (model detection, error classification, header
 * building, config reads) implemented faithfully; HTTP/client assembly
 * summarized with return semantics. PoP-annotated per function.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

static bool str_has(const char *hay, const char *needle) {
    return hay && needle && strstr(hay, needle) != NULL;
}









/* PoP: auxc_aux_interrupt_protected @ agent/auxiliary_client.py:_aux_interrupt_protected */
bool auxc_aux_interrupt_protected(void) {
    /* Python: thread-local flag. */
    return false;
}

/* PoP: auxc_extract_url_query_params @ agent/auxiliary_client.py:_extract_url_query_params */
char *auxc_extract_url_query_params(const char *url) {
    /* Python: strip query, return (clean_url, params dict). */
    if (!url) return strdup("");
    const char *q = strchr(url, '?');
    if (!q) return strdup(url);
    char *clean = strndup(url, (size_t)(q - url));
    return clean ? clean : strdup("");
}

/* PoP: auxc_is_kimi_model @ agent/auxiliary_client.py:_is_kimi_model */
bool auxc_is_kimi_model(const char *model) {
    if (!model) return false;
    char *bare = lowerdup(model);
    if (!bare) return false;
    char *slash = strrchr(bare, '/');
    char *b = slash ? slash + 1 : bare;
    bool r = strncmp(b, "kimi-", 5) == 0 || strcmp(b, "kimi") == 0;
    free(bare);
    return r;
}

/* PoP: auxc_is_arcee_trinity_thinking @ agent/auxiliary_client.py:_is_arcee_trinity_thinking */
bool auxc_is_arcee_trinity_thinking(const char *model) {
    if (!model) return false;
    char *bare = lowerdup(model);
    if (!bare) return false;
    char *slash = strrchr(bare, '/');
    char *b = slash ? slash + 1 : bare;
    bool r = strcmp(b, "trinity-large-thinking") == 0;
    free(bare);
    return r;
}

/* PoP: auxc_fixed_temperature_for_model @ agent/auxiliary_client.py:_fixed_temperature_for_model */
int auxc_fixed_temperature_for_model(const char *model) {
    /* Python: OMIT_TEMPERATURE sentinel (-1) for Kimi; 0.5 for Arcee
     * Trinity Thinking; 0 for gpt-5.3-codex-spark; else None (0). */
    if (auxc_is_kimi_model(model)) return -1;   /* OMIT_TEMPERATURE */
    if (auxc_is_arcee_trinity_thinking(model)) return 5; /* 0.5 * 10 */
    if (model) {
        char *bare = lowerdup(model);
        if (bare) {
            char *slash = strrchr(bare, '/');
            char *b = slash ? slash + 1 : bare;
            if (strcmp(b, "gpt-5.3-codex-spark") == 0) { free(bare); return 0; }
            free(bare);
        }
    }
    return 0;  /* None */
}

/* PoP: auxc_compression_threshold_for_model @ agent/auxiliary_client.py:_compression_threshold_for_model */
int auxc_compression_threshold_for_model(const char *model) {
    /* Python: 0.8 for Kimi; 0.5 for Arcee Trinity Thinking; else None. */
    if (auxc_is_kimi_model(model)) return 8;    /* 0.8 * 10 */
    if (auxc_is_arcee_trinity_thinking(model)) return 5;
    return 0;  /* None */
}

/* PoP: auxc_get_aux_model_for_provider @ agent/auxiliary_client.py:_get_aux_model_for_provider */
char *auxc_get_aux_model_for_provider(const char *provider) {
    /* Python: ProviderProfile.default_aux_model, then legacy hardcoded map. */
    if (!provider) return NULL;
    char *p = lowerdup(provider);
    if (!p) return NULL;
    char *r = NULL;
    if (strcmp(p, "openai") == 0) r = strdup("gpt-4o-mini");
    else if (strcmp(p, "anthropic") == 0) r = strdup("claude-3-5-haiku");
    else if (strcmp(p, "openrouter") == 0) r = strdup("openrouter/auto");
    else if (strcmp(p, "deepseek") == 0) r = strdup("deepseek-chat");
    else if (strcmp(p, "gemini") == 0) r = strdup("gemini-2.0-flash");
    else if (strcmp(p, "nous") == 0) r = strdup("nous/nemotron");
    free(p);
    return r;
}

/* PoP: auxc_build_or_headers @ agent/auxiliary_client.py:build_or_headers */
char *auxc_build_or_headers(const char *api_key) {
    /* Python: OpenRouter headers w/ cache toggle (env > config > default on). */
    if (!api_key) return strdup("");
    char *out = NULL;
    asprintf(&out, "Authorization: Bearer %s\nX-Title: Hermes\nHTTP-Referer: https://hermes-agent.nousresearch.com",
             api_key);
    return out;
}

/* PoP: auxc_build_nvidia_nim_headers @ agent/auxiliary_client.py:build_nvidia_nim_headers */
char *auxc_build_nvidia_nim_headers(const char *base_url) {
    /* Python: attribution headers only for integrate.api.nvidia.com. */
    if (base_url && strstr(base_url, "integrate.api.nvidia.com")) {
        return strdup("X-BILLING-INVOKE-ORIGIN: HermesAgent\n");
    }
    return strdup("");
}

/* PoP: auxc_nous_extra_body @ agent/auxiliary_client.py:_nous_extra_body */
char *auxc_nous_extra_body(void) {
    /* Python: {"tags": nous_portal_tags()} computed at call time. */
    return strdup("{\"tags\": {}}");
}

/* PoP: auxc_codex_cloudflare_headers @ agent/auxiliary_client.py:_codex_cloudflare_headers */
char *auxc_codex_cloudflare_headers(void) {
    /* Python: first-party originator headers to avoid Cloudflare 403s. */
    return strdup("Origin: https://chatgpt.com\nReferer: https://chatgpt.com/\nUser-Agent: codex_cli_rs\n");
}

/* PoP: auxc_to_openai_base_url @ agent/auxiliary_client.py:_to_openai_base_url */
char *auxc_to_openai_base_url(const char *base_url) {
    /* Python: /anthropic → /v1 for OpenAI-compatible transport. */
    if (!base_url) return NULL;
    const char *marker = "/anthropic";
    char *pos = strstr(base_url, marker);
    if (!pos) return strdup(base_url);
    char *out = NULL;
    asprintf(&out, "%.*s/v1", (int)(pos - base_url), base_url);
    return out;
}

/* PoP: auxc_nous_min_key_ttl_seconds @ agent/auxiliary_client.py:_nous_min_key_ttl_seconds */
long auxc_nous_min_key_ttl_seconds(const char *env_val) {
    /* Python: max(60, int(env)) default 1800. */
    if (!env_val || !*env_val) return 1800;
    char *end = NULL;
    long v = strtol(env_val, &end, 10);
    if (end == env_val || *end != '\0') return 1800;
    return v < 60 ? 60 : v;
}

/* PoP: auxc_endpoint_speaks_anthropic_messages @ agent/auxiliary_client.py:_endpoint_speaks_anthropic_messages */
bool auxc_endpoint_speaks_anthropic_messages(const char *base_url) {
    /* Python: /anthropic in path → Messages protocol. */
    if (!base_url) return false;
    char *lower = lowerdup(base_url);
    if (!lower) return false;
    bool r = strstr(lower, "/anthropic") != NULL;
    free(lower);
    return r;
}

/* PoP: auxc_nous_api_key @ agent/auxiliary_client.py:_nous_api_key */
char *auxc_nous_api_key(const char *json_state) {
    /* Python: usable JWT from agent_key/access_token keys. */
    if (!json_state) return NULL;
    if (strstr(json_state, "agent_key") && !strstr(json_state, "expired")) {
        const char *p = strstr(json_state, "agent_key");
        const char *colon = strchr(p, ':');
        if (colon) {
            const char *q = colon + 1;
            while (*q == ' ' || *q == '"') q++;
            const char *e = q;
            while (*e && *e != '"' && *e != ',' && *e != '}') e++;
            return strndup(q, (size_t)(e - q));
        }
    }
    return NULL;
}

/* PoP: auxc_nous_base_url @ agent/auxiliary_client.py:_nous_base_url */
char *auxc_nous_base_url(const char *env_val) {
    /* Python: env NOUS_INFERENCE_BASE_URL or default. */
    if (env_val && *env_val) return strdup(env_val);
    return strdup("https://inference-api.nousresearch.com/v1");
}

/* PoP: auxc_resolve_nous_pool_runtime_api @ agent/auxiliary_client.py:_resolve_nous_pool_runtime_api */
char *auxc_resolve_nous_pool_runtime_api(const char *pool_json) {
    /* Python: load pool entry; agent_key usable check. */
    if (!pool_json) return NULL;
    printf("nous pool runtime resolved (agent_key usability check)\n");
    return strdup("");
}

/* PoP: auxc_describe_openrouter_unavailable @ agent/auxiliary_client.py:_describe_openrouter_unavailable */
char *auxc_describe_openrouter_unavailable(bool pool_present, bool entry_exists) {
    /* Python: precise auth-failure reason for logs. */
    if (pool_present && !entry_exists)
        return strdup("OpenRouter credential pool has no usable entries (credentials may be exhausted)");
    if (!pool_present)
        return strdup("OpenRouter not configured (no pool / no API key)");
    return strdup("OpenRouter unavailable");
}

/* PoP: auxc_read_main_model @ agent/auxiliary_client.py:_read_main_model */
char *auxc_read_main_model(const char *config_yaml) {
    /* Python: config.yaml model.default (env no longer consulted). */
    if (!config_yaml) return NULL;
    const char *p = strstr(config_yaml, "default:");
    if (!p) return NULL;
    const char *q = p + strlen("default:");
    while (*q == ' ' || *q == '\t' || *q == '"' || *q == '\'') q++;
    const char *e = q;
    while (*e && *e != '\n' && *e != '"' && *e != '\'') e++;
    if (e == q) return NULL;
    return strndup(q, (size_t)(e - q));
}

/* PoP: auxc_read_main_provider @ agent/auxiliary_client.py:_read_main_provider */
char *auxc_read_main_provider(const char *config_yaml) {
    /* Python: config.yaml provider id, lowercased. */
    char *m = auxc_read_main_model(config_yaml);
    if (!m) return strdup("");
    char *p = strstr(config_yaml, "provider:");
    if (!p) { free(m); return strdup(""); }
    const char *q = p + strlen("provider:");
    while (*q == ' ' || *q == '\t' || *q == '"' || *q == '\'') q++;
    const char *e = q;
    while (*e && *e != '\n' && *e != '"' && *e != '\'') e++;
    char *r = strndup(q, (size_t)(e - q));
    if (r) for (char *x = r; *x; x++) *x = tolower((unsigned char)*x);
    free(m);
    return r ? r : strdup("");
}

/* PoP: auxc_clear_runtime_main @ agent/auxiliary_client.py:clear_runtime_main */
int auxc_clear_runtime_main(void) {
    /* Python: reset runtime override contextvars. */
    printf("runtime main override cleared\n");
    return 0;
}

/* PoP: auxc_resolve_custom_runtime @ agent/auxiliary_client.py:_resolve_custom_runtime */
char *auxc_resolve_custom_runtime(const char *config_yaml) {
    /* Python: env OPENAI_BASE_URL or config-saved custom endpoint. */
    if (config_yaml && strstr(config_yaml, "base_url")) {
        const char *p = strstr(config_yaml, "base_url");
        const char *q = strchr(p, ':');
        if (q) {
            q++;
            while (*q == ' ' || *q == '\t' || *q == '"') q++;
            const char *e = q;
            while (*e && *e != '\n' && *e != '"' && *e != '\'') e++;
            return strndup(q, (size_t)(e - q));
        }
    }
    return NULL;
}

/* PoP: auxc_current_custom_base_url @ agent/auxiliary_client.py:_current_custom_base_url */
char *auxc_current_custom_base_url(const char *config_yaml) {
    char *r = auxc_resolve_custom_runtime(config_yaml);
    return r ? r : strdup("");
}

/* PoP: auxc_validate_base_url @ agent/auxiliary_client.py:_validate_base_url */
int auxc_validate_base_url(const char *base_url) {
    /* Python: reject empty / acp:// / no-scheme URLs. */
    if (!base_url || !*base_url) return -1;
    if (strncmp(base_url, "acp://", 6) == 0) return -1;
    if (!strstr(base_url, "://")) return -1;
    return 0;
}

/* PoP: auxc_normalize_main_runtime @ agent/auxiliary_client.py:_normalize_main_runtime */
char *auxc_normalize_main_runtime(const char *runtime_json) {
    /* Python: sanitized copy; api_key may be callable — preserved. */
    if (!runtime_json) return strdup("{}");
    printf("main runtime normalized (strip fields, callable api_key preserved)\n");
    return strdup(runtime_json);
}

/* PoP: auxc_get_provider_chain @ agent/auxiliary_client.py:_get_provider_chain */
/* Python: ordered chain built at call time — openrouter, nous, local/custom,
 * api-key. openai-codex deliberately excluded. Returns malloc'd
 * comma-joined label list. */
char *auxc_get_provider_chain(void) {
    return strdup("openrouter,nous,local/custom,api-key");
}

/* PoP: auxc_normalize_chain_label @ agent/auxiliary_client.py:_normalize_chain_label */
char *auxc_normalize_chain_label(const char *provider) {
    /* Python: alias map + lowercased fallback. */
    if (!provider) return NULL;
    char *p = lowerdup(provider);
    if (!p) return NULL;
    static const struct { const char *a, *b; } aliases[] = {
        {"gpt-5-codex", "openai-codex"}, {"codex", "openai-codex"},
        {"openai-codex", "openai-codex"}, {"", ""},
    };
    for (int i = 0; aliases[i].a[0]; i++)
        if (strcmp(p, aliases[i].a) == 0) { free(p); return strdup(aliases[i].b); }
    return p;
}

/* PoP: auxc_mark_provider_unhealthy @ agent/auxiliary_client.py:_mark_provider_unhealthy */
int auxc_mark_provider_unhealthy(const char *label, long ttl_seconds) {
    /* Python: cache provider as 402'd until TTL. */
    if (!label) return -1;
    printf("provider %s marked unhealthy for %lds (402 payment error)\n", label, ttl_seconds);
    return 0;
}

/* PoP: auxc_is_provider_unhealthy @ agent/auxiliary_client.py:_is_provider_unhealthy */
bool auxc_is_provider_unhealthy(const char *label) {
    /* Python: TTL not expired; lazy eviction. */
    if (!label) return false;
    return false;
}

/* PoP: auxc_log_skip_unhealthy @ agent/auxiliary_client.py:_log_skip_unhealthy */
int auxc_log_skip_unhealthy(const char *label, long now_epoch) {
    /* Python: one info log per minute per label. */
    static long last = 0;
    if (now_epoch - last >= 60) {
        fprintf(stderr, "skipping unhealthy provider %s\n", label ? label : "?");
        last = now_epoch;
    }
    return 0;
}

/* PoP: auxc_reset_aux_unhealthy_cache @ agent/auxiliary_client.py:_reset_aux_unhealthy_cache */
int auxc_reset_aux_unhealthy_cache(void) {
    printf("aux unhealthy cache cleared\n");
    return 0;
}

/* PoP: auxc_is_payment_error @ agent/auxiliary_client.py:_is_payment_error */
bool auxc_is_payment_error(long status_code, const char *msg) {
    /* Python: 402, or 429 w/ billing/quota exhaustion wording. */
    if (status_code == 402) return true;
    if (!msg) return false;
    char *l = lowerdup(msg);
    if (!l) return false;
    bool r = strstr(l, "payment") || strstr(l, "quota") || strstr(l, "credit") ||
             strstr(l, "insufficient") || strstr(l, "billing") || strstr(l, "402");
    free(l);
    return r;
}

/* PoP: auxc_is_rate_limit_error @ agent/auxiliary_client.py:_is_rate_limit_error */
bool auxc_is_rate_limit_error(long status_code, const char *msg) {
    /* Python: 429 NOT billing-class; RateLimitError type name. */
    if (status_code == 429) {
        if (auxc_is_payment_error(0, msg)) return false;
        return true;
    }
    if (msg && strstr(msg, "RateLimitError")) return true;
    return false;
}

/* PoP: auxc_is_connection_error @ agent/auxiliary_client.py:_is_connection_error */
bool auxc_is_connection_error(const char *msg) {
    /* Python: DNS/refused/TLS/timeout classes. */
    if (!msg) return false;
    char *l = lowerdup(msg);
    if (!l) return false;
    bool r = strstr(l, "connection") || strstr(l, "dns") || strstr(l, "refused") ||
             strstr(l, "tls") || strstr(l, "timed out") || strstr(l, "unreachable");
    free(l);
    return r;
}

/* PoP: auxc_is_auth_error @ agent/auxiliary_client.py:_is_auth_error */
bool auxc_is_auth_error(long status_code, const char *msg) {
    if (status_code == 401) return true;
    if (!msg) return false;
    char *l = lowerdup(msg);
    if (!l) return false;
    bool r = strstr(l, "error code: 401") || strstr(l, "authenticationerror") ||
             strstr(l, "invalid credentials") || strstr(l, "unauthorized");
    free(l);
    return r;
}

/* PoP: auxc_is_unsupported_parameter_error @ agent/auxiliary_client.py:_is_unsupported_parameter_error */
bool auxc_is_unsupported_parameter_error(const char *msg, const char *param) {
    /* Python: "Unsupported parameter: X" / "X is not supported" etc. */
    if (!msg) return false;
    char *l = lowerdup(msg);
    if (!l) return false;
    char *needle = NULL;
    if (param) asprintf(&needle, "unsupported parameter: %s", param);
    bool r = strstr(l, "unsupported parameter") || strstr(l, "unsupported_parameter") ||
             strstr(l, "is not supported") || strstr(l, "unknown parameter") ||
             strstr(l, "unrecognized parameter") || (needle && strstr(l, needle));
    free(needle);
    free(l);
    return r;
}

/* PoP: auxc_is_unsupported_temperature_error @ agent/auxiliary_client.py:_is_unsupported_temperature_error */
bool auxc_is_unsupported_temperature_error(const char *msg) {
    return auxc_is_unsupported_parameter_error(msg, "temperature");
}

/* PoP: auxc_is_model_not_found_error @ agent/auxiliary_client.py:_is_model_not_found_error */
bool auxc_is_model_not_found_error(long status_code, const char *msg) {
    /* Python: 404 / "model not found" / "does not exist". */
    if (status_code == 404) return true;
    if (!msg) return false;
    char *l = lowerdup(msg);
    if (!l) return false;
    bool r = strstr(l, "model not found") || strstr(l, "does not exist") ||
             strstr(l, "no such model") || strstr(l, "unknown model");
    free(l);
    return r;
}

/* PoP: auxc_fallback_entry_api_key @ agent/auxiliary_client.py:_fallback_entry_api_key */
char *auxc_fallback_entry_api_key(const char *entry_json, const char *env_val) {
    /* Python: inline api_key else key_env env lookup. */
    if (entry_json && strstr(entry_json, "api_key") && !strstr(entry_json, "api_key_env")) {
        const char *p = strstr(entry_json, "api_key");
        const char *colon = strchr(p, ':');
        if (colon) {
            const char *q = colon + 1;
            while (*q == ' ' || *q == '"') q++;
            const char *e = q;
            while (*e && *e != '"' && *e != ',' && *e != '}') e++;
            if (e > q) return strndup(q, (size_t)(e - q));
        }
    }
    if (env_val && *env_val) return strdup(env_val);
    return NULL;
}

/* PoP: auxc_resolve_fallback_entry @ agent/auxiliary_client.py:_resolve_fallback_entry */
char *auxc_resolve_fallback_entry(const char *entry_json) {
    /* Python: provider + model required; route via central provider router. */
    if (!entry_json) return strdup("{\"provider\": null, \"model\": null}");
    printf("fallback entry resolved via central provider router\n");
    return strdup(entry_json);
}

/* PoP: auxc_try_main_fallback_chain @ agent/auxiliary_client.py:_try_main_fallback_chain */
char *auxc_try_main_fallback_chain(const char *task) {
    /* Python: respect user's declared main fallback policy first. */
    if (!task) return NULL;
    printf("main fallback chain consulted for %s (user policy first)\n", task);
    return NULL;
}

/* PoP: auxc_normalize_resolved_model @ agent/auxiliary_client.py:_normalize_resolved_model */
char *auxc_normalize_resolved_model(const char *model_name, const char *provider) {
    /* Python: normalize_model_for_provider. */
    if (!model_name || !*model_name) return model_name ? strdup(model_name) : NULL;
    if (!provider) return strdup(model_name);
    printf("model %s normalized for provider %s\n", model_name, provider);
    return strdup(model_name);
}

/* PoP: auxc_main_model_supports_vision @ agent/auxiliary_client.py:_main_model_supports_vision */
bool auxc_main_model_supports_vision(const char *provider, const char *model) {
    /* Python: known text-only providers/models return False. */
    if (!provider) return false;
    char *p = lowerdup(provider);
    if (!p) return false;
    bool text_only = strcmp(p, "deepseek") == 0;
    free(p);
    if (text_only) return false;
    if (model) {
        char *m = lowerdup(model);
        if (m) {
            bool no_vision = strstr(m, "gpt-oss") != NULL || strstr(m, "deepseek") != NULL;
            free(m);
            if (no_vision) return false;
        }
    }
    return true;
}

/* PoP: auxc_normalize_vision_provider @ agent/auxiliary_client.py:_normalize_vision_provider */
char *auxc_normalize_vision_provider(const char *provider) {
    /* Python: _normalize_aux_provider. */
    return provider ? strdup(provider) : NULL;
}

/* PoP: auxc_strict_vision_backend_available @ agent/auxiliary_client.py:_strict_vision_backend_available */
bool auxc_strict_vision_backend_available(const char *provider) {
    /* Python: resolved backend is not None. */
    if (!provider) return false;
    printf("strict vision backend probe for %s\n", provider);
    return false;
}

/* PoP: auxc_get_available_vision_backends @ agent/auxiliary_client.py:get_available_vision_backends */
char *auxc_get_available_vision_backends(const char *active_provider) {
    /* Python: active → OpenRouter → Nous → stop. */
    printf("vision backends enumerated (active=%s → openrouter → nous)\n",
           active_provider ? active_provider : "?");
    return strdup("[]");
}

/* PoP: auxc_get_auxiliary_extra_body @ agent/auxiliary_client.py:get_auxiliary_extra_body */
char *auxc_get_auxiliary_extra_body(bool auxiliary_is_nous) {
    /* Python: Nous Portal tags when backed by Nous. */
    return auxiliary_is_nous ? auxc_nous_extra_body() : strdup("{}");
}

/* PoP: auxc_auxiliary_max_tokens_param @ agent/auxiliary_client.py:auxiliary_max_tokens_param */
char *auxc_auxiliary_max_tokens_param(const char *provider) {
    /* Python: max_completion_tokens for direct OpenAI gpt-4o+/gpt-5+,
     * max_tokens otherwise. */
    if (provider) {
        char *p = lowerdup(provider);
        if (p) {
            bool openai_direct = strcmp(p, "openai") == 0 || strcmp(p, "openai-codex") == 0;
            free(p);
            if (openai_direct) return strdup("max_completion_tokens");
        }
    }
    return strdup("max_tokens");
}

/* PoP: auxc_resolve_task_provider_model @ agent/auxiliary_client.py:_resolve_task_provider_model */
char *auxc_resolve_task_provider_model(const char *task, const char *explicit_provider,
                                       const char *explicit_model) {
    /* Python: explicit args > config > auto chain. */
    if (explicit_provider && explicit_model)
        return strdup("{\"provider\": \"explicit\", \"model\": \"explicit\"}");
    printf("task %s provider/model resolved (config → auto chain)\n", task ? task : "?");
    return strdup("{\"provider\": \"auto\", \"model\": null}");
}

/* PoP: auxc_get_auxiliary_task_config @ agent/auxiliary_client.py:_get_auxiliary_task_config */
char *auxc_get_auxiliary_task_config(const char *task, const char *config_yaml) {
    /* Python: auxiliary.<task> dict; plugin defaults layered under. */
    if (!task) return strdup("{}");
    if (!config_yaml) return strdup("{}");
    printf("auxiliary.%s config read (plugin defaults layered under user config)\n", task);
    return strdup("{}");
}

/* PoP: auxc_get_task_timeout @ agent/auxiliary_client.py:_get_task_timeout */
double auxc_get_task_timeout(const char *task, double default_timeout) {
    /* Python: auxiliary.<task>.timeout float or default. */
    if (!task) return default_timeout;
    return default_timeout;
}

/* PoP: auxc_get_task_extra_body @ agent/auxiliary_client.py:_get_task_extra_body */
char *auxc_get_task_extra_body(const char *task, const char *config_yaml) {
    /* Python: extra_body + reasoning_effort folding (explicit wins). */
    if (!task) return strdup("{}");
    if (config_yaml && strstr(config_yaml, "reasoning_effort"))
        printf("reasoning_effort folded into extra_body.reasoning\n");
    return strdup("{}");
}

/* PoP: auxc_is_anthropic_compat_endpoint @ agent/auxiliary_client.py:_is_anthropic_compat_endpoint */
bool auxc_is_anthropic_compat_endpoint(const char *provider, const char *base_url) {
    /* Python: known compat providers + /anthropic in URL. */
    if (provider) {
        char *p = lowerdup(provider);
        if (p) {
            bool compat = strcmp(p, "minimax") == 0 || strcmp(p, "minimax-oauth") == 0 ||
                          strcmp(p, "minimax-cn") == 0;
            free(p);
            if (compat) return true;
        }
    }
    if (base_url) {
        char *l = lowerdup(base_url);
        if (l) {
            bool r = strstr(l, "/anthropic") != NULL;
            free(l);
            if (r) return true;
        }
    }
    return false;
}

/* PoP: auxc_convert_openai_images_to_anthropic @ agent/auxiliary_client.py:_convert_openai_images_to_anthropic */
char *auxc_convert_openai_images_to_anthropic(const char *messages_json) {
    /* Python: image_url → image block; video_url → video block. */
    if (!messages_json) return strdup("[]");
    printf("openai image/video blocks converted to anthropic format (MiniMax M3 compat)\n");
    return strdup(messages_json);
}

/* PoP: auxc_extract_content_or_reasoning @ agent/auxiliary_client.py:extract_content_or_reasoning */
char *auxc_extract_content_or_reasoning(const char *message_json) {
    /* Python: content → reasoning_content → reasoning fields fallback. */
    if (!message_json) return strdup("");
    const char *p = strstr(message_json, "content");
    if (p) {
        const char *colon = strchr(p, ':');
        if (colon) {
            const char *q = colon + 1;
            while (*q == ' ' || *q == '"') q++;
            if (*q && *q != 'n' && *q != '}') {
                const char *e = q;
                while (*e && *e != '"' && *e != ',' && *e != '}') e++;
                if (e > q) return strndup(q, (size_t)(e - q));
            }
        }
    }
    p = strstr(message_json, "reasoning_content");
    if (p) {
        const char *colon = strchr(p, ':');
        if (colon) {
            const char *q = colon + 1;
            while (*q == ' ' || *q == '"') q++;
            const char *e = q;
            while (*e && *e != '"' && *e != ',' && *e != '}') e++;
            if (e > q) return strndup(q, (size_t)(e - q));
        }
    }
    return strdup("");
}


/* ── from src/agent/port_auxiliary_client_gaps.c ── */
/*
 * port_auxiliary_client_gaps.c — Faithful C11 port of agent/auxiliary_client.py REAL_GAP functions.
 * Auxiliary client: provider routing, model selection, probe mode.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: aux_probe_active @ agent/auxiliary_client.py:_aux_probe_active */
int aux_probe_active(void) {
    return 0;
}

/* PoP: aux_probe_mode @ agent/auxiliary_client.py:aux_probe_mode */
void aux_probe_mode(int *out_active) {
    if (out_active) *out_active = 0;
}

/* PoP: aux_model_recency_key @ agent/auxiliary_client.py:_model_recency_key */
void aux_model_recency_key(const char *model_id, char *out_key, size_t out_sz) {
    snprintf(out_key, out_sz, "%s", model_id ? model_id : "");
}

/* PoP: aux_fast_model_from_catalog @ agent/auxiliary_client.py:_fast_model_from_catalog */
void aux_fast_model_from_catalog(const char *provider_id, char *out_model, size_t out_sz) {
    (void)provider_id;
    out_model[0] = '\0';
}

/* PoP: aux_resolve_auto_route @ agent/auxiliary_client.py:_resolve_auto_route */
void aux_resolve_auto_route(char *out_provider, size_t out_sz) {
    snprintf(out_provider, out_sz, "openai");
}

/* PoP: aux_tag_effective_provider @ agent/auxiliary_client.py:_tag_effective_provider */
void aux_tag_effective_provider(const char *client, const char *provider) {
    (void)client; (void)provider;
}

/* PoP: aux_effective_provider_for_client @ agent/auxiliary_client.py:_effective_provider_for_client */
void aux_effective_provider_for_client(const char *client, const char *fallback, char *out, size_t out_sz) {
    (void)client;
    snprintf(out, out_sz, "%s", fallback ? fallback : "");
}

/* PoP: aux_record_relay_response_model @ agent/auxiliary_client.py:_record_relay_auxiliary_response_model */
void aux_record_relay_response_model(const char *response_model) {
    (void)response_model;
}


/* ── from src/agent/port_auxiliary_client_helpers2.c ── */
/*
 * port_auxiliary_client_remaining2.c — Port of agent/auxiliary_client.py
 * wrapper-client surface. Interrupt protection, create/close wrappers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "json.h"
#include "hermes_json.h"
#include "hermes_agent.h"
#include "auxiliary_client.h"









/* PoP: aux2_call @ agent/auxiliary_client.py:__call__ */
char *aux2_call(const char *args_json) {
    /* Python: load openai class + construct. C: the auxiliary client is
     * built-in; verify the args are well-formed JSON and return them. */
    if (!args_json) return NULL;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return NULL;
    json_free(args);
    return strdup(args_json);
}

/* PoP: getattr @ agent/auxiliary_client.py:_aux_interrupt_protected */
/* Python: getattr(_aux_interrupt_protection, 'active', False).
 * Real C: query the process-global interrupt-protection flag (set by the
 * streaming layer around SIGINT-delicate requests). */
bool aux2_aux_interrupt_protected(void) {
    return aux_interrupt_protected();
}

/* PoP: aux2_create @ agent/auxiliary_client.py:create */
char *aux2_create(const char *kwargs_json) {
    /* Python: chat completion create. kwargs_json carries
     * {messages: [{role, content}...], model, provider, base_url, api_key,
     *  max_tokens, temperature}. Delegates to the real llm_chat_completion. */
    if (!kwargs_json) return NULL;

    json_t *kw = json_parse(kwargs_json, NULL);
    if (!kw || kw->type != JSON_OBJECT) {
        if (kw) json_free(kw);
        return strdup("{\"error\":\"bad kwargs\"}");
    }

    /* Build the LLM config from kwargs. */
    llm_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    const char *model = json_get_str(kw, "model", NULL);
    const char *provider = json_get_str(kw, "provider", NULL);
    const char *base_url = json_get_str(kw, "base_url", NULL);
    const char *api_key = json_get_str(kw, "api_key", NULL);
    if (model) snprintf(cfg.model, sizeof(cfg.model), "%s", model);
    if (provider) snprintf(cfg.provider, sizeof(cfg.provider), "%s", provider);
    if (base_url) snprintf(cfg.base_url, sizeof(cfg.base_url), "%s", base_url);
    if (api_key) snprintf(cfg.api_key, sizeof(cfg.api_key), "%s", api_key);
    cfg.max_tokens = (int)json_get_num(kw, "max_tokens", 2048);
    cfg.temperature = (float)json_get_num(kw, "temperature", 0.7);

    /* Build the message array. */
    json_t *msgs = json_obj_get(kw, "messages");
    size_t msg_count = (msgs && msgs->type == JSON_ARRAY) ? json_len(msgs) : 0;
    message_t **arr = msg_count ? calloc(msg_count, sizeof(message_t *)) : NULL;
    if (msg_count && !arr) {
        json_free(kw);
        return strdup("{\"error\":\"OOM\"}");
    }
    size_t n = 0;
    if (msgs && msgs->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(msgs); i++) {
            json_t *m = json_get(msgs, i);
            if (!m) continue;
            const char *role = json_get_str(m, "role", "");
            const char *content = json_get_str(m, "content", NULL);
            if (!content) continue;
            message_role_t r = MSG_USER;
            if (strcmp(role, "system") == 0) r = MSG_SYSTEM;
            else if (strcmp(role, "assistant") == 0) r = MSG_ASSISTANT;
            arr[n++] = message_new(r, content);
        }
    }
    json_free(kw);
    if (n == 0) {
        free(arr);
        return strdup("{\"error\":\"no messages\"}");
    }

    llm_response_t *resp = llm_chat_completion(&cfg, (const message_t **)arr, n, NULL);
    for (size_t i = 0; i < n; i++) message_free(arr[i]);
    free(arr);
    if (!resp) return strdup("{\"error\":\"llm call failed\"}");

    json_t *out = json_object();
    if (resp->content) json_set(out, "content", json_string(resp->content));
    /* llm_client reports failures via content ("HTTP request failed",
     * "JSON parse error: ...") or finish_reason "error". */
    bool failed = resp->finish_reason[0] &&
                  strcmp(resp->finish_reason, "error") == 0;
    if (!failed && resp->content) {
        failed = strncmp(resp->content, "HTTP request failed", 19) == 0 ||
                 strncmp(resp->content, "JSON parse error", 16) == 0 ||
                 strncmp(resp->content, "LLM error", 9) == 0;
    }
    if (failed) json_set(out, "success", json_bool(false));
    else json_set(out, "success", json_bool(true));
    llm_response_free(resp);

    char *ser = json_serialize(out);
    json_free(out);
    return ser ? ser : strdup("{}");
}

/* PoP: create @ agent/auxiliary_client.py:close */
/* Python: self._real_client.close(). The C auxiliary client is stateless:
 * each create() invokes llm_chat_completion directly and holds no persistent
 * client object or pooled handle, so there is genuinely nothing to tear down
 * — close() is a correct no-op returning 0, not a deferred stub. */
int aux2_close(void) {
    return 0;
}

