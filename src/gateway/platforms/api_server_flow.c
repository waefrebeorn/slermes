/*
 * api_server_flow.c — request-handling FLOW cores of gateway/platforms/api_server.py
 * (LIVE authority).
 *
 * Ports decision sequences: auth branches, validation preludes, error-response
 * construction, approval-event shaping and the ordered SSE event queue. Every
 * live effect (web.json_response, secret scope reads, profile lookups, clocks,
 * logging, loop/queue mechanics, run-status writes) enters through the seam
 * table declared in api_server_flow.h; the cores below perform no I/O.
 *
 * Python semantics honored: truthiness of dict.get results, `or` fallbacks,
 * `is not False` identity tests, short-circuit seam call order, str.strip()
 * whitespace set, code-point length/slicing, dict insertion order, setdefault
 * eager default evaluation, %r string formatting in log lines.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "api_server_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "../../../lib/libhash/hash.h"

/* ── internal helpers (no Python counterpart; semantics documented inline) ── */

static char *xstrdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Python truthiness for a JSON value (NULL pointer == Python None). */
static bool jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

/* `v is False` — only an actual boolean False, never 0/None/"". */
static bool is_false(const json_t *v) {
    return v && v->type == JSON_BOOL && !v->bool_val;
}

/* Python str.strip() whitespace set (ASCII subset; fixtures are ASCII). */
static bool is_py_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* malloc'd strip() copy. */
static char *py_strip(const char *s) {
    if (!s) return xstrdup("");
    while (*s && is_py_ws(*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && is_py_ws(s[n - 1])) n--;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* malloc'd ASCII lower() copy (Python str.lower() for the ASCII fixture set). */
static char *py_lower(const char *s) {
    char *out = xstrdup(s ? s : "");
    if (!out) return NULL;
    for (char *p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
    return out;
}

/* UTF-8 code-point count (Python len(str)). Invalid bytes count as one. */
static size_t py_len(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; ) {
        unsigned char c = *p;
        size_t adv = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        while (adv > 1 && p[1] >= 0x80 && p[1] < 0xC0) { p++; adv--; }
        p += adv;
        n++;
    }
    return n;
}

/* malloc'd copy of the first `max` code points (Python s[:max]). */
static char *py_slice(const char *s, size_t max) {
    if (!s) return xstrdup("");
    const unsigned char *p = (const unsigned char *)s;
    size_t n = 0;
    while (*p && n < max) {
        unsigned char c = *p;
        size_t adv = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        const unsigned char *q = p + 1;
        size_t left = adv - 1;
        while (left > 0 && *q >= 0x80 && *q < 0xC0) { q++; left--; }
        p = q;
        n++;
    }
    size_t bytes = (size_t)(p - (const unsigned char *)s);
    char *out = malloc(bytes + 1);
    if (!out) return NULL;
    memcpy(out, s, bytes);
    out[bytes] = '\0';
    return out;
}

/* Growable string buffer. */
typedef struct { char *p; size_t len, cap; } sb_t;

static void sb_init(sb_t *b) { b->cap = 64; b->len = 0; b->p = malloc(b->cap); if (b->p) b->p[0] = '\0'; }
static void sb_add(sb_t *b, const char *s, size_t n) {
    if (!b->p) return;
    if (b->len + n + 1 > b->cap) {
        size_t nc = (b->len + n + 1) * 2;
        char *np = realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}
static void sb_puts(sb_t *b, const char *s) { sb_add(b, s, strlen(s)); }
static void sb_putc(sb_t *b, char c) { sb_add(b, &c, 1); }

/* Python repr(str): quote selection + escape set. Printable non-ASCII bytes
 * are kept verbatim (Python repr keeps printable unicode); the fixture set
 * stays within ASCII + common printable unicode. */
static void py_repr_str(sb_t *b, const char *s) {
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    sb_putc(b, q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { sb_putc(b, '\\'); sb_putc(b, (char)c); }
        else if (c == '\n') sb_puts(b, "\\n");
        else if (c == '\r') sb_puts(b, "\\r");
        else if (c == '\t') sb_puts(b, "\\t");
        else if (c < 0x20 || c == 0x7f) {
            char esc[8];
            snprintf(esc, sizeof(esc), "\\x%02x", c);
            sb_puts(b, esc);
        } else sb_putc(b, (char)c);
    }
    sb_putc(b, q);
}

/* Shortest round-trip float repr (Python float repr algorithm shape). */
static void py_repr_float(sb_t *b, double v) {
    char tmp[64];
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(tmp, sizeof(tmp), "%.*g", prec, v);
        if (strtod(tmp, NULL) == v) break;
    }
    sb_puts(b, tmp);
    /* Python always marks a float: 1.0, never 1 (exponent forms already do). */
    if (!strpbrk(tmp, ".eE")) sb_puts(b, ".0");
}

/* Python repr() of a JSON value. Integral numbers repr as ints (json_t has
 * no int/float identity; JSON int literals are the fixture contract). */
static void py_repr(sb_t *b, const json_t *v) {
    if (!v || v->type == JSON_NULL) { sb_puts(b, "None"); return; }
    switch (v->type) {
        case JSON_BOOL: sb_puts(b, v->bool_val ? "True" : "False"); return;
        case JSON_NUMBER: {
            double d = v->num_val;
            if (floor(d) == d && fabs(d) < 1e16) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%.0f", d);
                sb_puts(b, tmp);
            } else py_repr_float(b, d);
            return;
        }
        case JSON_STRING: py_repr_str(b, v->str_val); return;
        case JSON_ARRAY:
            sb_putc(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) sb_puts(b, ", ");
                py_repr(b, v->c.items[i]);
            }
            sb_putc(b, ']');
            return;
        case JSON_OBJECT:
            sb_putc(b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) sb_puts(b, ", ");
                py_repr_str(b, v->c.keys[i]);
                sb_puts(b, ": ");
                py_repr(b, v->c.items[i]);
            }
            sb_putc(b, '}');
            return;
        default: sb_puts(b, "None"); return;
    }
}

/* request.headers double (borrowed). */
static const json_t *req_headers(const json_t *request) {
    const json_t *h = request ? json_obj_get(request, "headers") : NULL;
    return (h && h->type == JSON_OBJECT) ? h : NULL;
}

/* Merge src object's keys into dst in iteration order (dict.update). */
static void obj_update(json_t *dst, const json_t *src) {
    if (!src || src->type != JSON_OBJECT) return;
    for (size_t i = 0; i < src->c.count; i++)
        json_set(dst, src->c.keys[i], json_copy(src->c.items[i]));
}

/* ── trace recorder ── */

void apisf_trace_add(apisf_trace_t *t, json_t *ev) {
    if (!ev) return;
    if (!t) { json_free(ev); return; }
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}

void apisf_trace_free(apisf_trace_t *t) {
    if (t && t->events) { json_free(t->events); t->events = NULL; }
}

/* ── pure request helpers ── */

/* PoP: apisf_cap_text @ gateway/platforms/api_server.py:_cap_text */
char *apisf_cap_text(const char *text) {
    if (!text) return xstrdup("");
    if (py_len(text) > 65536) return py_slice(text, 65536);
    return xstrdup(text);
}

/* PoP: apisf_cap_list @ gateway/platforms/api_server.py:_cap_list */
json_t *apisf_cap_list(const json_t *items) {
    json_t *out = json_array();
    if (!items || items->type != JSON_ARRAY) return out;
    size_t n = items->c.count;
    if (n > 1000) n = 1000;
    for (size_t i = 0; i < n; i++) json_append(out, json_copy(items->c.items[i]));
    return out;
}

/* PoP: apisf_coerce_request_bool @ gateway/platforms/api_server.py:_coerce_request_bool */
bool apisf_coerce_request_bool(const json_t *value, bool default_value) {
    if (!value || value->type == JSON_NULL) return default_value;
    if (value->type == JSON_BOOL) return value->bool_val;
    if (value->type == JSON_STRING) {
        char *stripped = py_strip(value->str_val);
        char *norm = py_lower(stripped ? stripped : "");
        free(stripped);
        bool result = default_value;
        if (norm) {
            if (!strcmp(norm, "1") || !strcmp(norm, "true") ||
                !strcmp(norm, "yes") || !strcmp(norm, "on")) result = true;
            else if (!strcmp(norm, "0") || !strcmp(norm, "false") ||
                     !strcmp(norm, "no") || !strcmp(norm, "off")) result = false;
            free(norm);
        }
        return result;
    }
    if (value->type == JSON_NUMBER) return value->num_val != 0.0;
    return default_value;
}

/* PoP: apisf_request_relay_metadata @ gateway/platforms/api_server.py:_request_relay_metadata */
json_t *apisf_request_relay_metadata(const json_t *body) {
    if (!body || body->type != JSON_OBJECT) return json_object();
    const json_t *metadata = json_obj_get(body, "metadata");
    if (!metadata || metadata->type != JSON_OBJECT) return json_object();
    return json_copy(metadata);
}

/* PoP: apisf_normalize_image_part @ gateway/platforms/api_server.py:_normalize_image_part */
json_t *apisf_normalize_image_part(const json_t *part, char **err_out) {
    if (err_out) *err_out = NULL;
    if (!part || part->type != JSON_OBJECT) {
        if (err_out) *err_out = xstrdup("invalid_image_url:Image parts must include a non-empty image URL.");
        return NULL;
    }
    const json_t *detail = json_obj_get(part, "detail");
    const json_t *image_ref = json_obj_get(part, "image_url");
    const json_t *url_value;
    if (image_ref && image_ref->type == JSON_OBJECT) {
        url_value = json_obj_get(image_ref, "url");
        /* image_ref.get("detail", detail): a present key wins, even null. */
        if (json_has(image_ref, "detail")) detail = json_obj_get(image_ref, "detail");
    } else {
        url_value = image_ref;
    }
    if (!url_value || url_value->type != JSON_STRING) {
        if (err_out) *err_out = xstrdup("invalid_image_url:Image parts must include a non-empty image URL.");
        return NULL;
    }
    char *stripped = py_strip(url_value->str_val);
    if (!stripped || !stripped[0]) {
        free(stripped);
        if (err_out) *err_out = xstrdup("invalid_image_url:Image parts must include a non-empty image URL.");
        return NULL;
    }
    char *lowered = py_lower(stripped);
    bool ok_scheme;
    if (lowered && strncmp(lowered, "data:", 5) == 0) {
        ok_scheme = strncmp(lowered, "data:image/", 11) == 0 && strchr(stripped, ',') != NULL;
        if (!ok_scheme && err_out)
            *err_out = xstrdup("unsupported_content_type:Only image data URLs are supported. "
                               "Non-image data payloads are not supported.");
    } else {
        ok_scheme = lowered && (strncmp(lowered, "http://", 7) == 0 || strncmp(lowered, "https://", 8) == 0);
        if (!ok_scheme && err_out)
            *err_out = xstrdup("invalid_image_url:Image inputs must use http(s) URLs or data:image/... URLs.");
    }
    free(lowered);
    if (!ok_scheme) { free(stripped); return NULL; }
    json_t *image_url = json_object();
    json_set(image_url, "url", json_string(stripped));
    free(stripped);
    if (detail && detail->type != JSON_NULL) {
        if (detail->type != JSON_STRING) {
            json_free(image_url);
            if (err_out) *err_out = xstrdup("invalid_content_part:Image detail must be a non-empty string when provided.");
            return NULL;
        }
        char *dstrip = py_strip(detail->str_val);
        if (!dstrip || !dstrip[0]) {
            free(dstrip);
            json_free(image_url);
            if (err_out) *err_out = xstrdup("invalid_content_part:Image detail must be a non-empty string when provided.");
            return NULL;
        }
        json_set(image_url, "detail", json_string(dstrip));
        free(dstrip);
    }
    json_t *out = json_object();
    json_set(out, "type", json_string("image_url"));
    json_set(out, "image_url", image_url);
    return out;
}

/* PoP: apisf_chat_usage_payload @ gateway/platforms/api_server.py:_chat_usage_payload */
json_t *apisf_chat_usage_payload(const json_t *usage) {
    static const char *src_keys[3] = {"input_tokens", "output_tokens", "total_tokens"};
    static const char *dst_keys[3] = {"prompt_tokens", "completion_tokens", "total_tokens"};
    json_t *out = json_object();
    for (int i = 0; i < 3; i++) {
        const json_t *v = usage ? json_obj_get(usage, src_keys[i]) : NULL;
        json_set(out, dst_keys[i], v ? json_copy(v) : json_number(0));
    }
    return out;
}

/* PoP: apisf_responses_usage_payload @ gateway/platforms/api_server.py:_responses_usage_payload */
json_t *apisf_responses_usage_payload(const json_t *usage) {
    static const char *keys[3] = {"input_tokens", "output_tokens", "total_tokens"};
    json_t *out = json_object();
    for (int i = 0; i < 3; i++) {
        const json_t *v = usage ? json_obj_get(usage, keys[i]) : NULL;
        json_set(out, keys[i], v ? json_copy(v) : json_number(0));
    }
    return out;
}

/* PoP: apisf_make_request_fingerprint @ gateway/platforms/api_server.py:_make_request_fingerprint */
char *apisf_make_request_fingerprint(const json_t *body, const json_t *keys) {
    json_t *subset = json_object();
    if (keys && keys->type == JSON_ARRAY) {
        for (size_t i = 0; i < keys->c.count; i++) {
            const json_t *k = keys->c.items[i];
            if (!k || k->type != JSON_STRING) continue;
            const json_t *v = body ? json_obj_get(body, k->str_val) : NULL;
            json_set(subset, k->str_val, v ? json_copy(v) : json_null());
        }
    }
    sb_t b; sb_init(&b);
    py_repr(&b, subset);
    json_free(subset);
    char *hex = hash_sha256_hex((const unsigned char *)(b.p ? b.p : ""), b.len);
    free(b.p);
    return hex ? hex : xstrdup("");
}

/* PoP: apisf_session_headers @ gateway/platforms/api_server.py:_session_headers */
json_t *apisf_session_headers(const char *session_id, const char *gateway_session_key) {
    json_t *h = json_object();
    json_set(h, "X-Hermes-Session-Id", json_string(session_id ? session_id : ""));
    if (gateway_session_key && gateway_session_key[0])
        json_set(h, "X-Hermes-Session-Key", json_string(gateway_session_key));
    return h;
}

/* PoP: apisf_result_runtime @ gateway/platforms/api_server.py:_result_runtime */
json_t *apisf_result_runtime(const json_t *result, const json_t *usage) {
    const json_t *runtime = NULL;
    if (result && result->type == JSON_OBJECT) {
        const json_t *r = json_obj_get(result, "runtime");
        if (jtruth(r)) runtime = r;
    }
    if (!runtime && usage && usage->type == JSON_OBJECT) {
        const json_t *r = json_obj_get(usage, "runtime");
        if (jtruth(r)) runtime = r;
    }
    return runtime ? json_copy(runtime) : json_object();
}

/* PoP: apisf_model_lock_state @ gateway/platforms/api_server.py:_model_lock_state */
const char *apisf_model_lock_state(const json_t *runtime_request, const json_t *runtime) {
    if (!jtruth(json_obj_get(runtime_request, "require_model_lock"))) return "";
    return jtruth(runtime) ? "confirmed" : "accepted";
}

/* PoP: apisf_requested_ids @ gateway/platforms/api_server.py:_requested_ids */
json_t *apisf_requested_ids(const json_t *requested, const apisf_seams_t *s) {
    const json_t *req = jtruth(requested) ? requested : NULL;
    const json_t *model_val = req ? json_obj_get(req, "model") : NULL;
    const json_t *provider_val = req ? json_obj_get(req, "provider") : NULL;
    char *model = s->clean_runtime_id(s->ctx, model_val, 200);
    char *provider = s->clean_runtime_id(s->ctx, provider_val, 80);
    json_t *out = json_array();
    json_append(out, json_string(model ? model : ""));
    json_append(out, json_string(provider ? provider : ""));
    free(model);
    free(provider);
    return out;
}

/* ── profile-scoped identity ── */

/* PoP: apisf_names_launch_profile @ gateway/platforms/api_server.py:_names_launch_profile */
bool apisf_names_launch_profile(const char *profile, const apisf_seams_t *s) {
    bool raised = false;
    char *home = s->routing_process_home(s->ctx, &raised);
    if (raised) { free(home); return false; }
    bool matches = s->profile_matches_home(s->ctx, profile ? profile : "", home ? home : "", &raised);
    free(home);
    return raised ? false : matches;
}

/* PoP: apisf_derive_chat_session_id @ gateway/platforms/api_server.py:_derive_chat_session_id */
char *apisf_derive_chat_session_id(const char *system_prompt, const char *first_user_message,
                                   const char *profile, const apisf_seams_t *s) {
    const char *sp = (system_prompt && system_prompt[0]) ? system_prompt : "";
    const char *fum = first_user_message ? first_user_message : "";
    sb_t seed; sb_init(&seed);
    sb_puts(&seed, sp);
    sb_putc(&seed, '\n');
    sb_puts(&seed, fum);
    if (profile && profile[0] && strcmp(profile, "default") != 0 &&
        !apisf_names_launch_profile(profile, s)) {
        sb_t prefixed; sb_init(&prefixed);
        sb_puts(&prefixed, profile);
        sb_putc(&prefixed, '\0');
        sb_add(&prefixed, seed.p ? seed.p : "", seed.len);
        free(seed.p);
        seed = prefixed;
    }
    char *hex = hash_sha256_hex((const unsigned char *)(seed.p ? seed.p : ""), seed.len);
    free(seed.p);
    if (!hex) return xstrdup("api-");
    char *out = malloc(21);
    if (!out) { free(hex); return NULL; }
    memcpy(out, "api-", 4);
    memcpy(out + 4, hex, 16);
    out[20] = '\0';
    free(hex);
    return out;
}

/* PoP: apisf_request_turn_author @ gateway/platforms/api_server.py:_request_turn_author */
json_t *apisf_request_turn_author(const json_t *body, const apisf_seams_t *s, char **err_out) {
    if (err_out) *err_out = NULL;
    const json_t *raw = body ? json_obj_get(body, "author") : NULL;
    if (!raw || raw->type == JSON_NULL) return NULL;
    if (raw->type != JSON_OBJECT) {
        if (err_out) *err_out = xstrdup("author must be an object");
        return NULL;
    }
    bool raised = false;
    const char *exc_msg = NULL;
    json_t *parsed = s->parse_turn_author(s->ctx, raw, &raised, &exc_msg);
    if (raised) {
        if (err_out) *err_out = xstrdup(exc_msg ? exc_msg : "author must be an object");
        json_free(parsed);
        return NULL;
    }
    return parsed;
}

/* ── error-response construction ── */

/* PoP: apisf_error_response @ gateway/platforms/api_server.py:_error_response */
json_t *apisf_error_response(const char *message, int status, const char *err_type,
                             const char *param, const char *code, const json_t *headers,
                             const apisf_seams_t *s) {
    /* _openai_error envelope: redacted message, then type/param/code. */
    char *redacted = s->redact(s->ctx, message ? message : "");
    json_t *err = json_object();
    json_set(err, "message", json_string(redacted ? redacted : ""));
    json_set(err, "type", json_string(err_type ? err_type : "invalid_request_error"));
    json_set(err, "param", param ? json_string(param) : json_null());
    json_set(err, "code", code ? json_string(code) : json_null());
    free(redacted);
    json_t *body = json_object();
    json_set(body, "error", err);
    json_t *resp = s->json_response(s->ctx, body, status, headers);
    json_free(body);
    return resp;
}

/* PoP: apisf_invalid_request @ gateway/platforms/api_server.py:_invalid_request */
json_t *apisf_invalid_request(const char *message, const apisf_seams_t *s) {
    json_t *err = json_object();
    json_set(err, "message", json_string(message ? message : ""));
    json_set(err, "type", json_string("invalid_request_error"));
    json_t *body = json_object();
    json_set(body, "error", err);
    json_t *resp = s->json_response(s->ctx, body, 400, NULL);
    json_free(body);
    return resp;
}

/* PoP: apisf_auth_failed_response @ gateway/platforms/api_server.py:_auth_failed_response */
json_t *apisf_auth_failed_response(const apisf_seams_t *s) {
    json_t *err = json_object();
    json_set(err, "message", json_string("Invalid gateway API key (API_SERVER_KEY)"));
    json_set(err, "type", json_string("gateway_auth_error"));
    json_set(err, "code", json_string("gateway_auth_failed"));
    json_t *body = json_object();
    json_set(body, "error", err);
    json_t *resp = s->json_response(s->ctx, body, 401, NULL);
    json_free(body);
    return resp;
}

/* PoP: apisf_session_db_unavailable @ gateway/platforms/api_server.py:_session_db_unavailable */
json_t *apisf_session_db_unavailable(const apisf_seams_t *s) {
    return apisf_error_response("Session database unavailable", 503, "invalid_request_error",
                                NULL, "session_db_unavailable", NULL, s);
}

/* PoP: apisf_cron_error_response @ gateway/platforms/api_server.py:_cron_error_response */
json_t *apisf_cron_error_response(const char *exc_text, const apisf_seams_t *s) {
    char *redacted = s->redact(s->ctx, exc_text ? exc_text : "");
    json_t *body = json_object();
    json_set(body, "error", json_string(redacted ? redacted : ""));
    free(redacted);
    json_t *resp = s->json_response(s->ctx, body, 500, NULL);
    json_free(body);
    return resp;
}

/* ── auth flow ── */

/* PoP: apisf_expected_api_key @ gateway/platforms/api_server.py:_expected_api_key */
char *apisf_expected_api_key(const char *api_key, const apisf_seams_t *s) {
    const char *profile = s->request_profile(s->ctx);
    if (!profile || !profile[0] || strcmp(profile, "default") == 0)
        return xstrdup(api_key ? api_key : "");
    bool raised = false;
    const char *exc_type = "";
    char *key = s->get_secret(s->ctx, "API_SERVER_KEY", "", &raised, &exc_type);
    if (!raised) {
        /* get_secret(...) or "" — falsy result collapses to the empty key. */
        const char *k = (key && key[0]) ? key : "";
        bool usable = s->has_usable_secret(s->ctx, k, 16, &raised, &exc_type);
        if (!raised) {
            char *out = xstrdup(usable ? k : "");
            free(key);
            return out;
        }
    }
    /* Fail closed; never log the key or exception text. */
    free(key);
    sb_t msg; sb_init(&msg);
    sb_puts(&msg, "Failed to resolve a usable profile-scoped API_SERVER_KEY for ");
    py_repr_str(&msg, profile);
    sb_puts(&msg, ": ");
    sb_puts(&msg, exc_type ? exc_type : "");
    s->log(s->ctx, "warning", msg.p ? msg.p : "");
    free(msg.p);
    return xstrdup("");
}

/* PoP: apisf_check_auth @ gateway/platforms/api_server.py:_check_auth */
json_t *apisf_check_auth(const json_t *request, const char *api_key, const apisf_seams_t *s) {
    const char *profile = s->request_profile(s->ctx);
    char *expected_key = apisf_expected_api_key(api_key, s);
    if (!expected_key || !expected_key[0]) {
        free(expected_key);
        if (!(profile && profile[0] && strcmp(profile, "default") != 0))
            return NULL;
        char *suffix = s->audit_log_suffix(s->ctx, request);
        sb_t msg; sb_init(&msg);
        sb_puts(&msg, "API server rejected request for profile ");
        py_repr_str(&msg, profile);
        sb_puts(&msg, ": no profile-scoped API_SERVER_KEY is configured; ");
        sb_puts(&msg, suffix ? suffix : "");
        s->log(s->ctx, "warning", msg.p ? msg.p : "");
        free(msg.p);
        free(suffix);
        return apisf_auth_failed_response(s);
    }
    const json_t *headers = req_headers(request);
    const char *auth_header = json_get_str(headers, "Authorization", "");
    if (strncmp(auth_header, "Bearer ", 7) == 0) {
        char *token = py_strip(auth_header + 7);
        bool match = s->secrets_match(s->ctx, token ? token : "", expected_key);
        free(token);
        if (match) { free(expected_key); return NULL; }
    }
    free(expected_key);
    char *suffix = s->audit_log_suffix(s->ctx, request);
    sb_t msg; sb_init(&msg);
    sb_puts(&msg, "API server rejected invalid API key: ");
    sb_puts(&msg, suffix ? suffix : "");
    s->log(s->ctx, "warning", msg.p ? msg.p : "");
    free(msg.p);
    free(suffix);
    return apisf_auth_failed_response(s);
}

/* PoP: apisf_require_auth @ gateway/platforms/api_server.py:_require_auth */
json_t *apisf_require_auth(const json_t *request, const apisf_seams_t *s) {
    json_t *auth_err = s->check_auth(s->ctx, request);
    if (auth_err) return auth_err;
    return s->handler(s->ctx, request);
}

/* PoP: apisf_cron_request_guard @ gateway/platforms/api_server.py:_cron_request_guard */
json_t *apisf_cron_request_guard(const json_t *request, bool need_job_id, bool check_draining,
                                 bool cron_available, const apisf_seams_t *s, char **job_id_out) {
    if (job_id_out) *job_id_out = NULL;
    json_t *auth_err = s->check_auth(s->ctx, request);
    if (auth_err) return auth_err;
    if (check_draining) {
        json_t *draining = s->draining_response(s->ctx);
        if (draining) return draining;
    }
    if (!cron_available) {
        json_t *body = json_object();
        json_set(body, "error", json_string("Cron module not available"));
        json_t *resp = s->json_response(s->ctx, body, 501, NULL);
        json_free(body);
        return resp;
    }
    if (!need_job_id) return NULL;
    const json_t *match_info = request ? json_obj_get(request, "match_info") : NULL;
    const char *job_id = json_get_str(match_info, "job_id", "");
    /* _JOB_ID_RE.fullmatch(r"[a-f0-9]{12}") */
    bool valid = strlen(job_id) == 12;
    if (valid) {
        for (const char *p = job_id; *p; p++) {
            if (!((*p >= 'a' && *p <= 'f') || (*p >= '0' && *p <= '9'))) { valid = false; break; }
        }
    }
    if (job_id_out) *job_id_out = xstrdup(job_id);
    if (!valid) {
        char *suffix = s->audit_log_suffix(s->ctx, request);
        sb_t msg; sb_init(&msg);
        sb_puts(&msg, "Cron jobs API rejected invalid job_id ");
        py_repr_str(&msg, job_id);
        sb_puts(&msg, ": ");
        sb_puts(&msg, suffix ? suffix : "");
        s->log(s->ctx, "warning", msg.p ? msg.p : "");
        free(msg.p);
        free(suffix);
        json_t *body = json_object();
        json_set(body, "error", json_string("Invalid job ID format"));
        json_t *resp = s->json_response(s->ctx, body, 400, NULL);
        json_free(body);
        return resp;
    }
    return NULL;
}

/* PoP: apisf_validate_cron_prompt @ gateway/platforms/api_server.py:_validate_cron_prompt */
json_t *apisf_validate_cron_prompt(const char *prompt, bool scan_available, const apisf_seams_t *s) {
    const char *p = prompt ? prompt : "";
    if (py_len(p) > 5000) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Prompt must be \xe2\x89\xa4" " %d characters", 5000);
        json_t *body = json_object();
        json_set(body, "error", json_string(msg));
        json_t *resp = s->json_response(s->ctx, body, 400, NULL);
        json_free(body);
        return resp;
    }
    if (p[0] && scan_available) {
        char *scan_error = s->scan_cron_prompt(s->ctx, p);
        if (scan_error && scan_error[0]) {
            json_t *body = json_object();
            json_set(body, "error", json_string(scan_error));
            free(scan_error);
            json_t *resp = s->json_response(s->ctx, body, 400, NULL);
            json_free(body);
            return resp;
        }
        free(scan_error);
    }
    return NULL;
}

/* PoP: apisf_parse_session_key_header @ gateway/platforms/api_server.py:_parse_session_key_header */
json_t *apisf_parse_session_key_header(const json_t *request, const char *api_key,
                                       const apisf_seams_t *s, char **key_out) {
    if (key_out) *key_out = NULL;
    const json_t *headers = req_headers(request);
    char *raw = py_strip(json_get_str(headers, "X-Hermes-Session-Key", ""));
    if (!raw || !raw[0]) { free(raw); return NULL; }
    if (!api_key || !api_key[0]) {
        free(raw);
        s->log(s->ctx, "warning",
               "X-Hermes-Session-Key rejected: no API key configured. "
               "Set API_SERVER_KEY to enable long-term memory scoping.");
        return apisf_error_response(
            "X-Hermes-Session-Key requires API key authentication. "
            "Configure API_SERVER_KEY to enable this feature.", 403,
            "invalid_request_error", NULL, NULL, NULL, s);
    }
    /* Control characters could enable header injection on the echo path. */
    for (const char *c = raw; *c; c++) {
        if (*c == '\r' || *c == '\n' || *c == '\0') {
            free(raw);
            return apisf_invalid_request("Invalid session key", s);
        }
    }
    if (py_len(raw) > 256) {
        free(raw);
        return apisf_invalid_request("Session key too long", s);
    }
    if (key_out) *key_out = raw;
    else free(raw);
    return NULL;
}

/* PoP: apisf_sse_headers @ gateway/platforms/api_server.py:_sse_headers */
json_t *apisf_sse_headers(const json_t *request, const json_t *extra, const apisf_seams_t *s) {
    json_t *headers = json_object();
    json_set(headers, "Content-Type", json_string("text/event-stream"));
    json_set(headers, "Cache-Control", json_string("no-cache"));
    json_set(headers, "X-Accel-Buffering", json_string("no"));
    const json_t *hdrs = req_headers(request);
    const char *origin = json_get_str(hdrs, "Origin", "");
    json_t *cors = s->cors_headers_for_origin(s->ctx, origin);
    if (cors) { obj_update(headers, cors); json_free(cors); }
    if (jtruth(extra)) obj_update(headers, extra);
    return headers;
}

/* ── approval events / SSE ordering ── */

/* PoP: apisf_approval_request_event @ gateway/platforms/api_server.py:_approval_request_event */
json_t *apisf_approval_request_event(const char *run_id, const json_t *approval_data,
                                     const json_t *fields, const apisf_seams_t *s) {
    json_t *event = (approval_data && approval_data->type == JSON_OBJECT)
        ? json_copy(approval_data) : json_object();
    /* Redact command/description before egress, in tuple order. */
    static const char *redact_keys[2] = {"command", "description"};
    for (int i = 0; i < 2; i++) {
        const json_t *v = json_obj_get(event, redact_keys[i]);
        if (!v) continue;
        const char *text = (v->type == JSON_STRING) ? v->str_val : "";
        char *redacted = s->redact_approval_command(s->ctx, text);
        json_set(event, redact_keys[i], json_string(redacted ? redacted : ""));
        free(redacted);
    }
    bool smart_denied = jtruth(json_obj_get(event, "smart_denied"));
    bool allow_session = !is_false(json_obj_get(event, "allow_session"));
    bool allow_permanent = !is_false(json_obj_get(event, "allow_permanent"));
    json_t *choices = s->approval_choices(s->ctx, smart_denied, allow_session, allow_permanent);
    json_t *f = (fields && fields->type == JSON_OBJECT) ? json_copy(fields) : json_object();
    json_set(f, "choices", choices ? choices : json_array());
    json_t *envelope = s->run_event(s->ctx, run_id ? run_id : "", "approval.request", f);
    json_free(f);
    if (envelope) { obj_update(event, envelope); json_free(envelope); }
    return event;
}

/* PoP: apisf_register_session_stream_approval @ gateway/platforms/api_server.py:_register_session_stream_approval */
void apisf_register_session_stream_approval(json_t *state, const char *run_id) {
    if (!state || state->type != JSON_OBJECT || !run_id) return;
    json_t *map = json_obj_get(state, "run_approval_sessions");
    if (!map || map->type != JSON_OBJECT) {
        map = json_object();
        json_set(state, "run_approval_sessions", map);
    }
    json_set(map, run_id, json_string(run_id));
}

/* PoP: apisf_session_stream_approval_notify @ gateway/platforms/api_server.py:_register_session_stream_approval */
void apisf_session_stream_approval_notify(const char *run_id, const json_t *approval_data,
                                          const char *message_id, const apisf_seams_t *s) {
    json_t *fields = json_object();
    json_set(fields, "message_id", json_string(message_id ? message_id : ""));
    json_t *event = apisf_approval_request_event(run_id, approval_data, fields, s);
    json_free(fields);
    s->set_run_status(s->ctx, run_id ? run_id : "", "waiting_for_approval", "approval.request", event);
    s->enqueue(s->ctx, "approval.request", event);
    json_free(event);
}

/* ── _SessionEventQueue state flows ── */

/* PoP: apisf_queue_payload @ gateway/platforms/api_server.py:payload */
json_t *apisf_queue_payload(json_t *qstate, const char *name, json_t *payload, const apisf_seams_t *s) {
    double seq = json_get_num(qstate, "seq", 0) + 1;
    json_set(qstate, "seq", json_number(seq));
    if (!json_has(payload, "session_id"))
        json_set(payload, "session_id", json_string(json_get_str(qstate, "session_id", "")));
    if (!json_has(payload, "run_id"))
        json_set(payload, "run_id", json_string(json_get_str(qstate, "run_id", "")));
    if (!json_has(payload, "seq"))
        json_set(payload, "seq", json_number(seq));
    /* setdefault("ts", time.time()) evaluates the clock even when ts exists. */
    double ts = s->clock(s->ctx);
    if (!json_has(payload, "ts"))
        json_set(payload, "ts", json_number(ts));
    json_t *tuple = json_array();
    json_append(tuple, json_string(name ? name : ""));
    json_append(tuple, json_copy(payload));
    return tuple;
}

/* _apply closure: put the event, then settle one inflight slot (finally). */
typedef struct {
    json_t *qstate;
    const json_t *event;
    const apisf_seams_t *s;
} apisf_apply_ctx_t;

static void apisf_apply_cb(void *p) {
    apisf_apply_ctx_t *a = p;
    a->s->put_nowait(a->s->ctx, a->event);
    apisf_queue_settle_one(a->qstate);
}

/* PoP: apisf_queue_enqueue @ gateway/platforms/api_server.py:enqueue */
void apisf_queue_enqueue(json_t *qstate, const char *name, json_t *payload, const apisf_seams_t *s) {
    json_t *event = apisf_queue_payload(qstate, name, payload, s);
    bool raised = false;
    const char *running_loop = s->get_running_loop(s->ctx, &raised);
    const char *own_loop = json_get_str(qstate, "loop", "");
    /* `running_loop is self.loop` — token identity in the scripted model. */
    if (!raised && running_loop && strcmp(running_loop, own_loop) == 0) {
        s->put_nowait(s->ctx, event);
        json_free(event);
        return;
    }
    json_set(qstate, "inflight", json_number(json_get_num(qstate, "inflight", 0) + 1));
    apisf_apply_ctx_t apply = { qstate, event, s };
    bool cs_raised = false;
    s->call_soon_threadsafe(s->ctx, apisf_apply_cb, &apply, &cs_raised);
    if (cs_raised) apisf_queue_settle_one(qstate);  /* loop closed: nothing to deliver to */
    json_free(event);
}

/* PoP: apisf_queue_settle_one @ gateway/platforms/api_server.py:_settle_one */
void apisf_queue_settle_one(json_t *qstate) {
    json_set(qstate, "inflight", json_number(json_get_num(qstate, "inflight", 0) - 1));
}

/* PoP: apisf_queue_flush @ gateway/platforms/api_server.py:flush */
void apisf_queue_flush(json_t *qstate, const apisf_seams_t *s) {
    for (;;) {
        if (json_get_num(qstate, "inflight", 0) == 0) return;
        s->sleep_zero(s->ctx);
    }
}

/* ── _IdempotencyCache state flow ── */

/* PoP: apisf_idem_purge @ gateway/platforms/api_server.py:_purge */
void apisf_idem_purge(json_t *store, double ttl, long max_items, const apisf_seams_t *s) {
    if (!store || store->type != JSON_OBJECT) return;
    double now = s->clock(s->ctx);
    /* expired list is built in iteration order before any pop. */
    char **expired = NULL;
    size_t n_expired = 0;
    for (size_t i = 0; i < store->c.count; i++) {
        const json_t *v = store->c.items[i];
        double ts = json_get_num(v, "ts", 0);
        if (now - ts > ttl) {
            char **ne = realloc(expired, (n_expired + 1) * sizeof(char *));
            if (!ne) break;
            expired = ne;
            expired[n_expired++] = xstrdup(store->c.keys[i]);
        }
    }
    for (size_t i = 0; i < n_expired; i++) {
        json_obj_del(store, expired[i]);
        free(expired[i]);
    }
    free(expired);
    /* popitem(last=False): drop the oldest (first) key while over capacity. */
    while ((long)store->c.count > max_items && store->c.count > 0)
        json_obj_del(store, store->c.keys[0]);
}
