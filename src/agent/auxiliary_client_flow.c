/* auxiliary_client_flow.c — FLOW-core port of agent/auxiliary_client.py.
 *
 * Decision core for the auxiliary-LLM-call orchestration: selection, request
 * building, retry/fallback ladder, budget decisions. ALL I/O (provider calls,
 * config/registry reads, clock, verdict predicates, cache writes) goes through
 * the acf_io_t seam dispatcher; see auxiliary_client_flow.h for the live
 * counterpart of every seam name. Pure helpers port directly.
 *
 * Python semantics reproduced: truthiness (None/0/""/[]/{}/False falsy),
 * `or` defaulting, str.strip()/lower()/rstrip(), int() truncation toward zero,
 * short-circuit evaluation order of `and`/`or` chains (seam call ORDER is part
 * of the parity contract), dict copy/update/pop semantics, removesuffix.
 * Fixtures are ASCII: .lower()/\s use ASCII semantics (Python is unicode-aware).
 */
#include "auxiliary_client_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

/* ── shared Python-semantics helpers ───────────────────────────────────── */

/* read a JSON bool NODE directly (json_get_bool needs an object+key). */
static bool acf_bool_val(const json_t *v) {
    return v && v->type == JSON_BOOL && v->bool_val;
}

/* Python bool(v) for a json value (NULL = absent = falsy). */
static bool acf_truthy(const json_t *v) {
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

/* Python str(v or "") for scalar v. Containers stringify to their repr in
 * Python; fixtures for str()-consumed fields stay scalar (documented). */
static char *acf_str_or_empty(const json_t *v) {
    if (!acf_truthy(v)) return strdup("");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof(buf), "%.0f", d);
        else snprintf(buf, sizeof(buf), "%g", d);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}

/* Python str(v) for f-string interpolation of scalars (null -> "None"). */
static char *acf_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof(buf), "%.0f", d);
        else snprintf(buf, sizeof(buf), "%g", d);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}

static bool acf_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str.strip() (no args): whitespace both ends. Caller frees. */
static char *acf_strip(const char *s) {
    if (!s) return strdup("");
    const char *b = s;
    while (*b && acf_is_space(*b)) b++;
    const char *e = b + strlen(b);
    while (e > b && acf_is_space(e[-1])) e--;
    size_t n = (size_t)(e - b);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, b, n);
    r[n] = '\0';
    return r;
}

/* Python str.rstrip(chars): strip any of chars from the right. Caller frees. */
static char *acf_rstrip(const char *s, const char *chars) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    while (n > 0 && strchr(chars, s[n - 1])) n--;
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

/* ASCII lower (fixtures ASCII). Caller frees. */
static char *acf_lower(const char *s) {
    char *r = strdup(s ? s : "");
    if (!r) return NULL;
    for (char *p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    return r;
}

/* Python str.removesuffix(suf). Caller frees. */
static char *acf_removesuffix(const char *s, const char *suf) {
    size_t n = strlen(s), m = strlen(suf);
    if (m > 0 && n >= m && strcmp(s + n - m, suf) == 0) {
        char *r = (char *)malloc(n - m + 1);
        if (!r) return NULL;
        memcpy(r, s, n - m);
        r[n - m] = '\0';
        return r;
    }
    return strdup(s);
}

/* dict check: plain JSON object = Python dict; {"__obj__":true,...} = object. */
static bool acf_is_dict(const json_t *v) {
    if (!v || v->type != JSON_OBJECT) return false;
    json_t *m = json_obj_get(v, "__obj__");
    return !(m && m->type == JSON_BOOL && m->bool_val);
}

static json_t *acf_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

/* borrowed object field or NULL */
static json_t *acf_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}

static const char *acf_gets(const json_t *obj, const char *key, const char *def) {
    return json_get_str(obj, key, def);
}

/* client identity: `sender is route.client` compares the "id" tag. Clients
 * flow through routes/steps/failures as ID STRINGS; registry objects carry
 * the same tag in their "id" field. */
static bool acf_same_client(const json_t *a, const json_t *b) {
    if (!a || !b) return a == b;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val, b->str_val) == 0;
    const char *ia = acf_gets(a, "id", NULL);
    const char *ib = acf_gets(b, "id", NULL);
    if (!ia || !ib) return false;
    return strcmp(ia, ib) == 0;
}

/* getattr(client, "base_url", "") or "" — missing attr and None both "". */
static char *acf_client_base_url(const json_t *client) {
    json_t *b = acf_get(client, "base_url");
    char *s = acf_str_or_empty(b);
    return s;
}

/* PROFILE_REASONING_KEYS (agent/auxiliary_profile_gates.py). */
static const char *ACF_PROFILE_REASONING_KEYS[] = {
    "reasoning", "reasoning_effort", "thinking", "thinking_config", "thinkingconfig",
    "thinking_budget", "thinkingbudget", "enable_thinking", "think", "verbosity",
};
static bool acf_profile_reasoning_key(const char *k) {
    char *norm = acf_strip(k);
    char *low = acf_lower(norm);
    free(norm);
    bool hit = false;
    for (size_t i = 0; i < sizeof(ACF_PROFILE_REASONING_KEYS) / sizeof(char *); i++) {
        if (strcmp(low, ACF_PROFILE_REASONING_KEYS[i]) == 0) { hit = true; break; }
    }
    free(low);
    return hit;
}

/* ── seam dispatcher ───────────────────────────────────────────────────── */

/* PoP: acf_call @ agent/auxiliary_client.py:<seam dispatcher> */
json_t *acf_call(acf_io_t *io, const char *name, json_t *args) {
    if (io && io->trace) {
        json_t *ev = json_object();
        json_set_hdr(ev, "name", name);
        json_set(ev, "args", args ? json_copy(args) : json_object());
        json_append(io->trace, ev);
    }
    if (io && io->call) return io->call(io, name, args);
    return json_null();
}

/* PoP: acf_raised @ agent/auxiliary_client.py:<seam exception model> */
bool acf_raised(const json_t *result) {
    return result && result->type == JSON_OBJECT && json_obj_get(result, "__raise__") != NULL;
}

/* ── pure helpers ──────────────────────────────────────────────────────── */

/* PoP: acf_field @ agent/auxiliary_client.py:_field */
json_t *acf_field(acf_io_t *io, json_t *in) {
    (void)io;
    json_t *obj = acf_get(in, "obj");
    const char *key = acf_gets(in, "key", "");
    json_t *def = acf_get(in, "default");
    json_t *val = NULL;
    if (obj && obj->type == JSON_OBJECT) val = json_obj_get(obj, key);
    /* val is None -> default (dict.get / getattr both yield None) */
    if (!val || val->type == JSON_NULL) return acf_copy_or_null(def);
    return json_copy(val);
}

/* PoP: acf_message_field @ agent/auxiliary_client.py:_message_field */
json_t *acf_message_field(acf_io_t *io, json_t *in) {
    (void)io;
    json_t *msg = acf_get(in, "msg");
    const char *name = acf_gets(in, "name", "");
    json_t *val = (msg && msg->type == JSON_OBJECT) ? json_obj_get(msg, name) : NULL;
    return acf_copy_or_null(val);
}

/* PoP: acf_contains_any @ agent/auxiliary_client.py:_contains_any */
json_t *acf_contains_any(acf_io_t *io, json_t *in) {
    (void)io;
    const char *text = acf_gets(in, "text", "");
    json_t *needles = acf_get(in, "needles");
    bool hit = false;
    if (needles && needles->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(needles); i++) {
            json_t *n = json_get(needles, i);
            if (n && n->type == JSON_STRING && strstr(text, n->str_val)) { hit = true; break; }
        }
    }
    return json_bool(hit);
}

/* PoP: acf_creds_pair @ agent/auxiliary_client.py:_creds_pair */
json_t *acf_creds_pair(acf_io_t *io, json_t *in) {
    (void)io;
    json_t *creds = acf_get(in, "creds");
    char *k_raw = acf_str_or_empty(acf_get(creds, "api_key"));
    char *api_key = acf_strip(k_raw);
    free(k_raw);
    char *b_raw = acf_str_or_empty(acf_get(creds, "base_url"));
    char *b_strip = acf_strip(b_raw);
    free(b_raw);
    char *base_url = acf_rstrip(b_strip, "/");
    free(b_strip);
    json_t *out;
    if (!api_key[0] || !base_url[0]) {
        out = json_null();
    } else {
        out = json_array();
        json_append(out, json_string(api_key));
        json_append(out, json_string(base_url));
    }
    free(api_key);
    free(base_url);
    return out;
}

/* PoP: acf_creds_have_api_key @ agent/auxiliary_client.py:_creds_have_api_key */
json_t *acf_creds_have_api_key(acf_io_t *io, json_t *in) {
    (void)io;
    json_t *creds = acf_get(in, "creds");
    char *raw = acf_str_or_empty(acf_get(creds, "api_key"));
    char *s = acf_strip(raw);
    free(raw);
    bool has = s[0] != '\0';
    free(s);
    return json_bool(has);
}

/* PoP: acf_coerce_positive_timeout @ agent/auxiliary_client.py:_coerce_positive_timeout */
json_t *acf_coerce_positive_timeout(acf_io_t *io, json_t *in) {
    (void)io;
    json_t *raw = acf_get(in, "raw");
    /* isinstance(raw,(int,float)) and not isinstance(raw,bool) and raw > 0:
     * JSON_BOOL is a distinct type here, so bools are rejected structurally. */
    if (raw && raw->type == JSON_NUMBER && raw->num_val > 0)
        return json_number(raw->num_val);
    return json_null();
}

/* PoP: acf_without_temperature @ agent/auxiliary_client.py:_without_temperature */
json_t *acf_without_temperature(acf_io_t *io, json_t *in) {
    (void)io;
    json_t *kwargs = acf_get(in, "kwargs");
    if (!kwargs || kwargs->type != JSON_OBJECT || !json_has(kwargs, "temperature"))
        return json_null();
    json_t *out = json_object();
    for (size_t i = 0; i < kwargs->c.count; i++) {
        if (strcmp(kwargs->c.keys[i], "temperature") == 0) continue;
        json_set(out, kwargs->c.keys[i], json_copy(kwargs->c.items[i]));
    }
    return out;
}

/* PoP: acf_without_max_tokens @ agent/auxiliary_client.py:_without_max_tokens */
json_t *acf_without_max_tokens(acf_io_t *io, json_t *in) {
    (void)io;
    json_t *kwargs = acf_get(in, "kwargs");
    json_t *out = json_object();
    if (kwargs && kwargs->type == JSON_OBJECT) {
        for (size_t i = 0; i < kwargs->c.count; i++) {
            if (strcmp(kwargs->c.keys[i], "max_tokens") == 0) continue;
            if (strcmp(kwargs->c.keys[i], "max_completion_tokens") == 0) continue;
            json_set(out, kwargs->c.keys[i], json_copy(kwargs->c.items[i]));
        }
    }
    return out;
}

/* PoP: acf_without_reasoning_fields @ agent/auxiliary_client.py:_without_reasoning_fields */
json_t *acf_without_reasoning_fields(acf_io_t *io, json_t *in) {
    (void)io;
    json_t *kwargs = acf_get(in, "kwargs");
    json_t *retry = json_object();
    if (kwargs && kwargs->type == JSON_OBJECT) {
        for (size_t i = 0; i < kwargs->c.count; i++)
            json_set(retry, kwargs->c.keys[i], json_copy(kwargs->c.items[i]));
    }
    /* pop(k, None) is not None: a key present with value None does NOT count */
    bool changed = false;
    json_t *v = json_obj_get(retry, "reasoning_effort");
    if (v && v->type != JSON_NULL) { json_obj_del(retry, "reasoning_effort"); changed = true; }
    v = json_obj_get(retry, "_reasoning_config");
    if (v && v->type != JSON_NULL) { json_obj_del(retry, "_reasoning_config"); changed = true; }
    json_t *extra_body = json_obj_get(retry, "extra_body");
    if (acf_is_dict(extra_body)) {
        json_t *remaining = json_object();
        for (size_t i = 0; i < extra_body->c.count; i++) {
            if (!acf_profile_reasoning_key(extra_body->c.keys[i]))
                json_set(remaining, extra_body->c.keys[i], json_copy(extra_body->c.items[i]));
        }
        if (remaining->c.count != extra_body->c.count) {
            if (remaining->c.count > 0) json_set(retry, "extra_body", remaining);
            else { json_free(remaining); json_obj_del(retry, "extra_body"); }
            changed = true;
        } else {
            json_free(remaining);
        }
    }
    if (!changed) { json_free(retry); return json_null(); }
    return retry;
}

/* PoP: acf_affordable_max_tokens_from_error @ agent/auxiliary_client.py:_affordable_max_tokens_from_error */
json_t *acf_affordable_max_tokens_from_error(acf_io_t *io, json_t *in) {
    (void)io;
    json_t *exc = acf_get(in, "exc");
    const char *msg = acf_gets(exc, "message", "");
    /* _AFFORDABLE_TOKENS_RE = r"can only afford\s+([0-9][0-9,]*)" IGNORECASE */
    const char *needle = "can only afford";
    size_t nlen = strlen(needle);
    const char *p = msg;
    long long affordable = -1;
    while (p && *p) {
        const char *hit = NULL;
        for (const char *q = p; *q; q++) {
            if (strncasecmp(q, needle, nlen) == 0) { hit = q; break; }
        }
        if (!hit) break;
        const char *r = hit + nlen;
        const char *ws = r;
        while (*ws && acf_is_space(*ws)) ws++;
        if (ws > r && *ws >= '0' && *ws <= '9') {
            const char *e = ws;
            while (*e && ((*e >= '0' && *e <= '9') || *e == ',')) e++;
            char *digits = (char *)malloc((size_t)(e - ws) + 1);
            if (!digits) return json_null();
            size_t di = 0;
            for (const char *c = ws; c < e; c++) if (*c != ',') digits[di++] = *c;
            digits[di] = '\0';
            affordable = strtoll(digits, NULL, 10);
            free(digits);
            break;
        }
        p = hit + 1;
    }
    if (affordable < 0) return json_null();
    /* _AFFORDABLE_RETRY_MARGIN_TOKENS=64, _AFFORDABLE_RETRY_FLOOR_TOKENS=512 */
    long long capped = affordable - 64;
    if (capped >= 512) return json_number((double)capped);
    return json_null();
}

/* PoP: acf_elapsed_ms @ agent/auxiliary_client.py:_elapsed_ms */
json_t *acf_elapsed_ms(acf_io_t *io, json_t *in) {
    double started_at = json_get_num(in, "started_at", 0.0);
    json_t *now = acf_get(in, "now");
    double now_v;
    if (!now || now->type == JSON_NULL) {
        /* seam: time.monotonic() */
        json_t *r = acf_call(io, "time.monotonic", NULL);
        now_v = (r && r->type == JSON_NUMBER) ? r->num_val : 0.0;
        json_free(r);
    } else {
        now_v = now->num_val;
    }
    double ms = (now_v - started_at) * 1000.0;
    long long whole = (long long)ms;   /* int() truncates toward zero */
    if (whole < 0) whole = 0;          /* max(0, ...) */
    return json_number((double)whole);
}

/* PoP: acf_stamp_latency_once @ agent/auxiliary_client.py:_stamp_latency_once */
json_t *acf_stamp_latency_once(acf_io_t *io, json_t *in) {
    json_t *li = acf_get(in, "latency_info");
    json_t *out_li = acf_copy_or_null(li);
    const char *key = acf_gets(in, "key", "");
    if (out_li && out_li->type == JSON_OBJECT && !json_has(out_li, key)) {
        json_t *ms = acf_elapsed_ms(io, in);
        json_set(out_li, key, ms);
    }
    json_t *out = json_object();
    json_set(out, "latency_info", out_li);
    return out;
}

/* PoP: acf_record_route_info @ agent/auxiliary_client.py:_record_route_info */
json_t *acf_record_route_info(acf_io_t *io, json_t *in) {
    (void)io;
    json_t *ri = acf_get(in, "route_info");
    json_t *out_ri = acf_copy_or_null(ri);
    if (out_ri && out_ri->type == JSON_OBJECT) {
        json_t *provider = acf_get(in, "provider");
        json_t *model = acf_get(in, "model");
        char *ps = acf_truthy(provider) ? acf_pystr(provider) : strdup("auto");
        char *ms = acf_truthy(model) ? acf_pystr(model) : strdup("default");
        json_set(out_ri, "provider", json_string(ps));
        json_set(out_ri, "model", json_string(ms));
        free(ps);
        free(ms);
    }
    json_t *out = json_object();
    json_set(out, "route_info", out_ri);
    return out;
}

/* PoP: acf_fallback_lane @ agent/auxiliary_client.py:_fallback_lane */
json_t *acf_fallback_lane(acf_io_t *io, json_t *in) {
    (void)io;
    json_t *client = acf_get(in, "client");
    char *base = acf_client_base_url(client);
    json_t *out = json_array();
    json_append(out, acf_copy_or_null(acf_get(in, "label")));
    json_append(out, acf_copy_or_null(acf_get(in, "model")));
    json_append(out, json_string(base));
    free(base);
    return out;
}

/* PoP: acf_coerce_llm_message @ agent/auxiliary_client.py:_coerce_llm_message */
json_t *acf_coerce_llm_message(acf_io_t *io, json_t *in) {
    (void)io;
    json_t *response = acf_get(in, "response");
    if (!response || response->type == JSON_NULL || response->type == JSON_STRING)
        return acf_copy_or_null(response);
    json_t *choices = NULL;
    if (acf_is_dict(response)) {
        if (!json_has(response, "choices")) return json_copy(response);
        json_t *ch = json_obj_get(response, "choices");
        choices = acf_truthy(ch) ? ch : NULL;   /* .get("choices") or [] */
    } else if (response->type == JSON_OBJECT) {
        json_t *ch = json_obj_get(response, "choices");
        if (!acf_truthy(ch)) return json_copy(response);
        choices = ch;
    } else {
        return json_copy(response);
    }
    if (choices && choices->type == JSON_ARRAY && json_len(choices) > 0) {
        json_t *first = json_get(choices, 0);
        json_t *val = (first && first->type == JSON_OBJECT) ? json_obj_get(first, "message") : NULL;
        return acf_copy_or_null(val);
    }
    return json_null();
}

/* PoP: acf_dedupe_tool_names @ agent/auxiliary_client.py:_dedupe_tool_names */
json_t *acf_dedupe_tool_names(acf_io_t *io, json_t *in) {
    (void)io;   /* logger.warning on duplicates is silent on both sides */
    json_t *tools = acf_get(in, "tools");
    json_t *out = json_array();
    json_t *seen = json_object();   /* name -> true */
    if (tools && tools->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(tools); i++) {
            json_t *tool = json_get(tools, i);
            json_t *fn = acf_get(tool, "function");
            if (!acf_truthy(fn)) fn = NULL;   /* (tool.get("function") or {}) */
            const char *name = json_get_str(fn, "name", "");
            if (name[0] && json_has(seen, name)) continue;   /* duplicate: skip */
            if (name[0]) json_set(seen, name, json_bool(true));
            json_append(out, json_copy(tool));
        }
    }
    json_free(seen);
    return out;
}

/* PoP: acf_tag_backend @ agent/auxiliary_client.py:_tag_backend */
json_t *acf_tag_backend(acf_io_t *io, json_t *in) {
    (void)io;
    json_t *client = acf_get(in, "client");
    json_t *provider = acf_get(in, "provider");
    json_t *out = acf_copy_or_null(client);
    if (out && out->type != JSON_NULL && provider && provider->type == JSON_STRING
        && provider->str_val && provider->str_val[0]) {
        json_set(out, "_hermes_aux_backend_provider", json_copy(provider));
    }
    return out;
}

/* PoP: acf_provider_outcome @ agent/auxiliary_client.py:_provider_outcome */
json_t *acf_provider_outcome(acf_io_t *io, json_t *in) {
    (void)io;
    json_t *response = acf_get(in, "response");
    json_t *error = acf_get(in, "error");
    json_t *judged_in = acf_get(in, "judged");
    json_t *judged = (judged_in && judged_in->type == JSON_ARRAY) ? json_copy(judged_in) : json_array();
    json_t *out = json_object();
    if (!error || error->type == JSON_NULL) {
        json_set(out, "ret", acf_copy_or_null(response));
        json_set(out, "judged", judged);
        return out;
    }
    json_t *classified = acf_get(error, "classified");
    if (classified && classified->type != JSON_NULL) {
        /* isinstance(error, _ClassifiedAttempt): keep it judged, unwrap its error */
        json_append(judged, json_copy(error));
        json_t *inner = acf_get(classified, "error");
        json_set(out, "ret", acf_copy_or_null(inner));
    } else {
        json_set(out, "ret", json_copy(error));
    }
    json_set(out, "judged", judged);
    return out;
}

/* PoP: acf_ladder_step_call @ agent/auxiliary_client.py:_ladder_step_call */
json_t *acf_ladder_step_call(acf_io_t *io, json_t *in) {
    (void)io;
    json_t *step = acf_get(in, "step");
    json_t *req = acf_get(in, "req");
    const char *kind = acf_gets(step, "kind", "");
    json_t *args = acf_get(step, "args");
    json_t *out = json_array();
    if (strcmp(kind, "call") == 0) {
        json_append(out, json_string("call"));
        json_append(out, acf_copy_or_null(args));
        json_t *kw = json_object();
        json_set(kw, "provider", acf_copy_or_null(acf_get(req, "resolved_provider")));
        json_set(kw, "api_mode", acf_copy_or_null(acf_get(req, "resolved_api_mode")));
        json_append(out, kw);
    } else if (strcmp(kind, "retry_same_provider") == 0) {
        json_t *retry_kwargs = acf_get(in, "retry_kwargs");
        json_t *kw = retry_kwargs ? json_copy(retry_kwargs) : json_object();
        json_t *rprovider = (args && args->type == JSON_ARRAY && json_len(args) > 0) ? json_get(args, 0) : NULL;
        json_t *rmodel = (args && args->type == JSON_ARRAY && json_len(args) > 1) ? json_get(args, 1) : NULL;
        json_t *repairs = (args && args->type == JSON_ARRAY && json_len(args) > 2) ? json_get(args, 2) : NULL;
        json_set(kw, "resolved_provider", acf_copy_or_null(rprovider));
        json_set(kw, "resolved_model", acf_copy_or_null(rmodel));
        json_set(kw, "repairs", acf_copy_or_null(repairs));
        json_append(out, json_string("retry"));
        json_append(out, json_array());
        json_append(out, kw);
    } else {
        json_append(out, json_string("fallback"));
        json_append(out, acf_copy_or_null(args));
        json_append(out, acf_copy_or_null(acf_get(in, "candidate_kwargs")));
    }
    return out;
}

/* PoP: acf_failure_sender @ agent/auxiliary_client.py:_failure_sender */
json_t *acf_failure_sender(acf_io_t *io, json_t *in) {
    (void)io;
    json_t *route = acf_get(in, "route");
    json_t *failure = acf_get(in, "failure");
    json_t *fc = acf_get(failure, "client");
    if (!fc || fc->type == JSON_NULL) return acf_copy_or_null(acf_get(route, "client"));
    return json_copy(fc);
}
/* ── rejection detectors / rung acceptance ─────────────────────────────── */

/* status gate shared by the 400/422 detectors:
 * status = getattr(exc, "status_code", None); gate when not None and not 400/422 */
static bool acf_status_gate_pass(const json_t *exc) {
    json_t *st = acf_get(exc, "status_code");
    if (st && st->type == JSON_NUMBER) {
        double s = st->num_val;
        if (s != 400.0 && s != 422.0) return false;
    }
    return true;
}

/* PoP: acf_is_structured_output_rejection @ agent/auxiliary_client.py:_is_structured_output_rejection */
json_t *acf_is_structured_output_rejection(acf_io_t *io, json_t *in) {
    json_t *exc = acf_get(in, "exc");
    if (!acf_status_gate_pass(exc)) return json_bool(false);
    const char *msg = acf_gets(exc, "message", "");
    char *err_lower = acf_lower(msg);
    bool hit = false;
    /* vLLM grammar-backend failures name the translated parameter */
    if (strstr(err_lower, "guided_grammar") || strstr(err_lower, "xgrammar")
        || strstr(err_lower, "compile_grammar_error")) hit = true;
    if (!hit && strstr(err_lower, "extra inputs are not permitted")
        && (strstr(err_lower, "response_format") || strstr(err_lower, "output_config")))
        hit = true;
    if (!hit && strstr(err_lower, "response_format") && strstr(err_lower, "unavailable"))
        hit = true;
    if (!hit && strstr(err_lower, "response_format") && strstr(err_lower, "json_schema"))
        hit = true;
    if (!hit && (strstr(err_lower, "response mime type") || strstr(err_lower, "response_schema")
                 || strstr(err_lower, "response_json_schema")))
        hit = true;
    free(err_lower);
    if (hit) return json_bool(true);
    /* _is_unsupported_parameter_error(exc, "response_format") or (exc, "output_config") */
    const char *params[2] = { "response_format", "output_config" };
    for (int i = 0; i < 2; i++) {
        json_t *args = json_object();
        json_set(args, "exc", acf_copy_or_null(exc));
        json_set_hdr(args, "param", params[i]);
        json_t *r = acf_call(io, "_is_unsupported_parameter_error", args);
        json_free(args);
        bool yes = acf_truthy(r) && !acf_raised(r);
        json_free(r);
        if (yes) return json_bool(true);
    }
    return json_bool(false);
}

/* PoP: acf_is_reasoning_field_rejection @ agent/auxiliary_client.py:_is_reasoning_field_rejection */
json_t *acf_is_reasoning_field_rejection(acf_io_t *io, json_t *in) {
    json_t *exc = acf_get(in, "exc");
    if (!acf_status_gate_pass(exc)) return json_bool(false);
    json_t *args = json_object();
    json_set_hdr(args, "error_msg", acf_gets(exc, "message", ""));
    json_t *r = acf_call(io, "is_reasoning_field_rejection", args);
    json_free(args);
    bool yes = acf_truthy(r) && !acf_raised(r);
    json_free(r);
    return json_bool(yes);
}

/* PoP: acf_is_reasoning_required_rejection @ agent/auxiliary_client.py:_is_reasoning_required_rejection */
json_t *acf_is_reasoning_required_rejection(acf_io_t *io, json_t *in) {
    json_t *exc = acf_get(in, "exc");
    if (!acf_status_gate_pass(exc)) return json_bool(false);
    json_t *args = json_object();
    json_set_hdr(args, "error_msg", acf_gets(exc, "message", ""));
    json_t *r = acf_call(io, "is_reasoning_required_rejection", args);
    json_free(args);
    bool yes = acf_truthy(r) && !acf_raised(r);
    json_free(r);
    return json_bool(yes);
}

/* PoP: acf_is_max_tokens_rejection @ agent/auxiliary_client.py:_is_max_tokens_rejection */
json_t *acf_is_max_tokens_rejection(acf_io_t *io, json_t *in) {
    json_t *exc = acf_get(in, "exc");
    json_t *client = acf_get(in, "client");
    const char *err_str = acf_gets(exc, "message", "");
    /* ZAI vision: code 1210 on a bigmodel host, message never names max_tokens */
    char *base = acf_client_base_url(client);
    bool is_zai_param_error = strstr(err_str, "1210") && strstr(base, "bigmodel");
    free(base);
    if (strstr(err_str, "max_tokens") || strstr(err_str, "unsupported_parameter"))
        return json_bool(true);
    json_t *args = json_object();
    json_set(args, "exc", acf_copy_or_null(exc));
    json_set_hdr(args, "param", "max_tokens");
    json_t *r = acf_call(io, "_is_unsupported_parameter_error", args);
    json_free(args);
    bool yes = acf_truthy(r) && !acf_raised(r);
    json_free(r);
    if (yes) return json_bool(true);
    return json_bool(is_zai_param_error);
}

/* PoP: acf_param_rung_accepts @ agent/auxiliary_client.py:_param_rung_accepts */
json_t *acf_param_rung_accepts(acf_io_t *io, json_t *in) {
    json_t *failure = acf_get(in, "failure");
    /* failure.rejects_parameter or aux_errors.ladder_accepts(failure, rung_spent=True) */
    if (json_get_bool(failure, "rejects_parameter", false)) return json_bool(true);
    json_t *args = json_object();
    json_set(args, "failure", acf_copy_or_null(failure));
    json_set(args, "rung_spent", json_bool(true));
    json_t *r = acf_call(io, "ladder_accepts", args);
    json_free(args);
    bool yes = acf_truthy(r) && !acf_raised(r);
    json_free(r);
    return json_bool(yes);
}

/* ── selection / config / budget ───────────────────────────────────────── */

/* _FAST_MODEL_TASKS = frozenset({"title_generation"}) */
static bool acf_fast_model_task(const char *task) {
    return task && strcmp(task, "title_generation") == 0;
}

/* utils/env.py:is_truthy_value semantics over json (None->default, str->truthy
 * set, else bool(value)). Inlined: the utils port is string-only. */
static bool acf_truthy_value(const json_t *v, bool def) {
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_STRING) {
        char *s = acf_strip(v->str_val);
        char *l = acf_lower(s);
        free(s);
        bool hit = strcmp(l, "1") == 0 || strcmp(l, "true") == 0
                || strcmp(l, "yes") == 0 || strcmp(l, "on") == 0;
        free(l);
        return hit;
    }
    return acf_truthy(v);
}

/* PoP: acf_task_prefers_fast_model @ agent/auxiliary_client.py:_task_prefers_fast_model */
json_t *acf_task_prefers_fast_model(acf_io_t *io, json_t *in) {
    const char *task = acf_gets(in, "task", NULL);
    /* task in _FAST_MODEL_TASKS and is_truthy_value(cfg.get("prefer_fast_model")) */
    if (!acf_fast_model_task(task)) return json_bool(false);
    json_t *args = json_object();
    json_set_hdr(args, "task", task);
    json_t *cfg = acf_call(io, "_get_auxiliary_task_config", args);
    json_free(args);
    bool hit = acf_truthy_value(acf_get(cfg, "prefer_fast_model"), false);
    json_free(cfg);
    return json_bool(hit);
}

/* PoP: acf_fast_lane_config_fields @ agent/auxiliary_client.py:_fast_lane_config_fields */
json_t *acf_fast_lane_config_fields(acf_io_t *io, json_t *in) {
    json_t *config = acf_get(in, "config");
    char *p_raw = acf_str_or_empty(acf_get(config, "provider"));
    char *p_strip = acf_strip(p_raw);
    free(p_raw);
    char *provider = acf_lower(p_strip);
    free(p_strip);
    char *m_raw = acf_str_or_empty(acf_get(config, "model"));
    char *model = acf_strip(m_raw);
    free(m_raw);
    /* parse_reasoning_effort(config.get("reasoning_effort")) — seam */
    json_t *args = json_object();
    json_set(args, "effort", acf_copy_or_null(acf_get(config, "reasoning_effort")));
    json_t *parsed = acf_call(io, "parse_reasoning_effort", args);
    json_free(args);
    bool non_reasoning = false;
    if (parsed && parsed->type == JSON_OBJECT) {
        json_t *en = json_obj_get(parsed, "enabled");
        non_reasoning = en && en->type == JSON_BOOL && !en->bool_val;  /* is False */
    }
    json_free(parsed);
    json_t *out = json_array();
    json_append(out, json_string(provider));
    json_append(out, json_string(model));
    json_append(out, json_bool(non_reasoning));
    free(provider);
    free(model);
    return out;
}

/* PoP: acf_compression_config_claims_fast_lane @ agent/auxiliary_client.py:_compression_config_claims_fast_lane */
json_t *acf_compression_config_claims_fast_lane(acf_io_t *io, json_t *in) {
    json_t *fields = acf_fast_lane_config_fields(io, in);
    json_t *provider_j = json_get(fields, 0);
    const char *provider = (provider_j && provider_j->type == JSON_STRING) ? provider_j->str_val : "";
    json_t *model_j = json_get(fields, 1);
    char *ml = acf_lower(model_j && model_j->type == JSON_STRING ? model_j->str_val : "");
    json_t *nr_j = json_get(fields, 2);
    bool non_reasoning = nr_j && nr_j->type == JSON_BOOL && nr_j->bool_val;
    bool claims = provider[0] && strcmp(provider, "auto") != 0
               && ml[0] && strcmp(ml, "auto") != 0 && non_reasoning;
    free(ml);
    json_free(fields);
    return json_bool(claims);
}

/* PoP: acf_compression_fast_lane_controls @ agent/auxiliary_client.py:_compression_fast_lane_controls */
json_t *acf_compression_fast_lane_controls(acf_io_t *io, json_t *in) {
    const char *task = acf_gets(in, "task", NULL);
    json_t *max_tokens = acf_get(in, "max_tokens");
    json_t *extra_body = acf_get(in, "extra_body");
    json_t *out = json_array();
    /* task != "compression" or max_tokens is not None -> passthrough */
    if (!task || strcmp(task, "compression") != 0 || (max_tokens && max_tokens->type != JSON_NULL)) {
        json_append(out, acf_copy_or_null(max_tokens));
        json_append(out, acf_copy_or_null(extra_body));
        return out;
    }
    json_t *body = acf_is_dict(extra_body) ? json_copy(extra_body) : json_object();
    /* lane = resolve_compression_fast_lane(actual_provider, actual_model,
     *          requested_provider=..., requested_model=..., route_config=...) */
    json_t *args = json_object();
    json_set(args, "actual_provider", acf_copy_or_null(acf_get(in, "actual_provider")));
    json_set(args, "actual_model", acf_copy_or_null(acf_get(in, "actual_model")));
    json_set(args, "requested_provider", acf_copy_or_null(acf_get(in, "requested_provider")));
    json_set(args, "requested_model", acf_copy_or_null(acf_get(in, "requested_model")));
    json_set(args, "route_config", acf_copy_or_null(acf_get(in, "route_config")));
    json_t *lane = acf_call(io, "resolve_compression_fast_lane", args);
    json_free(args);
    json_t *lane_rc = acf_get(lane, "reasoning_config");
    if (lane_rc && lane_rc->type != JSON_NULL) {
        if (!json_has(body, "reasoning")) json_set(body, "reasoning", json_copy(lane_rc));
    } else {
        /* elif _compression_config_claims_fast_lane(leak_guard_config) */
        json_t *lg = json_object();
        json_set(lg, "config", acf_copy_or_null(acf_get(in, "leak_guard_config")));
        json_t *claims = acf_compression_config_claims_fast_lane(io, lg);
        json_free(lg);
        if (acf_bool_val(claims)) json_obj_del(body, "reasoning");
        json_free(claims);
    }
    json_free(lane);
    json_append(out, acf_copy_or_null(max_tokens));
    json_append(out, body);
    return out;
}

/* PoP: acf_get_task_no_progress_timeout @ agent/auxiliary_client.py:_get_task_no_progress_timeout */
json_t *acf_get_task_no_progress_timeout(acf_io_t *io, json_t *in) {
    const char *task = acf_gets(in, "task", NULL);
    if (!task || !task[0]) return json_null();   /* if not task */
    json_t *args = json_object();
    json_set_hdr(args, "task", task);
    json_t *cfg = acf_call(io, "_get_auxiliary_task_config", args);
    json_free(args);
    json_t *raw = acf_copy_or_null(acf_get(cfg, "no_progress_timeout"));
    json_free(cfg);
    if (!raw || raw->type == JSON_NULL) { json_free(raw); return json_null(); }
    /* try: value = float(raw) except (ValueError, TypeError): value = 0.0 */
    double value = 0.0;
    bool is_bool = raw->type == JSON_BOOL;
    if (raw->type == JSON_NUMBER) value = raw->num_val;
    else if (raw->type == JSON_BOOL) value = raw->bool_val ? 1.0 : 0.0;
    else if (raw->type == JSON_STRING) {
        /* Python float(str): surrounding whitespace ok, full parse required */
        char *s = acf_strip(raw->str_val);
        char *end = NULL;
        double d = strtod(s, &end);
        if (end && end != s && *end == '\0') value = d;
        free(s);
    }
    /* isinstance(raw, bool) or value <= 0 -> warning + None */
    if (is_bool || value <= 0) { json_free(raw); return json_null(); }
    json_free(raw);
    return json_number(value);
}

/* PoP: acf_unwrap_moa_provider @ agent/auxiliary_client.py:_unwrap_moa_provider */
json_t *acf_unwrap_moa_provider(acf_io_t *io, json_t *in) {
    json_t *prov = acf_get(in, "prov");
    json_t *mdl = acf_get(in, "mdl");
    json_t *out = json_array();
    const char *ps = prov && prov->type == JSON_STRING ? prov->str_val : "";
    char *strip = acf_strip(ps);
    char *low = acf_lower(strip);
    free(strip);
    bool is_moa = strcmp(low, "moa") == 0;
    free(low);
    if (!is_moa) {
        json_append(out, acf_copy_or_null(prov));
        json_append(out, acf_copy_or_null(mdl));
        return out;
    }
    json_t *args = json_object();
    json_set(args, "preset_name", acf_copy_or_null(mdl));
    json_t *agg = acf_call(io, "_resolve_moa_aggregator", args);
    json_free(args);
    json_t *ap = acf_get(agg, "provider");
    json_t *am = acf_get(agg, "model");
    if (acf_truthy(ap) && acf_truthy(am)) {
        json_append(out, json_copy(ap));
        json_append(out, json_copy(am));
    } else {
        json_append(out, acf_copy_or_null(prov));
        json_append(out, acf_copy_or_null(mdl));
    }
    json_free(agg);
    return out;
}

/* _LOCAL_SERVER_ALIASES keys */
static bool acf_local_server_alias(const char *name) {
    static const char *aliases[] = { "ollama", "vllm", "llamacpp", "llama.cpp", "llama-cpp" };
    for (size_t i = 0; i < sizeof(aliases) / sizeof(char *); i++)
        if (strcmp(name, aliases[i]) == 0) return true;
    return false;
}

/* PoP: acf_profile_credentials_bypass_custom @ agent/auxiliary_client.py:_profile_credentials_bypass_custom */
json_t *acf_profile_credentials_bypass_custom(acf_io_t *io, json_t *in) {
    const char *name = acf_gets(in, "name", "");
    json_t *args = json_object();
    json_set_hdr(args, "name", name);
    json_t *profile = acf_call(io, "get_provider_profile", args);
    json_free(args);
    if (acf_raised(profile)) {   /* except Exception: logger.debug; return False */
        json_free(profile);
        return json_bool(false);
    }
    bool hit = false;
    if (profile && profile->type == JSON_OBJECT) {
        const char *pname = acf_gets(profile, "name", NULL);
        /* profile is not None and profile.name == name and
         * (profile.ambient_credentials or profile.auth_type == "external_process") */
        if (pname && strcmp(pname, name) == 0) {
            if (acf_truthy(acf_get(profile, "ambient_credentials"))) hit = true;
            else if (strcmp(acf_gets(profile, "auth_type", ""), "external_process") == 0) hit = true;
        }
    }
    json_free(profile);
    return json_bool(hit);
}

/* PoP: acf_builtin_provider_present @ agent/auxiliary_client.py:_builtin_provider_present */
json_t *acf_builtin_provider_present(acf_io_t *io, json_t *in) {
    const char *name = acf_gets(in, "name", "");
    json_t *args = json_object();
    json_set_hdr(args, "name", name);
    json_t *r = acf_call(io, "get_provider", args);
    json_free(args);
    if (!acf_raised(r)) {
        bool present = r && r->type != JSON_NULL;   /* get_provider(name) is not None */
        json_free(r);
        return json_bool(present);
    }
    json_free(r);
    /* except Exception: high-risk set or profile-credentials bypass */
    static const char *high_risk[] = {
        "anthropic", "minimax-oauth", "nous", "openai-codex", "qwen-oauth", "xai-oauth",
    };
    for (size_t i = 0; i < sizeof(high_risk) / sizeof(char *); i++)
        if (strcmp(name, high_risk[i]) == 0) return json_bool(true);
    return acf_profile_credentials_bypass_custom(io, in);
}

/* PoP: acf_named_custom_provider_present @ agent/auxiliary_client.py:_named_custom_provider_present */
json_t *acf_named_custom_provider_present(acf_io_t *io, json_t *in) {
    const char *name = acf_gets(in, "name", "");
    json_t *args = json_object();
    json_set_hdr(args, "name", name);
    json_t *r = acf_call(io, "_get_named_custom_provider", args);
    json_free(args);
    if (acf_raised(r)) {   /* fail closed: never widen True on import/load failure */
        json_free(r);
        return json_bool(false);
    }
    bool present = r && r->type != JSON_NULL;
    json_free(r);
    return json_bool(present);
}

/* PoP: acf_preserve_provider_with_base_url @ agent/auxiliary_client.py:_preserve_provider_with_base_url */
json_t *acf_preserve_provider_with_base_url(acf_io_t *io, json_t *in) {
    json_t *prov = acf_get(in, "prov");
    char *raw = acf_str_or_empty(prov);
    char *strip = acf_strip(raw);
    free(raw);
    char *normalized = acf_lower(strip);
    free(strip);
    bool keep = false;
    if (!normalized[0] || strcmp(normalized, "auto") == 0 || strcmp(normalized, "custom") == 0
        || strncmp(normalized, "custom:", 7) == 0) {
        keep = false;
    } else if (acf_local_server_alias(normalized)) {
        keep = true;
    } else {
        json_t *sub = json_object();
        json_set_hdr(sub, "name", normalized);
        json_t *b = acf_builtin_provider_present(io, sub);
        if (acf_bool_val(b)) keep = true;
        json_free(b);
        if (!keep) {
            json_t *n = acf_named_custom_provider_present(io, sub);
            keep = acf_bool_val(n);
            json_free(n);
        }
        json_free(sub);
    }
    free(normalized);
    return json_bool(keep);
}

/* PoP: acf_routes_to_custom_endpoint @ agent/auxiliary_client.py:_routes_to_custom_endpoint */
json_t *acf_routes_to_custom_endpoint(acf_io_t *io, json_t *in) {
    json_t *args = json_object();
    json_set(args, "provider", acf_copy_or_null(acf_get(in, "provider_norm")));
    json_t *name_j = acf_call(io, "_normalize_aux_provider", args);
    json_free(args);
    const char *name = (name_j && name_j->type == JSON_STRING) ? name_j->str_val : "";
    if (strcmp(name, "custom") == 0) { json_free(name_j); return json_bool(true); }
    json_t *args2 = json_object();
    json_set_hdr(args2, "name", name);
    json_t *r = acf_call(io, "_get_named_custom_provider", args2);
    json_free(args2);
    json_free(name_j);
    bool present = !acf_raised(r) && r && r->type != JSON_NULL;
    json_free(r);
    return json_bool(present);
}

/* PoP: acf_classification_provider @ agent/auxiliary_client.py:_classification_provider */
json_t *acf_classification_provider(acf_io_t *io, json_t *in) {
    json_t *client = acf_get(in, "client");
    /* backend = getattr(client, "_hermes_aux_backend_provider", "") */
    json_t *backend = acf_get(client, "_hermes_aux_backend_provider");
    if (backend && backend->type == JSON_STRING && backend->str_val && backend->str_val[0])
        return json_copy(backend);
    /* label = _fallback_provider_from_label(_effective_provider_for_client(client, fallback)) */
    json_t *a1 = json_object();
    json_set(a1, "client", acf_copy_or_null(client));
    json_set(a1, "fallback", acf_copy_or_null(acf_get(in, "fallback")));
    json_t *eff = acf_call(io, "_effective_provider_for_client", a1);
    json_free(a1);
    json_t *a2 = json_object();
    json_set(a2, "label", eff);   /* ownership transferred */
    json_t *label_j = acf_call(io, "_fallback_provider_from_label", a2);
    json_free(a2);
    const char *label = (label_j && label_j->type == JSON_STRING) ? label_j->str_val : "";
    /* if (route := _normalize_aux_provider(label)) not in {auto, custom, api-key} */
    json_t *a3 = json_object();
    json_set_hdr(a3, "provider", label);
    json_t *route_j = acf_call(io, "_normalize_aux_provider", a3);
    json_free(a3);
    const char *route = (route_j && route_j->type == JSON_STRING) ? route_j->str_val : "";
    if (strcmp(route, "auto") != 0 && strcmp(route, "custom") != 0 && strcmp(route, "api-key") != 0) {
        json_t *out = json_copy(label_j);
        json_free(label_j);
        json_free(route_j);
        return out;
    }
    /* return _recoverable_pool_provider("custom" if route=="custom" else "auto",
     *          client, main_runtime=_normalize_main_runtime(main_runtime), same_path=True) or label */
    json_t *a4 = json_object();
    json_set(a4, "main_runtime", acf_copy_or_null(acf_get(in, "main_runtime")));
    json_t *norm_rt = acf_call(io, "_normalize_main_runtime", a4);
    json_free(a4);
    json_t *a5 = json_object();
    json_set_hdr(a5, "resolved_provider", strcmp(route, "custom") == 0 ? "custom" : "auto");
    json_set(a5, "client", acf_copy_or_null(client));
    json_set(a5, "main_runtime", norm_rt);   /* ownership transferred */
    json_set(a5, "same_path", json_bool(true));
    json_t *pool = acf_call(io, "_recoverable_pool_provider", a5);
    json_free(a5);
    json_t *out;
    if (acf_truthy(pool) && !acf_raised(pool)) out = json_copy(pool);
    else out = json_copy(label_j);
    json_free(pool);
    json_free(label_j);
    json_free(route_j);
    return out;
}

/* PoP: acf_provider_for_host @ agent/auxiliary_client.py:_provider_for_host */
json_t *acf_provider_for_host(acf_io_t *io, json_t *in) {
    const char *base_url = acf_gets(in, "base_url", "");
    json_t *table = acf_get(in, "table");
    if (table && table->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(table); i++) {
            json_t *row = json_get(table, i);
            if (!row || row->type != JSON_ARRAY || json_len(row) < 2) continue;
            const char *host = json_get_str(json_get(row, 0), NULL, "");
            json_t *args = json_object();
            json_set_hdr(args, "base_url", base_url);
            json_set_hdr(args, "domain", host);
            json_t *m = acf_call(io, "base_url_host_matches", args);
            json_free(args);
            bool hit = acf_truthy(m) && !acf_raised(m);
            json_free(m);
            if (hit) return json_copy(json_get(row, 1));
        }
    }
    /* from providers import profile_for_host; getattr(profile_for_host(base_url), "name", None) */
    json_t *args = json_object();
    json_set_hdr(args, "base_url", base_url);
    json_t *profile = acf_call(io, "profile_for_host", args);
    json_free(args);
    json_t *out = acf_copy_or_null(acf_get(profile, "name"));
    json_free(profile);
    return out;
}

/* PoP: acf_relay_path @ agent/auxiliary_client.py:_relay_path */
json_t *acf_relay_path(acf_io_t *io, json_t *in) {
    json_t *a1 = json_object();
    json_set(a1, "base_url", acf_copy_or_null(acf_get(in, "base_url")));
    json_t *openai = acf_call(io, "_to_openai_base_url", a1);
    json_free(a1);
    json_t *a2 = json_object();
    json_set(a2, "base_url", openai);   /* ownership transferred */
    json_t *path_j = acf_call(io, "base_url_path", a2);
    json_free(a2);
    const char *path = (path_j && path_j->type == JSON_STRING) ? path_j->str_val : "";
    char *out = acf_removesuffix(path, "/v1");
    json_free(path_j);
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* PoP: acf_context_too_small @ agent/auxiliary_client.py:_context_too_small */
json_t *acf_context_too_small(acf_io_t *io, json_t *in) {
    json_t *min_ctx = acf_get(in, "min_ctx");
    if (!min_ctx || min_ctx->type == JSON_NULL) return json_null();
    json_t *entry = acf_get(in, "entry");
    /* api_key=_fallback_entry_api_key(entry) or "" — evaluated first (inner arg) */
    json_t *a0 = json_object();
    json_set(a0, "entry", acf_copy_or_null(entry));
    json_t *key_j = acf_call(io, "_fallback_entry_api_key", a0);
    json_free(a0);
    char *key = acf_str_or_empty(key_j);   /* None or "" -> "" */
    json_free(key_j);
    char *base = acf_str_or_empty(acf_get(entry, "base_url"));
    json_t *args = json_object();
    json_set(args, "provider", acf_copy_or_null(acf_get(in, "provider")));
    json_set(args, "model", acf_copy_or_null(acf_get(in, "model")));
    json_set_hdr(args, "base_url", base);
    json_set_hdr(args, "api_key", key);
    free(base);
    free(key);
    json_t *ctx_j = acf_call(io, "_candidate_context_window", args);
    json_free(args);
    /* fb_ctx is None or fb_ctx >= min_ctx -> None */
    if (!ctx_j || ctx_j->type != JSON_NUMBER || ctx_j->num_val >= min_ctx->num_val) {
        json_free(ctx_j);
        return json_null();
    }
    /* return f"{label} (context too small: {fb_ctx}<{min_ctx})" */
    char *ctx_s = acf_pystr(ctx_j);
    char *min_s = acf_pystr(min_ctx);
    const char *label = acf_gets(in, "label", "");
    size_t n = strlen(label) + strlen(ctx_s) + strlen(min_s) + 40;
    char *buf = (char *)malloc(n);
    if (buf) snprintf(buf, n, "%s (context too small: %s<%s)", label, ctx_s, min_s);
    free(ctx_s);
    free(min_s);
    json_free(ctx_j);
    json_t *out = json_string(buf ? buf : "");
    free(buf);
    return out;
}

/* _NOUS_PROVIDER_NAMES = frozenset({"nous","nous-portal","nousresearch"}) */
static bool acf_nous_provider_name(const char *n) {
    return n && (strcmp(n, "nous") == 0 || strcmp(n, "nous-portal") == 0
                 || strcmp(n, "nousresearch") == 0);
}

/* PoP: acf_nous_on_messages_wire @ agent/auxiliary_client.py:_nous_on_messages_wire */
json_t *acf_nous_on_messages_wire(acf_io_t *io, json_t *in) {
    const char *provider_norm = acf_gets(in, "provider_norm", "");
    if (!acf_nous_provider_name(provider_norm)) return json_bool(false);
    /* from hermes_cli.providers import nous_api_mode; == "anthropic_messages" */
    json_t *args = json_object();
    json_set(args, "model", acf_copy_or_null(acf_get(in, "model")));
    json_t *mode = acf_call(io, "nous_api_mode", args);
    json_free(args);
    bool hit = mode && mode->type == JSON_STRING
            && strcmp(mode->str_val, "anthropic_messages") == 0;
    json_free(mode);
    return json_bool(hit);
}

/* _GEMINI_NATIVE_PROVIDER_NAMES */
static bool acf_gemini_native_name(const char *n) {
    return n && (strcmp(n, "gemini") == 0 || strcmp(n, "google") == 0
                 || strcmp(n, "google-gemini") == 0 || strcmp(n, "google-ai-studio") == 0);
}

/* PoP: acf_is_gemini_native_route @ agent/auxiliary_client.py:_is_gemini_native_route */
json_t *acf_is_gemini_native_route(acf_io_t *io, json_t *in) {
    const char *provider_norm = acf_gets(in, "provider_norm", "");
    if (acf_gemini_native_name(provider_norm)) return json_bool(true);
    const char *effective_base = acf_gets(in, "effective_base", "");
    if (!effective_base[0]) return json_bool(false);
    /* try: is_native_gemini_base_url(effective_base) except Exception: False */
    json_t *args = json_object();
    json_set_hdr(args, "base_url", effective_base);
    json_t *r = acf_call(io, "is_native_gemini_base_url", args);
    json_free(args);
    bool hit = !acf_raised(r) && acf_truthy(r);
    json_free(r);
    return json_bool(hit);
}

/* _NVIDIA_PROVIDER_NAMES */
static bool acf_nvidia_provider_name(const char *n) {
    static const char *names[] = { "nvidia", "nvidia-nim", "nim", "build-nvidia", "nemotron" };
    for (size_t i = 0; i < sizeof(names) / sizeof(char *); i++)
        if (n && strcmp(n, names[i]) == 0) return true;
    return false;
}

/* PoP: acf_forwards_max_tokens @ agent/auxiliary_client.py:_forwards_max_tokens */
json_t *acf_forwards_max_tokens(acf_io_t *io, json_t *in) {
    const char *provider = acf_gets(in, "provider", "");
    const char *provider_norm = acf_gets(in, "provider_norm", "");
    const char *effective_base = acf_gets(in, "effective_base", "");
    json_t *task = acf_get(in, "task");
    /* or-chain: evaluation ORDER is the parity contract (seams short-circuit) */
    json_t *a1 = json_object();
    json_set_hdr(a1, "provider", provider);
    json_set_hdr(a1, "base_url", effective_base);
    json_t *r1 = acf_call(io, "_is_anthropic_compat_endpoint", a1);
    json_free(a1);
    if (acf_truthy(r1) && !acf_raised(r1)) { json_free(r1); return json_bool(true); }
    json_free(r1);
    json_t *sub = json_object();
    json_set_hdr(sub, "provider_norm", provider_norm);
    json_set(sub, "model", acf_copy_or_null(acf_get(in, "model")));
    json_t *r2 = acf_nous_on_messages_wire(io, sub);
    json_free(sub);
    if (acf_bool_val(r2)) { json_free(r2); return json_bool(true); }
    json_free(r2);
    if (acf_nvidia_provider_name(provider_norm)) return json_bool(true);
    json_t *a4 = json_object();
    json_set_hdr(a4, "base_url", effective_base);
    json_set_hdr(a4, "domain", "integrate.api.nvidia.com");
    json_t *r4 = acf_call(io, "base_url_host_matches", a4);
    json_free(a4);
    if (acf_truthy(r4) && !acf_raised(r4)) { json_free(r4); return json_bool(true); }
    json_free(r4);
    char *task_s = acf_pystr(task);   /* str(task) == "moa_reference" */
    bool moa = strcmp(task_s, "moa_reference") == 0;
    free(task_s);
    if (moa) return json_bool(true);
    json_t *sub6 = json_object();
    json_set_hdr(sub6, "provider_norm", provider_norm);
    json_set_hdr(sub6, "effective_base", effective_base);
    json_t *r6 = acf_is_gemini_native_route(io, sub6);
    json_free(sub6);
    if (acf_bool_val(r6)) { json_free(r6); return json_bool(true); }
    json_free(r6);
    if (strcmp(provider_norm, "openrouter") == 0) return json_bool(true);
    json_t *a8 = json_object();
    json_set_hdr(a8, "base_url", effective_base);
    json_set_hdr(a8, "domain", "openrouter.ai");
    json_t *r8 = acf_call(io, "base_url_host_matches", a8);
    json_free(a8);
    if (acf_truthy(r8) && !acf_raised(r8)) { json_free(r8); return json_bool(true); }
    json_free(r8);
    json_t *a9 = json_object();
    json_set_hdr(a9, "base_url", effective_base);
    json_t *r9 = acf_call(io, "_is_managed_local_endpoint", a9);
    json_free(a9);
    if (acf_truthy(r9) && !acf_raised(r9)) { json_free(r9); return json_bool(true); }
    json_free(r9);
    json_t *a10 = json_object();
    json_set_hdr(a10, "provider_norm", provider_norm);
    json_t *r10 = acf_call(io, "profile_forwards_max_tokens", a10);
    json_free(a10);
    bool hit = acf_truthy(r10) && !acf_raised(r10);
    json_free(r10);
    return json_bool(hit);
}

/* PoP: acf_stream_request_plan @ agent/auxiliary_client.py:_stream_request_plan */
json_t *acf_stream_request_plan(acf_io_t *io, json_t *in) {
    json_t *kwargs = acf_get(in, "kwargs");
    json_t *client = acf_get(in, "client");
    json_t *stream_kwargs = acf_is_dict(kwargs) ? json_copy(kwargs) : json_object();
    json_set(stream_kwargs, "stream", json_bool(true));
    json_t *opts = json_object();
    json_set(opts, "include_usage", json_bool(true));
    json_set(stream_kwargs, "stream_options", opts);
    char *base = acf_client_base_url(client);
    json_t *a1 = json_object();
    json_set_hdr(a1, "base_url", base);
    free(base);
    json_t *managed = acf_call(io, "_is_managed_local_endpoint", a1);
    json_free(a1);
    if (acf_truthy(managed) && !acf_raised(managed)) {
        json_t *a2 = json_object();
        json_set(a2, "kwargs", stream_kwargs);   /* borrowed; dispatcher copies */
        json_t *pp = acf_call(io, "with_prompt_progress", a2);
        json_free(a2);
        if (pp && pp->type == JSON_OBJECT) {
            json_free(stream_kwargs);
            stream_kwargs = pp;
        } else {
            json_free(pp);
        }
    }
    json_free(managed);
    /* (stream_kwargs, str(kwargs.get("model") or ""), _aux_stream_total_ceiling(kwargs.get("timeout"))) */
    char *model = acf_str_or_empty(acf_get(kwargs, "model"));
    json_t *a3 = json_object();
    json_set(a3, "effective_timeout", acf_copy_or_null(acf_get(kwargs, "timeout")));
    json_t *ceiling = acf_call(io, "_aux_stream_total_ceiling", a3);
    json_free(a3);
    json_t *out = json_array();
    json_append(out, stream_kwargs);
    json_append(out, json_string(model));
    json_append(out, ceiling);
    free(model);
    return out;
}

/* PoP: acf_with_custom_endpoint_extra_body @ agent/auxiliary_client.py:_with_custom_endpoint_extra_body */
json_t *acf_with_custom_endpoint_extra_body(acf_io_t *io, json_t *in) {
    json_t *extra_body = acf_get(in, "extra_body");
    json_t *base_url = acf_get(in, "base_url");
    if (!acf_truthy(base_url)) return acf_copy_or_null(extra_body);
    /* try: load_config_readonly -> get_compatible_custom_providers ->
     *      _custom_provider_extra_body_for_agent; except: return extra_body */
    json_t *cfg = acf_call(io, "load_config_readonly", NULL);
    if (acf_raised(cfg)) { json_free(cfg); return acf_copy_or_null(extra_body); }
    json_t *a2 = json_object();
    json_set(a2, "config", cfg);   /* ownership transferred */
    json_t *providers = acf_call(io, "get_compatible_custom_providers", a2);
    json_free(a2);
    if (acf_raised(providers)) { json_free(providers); return acf_copy_or_null(extra_body); }
    char *ps = acf_str_or_empty(acf_get(in, "provider"));
    char *ms = acf_str_or_empty(acf_get(in, "model"));
    char *bs = acf_pystr(base_url);   /* str(base_url) */
    json_t *a3 = json_object();
    json_set_hdr(a3, "provider", ps);
    json_set_hdr(a3, "model", ms);
    json_set_hdr(a3, "base_url", bs);
    json_set(a3, "custom_providers", providers);   /* ownership transferred */
    free(ps); free(ms); free(bs);
    json_t *inherited = acf_call(io, "_custom_provider_extra_body_for_agent", a3);
    json_free(a3);
    if (acf_raised(inherited)) { json_free(inherited); return acf_copy_or_null(extra_body); }
    if (!acf_truthy(inherited)) { json_free(inherited); return acf_copy_or_null(extra_body); }
    /* {**inherited, **(extra_body or {})} */
    json_t *out = json_copy(inherited);
    json_free(inherited);
    if (acf_is_dict(extra_body)) {
        for (size_t i = 0; i < extra_body->c.count; i++)
            json_set(out, extra_body->c.keys[i], json_copy(extra_body->c.items[i]));
    }
    return out;
}

/* PoP: acf_merge_aux_extra_body @ agent/auxiliary_client.py:_merge_aux_extra_body */
json_t *acf_merge_aux_extra_body(acf_io_t *io, json_t *in) {
    json_t *extra_body = acf_get(in, "extra_body");
    json_t *projection = acf_get(in, "projection");
    json_t *reasoning_config = acf_get(in, "reasoning_config");
    const char *provider_norm = acf_gets(in, "provider_norm", "");
    json_t *merged = acf_is_dict(extra_body) ? json_copy(extra_body) : json_object();
    /* caller_reasoning_fields: PROFILE_REASONING_KEYS minus "reasoning" itself */
    json_t *caller_fields = json_object();
    for (size_t i = 0; i < merged->c.count; i++) {
        const char *k = merged->c.keys[i];
        char *strip = acf_strip(k);
        char *low = acf_lower(strip);
        free(strip);
        if (acf_profile_reasoning_key(k) && strcmp(low, "reasoning") != 0)
            json_set(caller_fields, k, json_copy(merged->c.items[i]));
        free(low);
    }
    /* caller_disabled = isinstance(reasoning_config, dict) and .get("enabled") is False */
    bool caller_disabled = false;
    if (acf_is_dict(reasoning_config)) {
        json_t *en = json_obj_get(reasoning_config, "enabled");
        caller_disabled = en && en->type == JSON_BOOL && !en->bool_val;
    }
    if (caller_disabled) json_obj_del(merged, "reasoning");
    /* merged.update(projection.body); .update(projection.reasoning_extra) */
    json_t *pbody = acf_get(projection, "body");
    if (acf_is_dict(pbody))
        for (size_t i = 0; i < pbody->c.count; i++)
            json_set(merged, pbody->c.keys[i], json_copy(pbody->c.items[i]));
    json_t *prex = acf_get(projection, "reasoning_extra");
    if (acf_is_dict(prex))
        for (size_t i = 0; i < prex->c.count; i++)
            json_set(merged, prex->c.keys[i], json_copy(prex->c.items[i]));
    /* explicit vendor wire controls win over the profile default */
    for (size_t i = 0; i < caller_fields->c.count; i++)
        json_set(merged, caller_fields->c.keys[i], json_copy(caller_fields->c.items[i]));
    json_free(caller_fields);
    if (!json_get_bool(projection, "handles_reasoning", false)) {
        json_t *a1 = json_object();
        json_set(a1, "reasoning_config", acf_copy_or_null(reasoning_config));
        json_t *gen = acf_call(io, "generic_nested_reasoning", a1);
        json_free(a1);
        if (acf_is_dict(gen))
            for (size_t i = 0; i < gen->c.count; i++)
                json_set(merged, gen->c.keys[i], json_copy(gen->c.items[i]));
        json_free(gen);
    }
    /* wire clamp on extra_body.reasoning */
    json_t *mr = json_obj_get(merged, "reasoning");
    if (acf_is_dict(mr)) {
        json_t *a2 = json_object();
        json_set(a2, "reasoning_config", json_copy(mr));
        json_t *clamped = acf_call(io, "clamp_reasoning_config", a2);
        json_free(a2);
        json_set(merged, "reasoning", clamped);
    }
    if (acf_nous_provider_name(provider_norm)) {
        if (!json_has(merged, "tags")) {
            json_t *tags = acf_call(io, "_nous_portal_tags", NULL);
            json_set(merged, "tags", tags);
        }
        if (!json_has(merged, "session_id")) {
            json_t *sticky = acf_call(io, "get_conversation_context", NULL);
            if (acf_raised(sticky)) { json_free(sticky); sticky = json_null(); }
            if (acf_truthy(sticky)) json_set(merged, "session_id", json_copy(sticky));
            json_free(sticky);
        }
    }
    return merged;
}

/* PoP: acf_unwrap_data_envelope @ agent/auxiliary_client.py:_unwrap_data_envelope */
json_t *acf_unwrap_data_envelope(acf_io_t *io, json_t *in) {
    json_t *response = acf_get(in, "response");
    const char *task = acf_gets(in, "task", NULL);
    /* if _field(response, "choices"): return response */
    json_t *choices = acf_get(response, "choices");
    if (acf_truthy(choices)) return acf_copy_or_null(response);
    json_t *data = acf_get(response, "data");
    if (!acf_is_dict(data)) return acf_copy_or_null(response);
    /* if _field(response, "success") is False: raise RuntimeError(...) */
    json_t *success = acf_get(response, "success");
    if (success && success->type == JSON_BOOL && !success->bool_val) {
        json_t *err = acf_get(data, "error");
        if (!acf_truthy(err)) err = data;   /* data.get("error") or data */
        json_t *msg = acf_is_dict(err) ? acf_get(err, "message") : NULL;
        char *err_s = acf_truthy(msg) ? acf_pystr(msg) : acf_pystr(err);
        const char *t = (task && task[0]) ? task : "call";
        size_t n = strlen(t) + strlen(err_s) + 80;
        char *buf = (char *)malloc(n);
        if (buf) snprintf(buf, n, "Auxiliary %s: provider returned error envelope: %s", t, err_s);
        free(err_s);
        json_t *raise = json_object();
        json_set_hdr(raise, "__raise__", "RuntimeError");
        json_set_hdr(raise, "message", buf ? buf : "");
        free(buf);
        return raise;
    }
    if (!acf_truthy(acf_get(data, "choices"))) return acf_copy_or_null(response);
    /* try: type(response).model_validate(data) except (AttributeError,TypeError,ValueError): pass */
    json_t *a1 = json_object();
    json_set(a1, "data", json_copy(data));
    json_t *validated = acf_call(io, "model_validate", a1);
    json_free(a1);
    if (acf_raised(validated)) {
        const char *ty = acf_gets(validated, "__raise__", "");
        bool tolerated = strcmp(ty, "AttributeError") == 0 || strcmp(ty, "TypeError") == 0
                      || strcmp(ty, "ValueError") == 0;
        if (!tolerated) return validated;   /* other exceptions propagate */
        json_free(validated);
    } else if (validated && validated->type != JSON_NULL) {
        return validated;
    } else {
        json_free(validated);
    }
    /* fallback: build SimpleNamespace-shaped completion from data["choices"] */
    json_t *dchoices = acf_get(data, "choices");
    json_t *built = json_array();
    if (dchoices && dchoices->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(dchoices); i++) {
            json_t *ch = json_get(dchoices, i);
            json_t *msg = acf_is_dict(ch) ? acf_get(ch, "message") : NULL;
            if (!acf_is_dict(msg)) continue;
            json_t *choice = json_object();
            /* message=SimpleNamespace(**msg) — an object, not a dict */
            json_t *msg_obj = json_copy(msg);
            json_set(msg_obj, "__obj__", json_bool(true));
            json_set(choice, "message", msg_obj);
            json_t *fr = acf_get(ch, "finish_reason");
            char *frs = acf_truthy(fr) ? acf_pystr(fr) : strdup("stop");
            json_set(choice, "finish_reason", json_string(frs));
            free(frs);
            json_append(built, choice);
        }
    }
    if (json_len(built) == 0) { json_free(built); return acf_copy_or_null(response); }
    json_t *out = json_object();
    json_set(out, "__obj__", json_bool(true));
    json_t *id = acf_get(data, "id");
    json_set(out, "id", id ? json_copy(id) : json_string(""));
    json_t *model = acf_get(data, "model");
    json_set(out, "model", model ? json_copy(model) : json_string(""));
    json_t *object = acf_get(data, "object");
    json_set(out, "object", object ? json_copy(object) : json_string("chat.completion"));
    json_set(out, "choices", built);
    json_set(out, "usage", acf_copy_or_null(acf_get(data, "usage")));
    return out;
}

/* In-place mirror of _note_relay_auxiliary_error on a context dict (the
 * contextvar's value). Used by the public wrapper and by _classify_failure. */
static void acf_note_relay_mutate(json_t *out_ctx, json_t *exc, json_t *failure) {
    /* if not context or not isinstance(exc, Exception): return */
    bool is_exception = exc && exc->type == JSON_OBJECT && !json_get_bool(exc, "base_only", false);
    if (!acf_truthy(out_ctx) || !is_exception) return;
    json_t *noted = acf_get(out_ctx, "attempt");
    json_t *count = acf_get(out_ctx, "attempt_count");
    /* failure is None and noted is not None and noted.error is exc and noted.attempt == count */
    bool same_exc = false;
    if (noted && noted->type == JSON_OBJECT) {
        json_t *ne = acf_get(noted, "error");
        const char *nid = acf_gets(ne, "exc_id", NULL);
        const char *eid = acf_gets(exc, "exc_id", NULL);
        same_exc = nid && eid && strcmp(nid, eid) == 0;
    }
    bool same_attempt = false;
    if (noted && noted->type == JSON_OBJECT) {
        json_t *na = acf_get(noted, "attempt");
        if (na && count && na->type == count->type) {
            if (na->type == JSON_NUMBER) same_attempt = na->num_val == count->num_val;
            else if (na->type == JSON_STRING) same_attempt = strcmp(na->str_val, count->str_val) == 0;
            else if (na->type == JSON_NULL && count->type == JSON_NULL) same_attempt = true;
        }
    }
    if ((!failure || failure->type == JSON_NULL) && noted && noted->type != JSON_NULL
        && same_exc && same_attempt) {
        return;
    }
    /* context["attempt"] = _RelayAttempt(exc, context["provider"], context["model"], failure, count) */
    json_t *attempt = json_object();
    json_set(attempt, "error", json_copy(exc));
    json_set(attempt, "provider", acf_copy_or_null(acf_get(out_ctx, "provider")));
    json_set(attempt, "model", acf_copy_or_null(acf_get(out_ctx, "model")));
    json_set(attempt, "failure", acf_copy_or_null(failure));
    json_set(attempt, "attempt", acf_copy_or_null(count));
    json_set(out_ctx, "attempt", attempt);
}

/* PoP: acf_note_relay_auxiliary_error @ agent/auxiliary_client.py:_note_relay_auxiliary_error */
json_t *acf_note_relay_auxiliary_error(acf_io_t *io, json_t *in) {
    (void)io;
    /* context = _RELAY_AUX_CALL_CONTEXT.get() — injected state */
    json_t *context = acf_get(in, "context");
    json_t *exc = acf_get(in, "exc");
    json_t *failure = acf_get(in, "failure");
    json_t *out_ctx = acf_copy_or_null(context);
    acf_note_relay_mutate(out_ctx, exc, failure);
    json_t *out = json_object();
    json_set(out, "context", out_ctx);
    return out;
}

/* _TIMEOUT_NO_RETRY_TASKS = frozenset({"compression","vision","title_generation"}) */
static bool acf_timeout_no_retry_task(const char *t) {
    return t && (strcmp(t, "compression") == 0 || strcmp(t, "vision") == 0
                 || strcmp(t, "title_generation") == 0);
}

/* PoP: acf_should_skip_same_provider_retry @ agent/auxiliary_client.py:_should_skip_same_provider_retry */
json_t *acf_should_skip_same_provider_retry(acf_io_t *io, json_t *in) {
    (void)io;
    const char *task = acf_gets(in, "task", NULL);
    json_t *failure = acf_get(in, "failure");
    json_t *verdict = acf_get(failure, "verdict");
    json_t *ctx = acf_get(verdict, "error_context");
    const char *transport = acf_gets(ctx, "transport", NULL);
    json_t *error = acf_get(failure, "error");
    const char *err_str = acf_gets(error, "message", "");
    bool skip = acf_timeout_no_retry_task(task)
             && transport && strcmp(transport, "timeout") == 0
             && !strstr(err_str, "no-progress timeout");
    return json_bool(skip);
}

/* PoP: acf_should_retry_same_provider @ agent/auxiliary_client.py:_should_retry_same_provider */
json_t *acf_should_retry_same_provider(acf_io_t *io, json_t *in) {
    json_t *failure = acf_get(in, "failure");
    json_t *verdict = acf_get(failure, "verdict");
    json_t *args = json_object();
    json_set(args, "verdict", acf_copy_or_null(verdict));
    json_t *r = acf_call(io, "same_provider_retry", args);
    json_free(args);
    bool worth = acf_truthy(r) && !acf_raised(r);
    json_free(r);
    if (!worth) return json_bool(false);
    json_t *skip = acf_should_skip_same_provider_retry(io, in);
    bool skips = acf_bool_val(skip);
    json_free(skip);
    if (skips) return json_bool(false);   /* logger.info silent on both sides */
    return json_bool(true);
}

/* ── outcome wrappers ──────────────────────────────────────────────────── */

/* PoP: acf_outcome @ agent/auxiliary_client.py:_outcome */
json_t *acf_outcome(acf_io_t *io, json_t *in) {
    /* (result, None) or (None, error): the thunk seam stands in for call(*args) */
    const char *thunk = acf_gets(in, "thunk", "");
    json_t *r = acf_call(io, thunk, acf_get(in, "args"));
    json_t *out = json_object();
    if (acf_raised(r)) {
        json_set(out, "result", json_null());
        json_set(out, "error", r);
    } else {
        json_set(out, "result", r);
        json_set(out, "error", json_null());
    }
    return out;
}

/* PoP: acf_aoutcome @ agent/auxiliary_client.py:_aoutcome */
json_t *acf_aoutcome(acf_io_t *io, json_t *in) {
    /* async twin: await awaitable -> (result, None) / (None, exc). The awaitable
     * seam runs the same scripted thunk; sync/async differ only in the driver. */
    const char *thunk = acf_gets(in, "thunk", "");
    json_t *r = acf_call(io, thunk, acf_get(in, "args"));
    json_t *out = json_object();
    if (acf_raised(r)) {
        json_set(out, "result", json_null());
        json_set(out, "error", r);
    } else {
        json_set(out, "result", r);
        json_set(out, "error", json_null());
    }
    return out;
}
/* ── recovery ladder: rungs, classification, decision core ─────────────── */

/* resolve a client id through the registry (borrowed; NULL when unknown) */
static json_t *acf_client_obj(acf_io_t *io, const json_t *id) {
    if (!id || id->type != JSON_STRING || !io || !io->clients) return NULL;
    return json_obj_get(io->clients, id->str_val);
}

/* _parameter_rungs table: (matcher, strip, message, remember).
 * Matcher/strip ids project to the names the oracle uses for the live
 * closures (identity-matched there). */
typedef enum {
    ACF_M_TEMPERATURE = 0, ACF_M_STRUCTURED, ACF_M_REASONING_REQUIRED,
    ACF_M_REASONING_FIELD, ACF_M_MAX_TOKENS
} acf_matcher_t;
typedef enum {
    ACF_S_TEMPERATURE = 0, ACF_S_STRUCTURED, ACF_S_REASONING_FLOOR,
    ACF_S_REASONING_FIELDS, ACF_S_MAX_TOKENS
} acf_strip_t;

static const char *acf_matcher_name(int m) {
    switch (m) {
    case ACF_M_TEMPERATURE: return "temperature";
    case ACF_M_STRUCTURED: return "structured_output";
    case ACF_M_REASONING_REQUIRED: return "reasoning_required";
    case ACF_M_REASONING_FIELD: return "reasoning_field";
    default: return "max_tokens";
    }
}
static const char *acf_strip_name(int s) {
    switch (s) {
    case ACF_S_TEMPERATURE: return "without_temperature";
    case ACF_S_STRUCTURED: return "without_structured_output_format";
    case ACF_S_REASONING_FLOOR: return "with_reasoning_floor";
    case ACF_S_REASONING_FIELDS: return "without_reasoning_fields";
    default: return "without_max_tokens";
    }
}

typedef struct {
    int matcher;
    int strip;
    const char *message;
    const char *remember;   /* seam name or NULL */
} acf_rung_t;

/* PoP: acf_parameter_rungs @ agent/auxiliary_client.py:_parameter_rungs */
json_t *acf_parameter_rungs(acf_io_t *io, json_t *in) {
    (void)io; (void)in;   /* table shape only; detectors run at match time */
    static const acf_rung_t rungs[] = {
        { ACF_M_TEMPERATURE, ACF_S_TEMPERATURE,
          "provider rejected temperature; retrying without it",
          "remember_temperature_rejection" },
        { ACF_M_STRUCTURED, ACF_S_STRUCTURED,
          "provider rejected the structured-output format field; retrying without it "
          "(schema enforcement degrades to prompt compliance)",
          "remember_structured_output_rejection" },
        { ACF_M_REASONING_REQUIRED, ACF_S_REASONING_FLOOR,
          "provider requires reasoning; retrying at the floor effort",
          "remember_reasoning_floor" },
        { ACF_M_REASONING_FIELD, ACF_S_REASONING_FIELDS,
          "provider rejected the reasoning field; retrying without it (route default applies)",
          NULL },
        { ACF_M_MAX_TOKENS, ACF_S_MAX_TOKENS,
          "provider rejected the output cap; retrying without it",
          NULL },
    };
    json_t *out = json_array();
    for (size_t i = 0; i < sizeof(rungs) / sizeof(rungs[0]); i++) {
        json_t *row = json_array();
        json_append(row, json_string(acf_matcher_name(rungs[i].matcher)));
        json_append(row, json_string(acf_strip_name(rungs[i].strip)));
        json_append(row, json_string(rungs[i].message));
        json_append(row, rungs[i].remember ? json_string(rungs[i].remember) : json_null());
        json_append(out, row);
    }
    return out;
}

static const acf_rung_t ACF_RUNGS[] = {
    { ACF_M_TEMPERATURE, ACF_S_TEMPERATURE,
      "provider rejected temperature; retrying without it",
      "remember_temperature_rejection" },
    { ACF_M_STRUCTURED, ACF_S_STRUCTURED,
      "provider rejected the structured-output format field; retrying without it "
      "(schema enforcement degrades to prompt compliance)",
      "remember_structured_output_rejection" },
    { ACF_M_REASONING_REQUIRED, ACF_S_REASONING_FLOOR,
      "provider requires reasoning; retrying at the floor effort",
      "remember_reasoning_floor" },
    { ACF_M_REASONING_FIELD, ACF_S_REASONING_FIELDS,
      "provider rejected the reasoning field; retrying without it (route default applies)",
      NULL },
    { ACF_M_MAX_TOKENS, ACF_S_MAX_TOKENS,
      "provider rejected the output cap; retrying without it",
      NULL },
};
#define ACF_RUNG_COUNT (sizeof(ACF_RUNGS) / sizeof(ACF_RUNGS[0]))

/* rung[0](exc): detector dispatch (seam call order matches the live table) */
static bool acf_matcher_eval(acf_io_t *io, int matcher, json_t *exc,
                             json_t *client_obj, json_t *max_tokens) {
    json_t *sub = json_object();
    json_set(sub, "exc", acf_copy_or_null(exc));
    json_t *r = NULL;
    bool hit = false;
    switch (matcher) {
    case ACF_M_TEMPERATURE: {
        json_t *args = json_object();
        json_set(args, "exc", acf_copy_or_null(exc));
        json_set_hdr(args, "param", "temperature");
        r = acf_call(io, "_is_unsupported_parameter_error", args);
        json_free(args);
        hit = acf_truthy(r) && !acf_raised(r);
        break;
    }
    case ACF_M_STRUCTURED:
        r = acf_is_structured_output_rejection(io, sub);
        hit = acf_bool_val(r);
        break;
    case ACF_M_REASONING_REQUIRED:
        r = acf_is_reasoning_required_rejection(io, sub);
        hit = acf_bool_val(r);
        break;
    case ACF_M_REASONING_FIELD:
        r = acf_is_reasoning_field_rejection(io, sub);
        hit = acf_bool_val(r);
        break;
    default: {
        /* max_tokens is not None and _is_max_tokens_rejection(exc, client) */
        bool active = max_tokens && max_tokens->type != JSON_NULL;
        if (active) {
            json_set(sub, "client", acf_copy_or_null(client_obj));
            r = acf_is_max_tokens_rejection(io, sub);
            hit = acf_bool_val(r);
        }
        break;
    }
    }
    json_free(r);
    json_free(sub);
    return hit;
}

/* rung[1](kwargs): strip dispatch. Pure strips compute directly; external
 * strips (_without_structured_output_format, with_reasoning_floor) are seams.
 * Returns NEW dict or NULL (field not on the wire). */
static json_t *acf_strip_apply(acf_io_t *io, int strip, json_t *kwargs) {
    json_t *sub = json_object();
    json_set(sub, "kwargs", acf_copy_or_null(kwargs));
    json_t *r = NULL;
    switch (strip) {
    case ACF_S_TEMPERATURE:
        r = acf_without_temperature(io, sub);
        break;
    case ACF_S_STRUCTURED:
        r = acf_call(io, "_without_structured_output_format", sub);
        break;
    case ACF_S_REASONING_FLOOR:
        r = acf_call(io, "with_reasoning_floor", sub);
        break;
    case ACF_S_REASONING_FIELDS:
        r = acf_without_reasoning_fields(io, sub);
        break;
    default:
        r = acf_without_max_tokens(io, sub);
        break;
    }
    json_free(sub);
    if (r && r->type == JSON_NULL) { json_free(r); return NULL; }
    if (acf_raised(r)) { json_free(r); return NULL; }
    return r;
}

/* PoP: acf_classify_failure @ agent/auxiliary_client.py:_classify_failure */
json_t *acf_classify_failure(acf_io_t *io, json_t *in) {
    json_t *exc = acf_get(in, "exc");
    /* isinstance(exc, _ClassifiedAttempt): recovery reads its failure */
    json_t *classified = acf_get(exc, "classified");
    if (classified && classified->type != JSON_NULL) return json_copy(classified);
    json_t *client_id = acf_get(in, "client");
    json_t *client_obj = acf_client_obj(io, client_id);
    /* provider = _classification_provider(client, provider or "", main_runtime) */
    json_t *provider = acf_get(in, "provider");
    json_t *cp_in = json_object();
    json_set(cp_in, "client", acf_copy_or_null(client_obj));
    json_set(cp_in, "fallback", acf_truthy(provider) ? json_copy(provider) : json_string(""));
    json_set(cp_in, "main_runtime", acf_copy_or_null(acf_get(in, "main_runtime")));
    json_t *cprovider = acf_classification_provider(io, cp_in);
    json_free(cp_in);
    /* verdict = aux_errors.classify_attempt(exc, provider, client, model, base_url) */
    json_t *args = json_object();
    json_set(args, "exc", acf_copy_or_null(exc));
    json_set(args, "provider", cprovider);   /* ownership transferred */
    json_set(args, "client", acf_copy_or_null(client_id));
    json_set(args, "model", acf_copy_or_null(acf_get(in, "model")));
    json_set(args, "base_url", acf_copy_or_null(acf_get(in, "base_url")));
    json_t *verdict = acf_call(io, "classify_attempt", args);
    json_free(args);
    /* any(rung[0](exc) for rung in _parameter_rungs(client, 0)) — 0 is not None */
    json_t *zero = json_number(0);
    bool rejects = false;
    for (size_t i = 0; i < ACF_RUNG_COUNT && !rejects; i++)
        rejects = acf_matcher_eval(io, ACF_RUNGS[i].matcher, exc, client_obj, zero);
    json_free(zero);
    json_t *failure = json_object();
    json_set(failure, "__failure__", json_bool(true));
    json_set(failure, "error", acf_copy_or_null(exc));
    json_set(failure, "verdict", verdict);
    json_set(failure, "rejects_parameter", json_bool(rejects));
    json_set(failure, "client", acf_copy_or_null(client_id));
    /* _note_relay_auxiliary_error(exc, failure) on the contextvar mirror */
    if (io && io->relay_context) acf_note_relay_mutate(io->relay_context, exc, failure);
    return failure;
}

/* route.base_info or route.resolved_base_url (Python `or` on strings) */
static json_t *acf_route_base(acf_io_t *io, json_t *route) {
    (void)io;
    json_t *bi = acf_get(route, "base_info");
    if (acf_truthy(bi)) return json_copy(bi);
    return acf_copy_or_null(acf_get(route, "resolved_base_url"));
}

/* PoP: acf_failure @ agent/auxiliary_client.py:_failure */
json_t *acf_failure(acf_io_t *io, json_t *in) {
    json_t *route = acf_get(in, "route");
    json_t *exc = acf_get(in, "exc");
    json_t *step = acf_get(in, "step");
    /* isinstance(exc, aux_errors.Failure): pass through */
    if (exc && json_get_bool(exc, "__failure__", false)) return json_copy(exc);
    json_t *client = acf_get(route, "client");
    json_t *model = acf_get(route, "final_model");
    json_t *client_use = acf_copy_or_null(client);
    json_t *model_use = acf_copy_or_null(model);
    if (step && strcmp(acf_gets(step, "kind", ""), "call") == 0) {
        json_t *args = acf_get(step, "args");
        if (args && args->type == JSON_ARRAY && json_len(args) >= 2) {
            json_free(client_use);
            client_use = json_copy(json_get(args, 0));
            json_t *kw = json_get(args, 1);
            json_t *m = acf_get(kw, "model");
            if (acf_truthy(m)) { json_free(model_use); model_use = json_copy(m); }
        }
    }
    json_t *sub = json_object();
    json_set(sub, "exc", acf_copy_or_null(exc));
    json_set(sub, "provider", acf_copy_or_null(acf_get(route, "resolved_provider")));
    json_set(sub, "client", client_use);
    json_set(sub, "model", model_use);
    json_set(sub, "base_url", acf_route_base(io, route));
    json_set(sub, "main_runtime", acf_copy_or_null(acf_get(route, "main_runtime")));
    json_t *out = acf_classify_failure(io, sub);
    json_free(sub);
    return out;
}

/* accept callbacks: "param" (_param_rung_accepts), "always" (lambda _f: True),
 * "ladder_spent"/"ladder_unspent" (partial(aux_errors.ladder_accepts, ...)). */
static bool acf_accept_eval(acf_io_t *io, const char *accept, json_t *failure) {
    if (strcmp(accept, "always") == 0) return true;
    if (strcmp(accept, "param") == 0) {
        json_t *sub = json_object();
        json_set(sub, "failure", acf_copy_or_null(failure));
        json_t *r = acf_param_rung_accepts(io, sub);
        json_free(sub);
        bool yes = acf_bool_val(r);
        json_free(r);
        return yes;
    }
    json_t *args = json_object();
    json_set(args, "failure", acf_copy_or_null(failure));
    json_set(args, "rung_spent", json_bool(strcmp(accept, "ladder_spent") == 0));
    json_t *r = acf_call(io, "ladder_accepts", args);
    json_free(args);
    bool yes = acf_truthy(r) && !acf_raised(r);
    json_free(r);
    return yes;
}

/* PoP: acf_rung @ agent/auxiliary_client.py:_rung */
json_t *acf_rung(acf_io_t *io, json_t *in) {
    json_t *step = acf_get(in, "step");
    const char *accept = acf_gets(in, "accept", "always");
    json_t *route = acf_get(in, "route");
    /* result = yield step — the perform seam stands in for the driver */
    json_t *result = acf_call(io, "perform", step);
    if (!acf_raised(result)) {
        json_t *out = json_object();
        json_set(out, "result", result);
        json_set(out, "failure", json_null());
        return out;
    }
    /* except Exception as exc: failure = _failure(route, exc, step) */
    json_t *sub = json_object();
    json_set(sub, "route", acf_copy_or_null(route));
    json_set(sub, "exc", json_copy(result));
    json_set(sub, "step", acf_copy_or_null(step));
    json_t *failure = acf_failure(io, sub);
    json_free(sub);
    if (acf_accept_eval(io, accept, failure)) {
        json_free(result);
        json_t *out = json_object();
        json_set(out, "result", json_null());
        json_set(out, "failure", failure);
        return out;
    }
    /* not isinstance(exc, _ClassifiedAttempt): re-raise the original */
    json_t *classified = acf_get(result, "classified");
    if (!classified || classified->type == JSON_NULL) {
        json_free(failure);
        return result;   /* raise descriptor propagates */
    }
    /* raise failure.error — the provider's own error, no wrapper context */
    json_t *inner = acf_copy_or_null(acf_get(failure, "error"));
    json_free(failure);
    json_free(result);
    return inner;
}

/* helper: read a _rung outcome; returns true when it re-raised (out param
 * *raise_out owns the descriptor), false when (result, failure) filled. */
typedef struct {
    json_t *result;    /* owned, may be NULL */
    json_t *failure;   /* owned, may be NULL */
    json_t *raised;    /* owned, non-NULL when the rung re-raised */
} acf_rung_out;

static acf_rung_out acf_rung_run(acf_io_t *io, json_t *step, const char *accept, json_t *route) {
    json_t *sub = json_object();
    json_set(sub, "step", step);   /* ownership transferred */
    json_set_hdr(sub, "accept", accept);
    json_set(sub, "route", acf_copy_or_null(route));
    json_t *r = acf_rung(io, sub);
    json_free(sub);
    acf_rung_out ro = { NULL, NULL, NULL };
    if (acf_raised(r)) { ro.raised = r; return ro; }
    ro.result = acf_copy_or_null(acf_get(r, "result"));
    ro.failure = acf_copy_or_null(acf_get(r, "failure"));
    json_free(r);
    return ro;
}

/* PoP: acf_ladder_parameter_rungs @ agent/auxiliary_client.py:_ladder_parameter_rungs */
json_t *acf_ladder_parameter_rungs(acf_io_t *io, json_t *in) {
    json_t *route = acf_get(in, "route");
    json_t *first_err = acf_get(in, "first_err");
    json_t *kwargs_in = acf_get(in, "kwargs");
    json_t *max_tokens = acf_get(in, "max_tokens");
    json_t *client_id = acf_get(route, "client");
    json_t *client_obj = acf_client_obj(io, client_id);
    /* normalize an exc to a Failure exactly as _aux_recovery_ladder does */
    if (!json_get_bool(first_err, "__failure__", false)) {
        json_t *sub = json_object();
        json_set(sub, "route", acf_copy_or_null(route));
        json_set(sub, "exc", acf_copy_or_null(first_err));
        json_t *f = acf_failure(io, sub);
        json_free(sub);
        first_err = f;   /* owned local */
    } else {
        first_err = json_copy(first_err);
    }
    json_t *kwargs = acf_is_dict(kwargs_in) ? json_copy(kwargs_in) : json_object();
    bool removed[ACF_RUNG_COUNT] = { false };
    json_t *repairs = json_array();
    json_t *response = NULL;
    for (;;) {
        /* while rungs and not aux_errors.resend_cannot_help(first_err.verdict) */
        size_t remaining = 0;
        for (size_t i = 0; i < ACF_RUNG_COUNT; i++) if (!removed[i]) remaining++;
        if (remaining == 0) break;
        json_t *va = json_object();
        json_set(va, "verdict", acf_copy_or_null(acf_get(first_err, "verdict")));
        json_t *nhelp = acf_call(io, "resend_cannot_help", va);
        json_free(va);
        bool cannot = acf_truthy(nhelp) && !acf_raised(nhelp);
        json_free(nhelp);
        if (cannot) break;
        /* hit = next(rung for rung in rungs if matcher(exc) and strip(kwargs) is not None) */
        long hit = -1;
        json_t *probe = NULL;
        json_t *err_exc = acf_get(first_err, "error");
        for (size_t i = 0; i < ACF_RUNG_COUNT; i++) {
            if (removed[i]) continue;
            if (!acf_matcher_eval(io, ACF_RUNGS[i].matcher, err_exc, client_obj, max_tokens))
                continue;
            probe = acf_strip_apply(io, ACF_RUNGS[i].strip, kwargs);
            if (probe) { hit = (long)i; break; }
        }
        if (hit < 0) break;
        removed[hit] = true;
        /* retry_kwargs = strip(kwargs) — the strip runs a SECOND time (live
         * evaluates it once in the search and once for the retry). */
        json_free(probe);
        json_t *retry_kwargs = acf_strip_apply(io, ACF_RUNGS[hit].strip, kwargs);
        json_t *rejection = acf_copy_or_null(err_exc);
        /* resp, first_err = yield from _rung(call step, _param_rung_accepts, route) */
        json_t *step = json_object();
        json_set_hdr(step, "kind", "call");
        json_t *sargs = json_array();
        json_append(sargs, acf_copy_or_null(client_id));
        json_append(sargs, json_copy(retry_kwargs));
        json_set(step, "args", sargs);
        acf_rung_out ro = acf_rung_run(io, step, "param", route);
        if (ro.raised) {
            json_free(rejection);
            json_free(retry_kwargs);
            json_free(kwargs);
            json_free(repairs);
            json_free(first_err);
            json_free(response);
            return ro.raised;   /* unclaimed failure re-raised out of the ladder */
        }
        if (!ro.failure || ro.failure->type == JSON_NULL) {
            /* success: remember(route.resolved_provider, base_info or resolved_base_url,
             *          kwargs [pre-strip], rejection) */
            if (ACF_RUNGS[hit].remember) {
                json_t *ra = json_object();
                json_set(ra, "provider", acf_copy_or_null(acf_get(route, "resolved_provider")));
                json_set(ra, "base_url", acf_route_base(io, route));
                json_set(ra, "rejected_kwargs", json_copy(kwargs));
                json_set(ra, "error", rejection);   /* ownership transferred */
                rejection = NULL;
                json_t *rr = acf_call(io, ACF_RUNGS[hit].remember, ra);
                json_free(ra);
                json_free(rr);
            }
            json_append(repairs, json_string(acf_strip_name(ACF_RUNGS[hit].strip)));
            json_t *out = json_object();
            json_set(out, "response", ro.result ? ro.result : json_null());
            json_set(out, "failure", json_null());
            json_set(out, "kwargs", retry_kwargs);
            json_set(out, "repairs", repairs);
            json_free(rejection);
            json_free(kwargs);
            json_free(first_err);
            json_free(response);
            json_free(ro.failure);
            return out;
        }
        /* failed retry: narrow first_err, carry the stripped kwargs + repair */
        json_free(first_err);
        first_err = ro.failure;
        ro.failure = NULL;
        json_free(kwargs);
        kwargs = retry_kwargs;
        retry_kwargs = NULL;
        json_append(repairs, json_string(acf_strip_name(ACF_RUNGS[hit].strip)));
        json_free(rejection);
        json_free(ro.result);
    }
    json_t *out = json_object();
    json_set(out, "response", json_null());
    json_set(out, "failure", first_err);
    json_set(out, "kwargs", kwargs);
    json_set(out, "repairs", repairs);
    json_free(response);
    return out;
}

/* PoP: acf_refreshed_nous_step @ agent/auxiliary_client.py:_refreshed_nous_step */
json_t *acf_refreshed_nous_step(acf_io_t *io, json_t *in) {
    json_t *route = acf_get(in, "route");
    json_t *kwargs_in = acf_get(in, "kwargs");
    json_t *kwargs = acf_is_dict(kwargs_in) ? json_copy(kwargs_in) : json_object();
    const char *task = acf_gets(route, "task", NULL);
    /* _refresh_nous_auxiliary_client(cache_provider=..., model=..., ...) */
    json_t *args = json_object();
    json_t *rp = acf_get(route, "resolved_provider");
    json_set(args, "cache_provider", acf_truthy(rp) ? json_copy(rp) : json_string("nous"));
    json_set(args, "model", acf_copy_or_null(acf_get(route, "final_model")));
    json_set(args, "lookup_model", acf_copy_or_null(acf_get(route, "resolved_model")));
    json_set(args, "lookup_task", acf_copy_or_null(acf_get(route, "task")));
    json_set(args, "async_mode", json_bool(json_get_bool(route, "async_mode", false)));
    json_set(args, "base_url", acf_copy_or_null(acf_get(route, "resolved_base_url")));
    json_set(args, "api_key", acf_copy_or_null(acf_get(route, "resolved_api_key")));
    json_set(args, "api_mode", acf_copy_or_null(acf_get(route, "resolved_api_mode")));
    json_set(args, "main_runtime", acf_copy_or_null(acf_get(route, "main_runtime")));
    json_set(args, "is_vision", json_bool(task && strcmp(task, "vision") == 0));
    json_t *refreshed = acf_call(io, "_refresh_nous_auxiliary_client", args);
    json_free(args);
    json_t *out = json_object();
    json_t *rclient = acf_get(refreshed, "client");
    if (!rclient || rclient->type == JSON_NULL) {
        json_free(refreshed);
        json_set(out, "step", json_null());
        json_set(out, "kwargs", kwargs);
        return out;
    }
    /* logger.info(message, ...) silent on both sides */
    json_t *rmodel = acf_get(refreshed, "model");
    json_t *kmodel = acf_get(kwargs, "model");
    bool differs = acf_truthy(rmodel) &&
        !(kmodel && kmodel->type == JSON_STRING && rmodel->type == JSON_STRING
          && strcmp(kmodel->str_val, rmodel->str_val) == 0);
    if (differs) json_set(kwargs, "model", json_copy(rmodel));
    json_t *step = json_object();
    json_set_hdr(step, "kind", "call");
    json_t *sargs = json_array();
    json_append(sargs, json_copy(rclient));
    json_append(sargs, json_copy(kwargs));
    json_set(step, "args", sargs);
    json_set(out, "step", step);
    json_set(out, "kwargs", kwargs);
    json_free(refreshed);
    return out;
}

/* PoP: acf_ladder_nous_rungs @ agent/auxiliary_client.py:_ladder_nous_rungs */
json_t *acf_ladder_nous_rungs(acf_io_t *io, json_t *in) {
    json_t *route = acf_get(in, "route");
    json_t *first_err = json_copy(acf_get(in, "first_err"));
    json_t *kwargs_in = acf_get(in, "kwargs");
    json_t *kwargs = acf_is_dict(kwargs_in) ? json_copy(kwargs_in) : json_object();
    bool client_is_nous = json_get_bool(in, "client_is_nous", false);
    json_t *repairs_in = acf_get(in, "repairs");
    bool spent = repairs_in && repairs_in->type == JSON_ARRAY && json_len(repairs_in) > 0;
    const char *accept = spent ? "ladder_spent" : "ladder_unspent";
    const char *task = acf_gets(route, "task", NULL);
    json_t *verdict = acf_get(first_err, "verdict");
    const char *reason = acf_gets(verdict, "reason", NULL);
    json_t *client_id = acf_get(route, "client");
    /* stale-model self-heal: reason is FailoverReason.model_not_found and nous */
    if (reason && strcmp(reason, "model_not_found") == 0 && client_is_nous) {
        json_t *args = json_object();
        json_set(args, "vision", json_bool(task && strcmp(task, "vision") == 0));
        json_set(args, "stale_model", acf_copy_or_null(acf_get(kwargs, "model")));
        json_t *healed = acf_call(io, "_refresh_nous_recommended_model", args);
        json_free(args);
        json_t *kmodel = acf_get(kwargs, "model");
        bool differs = acf_truthy(healed) &&
            !(kmodel && kmodel->type == JSON_STRING && healed->type == JSON_STRING
              && strcmp(kmodel->str_val, healed->str_val) == 0);
        if (differs) {
            json_set(kwargs, "model", json_copy(healed));
            json_t *step = json_object();
            json_set_hdr(step, "kind", "call");
            json_t *sargs = json_array();
            json_append(sargs, acf_copy_or_null(client_id));
            json_append(sargs, json_copy(kwargs));
            json_set(step, "args", sargs);
            acf_rung_out ro = acf_rung_run(io, step, "always", route);
            if (ro.raised) {
                json_free(healed); json_free(first_err); json_free(kwargs);
                return ro.raised;
            }
            if (!ro.failure || ro.failure->type == JSON_NULL) {
                json_t *out = json_object();
                json_set(out, "response", ro.result ? ro.result : json_null());
                json_set(out, "failure", json_null());
                json_set(out, "kwargs", kwargs);
                json_free(ro.failure); json_free(healed); json_free(first_err);
                return out;
            }
            json_free(first_err);
            first_err = ro.failure;
            json_free(ro.result);
        }
        json_free(healed);
    }
    /* paid-account refresh: benches_provider(verdict) and nous and fresh paid.
     * The benches seam runs BEFORE the client_is_nous test (and-order). */
    json_t *ba = json_object();
    json_set(ba, "verdict", acf_copy_or_null(acf_get(first_err, "verdict")));
    json_t *benches = acf_call(io, "benches_provider", ba);
    json_free(ba);
    bool benches_yes = acf_truthy(benches) && !acf_raised(benches);
    json_free(benches);
    if (benches_yes && client_is_nous) {
        json_t *paid = acf_call(io, "_nous_portal_account_has_fresh_paid_access", NULL);
        bool paid_yes = acf_truthy(paid) && !acf_raised(paid);
        json_free(paid);
        if (paid_yes) {
            json_t *sub = json_object();
            json_set(sub, "route", acf_copy_or_null(route));
            json_set(sub, "kwargs", json_copy(kwargs));
            json_set_hdr(sub, "message",
                "Auxiliary %s%s: refreshed Nous runtime credentials after paid account check, retrying");
            json_t *rs = acf_refreshed_nous_step(io, sub);
            json_free(sub);
            json_free(kwargs);
            kwargs = acf_copy_or_null(acf_get(rs, "kwargs"));
            json_t *step = acf_copy_or_null(acf_get(rs, "step"));
            json_free(rs);
            if (step && step->type != JSON_NULL) {
                acf_rung_out ro = acf_rung_run(io, step, accept, route);
                if (ro.raised) { json_free(first_err); json_free(kwargs); return ro.raised; }
                if (!ro.failure || ro.failure->type == JSON_NULL) {
                    json_t *out = json_object();
                    json_set(out, "response", ro.result ? ro.result : json_null());
                    json_set(out, "failure", json_null());
                    json_set(out, "kwargs", kwargs);
                    json_free(ro.failure); json_free(first_err);
                    return out;
                }
                json_free(first_err);
                first_err = ro.failure;
                json_free(ro.result);
            }
        }
    }
    /* 401 refresh: auth_refresh(verdict) and nous */
    json_t *aa = json_object();
    json_set(aa, "verdict", acf_copy_or_null(acf_get(first_err, "verdict")));
    json_t *auth = acf_call(io, "auth_refresh", aa);
    json_free(aa);
    bool auth_yes = acf_truthy(auth) && !acf_raised(auth);
    json_free(auth);
    if (auth_yes && client_is_nous) {
        json_t *sub = json_object();
        json_set(sub, "route", acf_copy_or_null(route));
        json_set(sub, "kwargs", json_copy(kwargs));
        json_set_hdr(sub, "message",
            "Auxiliary %s%s: refreshed Nous runtime credentials after 401, retrying");
        json_t *rs = acf_refreshed_nous_step(io, sub);
        json_free(sub);
        json_free(kwargs);
        kwargs = acf_copy_or_null(acf_get(rs, "kwargs"));
        json_t *step = acf_copy_or_null(acf_get(rs, "step"));
        json_free(rs);
        if (step && step->type != JSON_NULL) {
            acf_rung_out ro = acf_rung_run(io, step, accept, route);
            if (ro.raised) { json_free(first_err); json_free(kwargs); return ro.raised; }
            if (!ro.failure || ro.failure->type == JSON_NULL) {
                json_t *out = json_object();
                json_set(out, "response", ro.result ? ro.result : json_null());
                json_set(out, "failure", json_null());
                json_set(out, "kwargs", kwargs);
                json_free(ro.failure); json_free(first_err);
                return out;
            }
            json_free(first_err);
            first_err = ro.failure;
            json_free(ro.result);
        }
    }
    json_t *out = json_object();
    json_set(out, "response", json_null());
    json_set(out, "failure", first_err);
    json_set(out, "kwargs", kwargs);
    return out;
}

/* PoP: acf_failure_pool @ agent/auxiliary_client.py:_failure_pool */
json_t *acf_failure_pool(acf_io_t *io, json_t *in) {
    json_t *route = acf_get(in, "route");
    json_t *failure = acf_get(in, "failure");
    json_t *route_client = acf_get(route, "client");
    /* pool_provider = _recoverable_pool_provider(resolved_provider, route.client,
     *                                            main_runtime=route.main_runtime) */
    json_t *a1 = json_object();
    json_set(a1, "resolved_provider", acf_copy_or_null(acf_get(route, "resolved_provider")));
    json_set(a1, "client", acf_copy_or_null(route_client));
    json_set(a1, "main_runtime", acf_copy_or_null(acf_get(route, "main_runtime")));
    json_t *pool = acf_call(io, "_recoverable_pool_provider", a1);
    json_free(a1);
    /* sender = _failure_sender(route, failure) */
    json_t *fc = acf_get(failure, "client");
    json_t *sender = (!fc || fc->type == JSON_NULL) ? route_client : fc;
    bool sender_is_route = acf_same_client(sender, route_client);
    if (!sender_is_route) {
        json_t *a2 = json_object();
        json_set(a2, "resolved_provider", acf_copy_or_null(acf_get(route, "resolved_provider")));
        json_set(a2, "client", acf_copy_or_null(sender));
        json_set(a2, "main_runtime", acf_copy_or_null(acf_get(route, "main_runtime")));
        json_t *other = acf_call(io, "_recoverable_pool_provider", a2);
        json_free(a2);
        bool differs = !((!pool || pool->type == JSON_NULL) && (!other || other->type == JSON_NULL))
            && !(pool && other && pool->type == JSON_STRING && other->type == JSON_STRING
                 && strcmp(pool->str_val, other->str_val) == 0);
        json_free(other);
        if (differs) {
            json_free(pool);
            json_t *out = json_array();
            json_append(out, json_null());
            json_append(out, json_string(""));
            return out;
        }
    }
    /* str(getattr(sender, "api_key", "") or "") */
    json_t *sender_obj = acf_client_obj(io, sender);
    char *key = acf_str_or_empty(acf_get(sender_obj, "api_key"));
    json_t *out = json_array();
    json_append(out, acf_copy_or_null(pool));
    json_append(out, json_string(key));
    free(key);
    json_free(pool);
    return out;
}

/* PoP: acf_recover_failure_pool @ agent/auxiliary_client.py:_recover_failure_pool */
json_t *acf_recover_failure_pool(acf_io_t *io, json_t *in) {
    json_t *route = acf_get(in, "route");
    json_t *failure = acf_get(in, "failure");
    json_t *fp = acf_failure_pool(io, in);
    json_t *pool_provider = json_get(fp, 0);
    json_t *failed_api_key = json_get(fp, 1);
    /* bool(pool_provider) and _recover_provider_pool(...) — short-circuit */
    if (!acf_truthy(pool_provider)) { json_free(fp); return json_bool(false); }
    json_t *fc = acf_get(failure, "client");
    json_t *sender = (!fc || fc->type == JSON_NULL) ? acf_get(route, "client") : fc;
    json_t *sender_obj = acf_client_obj(io, sender);
    char *base = acf_client_base_url(sender_obj);
    json_t *args = json_object();
    json_set(args, "provider", json_copy(pool_provider));
    json_set(args, "exc", acf_copy_or_null(acf_get(failure, "error")));
    json_set(args, "failed_api_key", json_copy(failed_api_key));
    json_set(args, "verdict", acf_copy_or_null(acf_get(failure, "verdict")));
    json_set_hdr(args, "base_url", base);
    free(base);
    json_t *r = acf_call(io, "_recover_provider_pool", args);
    json_free(args);
    json_free(fp);
    bool yes = acf_truthy(r) && !acf_raised(r);
    json_free(r);
    return json_bool(yes);
}

/* PoP: acf_ladder_credential_rungs @ agent/auxiliary_client.py:_ladder_credential_rungs */
json_t *acf_ladder_credential_rungs(acf_io_t *io, json_t *in) {
    json_t *route = acf_get(in, "route");
    json_t *first_err = json_copy(acf_get(in, "first_err"));
    json_t *kwargs_in = acf_get(in, "kwargs");
    json_t *kwargs = acf_is_dict(kwargs_in) ? json_copy(kwargs_in) : json_object();
    bool client_is_nous = json_get_bool(in, "client_is_nous", false);
    json_t *repairs_in = acf_get(in, "repairs");
    bool spent = repairs_in && repairs_in->type == JSON_ARRAY && json_len(repairs_in) > 0;
    const char *accept = spent ? "ladder_spent" : "ladder_unspent";
    json_t *client_id = acf_get(route, "client");
    json_t *client_obj = acf_client_obj(io, client_id);
    json_t *resolved_provider = acf_get(route, "resolved_provider");
    /* auth_refresh_provider = _auth_refresh_provider_for_route(resolved_provider,
     *     route.base_info, _effective_provider_for_client(client, "")) */
    json_t *ea = json_object();
    json_set(ea, "client", acf_copy_or_null(client_id));
    json_set_hdr(ea, "fallback", "");
    json_t *eff = acf_call(io, "_effective_provider_for_client", ea);
    json_free(ea);
    json_t *ra = json_object();
    json_set(ra, "resolved_provider", acf_copy_or_null(resolved_provider));
    json_set(ra, "client_base_url", acf_copy_or_null(acf_get(route, "base_info")));
    json_set(ra, "effective_provider", eff);   /* ownership transferred */
    json_t *arp = acf_call(io, "_auth_refresh_provider_for_route", ra);
    json_free(ra);
    const char *auth_provider = (arp && arp->type == JSON_STRING) ? arp->str_val : "";
    /* auth_refresh(verdict) and provider not in {auto,"",None} and not nous */
    json_t *aa = json_object();
    json_set(aa, "verdict", acf_copy_or_null(acf_get(first_err, "verdict")));
    json_t *auth = acf_call(io, "auth_refresh", aa);
    json_free(aa);
    bool auth_yes = acf_truthy(auth) && !acf_raised(auth);
    json_free(auth);
    bool provider_ok = auth_provider[0] && strcmp(auth_provider, "auto") != 0;
    if (auth_yes && provider_ok && !client_is_nous) {
        /* refresh_kwargs = {"failed_api_key": client.api_key} when anthropic */
        json_t *rargs = json_object();
        json_set_hdr(rargs, "provider", auth_provider);
        if (strcmp(auth_provider, "anthropic") == 0) {
            json_t *k = acf_get(client_obj, "api_key");
            json_set(rargs, "failed_api_key", k ? json_copy(k) : json_string(""));
        }
        json_t *refreshed = acf_call(io, "_refresh_provider_credentials", rargs);
        json_free(rargs);
        bool refreshed_yes = acf_truthy(refreshed) && !acf_raised(refreshed);
        json_free(refreshed);
        if (refreshed_yes) {
            /* evict when the refreshed backend != the normalized route label */
            json_t *na = json_object();
            json_set(na, "provider", acf_copy_or_null(resolved_provider));
            json_t *norm = acf_call(io, "_normalize_aux_provider", na);
            json_free(na);
            const char *norm_s = (norm && norm->type == JSON_STRING) ? norm->str_val : "";
            if (strcmp(auth_provider, norm_s) != 0) {
                json_t *ev = json_object();
                json_set(ev, "provider", acf_copy_or_null(resolved_provider));
                json_t *evr = acf_call(io, "_evict_cached_clients", ev);
                json_free(ev);
                json_free(evr);
            }
            json_free(norm);
            /* step = retry_same_provider(auth_refresh_provider,
             *          route.resolved_model or route.final_model, repairs) */
            json_t *step = json_object();
            json_set_hdr(step, "kind", "retry_same_provider");
            json_t *sargs = json_array();
            json_append(sargs, json_string(auth_provider));
            json_t *rm = acf_get(route, "resolved_model");
            json_append(sargs, acf_truthy(rm) ? json_copy(rm) : acf_copy_or_null(acf_get(route, "final_model")));
            json_append(sargs, acf_copy_or_null(repairs_in));
            json_set(step, "args", sargs);
            acf_rung_out ro = acf_rung_run(io, step, accept, route);
            if (ro.raised) { json_free(arp); json_free(first_err); json_free(kwargs); return ro.raised; }
            if (!ro.failure || ro.failure->type == JSON_NULL) {
                json_t *out = json_object();
                json_set(out, "response", ro.result ? ro.result : json_null());
                json_set(out, "failure", json_null());
                json_set(out, "kwargs", kwargs);
                json_free(ro.failure); json_free(arp); json_free(first_err);
                return out;
            }
            /* first_err narrows to the retry's own failure */
            json_free(first_err);
            first_err = ro.failure;
            json_free(ro.result);
        }
    }
    json_free(arp);
    /* pool gate on the NARROWED error */
    json_t *fp_in = json_object();
    json_set(fp_in, "route", acf_copy_or_null(route));
    json_set(fp_in, "failure", json_copy(first_err));
    json_t *fp = acf_failure_pool(io, fp_in);
    json_free(fp_in);
    json_t *pool_provider = json_get(fp, 0);
    bool rotate = json_get_bool(acf_get(first_err, "verdict"), "should_rotate_credential", false);
    if (acf_truthy(pool_provider) && rotate) {
        /* same_key_retry(verdict): backoff rungs before rotation */
        json_t *sa = json_object();
        json_set(sa, "verdict", acf_copy_or_null(acf_get(first_err, "verdict")));
        json_t *skr = acf_call(io, "same_key_retry", sa);
        json_free(sa);
        bool same_key = acf_truthy(skr) && !acf_raised(skr);
        json_free(skr);
        if (same_key) {
            /* yield from rate_limit_retry_rungs(first_err, route, kwargs, accept):
             * the seam's wait/call sub-steps flow through perform/time.sleep. */
            json_t *ba = json_object();
            json_set(ba, "first_err", json_copy(first_err));
            json_set(ba, "route", acf_copy_or_null(route));
            json_set(ba, "kwargs", json_copy(kwargs));
            json_set_hdr(ba, "accept", accept);
            json_t *br = acf_call(io, "rate_limit_retry_rungs", ba);
            json_free(ba);
            if (acf_raised(br)) { json_free(fp); json_free(first_err); json_free(kwargs); return br; }
            json_t *bresp = acf_copy_or_null(acf_get(br, "resp"));
            json_t *bfail = acf_copy_or_null(acf_get(br, "failure"));
            json_free(br);
            if (!bfail || bfail->type == JSON_NULL) {
                json_t *out = json_object();
                json_set(out, "response", bresp);
                json_set(out, "failure", json_null());
                json_set(out, "kwargs", kwargs);
                json_free(bfail); json_free(fp); json_free(first_err);
                return out;
            }
            json_free(first_err);
            first_err = bfail;
            json_free(bresp);
        }
        /* _recover_failure_pool(route, first_err): rotate, then retry */
        json_t *rec_in = json_object();
        json_set(rec_in, "route", acf_copy_or_null(route));
        json_set(rec_in, "failure", json_copy(first_err));
        json_t *rec = acf_recover_failure_pool(io, rec_in);
        json_free(rec_in);
        bool recovered = acf_bool_val(rec);
        json_free(rec);
        if (recovered) {
            json_t *step = json_object();
            json_set_hdr(step, "kind", "retry_same_provider");
            json_t *sargs = json_array();
            json_append(sargs, acf_copy_or_null(resolved_provider));
            json_append(sargs, acf_copy_or_null(acf_get(route, "resolved_model")));
            json_append(sargs, acf_copy_or_null(repairs_in));
            json_set(step, "args", sargs);
            acf_rung_out ro = acf_rung_run(io, step, accept, route);
            if (ro.raised) { json_free(fp); json_free(first_err); json_free(kwargs); return ro.raised; }
            if (!ro.failure || ro.failure->type == JSON_NULL) {
                json_t *out = json_object();
                json_set(out, "response", ro.result ? ro.result : json_null());
                json_set(out, "failure", json_null());
                json_set(out, "kwargs", kwargs);
                json_free(ro.failure); json_free(fp); json_free(first_err);
                return out;
            }
            json_free(first_err);
            first_err = ro.failure;
            json_free(ro.result);
            /* rotated key also hit a wall: mark it now */
            if (json_get_bool(acf_get(first_err, "verdict"), "should_rotate_credential", false)) {
                json_t *r2_in = json_object();
                json_set(r2_in, "route", acf_copy_or_null(route));
                json_set(r2_in, "failure", json_copy(first_err));
                json_t *r2 = acf_recover_failure_pool(io, r2_in);
                json_free(r2_in);
                json_free(r2);
            }
        }
    }
    json_free(fp);
    json_t *out = json_object();
    json_set(out, "response", json_null());
    json_set(out, "failure", first_err);
    json_set(out, "kwargs", kwargs);
    return out;
}
/* ── provider fallback walk + ladder orchestration + driver ────────────── */

/* skip_lanes projection: a live set of lane tuples, ordered by compact JSON
 * so both sides emit the same sequence (ASCII fixtures). */
static int acf_lane_cmp(const void *a, const void *b) {
    json_t *la = *(json_t *const *)a;
    json_t *lb = *(json_t *const *)b;
    char *sa = json_serialize(la);
    char *sb = json_serialize(lb);
    int r = strcmp(sa ? sa : "", sb ? sb : "");
    free(sa);
    free(sb);
    return r;
}

static json_t *acf_lanes_sorted(const json_t *lanes) {
    json_t *out = json_array();
    if (!lanes || lanes->type != JSON_ARRAY || json_len(lanes) == 0) return out;
    size_t n = json_len(lanes);
    json_t **tmp = (json_t **)malloc(n * sizeof(json_t *));
    if (!tmp) return out;
    for (size_t i = 0; i < n; i++) tmp[i] = json_get(lanes, i);
    qsort(tmp, n, sizeof(json_t *), acf_lane_cmp);
    for (size_t i = 0; i < n; i++) json_append(out, json_copy(tmp[i]));
    free(tmp);
    return out;
}

/* chain-selector result {"client","model","label"} -> borrowed client test */
static bool acf_fb_present(const json_t *fb) {
    json_t *c = acf_get(fb, "client");
    return c && c->type != JSON_NULL;
}

/* PoP: acf_next_fallback_after_quarantine @ agent/auxiliary_client.py:_next_fallback_after_quarantine */
json_t *acf_next_fallback_after_quarantine(acf_io_t *io, json_t *in) {
    json_t *route = acf_get(in, "route");
    const char *task = acf_gets(in, "task", NULL);
    json_t *resolved_provider = acf_get(in, "resolved_provider");
    bool is_auto = json_get_bool(in, "is_auto", false);
    bool task_chain_only = json_get_bool(in, "task_chain_only", false);
    json_t *failed_model = acf_get(in, "failed_model");
    json_t *failure_scope = acf_get(in, "failure_scope");
    json_t *skip_lanes = acf_get(in, "skip_lanes");
    /* walk = dict(reason="fallback candidate unavailable",
     *             failed_base_url=route.base_info, failure_scope, skip_lanes) */
    json_t *lanes_sorted = acf_lanes_sorted(skip_lanes);
    json_t *a1 = json_object();
    json_set(a1, "task", acf_copy_or_null(acf_get(in, "task")));
    json_set(a1, "failed_provider", acf_truthy(resolved_provider) ? json_copy(resolved_provider) : json_string("auto"));
    json_set(a1, "failed_model", acf_copy_or_null(failed_model));
    json_set_hdr(a1, "reason", "fallback candidate unavailable");
    json_set(a1, "failed_base_url", acf_copy_or_null(acf_get(route, "base_info")));
    json_set(a1, "failure_scope", acf_copy_or_null(failure_scope));
    json_set(a1, "skip_lanes", json_copy(lanes_sorted));
    json_t *fb = acf_call(io, "_try_configured_fallback_chain", a1);
    json_free(a1);
    if (task_chain_only) { json_free(lanes_sorted); return fb; }
    if (!acf_fb_present(fb) && is_auto) {
        json_free(fb);
        json_t *a2 = json_object();
        json_set(a2, "task", acf_copy_or_null(acf_get(in, "task")));
        json_set(a2, "failed_provider", acf_truthy(resolved_provider) ? json_copy(resolved_provider) : json_string("auto"));
        json_set_hdr(a2, "reason", "fallback candidate unavailable");
        json_set(a2, "failed_model", acf_copy_or_null(failed_model));
        json_set(a2, "failed_base_url", acf_copy_or_null(acf_get(route, "base_info")));
        json_set(a2, "failure_scope", acf_copy_or_null(failure_scope));
        json_set(a2, "skip_lanes", json_copy(lanes_sorted));
        fb = acf_call(io, "_try_main_fallback_chain", a2);
        json_free(a2);
    }
    if (!acf_fb_present(fb)) {
        json_free(fb);
        json_t *a3 = json_object();
        json_set(a3, "failed_provider", acf_copy_or_null(resolved_provider));
        json_set(a3, "task", acf_copy_or_null(acf_get(in, "task")));
        json_set(a3, "main_runtime", acf_copy_or_null(acf_get(route, "main_runtime")));
        json_set_hdr(a3, "reason", "fallback candidate unavailable");
        json_set(a3, "failed_base_url", acf_copy_or_null(acf_get(route, "base_info")));
        json_set(a3, "failure_scope", acf_copy_or_null(failure_scope));
        json_set(a3, "skip_lanes", lanes_sorted);   /* ownership transferred */
        lanes_sorted = NULL;
        fb = acf_call(io, "_try_payment_fallback", a3);
        json_free(a3);
    }
    json_free(lanes_sorted);
    (void)task;
    return fb;
}

/* lane triple [label, model, base_url] equality (null-aware) */
static bool acf_lane_eq(const json_t *a, const json_t *b) {
    if (!a || !b || a->type != JSON_ARRAY || b->type != JSON_ARRAY) return false;
    if (json_len(a) != json_len(b)) return false;
    for (size_t i = 0; i < json_len(a); i++) {
        json_t *x = json_get(a, i);
        json_t *y = json_get(b, i);
        bool xn = !x || x->type == JSON_NULL;
        bool yn = !y || y->type == JSON_NULL;
        if (xn || yn) { if (xn != yn) return false; continue; }
        if (x->type != y->type) return false;
        if (x->type == JSON_STRING && strcmp(x->str_val, y->str_val) != 0) return false;
        if (x->type == JSON_NUMBER && x->num_val != y->num_val) return false;
        if (x->type == JSON_BOOL && x->bool_val != y->bool_val) return false;
    }
    return true;
}

/* PoP: acf_ladder_provider_fallback @ agent/auxiliary_client.py:_ladder_provider_fallback */
json_t *acf_ladder_provider_fallback(acf_io_t *io, json_t *in) {
    json_t *failure = acf_get(in, "failure");
    json_t *route = acf_get(in, "route");   /* mutable copy owned by the caller */
    bool rung_spent = json_get_bool(in, "rung_spent", false);
    const char *task = acf_gets(route, "task", NULL);
    json_t *resolved_provider = acf_get(route, "resolved_provider");
    json_t *first_err = acf_get(failure, "error");
    const char *rp = (resolved_provider && resolved_provider->type == JSON_STRING) ? resolved_provider->str_val : NULL;
    bool is_auto = !rp || !rp[0] || strcmp(rp, "auto") == 0;
    /* reason = aux_errors.fallback_label(failure, rung_spent=rung_spent) */
    json_t *fa = json_object();
    json_set(fa, "failure", acf_copy_or_null(failure));
    json_set(fa, "rung_spent", json_bool(rung_spent));
    json_t *reason_j = acf_call(io, "fallback_label", fa);
    json_free(fa);
    const char *reason = (reason_j && reason_j->type == JSON_STRING) ? reason_j->str_val : NULL;
    bool is_capacity_error = reason && strcmp(reason, "auth error") != 0;
    /* task_chain = _get_auxiliary_task_config(task).get("fallback_chain") if task else None */
    json_t *task_chain = NULL;
    if (task && task[0]) {
        json_t *ca = json_object();
        json_set_hdr(ca, "task", task);
        json_t *cfg = acf_call(io, "_get_auxiliary_task_config", ca);
        json_free(ca);
        task_chain = acf_copy_or_null(acf_get(cfg, "fallback_chain"));
        json_free(cfg);
    }
    bool has_task_fallback_chain = task_chain && task_chain->type == JSON_ARRAY && json_len(task_chain) > 0;
    bool explicit_auth_with_task_chain = reason && strcmp(reason, "auth error") == 0
                                      && !is_auto && has_task_fallback_chain;
    json_free(task_chain);
    json_t *out = json_object();
    /* gate: reason is None or not (is_auto or is_capacity_error or explicit_auth...) */
    if (!reason || !(is_auto || is_capacity_error || explicit_auth_with_task_chain)) {
        json_free(reason_j);
        json_set(out, "ret", json_null());
        json_set(out, "route_info", acf_copy_or_null(acf_get(route, "route_info")));
        return out;
    }
    if (strcmp(reason, "payment error") == 0) {
        /* mark the concrete backend that sent the failed request unhealthy */
        json_t *fc = acf_get(failure, "client");
        json_t *sender_id = (!fc || fc->type == JSON_NULL) ? acf_get(route, "client") : fc;
        json_t *sender_obj = acf_client_obj(io, sender_id);
        json_t *pa = json_object();
        json_set(pa, "resolved_provider", acf_copy_or_null(resolved_provider));
        json_set(pa, "client", acf_copy_or_null(sender_id));
        json_set(pa, "main_runtime", acf_copy_or_null(acf_get(route, "main_runtime")));
        json_set(pa, "same_path", json_bool(true));
        json_t *failed_provider = acf_call(io, "_recoverable_pool_provider", pa);
        json_free(pa);
        bool sender_is_route = acf_same_client(sender_id, acf_get(route, "client"));
        char *sender_url = sender_is_route ? strdup("") : acf_client_base_url(sender_obj);
        /* failed_provider or _classification_provider(sender, resolved_provider, main_runtime) */
        json_t *mark_provider;
        if (acf_truthy(failed_provider)) {
            mark_provider = json_copy(failed_provider);
        } else {
            json_t *cp_in = json_object();
            json_set(cp_in, "client", acf_copy_or_null(sender_obj));
            json_set(cp_in, "fallback", acf_copy_or_null(resolved_provider));
            json_set(cp_in, "main_runtime", acf_copy_or_null(acf_get(route, "main_runtime")));
            mark_provider = acf_classification_provider(io, cp_in);
            json_free(cp_in);
        }
        json_t *ma = json_object();
        json_set(ma, "provider", mark_provider);
        /* base_url=sender_url or route.base_info (None stays None) */
        json_set(ma, "base_url", sender_url[0] ? json_string(sender_url)
                                               : acf_copy_or_null(acf_get(route, "base_info")));
        json_t *mr = acf_call(io, "_mark_provider_unhealthy", ma);
        json_free(ma);
        json_free(mr);
        free(sender_url);
        json_free(failed_provider);
    }
    /* timeout/other logging is silent on both sides (no seam) */
    /* model-scoped skip for model failures; auth/payment are credential-wide */
    json_t *chain_failed_model = (strcmp(reason, "auth error") == 0 || strcmp(reason, "payment error") == 0)
        ? json_null() : acf_copy_or_null(acf_get(route, "final_model"));
    /* failure_scope: ENDPOINT for payment on a custom health base, else None */
    json_t *chain_failure_scope = json_null();
    if (strcmp(reason, "payment error") == 0) {
        json_t *ha = json_object();
        json_set(ha, "provider", acf_copy_or_null(resolved_provider));
        json_set(ha, "explicit_base_url", acf_copy_or_null(acf_get(route, "base_info")));
        json_t *hb = acf_call(io, "_custom_health_base_url", ha);
        json_free(ha);
        if (acf_truthy(hb) && !acf_raised(hb)) {
            json_free(chain_failure_scope);
            chain_failure_scope = json_string("endpoint");
        }
        json_free(hb);
    }
    /* first selection: configured task chain */
    json_t *sa = json_object();
    json_set(sa, "task", acf_copy_or_null(acf_get(route, "task")));
    json_set(sa, "failed_provider", acf_truthy(resolved_provider) ? json_copy(resolved_provider) : json_string("auto"));
    json_set_hdr(sa, "reason", reason);
    json_set(sa, "failed_model", json_copy(chain_failed_model));
    json_set(sa, "failed_base_url", acf_copy_or_null(acf_get(route, "base_info")));
    json_set(sa, "failure_scope", json_copy(chain_failure_scope));
    json_t *fb = acf_call(io, "_try_configured_fallback_chain", sa);
    json_free(sa);
    if (!acf_fb_present(fb) && is_auto) {
        json_free(fb);
        json_t *sb = json_object();
        json_set(sb, "task", acf_copy_or_null(acf_get(route, "task")));
        json_set(sb, "failed_provider", acf_truthy(resolved_provider) ? json_copy(resolved_provider) : json_string("auto"));
        json_set_hdr(sb, "reason", reason);
        json_set(sb, "failed_model", json_copy(chain_failed_model));
        json_set(sb, "failed_base_url", acf_copy_or_null(acf_get(route, "base_info")));
        json_set(sb, "failure_scope", json_copy(chain_failure_scope));
        fb = acf_call(io, "_try_main_fallback_chain", sb);
        json_free(sb);
        if (!acf_fb_present(fb)) {
            json_free(fb);
            json_t *sc = json_object();
            json_set(sc, "failed_provider", acf_copy_or_null(resolved_provider));
            json_set(sc, "task", acf_copy_or_null(acf_get(route, "task")));
            json_set_hdr(sc, "reason", reason);
            json_set(sc, "failed_base_url", acf_copy_or_null(acf_get(route, "base_info")));
            json_set(sc, "failure_scope", json_copy(chain_failure_scope));
            json_set(sc, "main_runtime", acf_copy_or_null(acf_get(route, "main_runtime")));
            fb = acf_call(io, "_try_payment_fallback", sc);
            json_free(sc);
        }
    } else if (!acf_fb_present(fb) && !explicit_auth_with_task_chain) {
        json_free(fb);
        json_t *sd = json_object();
        json_set(sd, "failed_provider", acf_copy_or_null(resolved_provider));
        json_set(sd, "task", acf_copy_or_null(acf_get(route, "task")));
        json_set_hdr(sd, "reason", reason);
        json_set(sd, "failed_model", json_copy(chain_failed_model));
        json_set(sd, "failed_base_url", acf_copy_or_null(acf_get(route, "base_info")));
        json_set(sd, "failure_scope", json_copy(chain_failure_scope));
        fb = acf_call(io, "_try_main_agent_model_fallback", sd);
        json_free(sd);
    }
    /* ordered walk: <=1 try per lane; a repeat ends the walk */
    json_t *tried_lanes = json_array();
    json_t *route_info = acf_get(route, "route_info");
    json_t *primary_route = (route_info && route_info->type == JSON_OBJECT) ? json_copy(route_info) : NULL;
    json_t *ret = json_null();
    while (acf_fb_present(fb)) {
        json_t *fb_client_id = acf_get(fb, "client");
        json_t *fb_model = acf_get(fb, "model");
        json_t *fb_label = acf_get(fb, "label");
        json_t *fb_client_obj = acf_client_obj(io, fb_client_id);
        char *fb_base = acf_client_base_url(fb_client_obj);
        json_t *lane = json_array();
        json_append(lane, acf_copy_or_null(fb_label));
        json_append(lane, acf_copy_or_null(fb_model));
        json_append(lane, json_string(fb_base));
        free(fb_base);
        bool seen = false;
        for (size_t i = 0; i < json_len(tried_lanes); i++) {
            if (acf_lane_eq(json_get(tried_lanes, i), lane)) { seen = true; break; }
        }
        if (seen) { json_free(lane); break; }
        json_append(tried_lanes, lane);
        /* _record_route_info(route_info, _fallback_provider_from_label(fb_label), fb_model) */
        json_t *la = json_object();
        json_set(la, "label", acf_copy_or_null(fb_label));
        json_t *fb_provider = acf_call(io, "_fallback_provider_from_label", la);
        json_free(la);
        if (route_info && route_info->type == JSON_OBJECT) {
            json_t *ri_in = json_object();
            json_set(ri_in, "route_info", json_copy(route_info));
            json_set(ri_in, "provider", fb_provider);
            json_set(ri_in, "model", acf_copy_or_null(fb_model));
            json_t *ri_out = acf_record_route_info(io, ri_in);
            json_free(ri_in);
            json_t *new_ri = acf_get(ri_out, "route_info");
            /* replace route_info contents in place (keep the object identity) */
            if (new_ri && new_ri->type == JSON_OBJECT) {
                while (route_info->c.count > 0) json_obj_del(route_info, route_info->c.keys[0]);
                for (size_t i = 0; i < new_ri->c.count; i++)
                    json_set(route_info, new_ri->c.keys[i], json_copy(new_ri->c.items[i]));
            }
            json_free(ri_out);
        }
        json_free(fb_provider);
        /* fb_resp = yield _LadderStep("fallback", (fb_client, fb_model, fb_label)) */
        json_t *step = json_object();
        json_set_hdr(step, "kind", "fallback");
        json_t *sargs = json_array();
        json_append(sargs, acf_copy_or_null(fb_client_id));
        json_append(sargs, acf_copy_or_null(fb_model));
        json_append(sargs, acf_copy_or_null(fb_label));
        json_set(step, "args", sargs);
        json_t *fb_resp = acf_call(io, "perform", step);
        json_free(step);
        if (acf_raised(fb_resp)) {
            /* an exception from the performer propagates out of the walk */
            json_free(ret);
            ret = fb_resp;
            json_free(fb);
            json_free(tried_lanes);
            json_free(primary_route);
            json_free(chain_failed_model);
            json_free(chain_failure_scope);
            json_free(reason_j);
            json_set(out, "ret", ret);
            json_set(out, "route_info", acf_copy_or_null(route_info));
            return out;
        }
        if (fb_resp && fb_resp->type != JSON_NULL) {
            if (route_info && route_info->type == JSON_OBJECT) {
                /* tell the caller its own route failed, not only where it landed */
                json_t *fm = acf_get(route, "final_model");
                json_set(route_info, "failed_model", json_string(acf_truthy(fm) ? acf_gets(route, "final_model", "") : ""));
                const char *err_s = acf_gets(first_err, "message", "");
                size_t n = strlen(reason) + strlen(err_s) + 4;
                char *buf = (char *)malloc(n);
                if (buf) snprintf(buf, n, "%s: %s", reason, err_s);
                json_set(route_info, "failure", json_string(buf ? buf : ""));
                free(buf);
                json_set(route_info, "fallback_label", acf_copy_or_null(fb_label));
            }
            json_free(ret);
            ret = fb_resp;
            json_free(fb);
            json_free(tried_lanes);
            json_free(primary_route);
            json_free(chain_failed_model);
            json_free(chain_failure_scope);
            json_free(reason_j);
            json_set(out, "ret", ret);
            json_set(out, "route_info", acf_copy_or_null(route_info));
            return out;
        }
        json_free(fb_resp);
        /* quarantined: re-walk configured chains past the tried lanes */
        json_t *na = json_object();
        json_set(na, "task", acf_copy_or_null(acf_get(route, "task")));
        json_set(na, "resolved_provider", acf_copy_or_null(resolved_provider));
        json_set(na, "is_auto", json_bool(is_auto));
        json_set(na, "route", acf_copy_or_null(route));
        json_set(na, "failed_model", json_copy(chain_failed_model));
        json_set(na, "failure_scope", json_copy(chain_failure_scope));
        json_set(na, "task_chain_only", json_bool(explicit_auth_with_task_chain));
        json_set(na, "skip_lanes", json_copy(tried_lanes));
        json_free(fb);
        fb = acf_next_fallback_after_quarantine(io, na);
        json_free(na);
    }
    /* exhausted: restore the primary route, return None (caller re-raises) */
    if (primary_route && route_info && route_info->type == JSON_OBJECT) {
        for (size_t i = 0; i < primary_route->c.count; i++)
            json_set(route_info, primary_route->c.keys[i], json_copy(primary_route->c.items[i]));
    }
    json_free(fb);
    json_free(tried_lanes);
    json_free(primary_route);
    json_free(chain_failed_model);
    json_free(chain_failure_scope);
    json_free(reason_j);
    json_set(out, "ret", ret);
    json_set(out, "route_info", acf_copy_or_null(route_info));
    return out;
}

/* PoP: acf_aux_recovery_ladder @ agent/auxiliary_client.py:_aux_recovery_ladder */
json_t *acf_aux_recovery_ladder(acf_io_t *io, json_t *in) {
    json_t *kwargs_in = acf_get(in, "kwargs");
    json_t *kwargs = acf_is_dict(kwargs_in) ? json_copy(kwargs_in) : json_object();
    bool async_mode = json_get_bool(in, "async_mode", false);
    /* route = _LadderRoute(client, task, tag, async_mode, base_info, resolved_*,
     *                      final_model, main_runtime, route_info, kwargs.get("timeout")) */
    json_t *route = json_object();
    json_set(route, "client", acf_copy_or_null(acf_get(in, "client")));
    json_set(route, "task", acf_copy_or_null(acf_get(in, "task")));
    json_set_hdr(route, "tag", async_mode ? " (async)" : "");
    json_set(route, "async_mode", json_bool(async_mode));
    json_set(route, "base_info", acf_copy_or_null(acf_get(in, "base_info")));
    json_set(route, "resolved_provider", acf_copy_or_null(acf_get(in, "resolved_provider")));
    json_set(route, "resolved_model", acf_copy_or_null(acf_get(in, "resolved_model")));
    json_set(route, "resolved_base_url", acf_copy_or_null(acf_get(in, "resolved_base_url")));
    json_set(route, "resolved_api_key", acf_copy_or_null(acf_get(in, "resolved_api_key")));
    json_set(route, "resolved_api_mode", acf_copy_or_null(acf_get(in, "resolved_api_mode")));
    json_set(route, "final_model", acf_copy_or_null(acf_get(in, "final_model")));
    json_set(route, "main_runtime", acf_copy_or_null(acf_get(in, "main_runtime")));
    json_set(route, "route_info", acf_copy_or_null(acf_get(in, "route_info")));
    json_set(route, "timeout", acf_copy_or_null(acf_get(kwargs, "timeout")));
    json_t *out = json_object();
    /* parameter rungs (first_err normalized inside, as _failure(route, first_err)) */
    json_t *pa = json_object();
    json_set(pa, "first_err", acf_copy_or_null(acf_get(in, "first_err")));
    json_set(pa, "route", json_copy(route));
    json_set(pa, "kwargs", json_copy(kwargs));
    json_set(pa, "max_tokens", acf_copy_or_null(acf_get(in, "max_tokens")));
    json_t *pr = acf_ladder_parameter_rungs(io, pa);
    json_free(pa);
    if (acf_raised(pr)) {
        json_free(kwargs);
        json_set(out, "ret", pr);
        json_set(out, "route_info", acf_copy_or_null(acf_get(route, "route_info")));
        json_set(out, "kwargs", json_null());
        json_free(route);
        return out;
    }
    json_free(kwargs);
    kwargs = acf_copy_or_null(acf_get(pr, "kwargs"));
    json_t *first_err = acf_copy_or_null(acf_get(pr, "failure"));
    json_t *repairs = acf_copy_or_null(acf_get(pr, "repairs"));
    json_t *response = acf_copy_or_null(acf_get(pr, "response"));
    json_free(pr);
    if (!first_err || first_err->type == JSON_NULL) {
        json_set(out, "ret", response);
        json_set(out, "route_info", acf_copy_or_null(acf_get(route, "route_info")));
        json_set(out, "kwargs", kwargs);
        json_free(first_err); json_free(repairs); json_free(route);
        return out;
    }
    json_free(response);
    /* client_is_nous = resolved_provider == "nous" or host match (short-circuit) */
    const char *rp = acf_gets(route, "resolved_provider", "");
    bool client_is_nous = strcmp(rp, "nous") == 0;
    if (!client_is_nous) {
        json_t *ha = json_object();
        json_set(ha, "base_url", acf_copy_or_null(acf_get(in, "base_info")));
        json_set_hdr(ha, "domain", "inference-api.nousresearch.com");
        json_t *hm = acf_call(io, "base_url_host_matches", ha);
        json_free(ha);
        client_is_nous = acf_truthy(hm) && !acf_raised(hm);
        json_free(hm);
    }
    /* Nous rungs */
    json_t *na = json_object();
    json_set(na, "first_err", json_copy(first_err));
    json_set(na, "route", json_copy(route));
    json_set(na, "kwargs", json_copy(kwargs));
    json_set(na, "client_is_nous", json_bool(client_is_nous));
    json_set(na, "repairs", json_copy(repairs));
    json_t *nr = acf_ladder_nous_rungs(io, na);
    json_free(na);
    if (acf_raised(nr)) {
        json_free(first_err); json_free(repairs); json_free(kwargs);
        json_set(out, "ret", nr);
        json_set(out, "route_info", acf_copy_or_null(acf_get(route, "route_info")));
        json_set(out, "kwargs", json_null());
        json_free(route);
        return out;
    }
    json_free(kwargs);
    kwargs = acf_copy_or_null(acf_get(nr, "kwargs"));
    json_free(first_err);
    first_err = acf_copy_or_null(acf_get(nr, "failure"));
    response = acf_copy_or_null(acf_get(nr, "response"));
    json_free(nr);
    if (!first_err || first_err->type == JSON_NULL) {
        json_set(out, "ret", response);
        json_set(out, "route_info", acf_copy_or_null(acf_get(route, "route_info")));
        json_set(out, "kwargs", kwargs);
        json_free(first_err); json_free(repairs); json_free(route);
        return out;
    }
    json_free(response);
    /* credential rungs */
    json_t *ca = json_object();
    json_set(ca, "first_err", json_copy(first_err));
    json_set(ca, "route", json_copy(route));
    json_set(ca, "kwargs", json_copy(kwargs));
    json_set(ca, "client_is_nous", json_bool(client_is_nous));
    json_set(ca, "repairs", json_copy(repairs));
    json_t *cr = acf_ladder_credential_rungs(io, ca);
    json_free(ca);
    if (acf_raised(cr)) {
        json_free(first_err); json_free(repairs); json_free(kwargs);
        json_set(out, "ret", cr);
        json_set(out, "route_info", acf_copy_or_null(acf_get(route, "route_info")));
        json_set(out, "kwargs", json_null());
        json_free(route);
        return out;
    }
    json_free(kwargs);
    kwargs = acf_copy_or_null(acf_get(cr, "kwargs"));
    json_free(first_err);
    first_err = acf_copy_or_null(acf_get(cr, "failure"));
    response = acf_copy_or_null(acf_get(cr, "response"));
    json_free(cr);
    if (!first_err || first_err->type == JSON_NULL) {
        json_set(out, "ret", response);
        json_set(out, "route_info", acf_copy_or_null(acf_get(route, "route_info")));
        json_set(out, "kwargs", kwargs);
        json_free(first_err); json_free(repairs); json_free(route);
        return out;
    }
    json_free(response);
    /* provider fallback (last rung) */
    json_t *pfa = json_object();
    json_set(pfa, "failure", json_copy(first_err));
    json_set(pfa, "route", json_copy(route));
    json_set(pfa, "rung_spent", json_bool(repairs && repairs->type == JSON_ARRAY && json_len(repairs) > 0));
    json_t *pf = acf_ladder_provider_fallback(io, pfa);
    json_free(pfa);
    json_t *fb_resp = acf_copy_or_null(acf_get(pf, "ret"));
    /* the walk may have mutated route_info on its copy; sync back */
    json_t *pf_ri = acf_get(pf, "route_info");
    json_t *route_ri = acf_get(route, "route_info");
    if (pf_ri && pf_ri->type == JSON_OBJECT && route_ri && route_ri->type == JSON_OBJECT) {
        while (route_ri->c.count > 0) json_obj_del(route_ri, route_ri->c.keys[0]);
        for (size_t i = 0; i < pf_ri->c.count; i++)
            json_set(route_ri, pf_ri->c.keys[i], json_copy(pf_ri->c.items[i]));
    } else if (pf_ri && pf_ri->type == JSON_OBJECT) {
        json_set(route, "route_info", json_copy(pf_ri));
    }
    json_free(pf);
    if (acf_raised(fb_resp)) {
        json_free(first_err); json_free(repairs); json_free(kwargs);
        json_set(out, "ret", fb_resp);
        json_set(out, "route_info", acf_copy_or_null(acf_get(route, "route_info")));
        json_set(out, "kwargs", json_null());
        json_free(route);
        return out;
    }
    if (fb_resp && fb_resp->type != JSON_NULL) {
        json_set(out, "ret", fb_resp);
        json_set(out, "route_info", acf_copy_or_null(acf_get(route, "route_info")));
        json_set(out, "kwargs", kwargs);
        json_free(first_err); json_free(repairs); json_free(route);
        return out;
    }
    json_free(fb_resp);
    /* connection/timeout poison: evict the cached client instance */
    json_t *transport = acf_get(acf_get(first_err, "verdict"), "error_context");
    if (acf_truthy(acf_get(transport, "transport"))) {
        json_t *ea = json_object();
        json_set(ea, "target", acf_copy_or_null(acf_get(in, "client")));
        json_t *er = acf_call(io, "_evict_cached_client_instance", ea);
        json_free(ea);
        json_free(er);   /* except Exception: logger.debug — silent */
    }
    /* _note_relay_auxiliary_error(first_err.error, first_err) */
    if (io && io->relay_context)
        acf_note_relay_mutate(io->relay_context, acf_get(first_err, "error"), first_err);
    /* raise the NARROWED error */
    json_t *raised = acf_copy_or_null(acf_get(first_err, "error"));
    json_set(out, "ret", raised);
    json_set(out, "route_info", acf_copy_or_null(acf_get(route, "route_info")));
    json_set(out, "kwargs", kwargs);
    json_free(first_err);
    json_free(repairs);
    json_free(route);
    return out;
}

/* PoP: acf_drive_ladder @ agent/auxiliary_client.py:_drive_ladder */
json_t *acf_drive_ladder(acf_io_t *io, json_t *in) {
    /* The ladder generator is injected as a scripted step source:
     *   gen.steps   ordered _LadderStep projections the generator yields
     *   gen.final   the StopIteration value
     *   gen.on_throw "propagate" (unhandled -> driver raises) | "swallow"
     * The driver loop mirrors send/throw switching: perform (or time.sleep for
     * "wait") -> (value, exc) via the _outcome contract -> send value / throw exc. */
    json_t *gen = acf_get(in, "gen");
    json_t *steps = acf_get(gen, "steps");
    json_t *final = acf_get(gen, "final");
    const char *on_throw = acf_gets(gen, "on_throw", "propagate");
    size_t n = (steps && steps->type == JSON_ARRAY) ? json_len(steps) : 0;
    size_t idx = 0;
    json_t *value = json_null();
    json_t *error = NULL;
    for (;;) {
        if (error) {
            /* ladder.throw(exc): the scripted generator handles or propagates */
            if (strcmp(on_throw, "propagate") == 0) {
                json_free(value);
                return error;   /* unhandled: raises out of the driver */
            }
            json_free(error);
            error = NULL;   /* swallowed: resume to the next yield */
        }
        if (idx >= n) {
            /* StopIteration(stop.value) */
            json_free(value);
            json_t *out = json_object();
            json_set(out, "ret", acf_copy_or_null(final));
            return out;
        }
        json_t *step = json_get(steps, idx++);
        const char *kind = acf_gets(step, "kind", "");
        json_t *r;
        if (strcmp(kind, "wait") == 0) {
            /* _outcome(time.sleep, step.args[0]) */
            json_t *args = json_object();
            json_t *sargs = acf_get(step, "args");
            json_set(args, "seconds", acf_copy_or_null(
                (sargs && sargs->type == JSON_ARRAY && json_len(sargs) > 0) ? json_get(sargs, 0) : NULL));
            r = acf_call(io, "time.sleep", args);
            json_free(args);
        } else {
            /* _outcome(perform, step) */
            r = acf_call(io, "perform", step);
        }
        json_free(value);
        if (acf_raised(r)) { value = json_null(); error = r; }
        else { value = r; error = NULL; }
    }
}

/* PoP: acf_primary_failure @ agent/auxiliary_client.py:_primary_failure */
json_t *acf_primary_failure(acf_io_t *io, json_t *in) {
    json_t *req = acf_get(in, "req");
    json_t *sub = json_object();
    json_set(sub, "exc", acf_copy_or_null(acf_get(in, "exc")));
    json_set(sub, "provider", acf_copy_or_null(acf_get(req, "resolved_provider")));
    json_set(sub, "client", acf_copy_or_null(acf_get(req, "client")));
    json_set(sub, "model", acf_copy_or_null(acf_get(req, "final_model")));
    /* req.base_info or req.resolved_base_url; main_runtime defaults to None */
    json_t *bi = acf_get(req, "base_info");
    json_set(sub, "base_url", acf_truthy(bi) ? json_copy(bi) : acf_copy_or_null(acf_get(req, "resolved_base_url")));
    json_set(sub, "main_runtime", json_null());
    json_t *out = acf_classify_failure(io, sub);
    json_free(sub);
    return out;
}

/* PoP: acf_start_recovery_ladder @ agent/auxiliary_client.py:_start_recovery_ladder */
json_t *acf_start_recovery_ladder(acf_io_t *io, json_t *in) {
    (void)io;
    /* Builds the recovery-ladder generator; the C mirror captures the bound
     * arguments (the oracle reads them from the generator frame). */
    json_t *req = acf_get(in, "req");
    json_t *retry_kwargs = acf_get(in, "retry_kwargs");
    json_t *out = json_object();
    json_set(out, "first_err", acf_copy_or_null(acf_get(in, "first_err")));
    json_set(out, "client", acf_copy_or_null(acf_get(req, "client")));
    json_set(out, "kwargs", acf_copy_or_null(acf_get(req, "kwargs")));
    json_set(out, "task", acf_copy_or_null(acf_get(in, "task")));
    json_set(out, "async_mode", json_bool(json_get_bool(in, "async_mode", false)));
    json_set(out, "base_info", acf_copy_or_null(acf_get(req, "base_info")));
    json_set(out, "resolved_provider", acf_copy_or_null(acf_get(req, "resolved_provider")));
    json_set(out, "resolved_model", acf_copy_or_null(acf_get(req, "resolved_model")));
    json_set(out, "resolved_base_url", acf_copy_or_null(acf_get(req, "resolved_base_url")));
    json_set(out, "resolved_api_key", acf_copy_or_null(acf_get(req, "resolved_api_key")));
    json_set(out, "resolved_api_mode", acf_copy_or_null(acf_get(req, "resolved_api_mode")));
    json_set(out, "final_model", acf_copy_or_null(acf_get(req, "final_model")));
    json_set(out, "max_tokens", acf_copy_or_null(acf_get(retry_kwargs, "max_tokens")));
    json_set(out, "main_runtime", acf_copy_or_null(acf_get(retry_kwargs, "main_runtime")));
    json_set(out, "route_info", acf_copy_or_null(acf_get(in, "route_info")));
    return out;
}
/* ── request preparation head + fallback candidates ────────────────────── */

/* PoP: acf_prepare_aux_request @ agent/auxiliary_client.py:_prepare_aux_request */
json_t *acf_prepare_aux_request(acf_io_t *io, json_t *in) {
    const char *task = acf_gets(in, "task", NULL);
    bool async_mode = json_get_bool(in, "async_mode", false);
    /* resolved = _resolve_task_provider_model(task, provider, model, base_url, api_key) */
    json_t *ra = json_object();
    json_set(ra, "task", acf_copy_or_null(acf_get(in, "task")));
    json_set(ra, "provider", acf_copy_or_null(acf_get(in, "provider")));
    json_set(ra, "model", acf_copy_or_null(acf_get(in, "model")));
    json_set(ra, "base_url", acf_copy_or_null(acf_get(in, "base_url")));
    json_set(ra, "api_key", acf_copy_or_null(acf_get(in, "api_key")));
    json_t *resolved = acf_call(io, "_resolve_task_provider_model", ra);
    json_free(ra);
    json_t *resolved_provider = acf_copy_or_null(acf_get(resolved, "resolved_provider"));
    json_t *resolved_model = acf_copy_or_null(acf_get(resolved, "resolved_model"));
    json_t *resolved_base_url = acf_copy_or_null(acf_get(resolved, "resolved_base_url"));
    json_t *resolved_api_key = acf_copy_or_null(acf_get(resolved, "resolved_api_key"));
    json_t *resolved_api_mode = acf_copy_or_null(acf_get(resolved, "resolved_api_mode"));
    json_free(resolved);
    /* if api_mode: resolved_api_mode = api_mode */
    json_t *api_mode = acf_get(in, "api_mode");
    if (acf_truthy(api_mode)) { json_free(resolved_api_mode); resolved_api_mode = json_copy(api_mode); }
    /* effective_extra_body = _get_task_extra_body(task); .update(extra_body or {}) */
    json_t *ea = json_object();
    json_set(ea, "task", acf_copy_or_null(acf_get(in, "task")));
    json_t *task_body = acf_call(io, "_get_task_extra_body", ea);
    json_free(ea);
    json_t *effective_extra_body = acf_is_dict(task_body) ? task_body : json_object();
    if (task_body && !acf_is_dict(task_body)) json_free(task_body);
    json_t *extra_body = acf_get(in, "extra_body");
    if (acf_is_dict(extra_body))
        for (size_t i = 0; i < extra_body->c.count; i++)
            json_set(effective_extra_body, extra_body->c.keys[i], json_copy(extra_body->c.items[i]));
    /* client, final_model, resolved_provider, effective_provider = _resolve_call_client(...) */
    json_t *rca = json_object();
    json_set(rca, "task", acf_copy_or_null(acf_get(in, "task")));
    json_set(rca, "provider", acf_copy_or_null(acf_get(in, "provider")));
    json_set(rca, "model", acf_copy_or_null(acf_get(in, "model")));
    json_set(rca, "base_url", acf_copy_or_null(acf_get(in, "base_url")));
    json_set(rca, "api_key", acf_copy_or_null(acf_get(in, "api_key")));
    json_set(rca, "resolved_provider", json_copy(resolved_provider));
    json_set(rca, "resolved_model", json_copy(resolved_model));
    json_set(rca, "resolved_base_url", json_copy(resolved_base_url));
    json_set(rca, "resolved_api_key", json_copy(resolved_api_key));
    json_set(rca, "resolved_api_mode", json_copy(resolved_api_mode));
    json_set(rca, "main_runtime", acf_copy_or_null(acf_get(in, "main_runtime")));
    json_set(rca, "async_mode", json_bool(async_mode));
    json_t *route = acf_call(io, "_resolve_call_client", rca);
    json_free(rca);
    json_t *client_id = acf_copy_or_null(acf_get(route, "client"));
    json_t *final_model = acf_copy_or_null(acf_get(route, "final_model"));
    json_free(resolved_provider);
    resolved_provider = acf_copy_or_null(acf_get(route, "resolved_provider"));
    json_t *effective_provider = acf_copy_or_null(acf_get(route, "effective_provider"));
    json_free(route);
    json_t *client_obj = acf_client_obj(io, client_id);
    /* effective_timeout = _effective_aux_timeout(task, timeout) */
    json_t *ta = json_object();
    json_set(ta, "task", acf_copy_or_null(acf_get(in, "task")));
    json_set(ta, "timeout", acf_copy_or_null(acf_get(in, "timeout")));
    json_t *effective_timeout = acf_call(io, "_effective_aux_timeout", ta);
    json_free(ta);
    /* Codex-only no_progress_timeout (isinstance check via the client kind tag) */
    json_t *no_progress_timeout = json_null();
    const char *kind = acf_gets(client_obj, "kind", "");
    if (strcmp(kind, "codex") == 0 || strcmp(kind, "async_codex") == 0) {
        json_free(no_progress_timeout);
        json_t *np_in = json_object();
        json_set(np_in, "task", acf_copy_or_null(acf_get(in, "task")));
        no_progress_timeout = acf_get_task_no_progress_timeout(io, np_in);
        json_free(np_in);
    }
    /* request_provider = effective_provider or resolved_provider */
    json_t *request_provider = acf_truthy(effective_provider) ? json_copy(effective_provider) : json_copy(resolved_provider);
    /* sync-only compression fast lane */
    if (!async_mode) {
        json_t *compression_config = json_object();
        if (task && strcmp(task, "compression") == 0) {
            json_free(compression_config);
            json_t *cca = json_object();
            json_set_hdr(cca, "task", task);
            json_t *cfg = acf_call(io, "_get_auxiliary_task_config", cca);
            json_free(cca);
            compression_config = acf_is_dict(cfg) ? cfg : json_object();
            if (cfg && !acf_is_dict(cfg)) json_free(cfg);
        }
        json_t *fl_in = json_object();
        json_set(fl_in, "task", acf_copy_or_null(acf_get(in, "task")));
        json_set(fl_in, "actual_provider", json_copy(request_provider));
        json_set(fl_in, "actual_model", json_copy(final_model));
        json_set(fl_in, "requested_provider", acf_copy_or_null(acf_get(in, "provider")));
        json_set(fl_in, "requested_model", acf_copy_or_null(acf_get(in, "model")));
        json_set(fl_in, "route_config", json_copy(compression_config));
        json_set(fl_in, "leak_guard_config", json_copy(compression_config));
        json_set(fl_in, "max_tokens", acf_copy_or_null(acf_get(in, "max_tokens")));
        json_set(fl_in, "extra_body", json_copy(effective_extra_body));
        json_t *fl = acf_compression_fast_lane_controls(io, fl_in);
        json_free(fl_in);
        json_free(compression_config);
        json_t *new_body = json_get(fl, 1);
        json_free(effective_extra_body);
        effective_extra_body = acf_copy_or_null(new_body);
        json_free(fl);
    }
    /* _set_relay_auxiliary_route(request_provider, final_model, resolved_api_mode) */
    json_t *sra = json_object();
    json_set(sra, "provider", json_copy(request_provider));
    json_set(sra, "model", json_copy(final_model));
    json_set(sra, "api_mode", json_copy(resolved_api_mode));
    json_t *srr = acf_call(io, "_set_relay_auxiliary_route", sra);
    json_free(sra);
    json_free(srr);
    /* _record_route_info(route_info, _fallback_provider_from_label(request_provider), final_model) */
    json_t *fla = json_object();
    json_set(fla, "label", json_copy(request_provider));
    json_t *rp_label = acf_call(io, "_fallback_provider_from_label", fla);
    json_free(fla);
    json_t *route_info = acf_copy_or_null(acf_get(in, "route_info"));
    if (route_info && route_info->type == JSON_OBJECT) {
        json_t *ri_in = json_object();
        json_set(ri_in, "route_info", json_copy(route_info));
        json_set(ri_in, "provider", json_copy(rp_label));
        json_set(ri_in, "model", json_copy(final_model));
        json_t *ri_out = acf_record_route_info(io, ri_in);
        json_free(ri_in);
        json_t *new_ri = acf_get(ri_out, "route_info");
        if (new_ri && new_ri->type == JSON_OBJECT) {
            while (route_info->c.count > 0) json_obj_del(route_info, route_info->c.keys[0]);
            for (size_t i = 0; i < new_ri->c.count; i++)
                json_set(route_info, new_ri->c.keys[i], json_copy(new_ri->c.items[i]));
        }
        json_free(ri_out);
    }
    json_free(rp_label);
    /* base_info: async getattr(client,"base_url","") vs sync default resolved_base_url */
    char *base_info;
    if (async_mode) {
        base_info = acf_client_base_url(client_obj);
    } else {
        json_t *b = acf_get(client_obj, "base_url");
        if (b) base_info = acf_str_or_empty(b);
        else base_info = acf_str_or_empty(resolved_base_url);
        /* sync logger.info silent on both sides */
    }
    /* kwargs = _build_call_kwargs(request_provider, final_model, messages, ...,
     *            base_url=base_info or resolved_base_url, task=task,
     *            no_progress_timeout=no_progress_timeout) */
    json_t *ka = json_object();
    json_set(ka, "provider", json_copy(request_provider));
    json_set(ka, "model", json_copy(final_model));
    json_set(ka, "messages", acf_copy_or_null(acf_get(in, "messages")));
    json_set(ka, "temperature", acf_copy_or_null(acf_get(in, "temperature")));
    json_set(ka, "max_tokens", acf_copy_or_null(acf_get(in, "max_tokens")));
    json_set(ka, "tools", acf_copy_or_null(acf_get(in, "tools")));
    json_set(ka, "timeout", json_copy(effective_timeout));
    json_set(ka, "extra_body", json_copy(effective_extra_body));
    json_set(ka, "reasoning_config", acf_copy_or_null(acf_get(in, "reasoning_config")));
    json_set(ka, "base_url", base_info[0] ? json_string(base_info) : json_copy(resolved_base_url));
    json_set(ka, "task", acf_copy_or_null(acf_get(in, "task")));
    json_set(ka, "no_progress_timeout", json_copy(no_progress_timeout));
    json_t *kwargs = acf_call(io, "_build_call_kwargs", ka);
    json_free(ka);
    if (!kwargs || kwargs->type != JSON_OBJECT) { json_free(kwargs); kwargs = json_object(); }
    /* if extra_headers: kwargs["extra_headers"] = dict(extra_headers) */
    json_t *extra_headers = acf_get(in, "extra_headers");
    if (acf_truthy(extra_headers)) json_set(kwargs, "extra_headers", json_copy(extra_headers));
    /* Anthropic-compatible endpoints: convert image blocks */
    char *client_base = acf_client_base_url(client_obj);
    json_t *aca = json_object();
    json_set(aca, "provider", json_copy(request_provider));
    json_set_hdr(aca, "base_url", client_base);
    json_t *compat = acf_call(io, "_is_anthropic_compat_endpoint", aca);
    json_free(aca);
    if (acf_truthy(compat) && !acf_raised(compat)) {
        json_t *cva = json_object();
        json_set(cva, "messages", acf_copy_or_null(acf_get(kwargs, "messages")));
        json_t *converted = acf_call(io, "_convert_openai_images_to_anthropic", cva);
        json_free(cva);
        json_set(kwargs, "messages", converted);
    }
    json_free(compat);
    free(client_base);
    /* _PreparedAuxRequest projection */
    json_t *out = json_object();
    json_set(out, "client", client_id);
    json_set(out, "final_model", final_model);
    json_set(out, "kwargs", kwargs);
    json_set(out, "resolved_provider", resolved_provider);
    json_set(out, "request_provider", request_provider);
    json_set(out, "resolved_model", resolved_model);
    json_set(out, "resolved_base_url", resolved_base_url);
    json_set(out, "resolved_api_key", resolved_api_key);
    json_set(out, "resolved_api_mode", resolved_api_mode);
    json_set(out, "effective_timeout", effective_timeout);
    json_set(out, "effective_extra_body", effective_extra_body);
    json_set(out, "base_info", json_string(base_info));
    json_set(out, "route_info", route_info);
    free(base_info);
    json_free(effective_provider);
    json_free(no_progress_timeout);
    return out;
}

/* PoP: acf_plan_aux_call @ agent/auxiliary_client.py:_plan_aux_call */
json_t *acf_plan_aux_call(acf_io_t *io, json_t *in) {
    /* main_runtime = _normalize_main_runtime(main_runtime) — one snapshot */
    json_t *na = json_object();
    json_set(na, "main_runtime", acf_copy_or_null(acf_get(in, "main_runtime")));
    json_t *main_runtime = acf_call(io, "_normalize_main_runtime", na);
    json_free(na);
    json_t *prep_in = json_copy(in);
    json_set(prep_in, "main_runtime", json_copy(main_runtime));
    json_t *req = acf_prepare_aux_request(io, prep_in);
    json_free(prep_in);
    /* candidate_kwargs = dict(task, messages, temperature, max_tokens, tools,
     *    effective_timeout, effective_extra_body, reasoning_config) */
    json_t *candidate_kwargs = json_object();
    json_set(candidate_kwargs, "task", acf_copy_or_null(acf_get(in, "task")));
    json_set(candidate_kwargs, "messages", acf_copy_or_null(acf_get(in, "messages")));
    json_set(candidate_kwargs, "temperature", acf_copy_or_null(acf_get(in, "temperature")));
    json_set(candidate_kwargs, "max_tokens", acf_copy_or_null(acf_get(in, "max_tokens")));
    json_set(candidate_kwargs, "tools", acf_copy_or_null(acf_get(in, "tools")));
    json_set(candidate_kwargs, "effective_timeout", acf_copy_or_null(acf_get(req, "effective_timeout")));
    json_set(candidate_kwargs, "effective_extra_body", acf_copy_or_null(acf_get(req, "effective_extra_body")));
    json_set(candidate_kwargs, "reasoning_config", acf_copy_or_null(acf_get(in, "reasoning_config")));
    /* retry_kwargs = dict(candidate_kwargs, resolved_*, main_runtime, final_model, extra_headers) */
    json_t *retry_kwargs = json_copy(candidate_kwargs);
    json_set(retry_kwargs, "resolved_base_url", acf_copy_or_null(acf_get(req, "resolved_base_url")));
    json_set(retry_kwargs, "resolved_api_key", acf_copy_or_null(acf_get(req, "resolved_api_key")));
    json_set(retry_kwargs, "resolved_api_mode", acf_copy_or_null(acf_get(req, "resolved_api_mode")));
    json_set(retry_kwargs, "main_runtime", json_copy(main_runtime));
    json_set(retry_kwargs, "final_model", acf_copy_or_null(acf_get(req, "final_model")));
    json_set(retry_kwargs, "extra_headers", acf_copy_or_null(acf_get(in, "extra_headers")));
    json_free(main_runtime);
    json_t *out = json_object();
    json_set(out, "req", req);
    json_set(out, "retry_kwargs", retry_kwargs);
    json_set(out, "candidate_kwargs", candidate_kwargs);
    return out;
}

/* PoP: acf_fallback_request_kwargs @ agent/auxiliary_client.py:_fallback_request_kwargs */
json_t *acf_fallback_request_kwargs(acf_io_t *io, json_t *in) {
    json_t *destination = acf_get(in, "destination");
    json_t *fallback_entry = acf_get(in, "fallback_entry");
    json_t *max_tokens = acf_get(in, "max_tokens");
    json_t *effective_extra_body = acf_get(in, "effective_extra_body");
    json_t *fallback_max_tokens = acf_copy_or_null(max_tokens);
    json_t *fallback_extra_body = acf_copy_or_null(effective_extra_body);
    if (json_get_bool(in, "apply_fast_lane", false)) {
        json_t *fl_in = json_object();
        json_set(fl_in, "task", acf_copy_or_null(acf_get(in, "task")));
        json_set(fl_in, "actual_provider", acf_copy_or_null(acf_get(destination, "provider")));
        json_set(fl_in, "actual_model", acf_copy_or_null(acf_get(destination, "model")));
        json_set(fl_in, "requested_provider", acf_copy_or_null(acf_get(fallback_entry, "provider")));
        json_set(fl_in, "requested_model", acf_copy_or_null(acf_get(fallback_entry, "model")));
        json_set(fl_in, "route_config", acf_copy_or_null(fallback_entry));
        json_set(fl_in, "leak_guard_config", acf_copy_or_null(acf_get(in, "task_config")));
        json_set(fl_in, "max_tokens", json_copy(max_tokens));
        json_set(fl_in, "extra_body", json_copy(effective_extra_body));
        json_t *fl = acf_compression_fast_lane_controls(io, fl_in);
        json_free(fl_in);
        json_free(fallback_max_tokens);
        json_free(fallback_extra_body);
        fallback_max_tokens = acf_copy_or_null(json_get(fl, 0));
        fallback_extra_body = acf_copy_or_null(json_get(fl, 1));
        json_free(fl);
    }
    /* fallback_messages, fallback_tools = _replan_synchronous_cache_sections(
     *     messages, tools, destination=destination) */
    json_t *rpa = json_object();
    json_set(rpa, "messages", acf_copy_or_null(acf_get(in, "messages")));
    json_set(rpa, "tools", acf_copy_or_null(acf_get(in, "tools")));
    json_set(rpa, "destination", acf_copy_or_null(destination));
    json_t *replanned = acf_call(io, "_replan_synchronous_cache_sections", rpa);
    json_free(rpa);
    /* fb_kwargs = _build_call_kwargs(destination.provider, destination.model, ...) */
    json_t *ka = json_object();
    json_set(ka, "provider", acf_copy_or_null(acf_get(destination, "provider")));
    json_set(ka, "model", acf_copy_or_null(acf_get(destination, "model")));
    json_set(ka, "messages", acf_copy_or_null(acf_get(replanned, "messages")));
    json_set(ka, "temperature", acf_copy_or_null(acf_get(in, "temperature")));
    json_set(ka, "max_tokens", fallback_max_tokens);
    json_set(ka, "tools", acf_copy_or_null(acf_get(replanned, "tools")));
    json_set(ka, "timeout", acf_copy_or_null(acf_get(in, "effective_timeout")));
    json_set(ka, "extra_body", fallback_extra_body);
    json_set(ka, "reasoning_config", acf_copy_or_null(acf_get(in, "reasoning_config")));
    json_set(ka, "base_url", acf_copy_or_null(acf_get(destination, "base_url")));
    json_set(ka, "task", acf_copy_or_null(acf_get(in, "task")));
    json_t *kwargs = acf_call(io, "_build_call_kwargs", ka);
    json_free(ka);
    json_free(replanned);
    return kwargs;
}

/* PoP: acf_plan_fallback_candidate @ agent/auxiliary_client.py:_plan_fallback_candidate */
json_t *acf_plan_fallback_candidate(acf_io_t *io, json_t *in) {
    const char *task = acf_gets(in, "task", NULL);
    const char *fb_label = acf_gets(in, "fb_label", "");
    double effective_timeout = json_get_num(in, "effective_timeout", 0.0);
    /* fb_timeout = _fallback_entry_timeout(task, fb_label); override when different */
    json_t *ta = json_object();
    json_set(ta, "task", acf_copy_or_null(acf_get(in, "task")));
    json_set_hdr(ta, "fb_label", fb_label);
    json_t *fb_timeout = acf_call(io, "_fallback_entry_timeout", ta);
    json_free(ta);
    if (fb_timeout && fb_timeout->type == JSON_NUMBER && fb_timeout->num_val != effective_timeout)
        effective_timeout = fb_timeout->num_val;   /* logger.info silent */
    json_free(fb_timeout);
    /* destination = _fallback_destination(task, fb_client, fb_model, fb_label) */
    json_t *da = json_object();
    json_set(da, "task", acf_copy_or_null(acf_get(in, "task")));
    json_set(da, "fb_client", acf_copy_or_null(acf_get(in, "fb_client")));
    json_set(da, "fb_model", acf_copy_or_null(acf_get(in, "fb_model")));
    json_set_hdr(da, "fb_label", fb_label);
    json_t *destination = acf_call(io, "_fallback_destination", da);
    json_free(da);
    /* task_config = _get_auxiliary_task_config(task) if task == "compression" else {} */
    json_t *task_config = json_object();
    if (task && strcmp(task, "compression") == 0) {
        json_free(task_config);
        json_t *cca = json_object();
        json_set_hdr(cca, "task", task);
        json_t *cfg = acf_call(io, "_get_auxiliary_task_config", cca);
        json_free(cca);
        task_config = acf_is_dict(cfg) ? cfg : json_object();
        if (cfg && !acf_is_dict(cfg)) json_free(cfg);
    }
    /* fallback_entry = _fallback_chain_entry(task, fb_label) or {} */
    json_t *cea = json_object();
    json_set(cea, "task", acf_copy_or_null(acf_get(in, "task")));
    json_set_hdr(cea, "fb_label", fb_label);
    json_t *entry = acf_call(io, "_fallback_chain_entry", cea);
    json_free(cea);
    json_t *fallback_entry = acf_is_dict(entry) ? entry : json_object();
    if (entry && !acf_is_dict(entry)) json_free(entry);
    /* common = dict(task, effective_timeout, fallback_entry, task_config,
     *               apply_fast_lane, **request) */
    json_t *request = acf_get(in, "request");
    json_t *common = json_object();
    json_set(common, "task", acf_copy_or_null(acf_get(in, "task")));
    json_set(common, "effective_timeout", json_number(effective_timeout));
    json_set(common, "fallback_entry", json_copy(fallback_entry));
    json_set(common, "task_config", json_copy(task_config));
    json_set(common, "apply_fast_lane", json_bool(json_get_bool(in, "apply_fast_lane", false)));
    if (acf_is_dict(request))
        for (size_t i = 0; i < request->c.count; i++)
            json_set(common, request->c.keys[i], json_copy(request->c.items[i]));
    json_t *fk_in = json_copy(common);
    json_set(fk_in, "destination", json_copy(destination));
    json_t *kwargs = acf_fallback_request_kwargs(io, fk_in);
    json_free(fk_in);
    json_t *out = json_object();
    json_set(out, "destination", json_copy(destination));
    json_set(out, "kwargs", kwargs);
    /* _rebuild closure: exercised when the case supplies rebuild_call */
    json_t *rebuild_call = acf_get(in, "rebuild_call");
    if (rebuild_call && rebuild_call->type == JSON_OBJECT) {
        json_t *rprovider = acf_get(rebuild_call, "provider");
        json_t *rclient_id = acf_get(rebuild_call, "client");
        json_t *rmodel = acf_get(rebuild_call, "model");
        json_t *rclient_obj = acf_client_obj(io, rclient_id);
        /* destination.base_url or str(getattr(client, "base_url", "") or "") */
        json_t *dbase = acf_get(destination, "base_url");
        json_t *rbase;
        if (acf_truthy(dbase)) rbase = json_copy(dbase);
        else {
            char *cb = acf_client_base_url(rclient_obj);
            rbase = json_string(cb);
            free(cb);
        }
        json_t *retry_destination = json_object();
        json_set(retry_destination, "provider", acf_copy_or_null(rprovider));
        json_set(retry_destination, "base_url", rbase);
        json_set(retry_destination, "api_mode", acf_copy_or_null(acf_get(destination, "api_mode")));
        /* model or destination.model */
        json_set(retry_destination, "model", acf_truthy(rmodel) ? json_copy(rmodel) : acf_copy_or_null(acf_get(destination, "model")));
        json_t *rk_in = json_copy(common);
        json_set(rk_in, "destination", json_copy(retry_destination));
        json_t *retry_kwargs = acf_fallback_request_kwargs(io, rk_in);
        json_free(rk_in);
        json_t *rebuild = json_object();
        json_set(rebuild, "destination", retry_destination);
        json_set(rebuild, "kwargs", retry_kwargs);
        json_set(out, "rebuild", rebuild);
    }
    json_free(common);
    json_free(destination);
    json_free(task_config);
    json_free(fallback_entry);
    return out;
}

/* PoP: acf_quarantine_fallback_candidate @ agent/auxiliary_client.py:_quarantine_fallback_candidate */
json_t *acf_quarantine_fallback_candidate(acf_io_t *io, json_t *in) {
    json_t *reason = acf_get(in, "reason");
    const char *reason_s = (reason && reason->type == JSON_STRING) ? reason->str_val : NULL;
    /* ttl=fallback_candidate_quarantine_ttl(reason) — evaluated before the mark */
    json_t *ta = json_object();
    json_set(ta, "reason", acf_copy_or_null(reason));
    json_t *ttl = acf_call(io, "fallback_candidate_quarantine_ttl", ta);
    json_free(ta);
    /* fb_provider or fb_label */
    json_t *fb_provider = acf_get(in, "fb_provider");
    json_t *provider = acf_truthy(fb_provider) ? json_copy(fb_provider) : acf_copy_or_null(acf_get(in, "fb_label"));
    /* model only for MODEL_NOT_FOUND / MODEL_INCOMPATIBLE reasons */
    bool model_scoped = reason_s && (strcmp(reason_s, "model not found") == 0
                                  || strcmp(reason_s, "model incompatible with route") == 0);
    json_t *ma = json_object();
    json_set(ma, "provider", provider);
    json_set(ma, "ttl", ttl);
    json_set(ma, "base_url", acf_copy_or_null(acf_get(in, "base_url")));
    json_set(ma, "reason", json_string(reason_s ? reason_s : "stale fallback credential"));
    json_set(ma, "model", model_scoped ? acf_copy_or_null(acf_get(in, "model")) : json_null());
    json_t *mr = acf_call(io, "_mark_provider_unhealthy", ma);
    json_free(ma);
    json_free(mr);
    /* logger.warning silent on both sides */
    return json_null();
}

/* PoP: acf_plan_fallback_auth_retry @ agent/auxiliary_client.py:_plan_fallback_auth_retry */
json_t *acf_plan_fallback_auth_retry(acf_io_t *io, json_t *in) {
    json_t *destination = acf_get(in, "destination");
    bool async_mode = json_get_bool(in, "async_mode", false);
    /* fb_provider = _auth_refresh_provider_for_route(dest.provider, dest.base_url) */
    json_t *ra = json_object();
    json_set(ra, "resolved_provider", acf_copy_or_null(acf_get(destination, "provider")));
    json_set(ra, "client_base_url", acf_copy_or_null(acf_get(destination, "base_url")));
    json_t *fb_provider = acf_call(io, "_auth_refresh_provider_for_route", ra);
    json_free(ra);
    const char *fp = (fb_provider && fb_provider->type == JSON_STRING) ? fb_provider->str_val : NULL;
    json_t *out = json_array();
    bool eligible = fp && fp[0] && strcmp(fp, "auto") != 0;
    bool refreshed_yes = false;
    if (eligible) {
        json_t *rargs = json_object();
        json_set(rargs, "provider", json_copy(fb_provider));
        if (strcmp(fp, "anthropic") == 0)
            json_set(rargs, "failed_api_key", acf_copy_or_null(acf_get(in, "failed_api_key")));
        json_t *refreshed = acf_call(io, "_refresh_provider_credentials", rargs);
        json_free(rargs);
        refreshed_yes = acf_truthy(refreshed) && !acf_raised(refreshed);
        json_free(refreshed);
    }
    if (eligible && refreshed_yes) {
        /* retry_client, retry_model = _get_cached_client(fb_provider, dest.model,
         *     **({"async_mode": True} if async_mode else {}),
         *     base_url=dest.base_url or None, api_mode=dest.api_mode) */
        json_t *ga = json_object();
        json_set(ga, "provider", json_copy(fb_provider));
        json_set(ga, "model", acf_copy_or_null(acf_get(destination, "model")));
        if (async_mode) json_set(ga, "async_mode", json_bool(true));
        json_t *dbase = acf_get(destination, "base_url");
        json_set(ga, "base_url", acf_truthy(dbase) ? json_copy(dbase) : json_null());
        json_set(ga, "api_mode", acf_copy_or_null(acf_get(destination, "api_mode")));
        json_t *cached = acf_call(io, "_get_cached_client", ga);
        json_free(ga);
        json_t *retry_client = acf_get(cached, "client");
        if (retry_client && retry_client->type != JSON_NULL) {
            /* retry_destination, retry_kwargs = rebuild(fb_provider, retry_client, retry_model) */
            json_t *ba = json_object();
            json_set(ba, "provider", json_copy(fb_provider));
            json_set(ba, "client", json_copy(retry_client));
            json_set(ba, "model", acf_copy_or_null(acf_get(cached, "model")));
            json_t *rebuilt = acf_call(io, "rebuild", ba);
            json_free(ba);
            json_t *retry = json_array();
            json_append(retry, json_copy(retry_client));
            json_append(retry, acf_copy_or_null(acf_get(rebuilt, "kwargs")));
            json_append(retry, acf_copy_or_null(acf_get(rebuilt, "destination")));
            json_free(rebuilt);
            json_free(cached);
            json_append(out, fb_provider);
            json_append(out, retry);
            return out;
        }
        json_free(cached);
    }
    json_append(out, fb_provider);
    json_append(out, json_null());
    return out;
}

/* PoP: acf_fallback_candidate_ladder @ agent/auxiliary_client.py:_fallback_candidate_ladder */
json_t *acf_fallback_candidate_ladder(acf_io_t *io, json_t *in) {
    json_t *fb_client_id = acf_get(in, "fb_client");
    json_t *fb_client_obj = acf_client_obj(io, fb_client_id);
    json_t *fb_kwargs = acf_get(in, "fb_kwargs");
    json_t *destination = json_copy(acf_get(in, "destination"));
    const char *task = acf_gets(in, "task", NULL);
    const char *fb_label = acf_gets(in, "fb_label", "");
    const char *tag = acf_gets(in, "tag", "");
    bool async_mode = json_get_bool(in, "async_mode", false);
    json_t *out = json_object();
    /* resp, failure, repairs = yield _LadderStep("attempt", (fb_client, fb_kwargs, dest)) */
    json_t *step = json_object();
    json_set_hdr(step, "kind", "attempt");
    json_t *sargs = json_array();
    json_append(sargs, acf_copy_or_null(fb_client_id));
    json_append(sargs, acf_copy_or_null(fb_kwargs));
    json_append(sargs, json_copy(destination));
    json_set(step, "args", sargs);
    json_t *r = acf_call(io, "perform", step);
    json_free(step);
    if (acf_raised(r)) { json_free(destination); json_set(out, "ret", r); return out; }
    json_t *resp = acf_copy_or_null(acf_get(r, "resp"));
    json_t *failure = acf_copy_or_null(acf_get(r, "failure"));
    json_t *repairs = acf_copy_or_null(acf_get(r, "repairs"));
    if (!repairs || repairs->type != JSON_ARRAY) { json_free(repairs); repairs = json_array(); }
    json_free(r);
    json_t *fb_provider = acf_copy_or_null(acf_get(destination, "provider"));
    /* auth error: refresh credentials and retry once with a rebuilt client */
    bool auth_yes = false;
    if (failure && failure->type != JSON_NULL) {
        json_t *aa = json_object();
        json_set(aa, "verdict", acf_copy_or_null(acf_get(failure, "verdict")));
        json_t *auth = acf_call(io, "auth_refresh", aa);
        json_free(aa);
        auth_yes = acf_truthy(auth) && !acf_raised(auth);
        json_free(auth);
    }
    json_t *retry = NULL;
    if (failure && failure->type != JSON_NULL && auth_yes) {
        json_t *pa = json_object();
        json_set(pa, "destination", json_copy(destination));
        json_set(pa, "async_mode", json_bool(async_mode));
        /* failed_api_key=getattr(fb_client, "api_key", "") */
        json_t *k = acf_get(fb_client_obj, "api_key");
        json_set(pa, "failed_api_key", k ? json_copy(k) : json_string(""));
        json_t *plan = acf_plan_fallback_auth_retry(io, pa);
        json_free(pa);
        json_free(fb_provider);
        fb_provider = acf_copy_or_null(json_get(plan, 0));
        retry = acf_copy_or_null(json_get(plan, 1));
        json_free(plan);
        if (retry && retry->type != JSON_NULL) {
            /* re-apply the first attempt's strips to the rebuilt request */
            json_t *rka = json_object();
            json_set(rka, "kwargs", acf_copy_or_null(json_get(retry, 1)));
            json_set(rka, "repairs", json_copy(repairs));
            json_t *reapplied = acf_call(io, "reapply_repairs", rka);
            json_free(rka);
            json_t *step2 = json_object();
            json_set_hdr(step2, "kind", "attempt");
            json_t *sargs2 = json_array();
            json_append(sargs2, acf_copy_or_null(json_get(retry, 0)));
            json_append(sargs2, reapplied);
            json_append(sargs2, acf_copy_or_null(json_get(retry, 2)));
            json_set(step2, "args", sargs2);
            json_t *r2 = acf_call(io, "perform", step2);
            json_free(step2);
            if (acf_raised(r2)) {
                json_free(resp); json_free(failure); json_free(repairs);
                json_free(retry); json_free(fb_provider); json_free(destination);
                json_set(out, "ret", r2);
                return out;
            }
            json_free(resp);
            resp = acf_copy_or_null(acf_get(r2, "resp"));
            json_free(failure);
            failure = acf_copy_or_null(acf_get(r2, "failure"));
            json_t *more = acf_get(r2, "repairs");
            if (more && more->type == JSON_ARRAY)
                for (size_t i = 0; i < json_len(more); i++)
                    json_append(repairs, json_copy(json_get(more, i)));
            json_free(r2);
            /* destination, repairs = retry[2], (*repairs, *more) */
            json_free(destination);
            destination = acf_copy_or_null(json_get(retry, 2));
        }
        /* still auth-failing (or no retry): quarantine and move on */
        bool auth2 = false;
        if (failure && failure->type != JSON_NULL && retry && retry->type != JSON_NULL) {
            json_t *aa2 = json_object();
            json_set(aa2, "verdict", acf_copy_or_null(acf_get(failure, "verdict")));
            json_t *auth2j = acf_call(io, "auth_refresh", aa2);
            json_free(aa2);
            auth2 = acf_truthy(auth2j) && !acf_raised(auth2j);
            json_free(auth2j);
        }
        if (failure && failure->type != JSON_NULL
            && ((!retry || retry->type == JSON_NULL) || auth2)) {
            json_t *qa = json_object();
            json_set(qa, "task", acf_copy_or_null(acf_get(in, "task")));
            json_set_hdr(qa, "fb_label", fb_label);
            json_set(qa, "fb_provider", json_copy(fb_provider));
            json_set(qa, "fb_err", acf_copy_or_null(acf_get(failure, "error")));
            json_set(qa, "base_url", acf_copy_or_null(acf_get(destination, "base_url")));
            json_set_hdr(qa, "tag", tag);
            json_t *qr = acf_quarantine_fallback_candidate(io, qa);
            json_free(qa);
            json_free(qr);
            json_free(resp); json_free(failure); json_free(repairs);
            json_free(retry); json_free(fb_provider); json_free(destination);
            json_set(out, "ret", json_null());
            return out;
        }
    }
    if (!failure || failure->type == JSON_NULL) {
        json_set(out, "ret", resp);
        json_free(failure); json_free(repairs); json_free(retry);
        json_free(fb_provider); json_free(destination);
        return out;
    }
    /* any other failure another candidate could avoid: quarantine + advance */
    json_t *fla = json_object();
    json_set(fla, "failure", json_copy(failure));
    json_set(fla, "rung_spent", json_bool(json_len(repairs) > 0));
    json_t *reason = acf_call(io, "fallback_label", fla);
    json_free(fla);
    if (!reason || reason->type == JSON_NULL) {
        /* raise failure.error */
        json_t *raised = acf_copy_or_null(acf_get(failure, "error"));
        json_free(reason);
        json_free(resp); json_free(failure); json_free(repairs);
        json_free(retry); json_free(fb_provider); json_free(destination);
        json_set(out, "ret", raised);
        return out;
    }
    json_t *qa = json_object();
    json_set(qa, "task", acf_copy_or_null(acf_get(in, "task")));
    json_set_hdr(qa, "fb_label", fb_label);
    json_set(qa, "fb_provider", json_copy(fb_provider));
    json_set(qa, "fb_err", acf_copy_or_null(acf_get(failure, "error")));
    json_set(qa, "base_url", acf_copy_or_null(acf_get(destination, "base_url")));
    json_set_hdr(qa, "tag", tag);
    json_set(qa, "reason", json_copy(reason));
    /* model=failure.verdict.model or destination.model */
    json_t *vm = acf_get(acf_get(failure, "verdict"), "model");
    json_set(qa, "model", acf_truthy(vm) ? json_copy(vm) : acf_copy_or_null(acf_get(destination, "model")));
    json_t *qr = acf_quarantine_fallback_candidate(io, qa);
    json_free(qa);
    json_free(qr);
    json_free(reason);
    json_free(resp);
    json_free(failure);
    json_free(repairs);
    json_free(retry);
    json_free(fb_provider);
    json_free(destination);
    json_set(out, "ret", json_null());
    return out;
}

/* PoP: acf_candidate_classification_provider @ agent/auxiliary_client.py:_candidate_classification_provider */
json_t *acf_candidate_classification_provider(acf_io_t *io, json_t *in) {
    json_t *da = json_object();
    json_set(da, "task", acf_copy_or_null(acf_get(in, "task")));
    json_set(da, "fb_client", acf_copy_or_null(acf_get(in, "fb_client")));
    json_set(da, "fb_model", acf_copy_or_null(acf_get(in, "fb_model")));
    json_set(da, "fb_label", acf_copy_or_null(acf_get(in, "fb_label")));
    json_t *destination = acf_call(io, "_fallback_destination", da);
    json_free(da);
    json_t *fb_client_obj = acf_client_obj(io, acf_get(in, "fb_client"));
    json_t *cp_in = json_object();
    json_set(cp_in, "client", acf_copy_or_null(fb_client_obj));
    json_set(cp_in, "fallback", acf_copy_or_null(acf_get(destination, "provider")));
    json_set(cp_in, "main_runtime", json_null());
    json_t *out = acf_classification_provider(io, cp_in);
    json_free(cp_in);
    json_free(destination);
    return out;
}
