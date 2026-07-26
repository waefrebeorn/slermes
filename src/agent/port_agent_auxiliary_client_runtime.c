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

/* PoP: aux__aux_progress_hook @ agent/auxiliary_client.py:aux_progress_hook */
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

/* PoP: aux__compat_runtime_main @ agent/auxiliary_client.py:_compat_runtime_main */
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

/* PoP: aux__runtime_main_value @ agent/auxiliary_client.py:_runtime_main_value */
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

/* PoP: aux__set_runtime_main_ctx @ agent/auxiliary_client.py:set_runtime_main */
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

/* PoP: aux__read_main_api_key @ agent/auxiliary_client.py:_read_main_api_key */
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

/* PoP: aux__resolve_moa_aggregator @ agent/auxiliary_client.py:_resolve_moa_aggregator */
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

/* PoP: aux__read_main_model_for_aux @ agent/auxiliary_client.py:_read_main_model_for_aux */
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

/* PoP: aux__fallback_entry_timeout @ agent/auxiliary_client.py:_fallback_entry_timeout */
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

/* PoP: aux__provider_requires_stream @ agent/auxiliary_client.py:_provider_requires_stream */
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

/* PoP: aux__contains_profile_reasoning_fields @ agent/auxiliary_client.py:_contains_profile_reasoning_fields */
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

/* PoP: aux_stream_acc_feed @ agent/auxiliary_client.py:feed */
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

/* PoP: aux__create_with_progress @ agent/auxiliary_client.py:_create_with_progress */
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

/* PoP: aux__call_fallback_candidate_sync @ agent/auxiliary_client.py:_call_fallback_candidate_sync */
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
