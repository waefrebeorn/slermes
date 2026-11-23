/*
 * stt_lease.c — C home for upstream Hermes tools/stt_lease.py.
 *
 * PoP: w5_sttlease_a1f613_warm_stt_provider @ tools/stt_lease.py:warm_stt_provider
 *
 * The Python function is a decision tree around a singleton model cache; the
 * side effects (config load, model download/load, idle watcher, timing) are
 * injected as explicit seams. No stubs: every branch produces the exact
 * provider / warmed / action / error / elapsed_ms result Python produces.
 */
#include "noc_w5_sttlease_a1f613.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* (provider or config or "").lower().strip() */
static char *stt_lower_strip(const char *provider, const char *config) {
    const char *s = provider ? provider : (config ? config : "");
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) ++a;
    const char *z = a + strlen(a);
    while (z > a && isspace((unsigned char)z[-1])) --z;
    size_t n = (size_t)(z - a);
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; ++i) o[i] = (char)tolower((unsigned char)a[i]);
    o[n] = '\0';
    return o;
}

struct json_t *w5_sttlease_a1f613_warm_stt_provider_ex(
        const char *provider_arg, const char *config_provider,
        int has_fw, int lazy_install_ok, const char *model_name,
        int hot_before, int load_ok, const char *load_error, long long elapsed_ms) {
    char *name = stt_lower_strip(provider_arg, config_provider);
    json_t *out = json_object();
    json_set(out, "provider", json_string(name ? name : ""));
    json_set(out, "warmed", json_bool(false));
    json_set(out, "action", json_string("noop"));
    int is_local = name && !strcmp(name, "local");
    int is_local_command = name && !strcmp(name, "local_command");
    free(name);

    if (!is_local && !is_local_command) return out;
    if (is_local_command) {
        json_set(out, "warmed", json_bool(true));
        return out;
    }
    if (!has_fw && !lazy_install_ok) {
        json_set(out, "action", json_string("error"));
        json_set(out, "error", json_string("faster-whisper not installed"));
        return out;
    }
    if (!load_ok) {
        json_set(out, "action", json_string("error"));
        json_set(out, "error", json_string(load_error ? load_error : "Local whisper model failed to load"));
        return out;
    }
    json_set(out, "warmed", json_bool(true));
    json_set(out, "action", json_string(hot_before ? "cached" : "loaded"));
    json_set(out, "elapsed_ms", json_number((double)elapsed_ms));
    (void)model_name;
    return out;
}

