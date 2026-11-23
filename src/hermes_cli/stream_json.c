/*
 * stream_json.c — C home for upstream Hermes hermes_cli/stream_json.py.
 * PoP: w8_streamjson_4c9aec_requested @ hermes_cli/stream_json.py:stream_json_requested
 * PoP: w8_streamjson_4c9aec_text_delta @ hermes_cli/stream_json.py:StreamJsonEmitter.on_text_delta
 * PoP: w8_streamjson_4c9aec_result_payload @ hermes_cli/stream_json.py:StreamJsonEmitter.emit_result
 */
#include "noc_w8_streamjson_4c9aec.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        default:          return v->c.count ? 1 : 0;
    }
}
static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static char *py_str(const json_t *v) {
    if (!v) return dupstr("");
    switch (v->type) {
        case JSON_STRING: return dupstr(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return dupstr(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char b[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(b, sizeof(b), "%.0f", v->num_val);
            else
                snprintf(b, sizeof(b), "%.17g", v->num_val);
            return dupstr(b);
        }
        default: return dupstr("");
    }
}
/* dict.get(key) or default, with Python truthiness. */
static json_t *get_or(const json_t *obj, const char *key, json_t *def) {
    json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (v && truthy(v)) { json_free(def); return json_copy(v); }
    return def;
}

struct json_t *w8_streamjson_4c9aec_requested(const struct json_t *args) {
    const char *fmt = json_get_str(args, "output_format", "text");
    int has_query = truthy(json_obj_get(args, "query")) || truthy(json_obj_get(args, "query_file"));
    int tui = json_get_bool(args, "tui", false) ? 1 : 0;
    int already_quiet = json_get_bool(args, "quiet", false) ? 1 : 0;
    int req = 0;
    const char *err = NULL;
    if (!strcmp(fmt, "stream-json")) {
        if (!has_query)       err = "Error: --format stream-json requires -q/--query.";
        else if (tui)         err = "Error: --format stream-json cannot be used with --tui.";
        else                  req = 1;
    }
    json_t *o = json_object();
    json_set(o, "requested", json_bool(req ? true : false));
    json_set(o, "error", err ? json_string(err) : json_null());
    json_set(o, "quiet", json_bool((req || already_quiet) ? true : false));
    return o;
}

struct json_t *w8_streamjson_4c9aec_text_delta(const struct json_t *text) {
    if (!truthy(text)) return json_null();
    char *s = py_str(text);
    json_t *o = json_object();
    json_set(o, "type", json_string("text"));
    json_set(o, "text", json_string(s ? s : ""));
    free(s);
    return o;
}

struct json_t *w8_streamjson_4c9aec_result_payload(const struct json_t *result,
                                                  const char *session_id,
                                                  const struct json_t *exit_code,
                                                  const struct json_t *duration_ms,
                                                  const char *bound_session_id) {
    json_t *synthetic = NULL;
    const json_t *data = result;
    if (!result || result->type != JSON_OBJECT) {
        synthetic = json_object();
        char *s = (result && result->type == JSON_STRING)
                      ? dupstr(result->str_val ? result->str_val : "")
                      : dupstr("");
        json_set(synthetic, "final_response", json_string(s));
        free(s);
        data = synthetic;
    }
    int failed = truthy(json_obj_get(data, "failed"));
    int code;
    if (truthy(exit_code)) code = (int)exit_code->num_val;
    else                   code = failed ? 1 : 0;

    json_t *o = json_object();
    const char *sid = (session_id && session_id[0]) ? session_id
                     : (bound_session_id ? bound_session_id : "");
    json_set(o, "type", json_string("result"));
    json_set(o, "session_id", json_string(sid));
    json_set(o, "exit_code", json_number((double)code));
    json_set(o, "text", get_or(data, "final_response", json_string("")));
    json_t *tokens = json_object();
    json_set(tokens, "input",      get_or(data, "input_tokens",      json_number(0)));
    json_set(tokens, "output",     get_or(data, "output_tokens",     json_number(0)));
    json_set(tokens, "total",      get_or(data, "total_tokens",      json_number(0)));
    json_set(tokens, "cache_read", get_or(data, "cache_read_tokens", json_number(0)));
    json_set(tokens, "cache_write",get_or(data, "cache_write_tokens",json_number(0)));
    json_set(o, "tokens", tokens);
    long long ms = duration_ms ? (long long)duration_ms->num_val : 0;
    json_set(o, "duration_ms", json_number((double)ms));
    const json_t *err = json_obj_get(data, "error");
    if (truthy(err)) {
        char *s = py_str(err);
        json_set(o, "error", json_string(s ? s : ""));
        free(s);
    }
    if (truthy(json_obj_get(data, "replay_gap")))
        json_set(o, "replay_gap", json_bool(true));
    json_free(synthetic);
    return o;
}
