/*
 * port_agent_model_metadata.c — C port of agent/model_metadata.py
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "provider_metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <ctype.h>

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

/* PoP: cli_agent_model_metadata_is_output_cap_error @ agent/model_metadata.py:is_output_cap_error */

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

/* PoP: cli_agent_model_metadata_query_ollama_supports_vision @ agent/model_metadata.py:query_ollama_supports_vision */
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

/* PoP: _msg_fingerprint @ agent/model_metadata.py:_msg_fingerprint */
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
