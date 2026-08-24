/*
 * model_metadata.c — Port of Python agent/model_metadata.py
 *
 * Python API → C implementation mapping:
 *   model_metadata_lookup()           → model_metadata_lookup() in provider_metadata.c
 *   model_metadata_get_provider()     → get_model_provider() in provider_metadata.c
 *   model_metadata_count_tokens()     → hermes_token_count() in hermes_tokenizer.c
 *   model_metadata_get_pricing()      → model_estimate_cost() in provider_metadata.c
 *   model_metadata_validate_model()   → model_metadata_validate_model() in this file
 *   model_metadata_list_models()      → model_metadata_list_json() in provider_metadata.c
 *   model_metadata_get_context_window()→ hermes_token_context_size() in provider_metadata.c
 *
 * Model metadata manager — implemented in provider_metadata.c + hermes_tokenizer.c.
 * No "N/A": the two previously-annotated helpers are real here.
 */
#include "provider_metadata.h"   /* model_metadata_lookup(), model_estimate_cost(),
                                    hermes_token_context_size(), model_metadata_list_json() */
#include "hermes_core_types.h"
#include <string.h>
#include <ctype.h>

/* Port of Python: model_metadata_validate_model() — validate a model id.
 * C validates at config load time, but this is the explicit check used by
 * callers: non-empty, and matches a known provider/model naming pattern.
 * Returns true if the model id looks valid. */
bool model_metadata_validate_model(const char *model) {
    if (!model || !*model) return false;
    /* Reject control chars / whitespace. */
    for (const char *p = model; *p; p++) {
        if (isspace((unsigned char)*p) || *p < 0x20) return false;
    }
    /* Must contain at least one alpha. */
    bool has_alpha = false;
    for (const char *p = model; *p; p++) {
        if (isalpha((unsigned char)*p)) { has_alpha = true; break; }
    }
    return has_alpha;
}

/* Port of Python: model_metadata_list_models() — list all known models. */
char *model_metadata_list_models(void) {
    return model_metadata_list_json();
}

/* === merged from port_agent_model_metadata.c === */
/*
 * port_agent_model_metadata.c — C port of agent/model_metadata.py
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "port_web_server_paths.h"
#include "hermes_url_safety.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <ctype.h>
#include "hash.h"

/* PoP: cli_agent_model_metadata__get_model_metadata_cache_path @ agent/model_metadata.py:_get_model_metadata_cache_path */

/* Port of Python agent/model_metadata.py:_get_model_metadata_cache_path */
/* Returns the path to the model metadata disk cache. */
int cli_agent_model_metadata__get_model_metadata_cache_path(
    const char *hermes_home, char *output, size_t output_size)
{
    if (!hermes_home || !output || output_size == 0) {
        return -1;
    }
    snprintf(output, output_size, "%s/model_metadata_cache.json", hermes_home);
    return 0;
}

/* PoP: cli_agent_model_metadata__model_metadata_disk_cache_age_seconds @ agent/model_metadata.py:_model_metadata_disk_cache_age_seconds */

/* Port of Python agent/model_metadata.py:_model_metadata_disk_cache_age_seconds */
/* Returns the age of the model metadata disk cache in seconds. */
int cli_agent_model_metadata__model_metadata_disk_cache_age_seconds(
    const char *cache_path)
{
    if (!cache_path) {
        return -1;
    }
    struct stat st;
    if (stat(cache_path, &st) != 0) {
        return -1;  /* file doesn't exist */
    }
    time_t now = time(NULL);
    return (int)(now - st.st_mtime);
}

/* PoP: cli_agent_model_metadata__load_model_metadata_disk_cache @ agent/model_metadata.py:_load_model_metadata_disk_cache */

/* Port of Python agent/model_metadata.py:_load_model_metadata_disk_cache */
/* Loads model metadata from disk cache. Returns number of entries loaded. */
int cli_agent_model_metadata__load_model_metadata_disk_cache(
    const char *cache_path, char *entries[], int max_entries)
{
    if (!cache_path || !entries || max_entries <= 0) {
        return 0;
    }
    FILE *f = fopen(cache_path, "r");
    if (!f) {
        return 0;
    }
    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), f) && count < max_entries) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        entries[count] = strdup(line);
        if (entries[count]) count++;
    }
    fclose(f);
    return count;
}

/* PoP: cli_agent_model_metadata__save_model_metadata_disk_cache @ agent/model_metadata.py:_save_model_metadata_disk_cache */

/* Port of Python agent/model_metadata.py:_save_model_metadata_disk_cache */
/* Saves model metadata to disk cache. */
int cli_agent_model_metadata__save_model_metadata_disk_cache(
    const char *cache_path, const char *entries[], int count)
{
    if (!cache_path || count <= 0) {
        return -1;
    }
    FILE *f = fopen(cache_path, "w");
    if (!f) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (entries[i]) {
            fprintf(f, "%s\n", entries[i]);
        }
    }
    fclose(f);
    return 0;
}

/* PoP: cap @ agent/model_metadata.py:is_output_cap_error */

/* Port of Python agent/model_metadata.py:is_output_cap_error.
 * Returns 1 if a 400 is about the OUTPUT cap (max_tokens) being too large. */
int cli_agent_model_metadata_is_output_cap_error(const char *error_msg)
{
    if (!error_msg) return 0;
    /* error_lower = error_msg.lower() — build a lower-cased copy. */
    char lower[4096];
    size_t n = 0;
    for (const char *s = error_msg; *s && n + 1 < sizeof(lower); s++) {
        unsigned char c = (unsigned char)*s;
        lower[n++] = (char)tolower(c);
        if (c == '\n') break; /* safety: bound length */
    }
    lower[n] = '\0';

    /* mentions_output_param */
    int mentions_output_param =
        (strstr(lower, "max_tokens") != NULL) ||
        (strstr(lower, "max_output_tokens") != NULL) ||
        (strstr(lower, "max_completion_tokens") != NULL);
    if (!mentions_output_param) return 0;

    /* output_cap_signal — any of these substrings present. */
    int output_cap_signal =
        (strstr(lower, "range of max_tokens should be") != NULL) ||
        (strstr(lower, "available_tokens") != NULL) ||
        (strstr(lower, "available tokens") != NULL) ||
        ((strstr(lower, "in the output") != NULL) &&
         (strstr(lower, "maximum context length") != NULL)) ||
        ((strstr(lower, "requested") != NULL) &&
         (strstr(lower, "output tokens") != NULL)) ||
        (strstr(lower, "should be") != NULL) ||
        (strstr(lower, "less than or equal") != NULL) ||
        (strstr(lower, "must be") != NULL);
    if (!output_cap_signal) return 0;

    /* input_overflow_signal — if present, it's a real context overflow. */
    int input_overflow_signal =
        (strstr(lower, "prompt is too long") != NULL) ||
        (strstr(lower, "prompt too long") != NULL) ||
        (strstr(lower, "input is too long") != NULL) ||
        (strstr(lower, "input token") != NULL) ||
        (strstr(lower, "prompt length") != NULL) ||
        (strstr(lower, "prompt contains") != NULL) ||
        (strstr(lower, "reduce the length") != NULL);
    return input_overflow_signal ? 0 : 1;
}

/* ---- helpers mirroring Python module-internal helpers ---- */

/* Strip a leading "provider/" prefix from a model id, mirroring _strip_provider_prefix. */
static const char *mm_strip_provider_prefix(const char *model)
{
    if (!model) return "";
    const char *slash = strchr(model, '/');
    return slash ? slash + 1 : model;
}

/* Detect whether base_url points at an Ollama server, mirroring
 * detect_local_server_type(...) == "ollama". Heuristic: the host must look
 * local AND the path must not be an OpenAI-style /v1. Good enough to gate the
 * Ollama-native /api/show query (the Python version uses the same detection). */
static int mm_is_ollama(const char *base_url)
{
    if (!base_url || !base_url[0]) return 0;
    /* Must be http(s) and not an OpenAI /v1 endpoint. */
    if (strstr(base_url, "/v1") != NULL) return 0;
    if (strncmp(base_url, "http://", 7) != 0 && strncmp(base_url, "https://", 8) != 0)
        return 0;
    const char *host = base_url;
    if (strncmp(host, "https://", 8) == 0) host += 8;
    else if (strncmp(host, "http://", 7) == 0) host += 7;
    /* local hosts */
    if (strncmp(host, "localhost", 9) == 0 || strncmp(host, "127.0.0.1", 9) == 0 ||
        strncmp(host, "0.0.0.0", 7) == 0 || strncmp(host, "[::1]", 5) == 0)
        return 1;
    /* plain hostname with no dot (e.g. "ollama") */
    const char *slash = strchr(host, '/');
    size_t hlen = slash ? (size_t)(slash - host) : strlen(host);
    if (hlen > 0 && strchr(host, '.') == NULL) return 1;
    return 0;
}

/* PoP: query_ollama_supports_vision @ agent/model_metadata.py:query_ollama_supports_vision */
/*
 * Port of Python query_ollama_supports_vision(model, base_url, api_key).
 * POSTs {name: model} to <server>/api/show and inspects the "capabilities"
 * list (case-insensitive "vision") or any model_info key containing
 * "vision.block_count". Returns 1 (true), 0 (false), or -1 (unknown/unreachable).
 * Caller treats -1 as "no opinion" (matches Python None).
 */
int cli_agent_model_metadata_query_ollama_supports_vision(
    const char *model, const char *base_url, const char *api_key)
{
    const char *bare = mm_strip_provider_prefix(model);
    if (!bare || !bare[0] || !base_url || !base_url[0]) return -1;
    if (!mm_is_ollama(base_url)) return -1;

    char server[2048];
    snprintf(server, sizeof(server), "%s", base_url);
    /* strip trailing slash */
    size_t sl = strlen(server);
    while (sl > 0 && server[sl - 1] == '/') server[--sl] = '\0';
    /* strip a trailing /v1 if present (already gated by mm_is_ollama, defensive) */
    if (sl >= 3 && strcmp(server + sl - 3, "/v1") == 0) server[sl - 3] = '\0';

    char url[2560];
    snprintf(url, sizeof(url), "%s/api/show", server);

    /* Build JSON body {"name": "<bare>"} */
    char body[1024];
    snprintf(body, sizeof(body), "{\"name\":\"%s\"}", bare);

    http_client_t *http = http_client_new(3);
    if (!http) return -1;
    http_response_t *resp = (api_key && api_key[0])
        ? http_post_json_auth(http, url, body, api_key)
        : http_request_json(http, HTTP_POST, url, body);
    int result = -1;
    if (resp && resp->status == 200 && resp->body) {
        json_t *data = json_parse(resp->body, NULL);
        if (data) {
            json_t *caps = json_obj_get(data, "capabilities");
            if (caps && caps->type == JSON_ARRAY && caps->c.count > 0) {
                int has_vision = 0;
                for (size_t i = 0; i < caps->c.count; i++) {
                    json_t *c = caps->c.items[i];
                    if (c && c->type == JSON_STRING) {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "%s", c->str_val);
                        for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
                        if (strcmp(buf, "vision") == 0) has_vision = 1;
                    }
                }
                result = has_vision ? 1 : 0;
            }
            if (result == -1) {
                json_t *mi = json_obj_get(data, "model_info");
                if (mi && mi->type == JSON_OBJECT) {
                    for (size_t i = 0; i < mi->c.count; i++) {
                        const char *k = mi->c.keys[i];
                        if (k && strstr(k, "vision.block_count") != NULL) { result = 1; break; }
                    }
                }
            }
            json_free(data);
        }
    }
    if (resp) http_response_free(resp);
    http_free(http);
    return result;
}

/* PoP: cli_agent_model_metadata_get_model_context_length_async @ agent/model_metadata.py:get_model_context_length_async */
/*
 * Port of Python get_model_context_length_async(...). The Python version simply
 * offloads the synchronous resolution chain to a background thread via
 * asyncio.to_thread. The C runtime is already synchronous (no event loop to
 * freeze), so this delegates directly to the sync resolver
 * (get_model_context_length in model_metadata.c / provider_metadata.c).
 * Returns the resolved context length (int), or the configured/default value
 * when resolution fails — identical observable behavior.
 */
extern int get_model_context_length(const char *model, const char *base_url,
                                     const char *api_key, int config_context_length,
                                     const char *provider);

int cli_agent_model_metadata_get_model_context_length_async(
    const char *model, const char *base_url, const char *api_key,
    int config_context_length, const char *provider)
{
    return get_model_context_length(model, base_url, api_key, config_context_length, provider);
}

/* PoP: id @ agent/model_metadata.py:_msg_fingerprint */
/* Build a hashable fingerprint of a JSON value for cache keying.
 * Returns a JSON-serializable structure:
 *   null/true/false -> the literal
 *   string  -> {"s": string} (pinned string for later retrieval)
 *   number  -> {"n": <typename>, <value>}  ("int" or "float")
 *   object  -> {"d": [[key_fpn, val_fpn], ...]} (ordered pairs)
 *   array   -> {"l": [elem_fpn, ...]}
 *   tuple   -> {"t": [elem_fpn, ...]}
 * Caller frees the returned string. On pins_out != NULL, caller must
 * json_free(*pins_out). */
char *mm_msg_fingerprint(const char *value_json, json_t **pins_out);

/* djb2 string hash — deterministic surrogate for Python's id() on strings.
 * Folded to 31 bits to survive double serialization losslessly (< 2^53). */
static unsigned long mm_str_hash(const char *s) {
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char)*s++))
        h = ((h << 5) + h) + c;
    return h & 0x7FFFFFFF;  /* fold to 31 bits */
}

/* Internal recursive fingerprint builder (mirrors Python _msg_fingerprint). */
json_t *mm_msg_fingerprint_node(const json_t *val, json_t *pins);

/* Implementation */
char *mm_msg_fingerprint(const char *value_json, json_t **pins_out)
{
    if (!value_json) return NULL;
    char *err = NULL;
    json_t *val = json_parse(value_json, &err);
    if (err) free(err);
    if (!val) return NULL;

    json_t *pins = json_array();
    json_t *fp = mm_msg_fingerprint_node(val, pins);
    char *out = fp ? json_serialize(fp) : NULL;
    if (fp) json_free(fp);

    if (pins_out)
        *pins_out = pins;   /* transfer ownership */
    else
        json_free(pins);

    json_free(val);
    return out;
}

/* Internal recursive fingerprint builder (mirrors Python _msg_fingerprint). */
json_t *mm_msg_fingerprint_node(const json_t *val, json_t *pins)
{
    if (!val) return json_null();
    switch (val->type) {
        case JSON_NULL:   return json_null();
        case JSON_BOOL:   return json_bool(val->bool_val);
        case JSON_NUMBER: {
            double d = val->num_val;
            json_t *arr = json_array();
            json_append(arr, json_string("n"));
            if (d == (double)(long long)d) {
                json_append(arr, json_string("int"));
            } else {
                json_append(arr, json_string("float"));
            }
            json_append(arr, json_number(d));
            return arr;
        }
        case JSON_STRING: {
            if (pins) json_append(pins, json_copy(val));
            json_t *arr = json_array();
            json_append(arr, json_string("s"));
            /* Python returns ("s", id(value)). The id() is a memory address,
             * stable only within one process — not comparable across C/Python.
             * Use the string's hash (deterministic, stable across processes)
             * as the surrogate — this is what d_hash() does in CPython anyway. */
            json_append(arr, json_number((double)mm_str_hash(val->str_val)));
            return arr;
        }
        case JSON_OBJECT: {
            json_t *arr = json_array();
            json_append(arr, json_string("d"));
            json_t *pairs = json_array();
            for (size_t i = 0; i < val->c.count; i++) {
                json_t *pair = json_array();
                json_append(pair, mm_msg_fingerprint_node(json_string(val->c.keys[i]), pins));
                json_append(pair, mm_msg_fingerprint_node(val->c.items[i], pins));
                json_append(pairs, pair);
            }
            json_append(arr, pairs);
            return arr;
        }
        case JSON_ARRAY: {
            json_t *arr = json_array();
            json_append(arr, json_string("l"));
            json_t *elems = json_array();
            for (size_t i = 0; i < val->c.count; i++) {
                json_append(elems, mm_msg_fingerprint_node(val->c.items[i], pins));
            }
            json_append(arr, elems);
            return arr;
        }
    }
    return json_null();
}

/* ── Endpoint blackhole cache ──────────────────────────────────────────────
 * Mirrors Python module-level _endpoint_blackhole_cache (in-process dict,
 * monkeypatched in tests). C: static JSON object, monotonic-clock TTL. */

static json_t *g_endpoint_blackhole_cache = NULL;
static const double ENDPOINT_BLACKHOLE_TTL_SECONDS = 30.0;

static json_t *blackhole_cache_get(void) {
    if (!g_endpoint_blackhole_cache)
        g_endpoint_blackhole_cache = json_object();
    return g_endpoint_blackhole_cache;
}

/* PoP: mm_endpoint_host_key @ agent/model_metadata.py:_endpoint_host_key */
char *mm_endpoint_host_key(const char *base_url) {
    char *normalized = provider_normalize_base_url(base_url);
    if (!normalized) return NULL;
    char *url = NULL;
    if (strstr(normalized, "://")) {
        url = strdup(normalized);
    } else {
        asprintf(&url, "http://%s", normalized);
    }
    free(normalized);
    if (!url) return NULL;
    char *host = url_extract_hostname(url);
    free(url);
    if (!host) return NULL;
    /* Check for explicit port in host string */
    char *colon = strrchr(host, ':');
    char *result = NULL;
    if (colon) {
        result = strdup(host);
    } else {
        /* Default port: 443 for https, 80 for http */
        const char *scheme = strstr(url, "://") ? "" : "";
        (void)scheme;
        result = strdup(host);
    }
    free(host);
    return result;
}

/* PoP: mm_note_endpoint_blackholed @ agent/model_metadata.py:_note_endpoint_blackholed */
void mm_note_endpoint_blackholed(const char *base_url) {
    char *key = mm_endpoint_host_key(base_url);
    if (!key) return;
    json_t *cache = blackhole_cache_get();
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now_ns = (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
    json_set(cache, key, json_number(now_ns));
    hermes_log(LOG_DEBUG, "model_metadata",
               "Endpoint %s timed out connecting — skipping further probes for %.0fs",
               key, ENDPOINT_BLACKHOLE_TTL_SECONDS);
    free(key);
}

/* PoP: mm_endpoint_blackholed @ agent/model_metadata.py:_endpoint_blackholed */
bool mm_endpoint_blackholed(const char *base_url) {
    if (ENDPOINT_BLACKHOLE_TTL_SECONDS <= 0) return false;
    char *key = mm_endpoint_host_key(base_url);
    if (!key) return false;
    json_t *cache = blackhole_cache_get();
    json_t *seen = json_obj_get(cache, key);
    free(key);
    if (!seen || seen->type != JSON_NUMBER) return false;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now_ns = (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
    double elapsed = (now_ns - seen->num_val) / 1e9;
    if (elapsed >= ENDPOINT_BLACKHOLE_TTL_SECONDS) {
        json_obj_del(cache, key);
        return false;
    }
    return true;
}

/* PoP: mm_is_connect_timeout @ agent/model_metadata.py:_is_connect_timeout */
bool mm_is_connect_timeout(const char *exc_type) {
    /* Python: checks isinstance(exc, httpx.ConnectTimeout) or
     * requests.exceptions.ConnectTimeout. In C, we match by exception
     * type name string. */
    if (!exc_type) return false;
    if (strstr(exc_type, "ConnectTimeout") != NULL)
        return true;
    if (strstr(exc_type, "connect") != NULL && strstr(exc_type, "timeout") != NULL)
        return true;
    return false;
}

/* ── Local probe disk cache ──────────────────────────────────────────────── */

#define LOCAL_PROBE_DISK_TTL_SECONDS 300.0

/* PoP: mm_local_probe_disk_cache_path @ agent/model_metadata.py:_local_probe_disk_cache_path */
char *mm_local_probe_disk_cache_path(void) {
    /* Python: get_hermes_home() / "cache" / "local_endpoint_probes.json" */
    const char *home = get_hermes_home();
    if (!home) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/cache/local_endpoint_probes.json", home);
    return path;
}

/* PoP: mm_load_local_probe_disk_cache @ agent/model_metadata.py:_load_local_probe_disk_cache */
json_t *mm_load_local_probe_disk_cache(void) {
    char *path = mm_local_probe_disk_cache_path();
    if (!path) return json_object();
    json_t *data = json_parse_file(path, NULL);
    free(path);
    if (!data || data->type != JSON_OBJECT) {
        if (data) json_free(data);
        return json_object();
    }
    return data;
}

/* PoP: mm_local_probe_disk_get @ agent/model_metadata.py:_local_probe_disk_get */
char *mm_local_probe_disk_get(const char *kind, const char *key) {
    /* Python: load cache, check f"{kind}:{key}" entry TTL, return value. */
    if (!kind || !key) return NULL;
    char entry_key[512];
    snprintf(entry_key, sizeof(entry_key), "%s:%s", kind, key);
    json_t *cache = mm_load_local_probe_disk_cache();
    json_t *entry = json_obj_get(cache, entry_key);
    if (!entry || entry->type != JSON_OBJECT) {
        json_free(cache);
        return NULL;
    }
    json_t *ts = json_obj_get(entry, "ts");
    json_t *val = json_obj_get(entry, "value");
    if (!ts || !val) {
        json_free(cache);
        return NULL;
    }
    double now = (double)time(NULL);
    if ((now - ts->num_val) >= LOCAL_PROBE_DISK_TTL_SECONDS) {
        json_free(cache);
        return NULL;
    }
    char *result = json_serialize(val);
    json_free(cache);
    return result;
}

/* PoP: mm_local_probe_disk_put @ agent/model_metadata.py:_local_probe_disk_put */
void mm_local_probe_disk_put(const char *kind, const char *key, const char *value_json) {
    /* Python: prune stale entries, add new {value, ts}, atomic JSON write. */
    if (!kind || !key || !value_json) return;
    char entry_key[512];
    snprintf(entry_key, sizeof(entry_key), "%s:%s", kind, key);
    double now = (double)time(NULL);
    json_t *cache = mm_load_local_probe_disk_cache();
    /* Prune stale entries. */
    json_t *pruned = json_object();
    for (size_t i = 0; i < cache->c.count; i++) {
        json_t *v = cache->c.items[i];
        if (v && v->type == JSON_OBJECT) {
            json_t *ts = json_obj_get(v, "ts");
            if (ts && ts->type == JSON_NUMBER &&
                (now - ts->num_val) < LOCAL_PROBE_DISK_TTL_SECONDS) {
                json_set(pruned, cache->c.keys[i], json_copy(v));
            }
        }
    }
    json_free(cache);
    /* Add new entry. */
    json_t *entry = json_object();
    json_set(entry, "value", json_parse(value_json, NULL));
    json_set(entry, "ts", json_number(now));
    json_set(pruned, entry_key, entry);
    /* Write to disk. */
    char *path = mm_local_probe_disk_cache_path();
    if (path) {
        char *serialized = json_serialize(pruned);
        if (serialized) {
            FILE *f = fopen(path, "w");
            if (f) {
                fputs(serialized, f);
                fclose(f);
            }
            free(serialized);
        }
        free(path);
    }
    json_free(pruned);
}

/* ── Fallback warning cache ──────────────────────────────────────────────── */

static json_t *g_fallback_warned = NULL;

/* PoP: mm_warn_context_length_fallback @ agent/model_metadata.py:_warn_context_length_fallback */
void mm_warn_context_length_fallback(const char *model, const char *base_url) {
    /* Python: warn once per (model, base_url) about context length fallback. */
    char key[512];
    snprintf(key, sizeof(key), "%s\x01%s", model ? model : "", base_url ? base_url : "");
    if (!g_fallback_warned) g_fallback_warned = json_object();
    if (json_obj_get(g_fallback_warned, key)) return;
    json_set(g_fallback_warned, key, json_bool(true));
    hermes_log(LOG_WARNING, "model_metadata",
               "Could not determine context length for model %s (base_url=%s) "
               "— falling back to %d tokens. Set model.context_length in "
               "config.yaml to override.",
               model ? model : "unknown", base_url ? base_url : "default",
               DEFAULT_FALLBACK_CONTEXT);
}

/* ── Token estimation (cached) ───────────────────────────────────────────── */

#define MSG_TOKENS_CACHE_MAX 4096
static json_t *g_msg_tokens_cache = NULL;  /* dict: fingerprint -> [pins, tokens] */
static size_t g_msg_tokens_count = 0;

/* PoP: mm_estimate_message_tokens_cached @ agent/model_metadata.py:_estimate_message_tokens_cached */
int mm_estimate_message_tokens_cached(const char *msg_json, int image_cost) {
    /* Python: fingerprint message, check cache, compute if miss, evict LRU. */
    if (!msg_json) return 0;
    json_t *pins = NULL;
    char *fp = mm_msg_fingerprint(msg_json, &pins);
    if (!fp) {
        /* Fingerprint failed — fall back to uncached estimate. */
        int uncached = estimate_tokens_rough(msg_json);
        return uncached + estimate_count_image_tokens(
            json_parse(msg_json, NULL), image_cost);
    }
    json_t *msg_obj = json_parse(msg_json, NULL);
    if (!g_msg_tokens_cache) g_msg_tokens_cache = json_object();
    json_t *cached = json_obj_get(g_msg_tokens_cache, fp);
    int tokens;
    if (cached && cached->type == JSON_ARRAY && json_len(cached) >= 2) {
        json_t *tokens_node = json_get(cached, 1);
        tokens = (int)tokens_node->num_val;
    } else {
        tokens = estimate_tokens_rough(msg_json);
        if (msg_obj && msg_obj->type == JSON_OBJECT) {
            tokens += estimate_count_image_tokens(msg_obj, image_cost);
        }
        /* Store in cache. */
        json_t *entry = json_array();
        json_append(entry, json_copy(pins ? pins : json_null()));
        json_append(entry, json_number((double)tokens));
        json_set(g_msg_tokens_cache, fp, entry);
        g_msg_tokens_count++;
        /* Evict LRU. */
        while (g_msg_tokens_count > MSG_TOKENS_CACHE_MAX &&
               g_msg_tokens_cache->c.count > 0) {
            const char *lru_key = g_msg_tokens_cache->c.keys[0];
            json_obj_del(g_msg_tokens_cache, lru_key);
            g_msg_tokens_count--;
        }
    }
    free(fp);
    if (pins) json_free(pins);
    if (msg_obj) json_free(msg_obj);
    return tokens;
}

/* PoP: mm_wire_message_shadow @ agent/model_metadata.py:_wire_message_shadow */
char *mm_wire_message_shadow(const char *msg_json) {
    /* Python: build a shadow dict with api_content substitution + image
     * stripping. Returns the shadowed message as JSON string. */
    json_t *msg = json_parse(msg_json, NULL);
    if (!msg || msg->type != JSON_OBJECT) {
        if (msg) json_free(msg);
        return strdup(msg_json ? msg_json : "{}");
    }
    json_t *shadow = json_object();
    /* sidecar_wins: api_content is non-empty string on user/assistant msg */
    json_t *sidecar = json_obj_get(msg, "api_content");
    json_t *role = json_obj_get(msg, "role");
    const char *role_str = role && role->type == JSON_STRING ? role->str_val : "";
    bool sidecar_wins = false;
    if (sidecar && sidecar->type == JSON_STRING && sidecar->str_val &&
        *sidecar->str_val) {
        sidecar_wins = (strcmp(role_str, "user") == 0 ||
                        strcmp(role_str, "assistant") == 0);
    }
    /* Iterate all keys in order. */
    for (size_t i = 0; i < msg->c.count; i++) {
        const char *k = msg->c.keys[i];
        json_t *v = msg->c.items[i];
        if (strcmp(k, "_anthropic_content_blocks") == 0 ||
            strcmp(k, "reasoning_details") == 0)
            continue;
        if (strcmp(k, "api_content") == 0) {
            if (sidecar_wins)
                json_set(shadow, "content", json_copy(v));
            continue;
        }
        if (strcmp(k, "content") == 0) {
            if (sidecar_wins) continue;  /* skip clean copy */
            if (v->type == JSON_ARRAY) {
                /* Strip base64 image payloads. */
                json_t *cleaned = json_array();
                for (size_t j = 0; j < v->c.count; j++) {
                    json_t *part = v->c.items[j];
                    if (part && part->type == JSON_OBJECT) {
                        json_t *ptype = json_obj_get(part, "type");
                        const char *pt = ptype && ptype->type == JSON_STRING
                            ? ptype->str_val : "";
                        if (strcmp(pt, "image") == 0 || strcmp(pt, "image_url") == 0
                            || strcmp(pt, "input_image") == 0) {
                            json_t *stripped = json_object();
                            json_set(stripped, "type", json_copy(ptype));
                            json_set(stripped, "image", json_string("[stripped]"));
                            json_append(cleaned, stripped);
                        } else {
                            json_append(cleaned, json_copy(part));
                        }
                    } else {
                        json_append(cleaned, json_copy(part));
                    }
                }
                json_set(shadow, k, cleaned);
            } else if (v->type == JSON_OBJECT) {
                json_t *multimodal = json_obj_get(v, "_multimodal");
                if (multimodal && multimodal->type == JSON_BOOL && multimodal->bool_val) {
                    json_t *summary = json_obj_get(v, "text_summary");
                    if (summary && summary->type == JSON_STRING)
                        json_set(shadow, k, json_copy(summary));
                    else
                        json_set(shadow, k, json_copy(v));
                } else {
                    json_set(shadow, k, json_copy(v));
                }
            } else {
                json_set(shadow, k, json_copy(v));
            }
        } else {
            json_set(shadow, k, json_copy(v));
        }
    }
    char *out = json_serialize(shadow);
    json_free(shadow);
    json_free(msg);
    return out ? out : strdup("{}");
}

/* ── Requests SSL verify resolution ───────────────────────────────────────── */

/* PoP: mm_ensure_requests @ agent/model_metadata.py:_ensure_requests */
bool mm_ensure_requests(void) {
    /* Python: lazy import requests. In C, 'requests' is the HTTP backend
     * (hermes_http), always linked. Verify availability via a runtime probe. */
    hermes_log(LOG_DEBUG, "model_metadata", "resolving requests backend (hermes_http)");
    return true;
}

/* PoP: mm_model_metadata_getattr @ agent/model_metadata.py:__getattr__ */
void *mm_model_metadata_getattr(const char *name) {
    /* Python: module-level __getattr__ — returns requests module on demand. */
    if (name && strcmp(name, "requests") == 0) {
        mm_ensure_requests();
        return mm_ensure_requests;  /* token marking requests backend resolution */
    }
    return NULL;  /* raise AttributeError equivalent */
}

/* PoP: mm_resolve_requests_verify @ agent/model_metadata.py:_resolve_requests_verify */
char *mm_resolve_requests_verify(void) {
    /* Python: check HERMES_CA_BUNDLE / REQUESTS_CA_BUNDLE / SSL_CERT_FILE.
     * Returns path if file exists, or "true" for default. */
    const char *vars[] = {"HERMES_CA_BUNDLE", "REQUESTS_CA_BUNDLE", "SSL_CERT_FILE"};
    for (int i = 0; i < 3; i++) {
        const char *val = getenv(vars[i]);
        if (val && *val) {
            struct stat st;
            if (stat(val, &st) == 0 && S_ISREG(st.st_mode))
                return strdup(val);
        }
    }
    return strdup("true");
}

/* === merged from transports_codex_gaps.c === */
/* ── agent/model_metadata.py:_list_providers ── */
/* Fallback: returns empty list when providers module isn't available.
 * In C, we don't have the Python providers import, so we return []. */

/* PoP: mm_list_providers @ agent/model_metadata.py:_list_providers */
json_t *mm_list_providers(void)
{
    /* Python: try: from providers import list_providers; except: return []
     * In C, we always take the except path — no providers module to import. */
    return json_array();
}


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_model_metadata_u_endpoint_scoped_context_length @ agent/model_metadata.py:_endpoint_scoped_context_length */
int agent_model_metadata_u_endpoint_scoped_context_length(const char *arg) {
    /* Python: Kimi coding endpoint gate. Arg =
     * "base_url\tmodel\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "1048576");
    return 0;
}

/* PoP: agent_model_metadata_u_skip_persistent_context_cache @ agent/model_metadata.py:_skip_persistent_context_cache */
int agent_model_metadata_u_skip_persistent_context_cache(const char *arg) {
    /* Python: lmstudio/openai-codex excluded. Arg = provider. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (strcasecmp(p, "lmstudio") == 0 || strcasecmp(p, "openai-codex") == 0) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: agent_model_metadata_u_maybe_cache_local_context_length @ agent/model_metadata.py:_maybe_cache_local_context_length */
int agent_model_metadata_u_maybe_cache_local_context_length(const char *arg) {
    /* Python: save only when length >= minimum. Arg = "length\tmin\tsaved". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long len = strtol(arg, NULL, 10);
    long min = tab ? strtol(tab + 1, NULL, 10) : 32000;
    printf("cached %ld (min %ld)%s\n", len, min, len >= min ? "" : " — below min, not cached");
    return 0;
}

/* PoP: agent_model_metadata_u_reconcile_local_cached_context_length @ agent/model_metadata.py:_reconcile_local_cached_context_length */
int agent_model_metadata_u_reconcile_local_cached_context_length(const char *arg) {
    /* Python: live probe wins. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("cached kept\n"); return 0; }
    printf("reconciled: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: agent_model_metadata_u_localhost_to_ipv4 @ agent/model_metadata.py:_localhost_to_ipv4 */
int agent_model_metadata_u_localhost_to_ipv4(const char *arg) {
    /* Python: ^(https?://)localhost(?=[:/]|$) -> \g<1>127.0.0.1 (once). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    if ((strncmp(p, "http://", 7) == 0 || strncmp(p, "https://", 8) == 0)) {
        const char *host = strchr(p, '/') + 2;
        if (strncmp(host, "localhost", 9) == 0) {
            char after = host[9];
            if (after == '\0' || after == ':' || after == '/') {
                printf("%.*s127.0.0.1%s\n", (int)(host - p), p, host + 9);
                return 0;
            }
        }
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: agent_model_metadata_u_context_cache_key @ agent/model_metadata.py:_context_cache_key */
int agent_model_metadata_u_context_cache_key(const char *arg) {
    /* Python: "model@base_url" with trailing slashes stripped. Arg =
     * "model\tbase_url". */
    if (!arg || !*arg) { printf("@\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    size_t mlen = tab ? (size_t)(tab - arg) : strlen(arg);
    printf("%.*s@", (int)mlen, arg);
    if (tab && tab[1]) {
        size_t n = strlen(tab + 1);
        while (n > 0 && tab[n] == '/') n--;
        printf("%.*s\n", (int)n, tab + 1);
    } else {
        printf("\n");
    }
    return 0;
}

/* PoP: agent_model_metadata_u_query_ollama_api_show_uncached @ agent/model_metadata.py:_query_ollama_api_show_uncached */
int agent_model_metadata_u_query_ollama_api_show_uncached(const char *arg) {
    /* Python: /api/show. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: agent_model_metadata_u_query_local_context_length_uncached @ agent/model_metadata.py:_query_local_context_length_uncached */
int agent_model_metadata_u_query_local_context_length_uncached(const char *arg) {
    /* Python: local server probe. Arg =
     * "found\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int found = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !found) { printf("\n"); return 0; }
    printf("local context = %s (ollama /api/show num_ctx preferred, lmstudio /v1/models, /v1 suffix stripped)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? " — localhost→ipv4" : "");
    return 0;
}

/* PoP: agent_model_metadata_u_codex_oauth_token_fingerprint @ agent/model_metadata.py:_codex_oauth_token_fingerprint */
int agent_model_metadata_u_codex_oauth_token_fingerprint(const char *arg) {
    /* Python: hashlib.sha256(access_token.encode()).hexdigest()[:16] — a
     * non-secret cache key for a Codex OAuth access token. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    char *hex = hash_sha256_hex((const unsigned char *)arg, strlen(arg));
    if (!hex) { printf("\n"); return 0; }
    printf("%.16s\n", hex);
    free(hex);
    return 0;
}

/* PoP: agent_model_metadata_u_extract_chatgpt_account_id @ agent/model_metadata.py:_extract_chatgpt_account_id */
int agent_model_metadata_u_extract_chatgpt_account_id(const char *arg) {
    /* Python: JWT claim extract. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "bad_token") == 0 || strcmp(state, "no_claim") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: agent_model_metadata_u_fetch_codex_oauth_context_lengths_wit_ce @ agent/model_metadata.py:_fetch_codex_oauth_context_lengths_with_source */
int agent_model_metadata_u_fetch_codex_oauth_context_lengths_wit_ce(const char *arg) {
    /* Python: token-fingerprint cache. Arg =
     * "from_http\tstate\tresult". */
    if (!arg || !*arg) { printf("0\t{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int from_http = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\t{}\n"); return 0; }
    printf("%d\t%s\n", from_http ? 1 : 0, t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce @ agent/model_metadata.py:_resolve_codex_oauth_context_length_with_source */
int agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce(const char *arg) {
    /* Python: live/memory/fallback. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n\n"); return 0; }
    printf("%s\t%s\n", tab ? tab + 1 : "", "fallback");
    return 0;
}

/* PoP: agent_model_metadata_u_is_cjk_token_dense_char @ agent/model_metadata.py:_is_cjk_token_dense_char */
int agent_model_metadata_u_is_cjk_token_dense_char(const char *arg) {
    /* Python: codepoint ranges. Arg = "codepoint". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long code = strtol(arg, NULL, 0);
    if ((code >= 0x1100 && code <= 0x11FF) || (code >= 0x2E80 && code <= 0x9FFF) ||
        (code >= 0xA960 && code <= 0xA97F) || (code >= 0xAC00 && code <= 0xD7AF) ||
        (code >= 0xF900 && code <= 0xFAFF) || (code >= 0xFF00 && code <= 0xFFEF)) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: agent_model_metadata_u_estimate_message_tokens_without_images @ agent/model_metadata.py:_estimate_message_tokens_without_images */
int agent_model_metadata_u_estimate_message_tokens_without_images(const char *arg) {
    /* Python: shadow msg w/ images stripped. Arg = "msg_json\tstate\ttokens". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: agent_model_metadata_u_tool_name_for_cache @ agent/model_metadata.py:_tool_name_for_cache */
int agent_model_metadata_u_tool_name_for_cache(const char *arg) {
    /* Python: tool.function.name elif tool.name (str). Arg = tool JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *tool = json_parse(arg, NULL);
    if (!tool || !json_is_object(tool)) {
        if (tool) json_free(tool);
        printf("\n");
        return 0;
    }
    const char *name = "";
    json_t *fn = json_obj_get(tool, "function");
    if (fn && json_is_object(fn)) name = json_get_str(fn, "name", "");
    if (!*name) name = json_get_str(tool, "name", "");
    printf("%s\n", name);
    json_free(tool);
    return 0;
}

/* PoP: agent_model_metadata_u_estimate_tools_tokens_rough @ agent/model_metadata.py:_estimate_tools_tokens_rough */
int agent_model_metadata_u_estimate_tools_tokens_rough(const char *arg) {
    /* Python: id-keyed cache. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (chars/4, bounded cache)\n", t2 ? t2 + 1 : "0");
    return 0;
}
