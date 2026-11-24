/*
 * oauth.c - C11 port of upstream hermes_cli/web_routers/oauth.py
 * (gates w29_oauth_dc3549_p1, w29_oauth_dc3549_p2).
 *
 * PoP: w29_oauth_dc3549_p1_truncate_token @ hermes_cli/web_server_oauth.py:_truncate_token
 * PoP: w29_oauth_dc3549_p1_epoch_ms_to_iso @ hermes_cli/web_routers/oauth.py:_epoch_ms_to_iso
 * PoP: w29_oauth_dc3549_p1_status_card @ hermes_cli/web_routers/oauth.py:_status_card
 * PoP: w29_oauth_dc3549_p1_resolve_provider_status @ hermes_cli/web_routers/oauth.py:_resolve_provider_status + _PROVIDER_STATUS
 * PoP: w29_oauth_dc3549_p1_claude_disconnect_command @ hermes_cli/web_routers/oauth.py:_claude_code_disconnect_command
 * PoP: w29_oauth_dc3549_p1_provider_disconnect_command @ hermes_cli/web_routers/oauth.py:_oauth_provider_disconnect_command
 * PoP: w29_oauth_dc3549_p1_provider_disconnect_hint @ hermes_cli/web_routers/oauth.py:_oauth_provider_disconnect_hint
 * PoP: w29_oauth_dc3549_p1_disconnect_http_error @ hermes_cli/web_routers/oauth.py:_disconnect_http_error
 * PoP: w29_oauth_dc3549_p1_reject_detail @ hermes_cli/web_routers/oauth.py:_reject_if_not_disconnectable
 * PoP: w29_oauth_dc3549_p1_profile_name @ hermes_cli/web_server_oauth.py:_oauth_profile_name
 * PoP: w29_oauth_dc3549_p2_codex_http_error @ hermes_cli/web_routers/oauth.py:_codex_http_error
 * PoP: w29_oauth_dc3549_p2_http_error_detail @ hermes_cli/web_routers/oauth.py:_http_response_error_detail
 * PoP: w29_oauth_dc3549_p2_codex_start_error @ hermes_cli/web_routers/oauth.py:_codex_device_code_start_error
 * PoP: w29_oauth_dc3549_p2_codex_user_code @ hermes_cli/web_routers/oauth.py:_codex_request_user_code
 * PoP: w29_oauth_dc3549_p2_codex_exchange @ hermes_cli/web_routers/oauth.py:_codex_exchange_tokens
 * PoP: w29_oauth_dc3549_p2_codex_cancelled @ hermes_cli/web_routers/oauth.py:_codex_cancelled
 * PoP: w29_oauth_dc3549_p2_gc_survivors @ hermes_cli/web_routers/oauth.py:_gc_oauth_sessions
 * PoP: w29_oauth_dc3549_p2_device_start @ hermes_cli/web_routers/oauth.py:_device_session_started + _new_oauth_session
 * PoP: w29_oauth_dc3549_p2_validate_profile @ hermes_cli/web_routers/oauth.py:_validate_oauth_profile
 *
 * SEAMS (everything this file does NOT own):
 *   - FastAPI routes, HTTPException, Request/response objects;
 *   - httpx transports: the Codex usercode/token network calls are replaced by a
 *     scripted response (status + parsed payload + text) and a scripted
 *     `_codex_post` that records the form it would have sent;
 *   - the in-process session table (`_oauth_sessions` + its lock), the poller
 *     threads and `secrets.token_urlsafe` - the session id and clocks are
 *     fixture values;
 *   - the auth store getters (`get_nous_auth_status_local`, ...) and the
 *     provider registry: each is handed a canned raw dict;
 *   - the metrics/observability runtime.
 *   What is ported is exactly the part that is a pure function of values.
 *
 * Note on numbers: libjson stores every JSON number as a double, so Python's
 * int/float distinction is gone.  `oa_py_str` renders an integral double as an
 * integer, which is what the upstream f-strings see for the values these
 * functions stringify (regions, status codes, error codes).
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hermes_json.h"
#include "noc_w29_oauth_dc3549_p1.h"
#include "noc_w29_oauth_dc3549_p2.h"

/* ------------------------------------------------------------------ */
/* Python value semantics helpers                                      */
/* ------------------------------------------------------------------ */

static char *oa_strdup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Python truthiness. */
static int oa_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static int oa_is_object(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int oa_is_string(const json_t *v) { return v && v->type == JSON_STRING; }

/* obj.get(key) with a NULL-safe object. */
static const json_t *oa_get(const json_t *obj, const char *key) {
    return oa_is_object(obj) ? json_obj_get(obj, key) : NULL;
}

/* Python str(value) for the shapes these functions stringify. */
static char *oa_py_str(const json_t *v) {
    char buf[64];
    if (!v) return oa_strdup("None");
    switch (v->type) {
    case JSON_NULL: return oa_strdup("None");
    case JSON_BOOL: return oa_strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER: {
        double d = v->num_val;
        int p;
        if (d == floor(d) && fabs(d) < 9e15) {
            snprintf(buf, sizeof buf, "%lld", (long long)d);
            return oa_strdup(buf);
        }
        for (p = 1; p <= 17; p++) {
            snprintf(buf, sizeof buf, "%.*g", p, d);
            if (strtod(buf, NULL) == d) break;
        }
        return oa_strdup(buf);
    }
    case JSON_STRING: return oa_strdup(v->str_val ? v->str_val : "");
    default: {
        char *j = json_dumps(v, 0);
        return j ? j : oa_strdup("");
    }
    }
}

/* Python str.strip() over ASCII whitespace; returns a new string. */
static char *oa_strip(const char *s) {
    const char *b, *e;
    size_t n;
    char *out;
    if (!s) return oa_strdup("");
    b = s;
    while (*b && strchr(" \t\n\r\f\v", *b)) b++;
    e = b + strlen(b);
    while (e > b && strchr(" \t\n\r\f\v", e[-1])) e--;
    n = (size_t)(e - b);
    out = (char *)malloc(n + 1);
    if (!out) return oa_strdup("");
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}

/* Python int(value) for the shapes a device-code payload carries. */
static int oa_int_of(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = trunc(v->num_val); return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_STRING && v->str_val) {
        char *end = NULL;
        long long x = strtoll(v->str_val, &end, 10);
        if (end == v->str_val) return 0;
        while (end && *end && strchr(" \t\n\r\f\v", *end)) end++;
        if (end && *end) return 0;
        *out = (double)x;
        return 1;
    }
    return 0;
}

/* `a or b or ...`: the first truthy operand, else the last one (Python). */
static const json_t *oa_or_chain(const json_t *const *vals, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        if (oa_truthy(vals[i])) return vals[i];
    return n ? vals[n - 1] : NULL;
}

/* `obj.get(key) or default`: a truthy value stays, anything else -> default. */
static json_t *oa_str_or(const json_t *obj, const char *key, const char *dflt) {
    const json_t *v = oa_get(obj, key);
    if (oa_truthy(v)) {
        if (v->type == JSON_STRING) return json_string(v->str_val ? v->str_val : "");
        {
            char *s = oa_py_str(v);
            json_t *out = json_string(s);
            free(s);
            return out;
        }
    }
    return json_string(dflt);
}

static json_t *oa_copy_or_null(const json_t *v) { return v ? json_copy(v) : json_null(); }

/* _truncate_token: last `visible` chars, "…"-prefixed when longer. */
static json_t *oa_truncate_impl(const json_t *v, int visible) {
    char *s, *out;
    size_t len, i, dots = 0, cap;
    if (!oa_truthy(v)) return json_string("");
    if (oa_is_object(v) && oa_truthy(json_obj_get(v, "__callable__")))
        return json_string("<entra-id-bearer>");   /* a callable is never invoked */
    s = oa_py_str(v);
    for (i = 0; s[i]; i++)
        if (s[i] == '.') dots++;
    if (dots >= 2) {                                  /* JWT: tail of the signature */
        char *dot = strrchr(s, '.');
        if (dot) memmove(s, dot + 1, strlen(dot + 1) + 1);
    }
    len = strlen(s);
    if (visible < 0) visible = 0;
    if ((int)len <= visible) {
        json_t *o = json_string(s);
        free(s);
        return o;
    }
    /* visible == 0 is Python's s[-0:], i.e. the whole string */
    cap = (size_t)(visible ? visible : (int)len) + 4;
    out = (char *)malloc(cap);
    if (!out) { free(s); return json_string(""); }
    snprintf(out, cap, "\xE2\x80\xA6%s", visible ? s + (len - (size_t)visible) : s);
    free(s);
    {
        json_t *o = json_string(out);
        free(out);
        return o;
    }
}

/* ------------------------------------------------------------------ */
/* Gate p1: accounts-tab surface values                                */
/* ------------------------------------------------------------------ */

json_t *w29_oauth_dc3549_p1_truncate_token(const json_t *value, double visible) {
    return oa_truncate_impl(value, (int)visible);
}

json_t *w29_oauth_dc3549_p1_epoch_ms_to_iso(const json_t *value) {
    double ms, total, frac;
    time_t sec;
    long us;
    struct tm tmv;
    struct tm *tmp;
    char buf[96];
    int n;

    if (!oa_truthy(value)) return json_null();
    if (value->type != JSON_NUMBER) return json_null();  /* upstream would raise here */
    ms = value->num_val;
    total = ms / 1000.0;
    sec = (time_t)floor(total);
    frac = total - (double)sec;
    us = (long)llround(frac * 1e6);
    if (us >= 1000000L) { sec += 1; us -= 1000000L; }
    tmp = gmtime(&sec);
    if (!tmp) return json_null();
    tmv = *tmp;
    n = snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02d",
                 tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                 tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    if (n > 0 && (size_t)n < sizeof buf && us)
        snprintf(buf + n, sizeof buf - (size_t)n, ".%06ld", us);   /* isoformat: 6 digits */
    strncat(buf, "+00:00", sizeof buf - strlen(buf) - 1);
    return json_string(buf);
}

/* _status_card(raw, source, source_label, token_preview, expires_at,
 *              has_refresh_token, last_refresh=_OMIT) */
static json_t *oa_card(const json_t *raw, json_t *source, json_t *label, json_t *preview,
                       json_t *expires, json_t *refresh, int have_last, json_t *last_refresh) {
    json_t *card = json_object();
    json_set(card, "logged_in", json_bool(oa_truthy(oa_get(raw, "logged_in"))));
    json_set(card, "source", source ? source : json_null());
    json_set(card, "source_label", label ? label : json_null());
    json_set(card, "token_preview", preview ? preview : json_null());
    json_set(card, "expires_at", expires ? expires : json_null());
    json_set(card, "has_refresh_token", refresh ? refresh : json_bool(false));
    if (have_last) json_set(card, "last_refresh", last_refresh ? last_refresh : json_null());
    return card;
}

json_t *w29_oauth_dc3549_p1_status_card(const json_t *req) {
    const json_t *raw = oa_get(req, "raw");
    int have_last = json_get_bool(req, "last_refresh_present", false);
    return oa_card(raw,
                   oa_copy_or_null(oa_get(req, "source")),
                   oa_copy_or_null(oa_get(req, "source_label")),
                   oa_copy_or_null(oa_get(req, "token_preview")),
                   oa_copy_or_null(oa_get(req, "expires_at")),
                   oa_copy_or_null(oa_get(req, "has_refresh_token")),
                   have_last,
                   oa_copy_or_null(oa_get(req, "last_refresh")));
}

/* MiniMax's card label: f"MiniMax ({raw.get('region', 'global')})" */
static json_t *oa_minimax_label(const json_t *raw) {
    const json_t *region = oa_get(raw, "region");
    char *s, *buf;
    size_t need;
    json_t *out;
    if (!region) return json_string("MiniMax (global)");
    s = oa_py_str(region);
    need = strlen(s) + sizeof "MiniMax ()";
    buf = (char *)malloc(need);
    if (!buf) { free(s); return json_string("MiniMax (global)"); }
    snprintf(buf, need, "MiniMax (%s)", s);
    free(s);
    out = json_string(buf);
    free(buf);
    return out;
}

static json_t *oa_error_card(const char *msg) {
    json_t *o = json_object();
    json_set(o, "logged_in", json_bool(false));
    json_set(o, "error", json_string(msg ? msg : ""));
    return o;
}

static json_t *oa_logged_out(void) {
    json_t *o = json_object();
    json_set(o, "logged_in", json_bool(false));
    return o;
}

/* The hand-written _PROVIDER_STATUS card shapes. */
static json_t *oa_known_card(const char *pid, const json_t *raw) {
    json_t *card;

    if (!strcmp(pid, "nous")) {
        card = oa_card(raw, json_string("nous_portal"),
                       oa_str_or(raw, "portal_base_url", "Nous Portal"),
                       oa_truncate_impl(oa_get(raw, "access_token"), 6),
                       oa_copy_or_null(oa_get(raw, "access_expires_at")),
                       json_bool(oa_truthy(oa_get(raw, "has_refresh_token"))), 0, NULL);
        json_set(card, "free_tier", json_bool(oa_truthy(oa_get(raw, "free_tier"))));
        json_set(card, "account_tier", oa_copy_or_null(oa_get(raw, "account_tier")));
        return card;
    }
    if (!strcmp(pid, "openai-codex")) {
        return oa_card(raw,
                       oa_str_or(raw, "source", "openai_codex"),
                       oa_str_or(raw, "auth_mode", "OpenAI Codex"),
                       oa_truncate_impl(oa_get(raw, "api_key"), 6),
                       json_null(), json_bool(false), 1,
                       oa_copy_or_null(oa_get(raw, "last_refresh")));
    }
    if (!strcmp(pid, "qwen-oauth")) {
        return oa_card(raw, json_string("qwen_cli"),
                       oa_str_or(raw, "auth_file", "Qwen CLI"),
                       oa_truncate_impl(oa_get(raw, "api_key"), 6),
                       w29_oauth_dc3549_p1_epoch_ms_to_iso(oa_get(raw, "expires_at_ms")),
                       json_bool(oa_truthy(oa_get(raw, "has_refresh_token"))), 0, NULL);
    }
    if (!strcmp(pid, "minimax-oauth")) {
        return oa_card(raw, json_string("minimax_oauth"), oa_minimax_label(raw),
                       json_null(), oa_copy_or_null(oa_get(raw, "expires_at")),
                       json_bool(true), 0, NULL);
    }
    /* xai-oauth: label prefers the auth_store path, then the source string */
    {
        json_t *label = oa_truthy(oa_get(raw, "auth_store"))
                            ? oa_str_or(raw, "auth_store", "")
                            : (oa_truthy(oa_get(raw, "source")) ? oa_str_or(raw, "source", "")
                                                                : json_string("xAI Grok OAuth"));
        return oa_card(raw, oa_str_or(raw, "source", "xai_oauth"), label,
                       oa_truncate_impl(oa_get(raw, "api_key"), 6),
                       json_null(), json_bool(true), 1,
                       oa_copy_or_null(oa_get(raw, "last_refresh")));
    }
}

/* The slug-driven card for catalog-derived providers. */
static json_t *oa_generic_card(const char *pid, const json_t *raw) {
    const json_t *label_chain[5];
    const json_t *expires_chain[2];
    const json_t *token_chain[2];
    const json_t *src_chain[2];
    const json_t *label, *src;
    json_t *label_j, *preview, *src_j;

    if (!oa_is_object(raw) || !json_obj_get(raw, "logged_in")) return oa_logged_out();

    label_chain[0] = oa_get(raw, "source_label");
    label_chain[1] = oa_get(raw, "auth_store");
    label_chain[2] = oa_get(raw, "auth_store_path");
    label_chain[3] = oa_get(raw, "base_url");
    label_chain[4] = oa_get(raw, "name");
    label = oa_or_chain(label_chain, 5);
    label_j = oa_truthy(label) ? oa_copy_or_null(label) : json_string("");

    token_chain[0] = oa_get(raw, "access_token");
    token_chain[1] = oa_get(raw, "api_key");
    preview = oa_truncate_impl(oa_or_chain(token_chain, 2), 6);

    expires_chain[0] = oa_get(raw, "expires_at");
    expires_chain[1] = oa_get(raw, "access_expires_at");

    src_chain[0] = oa_get(raw, "source");
    src_chain[1] = oa_get(raw, "provider");
    src = oa_or_chain(src_chain, 2);
    src_j = oa_truthy(src) ? oa_copy_or_null(src) : json_string(pid);

    return oa_card(raw, src_j, label_j, preview,
                   oa_copy_or_null(oa_or_chain(expires_chain, 2)),
                   json_bool(oa_truthy(oa_get(raw, "has_refresh_token"))), 0, NULL);
}

json_t *w29_oauth_dc3549_p1_resolve_provider_status(const json_t *req) {
    const char *pid = json_get_str(req, "provider_id", "");
    const json_t *raw = oa_get(req, "raw");
    const json_t *raises = oa_get(req, "raises");
    int have_status_fn = json_get_bool(req, "status_fn", false);

    if (oa_truthy(raises)) {
        char *msg = oa_py_str(raises);
        json_t *out = oa_error_card(msg);
        free(msg);
        return out;
    }
    if (have_status_fn) return raw ? json_copy(raw) : json_null();   /* status_fn() wins */
    if (!strcmp(pid, "nous") || !strcmp(pid, "openai-codex") || !strcmp(pid, "qwen-oauth") ||
        !strcmp(pid, "minimax-oauth") || !strcmp(pid, "xai-oauth"))
        return oa_known_card(pid, raw);
    return oa_generic_card(pid, raw);
}

json_t *w29_oauth_dc3549_p1_claude_disconnect_command(const char *platform) {
    const char *rm_file = "rm -f ~/.claude/.credentials.json";
    char buf[512];
    if (!platform) platform = "linux";
    if (!strcmp(platform, "win32")) {
        return json_string(
            "if (Test-Path -LiteralPath \"$HOME/.claude/.credentials.json\") { "
            "Remove-Item -LiteralPath \"$HOME/.claude/.credentials.json\" -Force -ErrorAction Stop }");
    }
    if (!strcmp(platform, "darwin")) {
        snprintf(buf, sizeof buf,
                 "security delete-generic-password -s \"Claude Code-credentials\" 2>/dev/null; %s",
                 rm_file);
        return json_string(buf);
    }
    return json_string(rm_file);
}

json_t *w29_oauth_dc3549_p1_provider_disconnect_command(const json_t *provider, const char *platform) {
    const char *flow = json_get_str(provider, "flow", NULL);
    const char *id = json_get_str(provider, "id", NULL);
    if (!flow || !id) return json_null();
    if (strcmp(flow, "external") != 0 || strcmp(id, "claude-code") != 0) return json_null();
    return w29_oauth_dc3549_p1_claude_disconnect_command(platform);
}

json_t *w29_oauth_dc3549_p1_provider_disconnect_hint(const json_t *provider, const json_t *status) {
    const char *flow = json_get_str(provider, "flow", NULL);
    const char *id = json_get_str(provider, "id", NULL);

    if (flow && !strcmp(flow, "external") && !(id && !strcmp(id, "anthropic"))) {
        json_t *cmd = w29_oauth_dc3549_p1_provider_disconnect_command(provider, NULL);
        int managed = oa_truthy(cmd);
        json_free(cmd);
        if (managed) return json_string("Managed outside Hermes \xE2\x80\x94 run the disconnect command to remove it.");
        return json_string("Managed by that provider's CLI; remove it there.");
    }
    if (!strcmp(json_get_str(status, "source", ""), "env_var"))
        return json_string("Remove the API key from Settings \xE2\x86\x92 Keys instead.");
    return json_null();
}

json_t *w29_oauth_dc3549_p1_disconnect_http_error(const json_t *status_code, const char *provider_name) {
    double code = 0.0;
    json_t *o;
    char buf[512];
    const char *name = provider_name ? provider_name : "";
    if (!oa_int_of(status_code, &code)) code = 0.0;
    o = json_object();
    json_set(o, "status_code", json_number(code));
    if ((long long)code == 409)
        snprintf(buf, sizeof buf, "No stored credentials were removed for %s.", name);
    else
        snprintf(buf, sizeof buf, "Failed to remove stored credentials for %s.", name);
    json_set(o, "detail", json_string(buf));
    return o;
}

json_t *w29_oauth_dc3549_p1_reject_detail(const json_t *provider, const json_t *status) {
    json_t *hint = w29_oauth_dc3549_p1_provider_disconnect_hint(provider, status);
    char *h, *name_raw, *buf;
    size_t need;
    json_t *out;

    if (!oa_truthy(hint)) { json_free(hint); return json_null(); }
    h = oa_py_str(hint);
    name_raw = oa_py_str(oa_get(provider, "name"));
    need = strlen(name_raw) + strlen(h) + sizeof " cannot be disconnected automatically. ";
    buf = (char *)malloc(need);
    if (!buf) { free(h); free(name_raw); json_free(hint); return json_null(); }
    snprintf(buf, need, "%s cannot be disconnected automatically. %s", name_raw, h);
    out = json_string(buf);
    free(buf);
    free(name_raw);
    free(h);
    json_free(hint);
    return out;
}

/* _oauth_profile_name: strip, then "" / "current" (any case) -> no profile. */
json_t *w29_oauth_dc3549_p1_profile_name(const json_t *profile) {
    char *raw, *s;
    size_t i;
    int is_current;

    if (!profile || profile->type == JSON_NULL) return json_null();
    raw = oa_py_str(profile);
    s = oa_strip(raw);
    free(raw);
    if (!s[0]) { free(s); return json_null(); }
    is_current = (strlen(s) == 7);
    for (i = 0; is_current && i < 7; i++)
        if (tolower((unsigned char)s[i]) != "current"[i]) is_current = 0;
    if (is_current) { free(s); return json_null(); }
    {
        json_t *o = json_string(s);
        free(s);
        return o;
    }
}

/* ------------------------------------------------------------------ */
/* Gate p2: device-code flow values                                    */
/* ------------------------------------------------------------------ */

json_t *w29_oauth_dc3549_p2_codex_http_error(const json_t *status) {
    double s = 0.0;
    if (!oa_int_of(status, &s)) s = 0.0;
    if (s == 401.0 || s == 403.0) return json_string("codex_err");
    if (s == 408.0 || s == 429.0 || s >= 500.0) return json_string("ConnectionError");
    return json_string("RuntimeError");
}

/* _http_response_error_detail, as a malloc'd string. */
static char *oa_detail_str(const json_t *payload, const json_t *text) {
    if (oa_is_object(payload)) {
        const json_t *error = json_obj_get(payload, "error");
        if (oa_is_object(error)) {
            static const char *const keys[4] = {"message", "error_description", "code", "type"};
            char *parts[4];
            int np = 0, i;
            for (i = 0; i < 4; i++) {
                const json_t *e = json_obj_get(error, keys[i]);
                char *raw = e ? oa_py_str(e) : oa_strdup("");
                char *t = oa_strip(raw);
                free(raw);
                if (t[0]) parts[np++] = t;
                else free(t);
            }
            if (np) {
                size_t need = 1;
                char *joined, *p;
                for (i = 0; i < np; i++) need += strlen(parts[i]) + 2;
                joined = (char *)malloc(need);
                if (!joined) { for (i = 0; i < np; i++) free(parts[i]); return oa_strdup(""); }
                p = joined;
                p += sprintf(p, "%s", parts[0]);
                for (i = 1; i < np; i++) p += sprintf(p, ": %s", parts[i]);
                for (i = 0; i < np; i++) free(parts[i]);
                return joined;
            }
        }
        if (oa_is_string(error)) {
            char *t = oa_strip(error->str_val ? error->str_val : "");
            if (t[0]) return t;
            free(t);
        }
        {
            static const char *const keys[3] = {"detail", "message", "error_description"};
            int i;
            for (i = 0; i < 3; i++) {
                const json_t *v = json_obj_get(payload, keys[i]);
                if (oa_is_string(v)) {
                    char *t = oa_strip(v->str_val ? v->str_val : "");
                    if (t[0]) return t;
                    free(t);
                }
            }
        }
    }
    {
        char *raw = oa_truthy(text) ? oa_py_str(text) : oa_strdup("");
        char *t = oa_strip(raw);
        free(raw);
        if (strlen(t) > 500) t[500] = '\0';
        return t;
    }
}

json_t *w29_oauth_dc3549_p2_http_error_detail(const json_t *req) {
    const json_t *payload = json_get_bool(req, "json_raises", false) ? NULL : oa_get(req, "payload");
    char *d = oa_detail_str(payload, oa_get(req, "text"));
    json_t *out = json_string(d);
    free(d);
    return out;
}

json_t *w29_oauth_dc3549_p2_codex_start_error(const json_t *req) {
    const json_t *status = oa_get(req, "status");
    const json_t *payload = json_get_bool(req, "json_raises", false) ? NULL : oa_get(req, "payload");
    const json_t *text = oa_get(req, "text");
    char *detail = oa_detail_str(payload, text);
    char *lower, *status_s, *buf;
    size_t need, i;
    json_t *out;

    lower = oa_strdup(detail);
    for (i = 0; lower[i]; i++) lower[i] = (char)tolower((unsigned char)lower[i]);
    status_s = oa_truthy(status) ? oa_py_str(status) : oa_strdup("unknown");

    need = strlen(detail) + strlen(status_s) + 900;
    buf = (char *)malloc(need);
    if (!buf) { free(detail); free(lower); free(status_s); return json_null(); }
    {
        const char *msg;
        if (strstr(lower, "device") && (strstr(lower, "authori") || strstr(lower, "enable"))) {
            msg = "OpenAI rejected the device-code login request. Your OpenAI "
                  "account may need device-code authorization enabled before Hermes "
                  "can start this dashboard login. Enable device-code authorization "
                  "in OpenAI, then return here and click Login again.";
        } else {
            msg = "OpenAI rejected the device-code login request. Please try Login "
                  "again from the dashboard after checking your OpenAI account settings.";
        }
        if (detail[0]) snprintf(buf, need, "%s (HTTP %s: %s)", msg, status_s, detail);
        else snprintf(buf, need, "%s (HTTP %s)", msg, status_s);
    }
    out = json_string(buf);
    free(buf);
    free(detail);
    free(lower);
    free(status_s);
    return out;
}

json_t *w29_oauth_dc3549_p2_codex_user_code(const json_t *device_data) {
    const json_t *raw_interval = oa_get(device_data, "interval");
    const json_t *uc = oa_get(device_data, "user_code");
    const json_t *did = oa_get(device_data, "device_auth_id");
    double interval = 5.0;
    json_t *o = json_object();

    if (!raw_interval) interval = 5.0;            /* .get("interval", "5") */
    else if (!oa_int_of(raw_interval, &interval)) interval = 5.0;
    if (interval < 3.0) interval = 3.0;           /* max(3, int(...)) */

    if (!oa_truthy(uc) || !oa_truthy(did)) {
        json_set(o, "interval", json_null());
        json_set(o, "ok", json_bool(false));
        json_set(o, "error", json_string("device-code response missing user_code or device_auth_id"));
        return o;
    }
    json_set(o, "interval", json_number(interval));
    json_set(o, "ok", json_bool(true));
    json_set(o, "error", json_null());
    return o;
}

json_t *w29_oauth_dc3549_p2_codex_exchange(const json_t *req) {
    const json_t *code_resp = oa_get(req, "code_resp");
    const json_t *ac = oa_get(code_resp, "authorization_code");
    const json_t *cv = oa_get(code_resp, "code_verifier");
    const char *cid = json_get_str(req, "client_id", "");
    json_t *o = json_object();

    if (!oa_truthy(ac) || !oa_truthy(cv)) {
        json_set(o, "error", json_string("device-auth response missing authorization_code/code_verifier"));
        json_set(o, "data", json_null());
        return o;
    }
    {
        json_t *data = json_object();
        json_set(data, "grant_type", json_string("authorization_code"));
        json_set(data, "code", oa_copy_or_null(ac));
        json_set(data, "redirect_uri", json_string("https://auth.openai.com/deviceauth/callback"));
        json_set(data, "client_id", json_string(cid));
        json_set(data, "code_verifier", oa_copy_or_null(cv));
        json_set(o, "error", json_null());
        json_set(o, "data", data);
    }
    return o;
}

json_t *w29_oauth_dc3549_p2_codex_cancelled(const json_t *sess) {
    return json_bool(oa_truthy(oa_get(sess, "cancelled")));
}

json_t *w29_oauth_dc3549_p2_gc_survivors(const json_t *req) {
    const json_t *now = oa_get(req, "now");
    const json_t *sessions = oa_get(req, "sessions");
    double t = 0.0, cutoff;
    size_t i, n;
    json_t *out = json_array();

    if (oa_truthy(now)) oa_int_of(now, &t);
    cutoff = t - 900.0;                                    /* _OAUTH_SESSION_TTL_SECONDS */
    n = (sessions && sessions->type == JSON_ARRAY) ? json_len(sessions) : 0;
    for (i = 0; i < n; i++) {
        const json_t *sess = json_get(sessions, i);
        const json_t *created = oa_get(sess, "created_at");
        double c = 0.0;
        if (!oa_truthy(created) || !oa_int_of(created, &c)) continue;
        if (c < cutoff) continue;                          /* sess["created_at"] < cutoff */
        json_append(out, oa_copy_or_null(oa_get(sess, "session_id")));
    }
    return out;
}

json_t *w29_oauth_dc3549_p2_device_start(const json_t *req) {
    json_t *sess = json_object();
    json_t *body = json_object();
    json_t *out = json_object();
    const json_t *fields = oa_get(req, "fields");
    const json_t *sid = oa_get(req, "session_id");
    size_t i;

    json_set(sess, "session_id", oa_copy_or_null(sid));
    json_set(sess, "provider", oa_copy_or_null(oa_get(req, "provider_id")));
    json_set(sess, "flow", json_string("device_code"));
    json_set(sess, "profile", w29_oauth_dc3549_p1_profile_name(oa_get(req, "profile")));
    json_set(sess, "created_at", oa_copy_or_null(oa_get(req, "created_at")));
    json_set(sess, "status", json_string("pending"));
    json_set(sess, "error_message", json_null());

    if (oa_is_object(fields)) {
        for (i = 0; i < json_object_size(fields); i++) {
            const char *k = json_object_get_key_at(fields, i);
            if (!k) continue;
            json_set(sess, k, json_copy(json_object_get_at(fields, i)));
        }
    }

    json_set(body, "session_id", oa_copy_or_null(sid));
    json_set(body, "flow", json_string("device_code"));
    json_set(body, "user_code", oa_copy_or_null(oa_get(req, "user_code")));
    json_set(body, "verification_url", oa_copy_or_null(oa_get(req, "verification_url")));
    json_set(body, "expires_in", oa_copy_or_null(oa_get(req, "expires_in")));
    json_set(body, "poll_interval", oa_copy_or_null(oa_get(req, "poll_interval")));

    json_set(out, "session", sess);
    json_set(out, "body", body);
    return out;
}

json_t *w29_oauth_dc3549_p2_validate_profile(const json_t *profile) {
    json_t *name = w29_oauth_dc3549_p1_profile_name(profile);
    json_t *o = json_object();
    json_set(o, "name", name);
    json_set(o, "resolved", json_bool(oa_truthy(name)));
    return o;
}
