/*
 * transcription_common.c — C port of upstream Hermes tools/transcription_common.py.
 * Gate: w14_transcripti_1ec31e
 *
 * PoP: w14_transcripti_1ec31e_error_result @ tools/transcription_common.py:_error_result
 * PoP: w14_transcripti_1ec31e_ok_result @ tools/transcription_common.py:_ok_result
 * PoP: w14_transcripti_1ec31e_normalize_xai_stt_model @ tools/transcription_common.py:normalize_xai_stt_model
 * PoP: w14_transcripti_1ec31e_config_number @ tools/transcription_common.py:_config_number
 * PoP: w14_transcripti_1ec31e_process_error_detail @ tools/transcription_common.py:_process_error_detail
 *
 * Env reads (STT_XAI_MODEL) are injected as explicit seams from the fixture;
 * the process environment is never mutated (Windows CRT setenv("") unsets).
 */
#include "noc_w14_transcripti_1ec31e.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? (v->str_val ? v->str_val : "") : NULL;
}

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    return v->c.count != 0;
}

/* Python str() coercion for the value kinds accepted as model / error. */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof(b), "%.17g", v->num_val);
        return dupstr(b);
    }
    return dupstr("");
}

static char *strip_ws(const char *s) {
    size_t a = 0, b = s ? strlen(s) : 0;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { if (b > a) memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}

json_t *w14_transcripti_1ec31e_error_result(const struct json_t *error,
                                            const struct json_t *extra) {
    json_t *out = json_object();
    json_set(out, "success", json_bool(false));
    json_set(out, "transcript", json_string(""));
    json_set(out, "error", json_string(jstr(error) ? jstr(error) : ""));
    if (json_is_object(extra)) {
        for (size_t i = 0; i < json_object_size(extra); i++) {
            const char *k = json_object_get_key_at(extra, i);
            if (!k) continue;
            json_set(out, k, json_copy(json_object_get_at(extra, i)));
        }
    }
    return out;
}

json_t *w14_transcripti_1ec31e_ok_result(const struct json_t *transcript,
                                        const struct json_t *provider) {
    json_t *out = json_object();
    json_set(out, "success", json_bool(true));
    json_set(out, "transcript", json_string(jstr(transcript) ? jstr(transcript) : ""));
    json_set(out, "provider", json_string(jstr(provider) ? jstr(provider) : ""));
    return out;
}

json_t *w14_transcripti_1ec31e_normalize_xai_stt_model(const struct json_t *model,
                                                      const struct json_t *env_value) {
    char *value = truthy(model) ? py_str(model) : dupstr("");
    if (!value) value = dupstr("");
    char *trimmed = strip_ws(value);
    free(value);
    if (!*trimmed || !strcmp(trimmed, "grok-stt")) {
        char *e = strip_ws(jstr(env_value) ? jstr(env_value) : "");
        json_t *out = json_string(e && *e ? e : "grok-voice-transcribe-2.0");
        free(e); free(trimmed);
        return out;
    }
    json_t *out = json_string(trimmed);
    free(trimmed);
    return out;
}

static double numeric(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING && v->str_val) return strtod(v->str_val, NULL);
    return 0;
}

static int parse_full_double(const char *s, double *out) {
    char *end = NULL;
    double d = strtod(s, &end);
    if (end == s) return 0;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end) return 0;
    *out = d;
    return 1;
}

static int parse_full_int(const char *s, double *out) {
    char *end = NULL;
    long long v = strtoll(s, &end, 10);
    if (end == s) return 0;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end) return 0;
    *out = (double)v;
    return 1;
}

json_t *w14_transcripti_1ec31e_config_number(const struct json_t *cfg, const char *key,
                                             const struct json_t *default_value,
                                             const char *cast) {
    double def = numeric(default_value);
    const json_t *raw = json_obj_get(cfg, key);
    if (!raw) raw = default_value;
    int is_int = cast && !strcmp(cast, "int");
    double result = def;
    int ok = 1;
    if (raw && raw->type == JSON_NUMBER) {
        result = is_int ? (double)(long long)raw->num_val : raw->num_val;
    } else if (raw && raw->type == JSON_BOOL) {
        result = raw->bool_val ? 1 : 0;
    } else if (raw && raw->type == JSON_STRING && raw->str_val) {
        ok = is_int ? parse_full_int(raw->str_val, &result)
                    : parse_full_double(raw->str_val, &result);
    } else {
        ok = 0;
    }
    if (!ok) result = def;
    return json_number(result);
}

json_t *w14_transcripti_1ec31e_process_error_detail(const struct json_t *err_text,
                                                    const struct json_t *out_text,
                                                    const struct json_t *exc_str) {
    const json_t *streams[2] = {err_text, out_text};
    for (int i = 0; i < 2; i++) {
        char *detail = strip_ws(jstr(streams[i]) ? jstr(streams[i]) : "");
        if (detail && *detail) { json_t *out = json_string(detail); free(detail); return out; }
        free(detail);
    }
    return json_string(jstr(exc_str) ? jstr(exc_str) : "");
}
