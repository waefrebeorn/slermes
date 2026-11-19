/* port_credential_pool_flow.c — FLOW-core port of agent/credential_pool.py.
 *
 * Ports the credential selection / rotation ORCHESTRATION: availability
 * filtering (cooldown / exhaustion / DEAD-prune branches), refresh triggers,
 * failed-identity resolution, bounded unmatched rotation, exhaustion marking,
 * adoption / quarantine mutation order, and the failed-refresh recovery tree.
 * All I/O (clock, store reads/writes, OAuth POSTs, cross-process locks, env,
 * config, logging) is INJECTED through the cp_flow_io_t seam dispatcher; the
 * decision logic around each seam matches the live Python exactly.
 *
 * Seam model mirrors context_compressor_flow / port_scheduler_flow: every live
 * `self.<method>(...)` that performs I/O and every module-level I/O call goes
 * through cp_flow_io_t.call(io, name, args); the dispatcher appends `name` to
 * io->trace, returns the scripted result, applies "state" effects, and models a
 * raised exception with {"__raise__": "<Type>", "message": "..."}. Pure siblings
 * (_is_manual_source, _exhausted_until, _is_sole_credential, _find,
 * _replace_entry, ...) are ported directly and perform no I/O. io->state is a
 * json_t OBJECT mirroring the pool attribute dict (_entries, _current_id,
 * _unmatched_rotation_streak, provider, ...). Entries are json OBJECTS carrying
 * the PooledCredential dataclass fields; ``extra`` rides as a sub-object.
 *
 * Ownership: seam results returned by call are owned by the caller (json_free).
 * io->trace / io->state are owned by the harness. seam() frees its args.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

/* ── live module constants (byte-pinned to agent/credential_pool.py) ───── */
#define CP_STATUS_OK "ok"
#define CP_STATUS_EXHAUSTED "exhausted"
#define CP_STATUS_DEAD "dead"
#define CP_AUTH_TYPE_OAUTH "oauth"
#define CP_AUTH_TYPE_API_KEY "api_key"
#define CP_SOURCE_MANUAL "manual"
#define CP_SOURCE_MANUAL_DEVICE_CODE "manual:device_code"
#define CP_CREDENTIAL_PERSIST_FAILED_REASON "credential_persist_failed"
#define CP_DEAD_MANUAL_PRUNE_TTL_SECONDS (24 * 60 * 60)
#define CP_EXHAUSTED_TTL_401_SECONDS (5 * 60)
#define CP_EXHAUSTED_TTL_429_SECONDS (60 * 60)
#define CP_EXHAUSTED_TTL_DEFAULT_SECONDS (60 * 60)
#define CP_EXHAUSTED_TTL_SOLE_CREDENTIAL_SECONDS 60
#define CP_FAILURE_REASON_BILLING "billing"
#define CP_FAILURE_REASON_BILLING_UNVERIFIED "billing_unverified"
#define CP_REFRESH_SWEEP_SPACING_SECONDS 0.5
#define CP_CODEX_ACCESS_TOKEN_REFRESH_SKEW_SECONDS 120

static const char *CP_TERMINAL_AUTH_REASONS[] = {
    "token_invalidated", "token_revoked", "invalid_token", "invalid_grant",
    "unauthorized_client", "refresh_token_reused", NULL,
};
static const char *CP_TOKENS_SINGLETON_PROVIDERS[] = { "openai-codex", "xai-oauth", NULL };
static const char *CP_SINGLE_USE_REFRESH_PROVIDERS[] = { "openai-codex", "xai-oauth", "anthropic", NULL };

/* _RESYNC_SOURCE map */
static const char *cp_resync_source(const char *provider) {
    if (strcmp(provider, "anthropic") == 0) return "claude_code";
    if (strcmp(provider, "nous") == 0) return "device_code";
    if (strcmp(provider, "openai-codex") == 0) return "device_code";
    if (strcmp(provider, "xai-oauth") == 0) return "device_code";
    return NULL;
}

/* _ANTHROPIC_SOURCE_RANK map */
static int cp_anthropic_source_rank(const char *source) {
    if (strcmp(source, "env:ANTHROPIC_TOKEN") == 0) return 0;
    if (strcmp(source, "env:CLAUDE_CODE_OAUTH_TOKEN") == 0) return 1;
    if (strcmp(source, "hermes_pkce") == 0) return 2;
    if (strcmp(source, "claude_code") == 0) return 3;
    if (strcmp(source, "env:ANTHROPIC_API_KEY") == 0) return 4;
    return 5;  /* len(_ANTHROPIC_SOURCE_RANK) */
}

/* ── generic seam dispatcher + trace + attribute state ─────────────────── */
typedef struct cp_flow_io_t {
    json_t *(*call)(struct cp_flow_io_t *io, const char *name, json_t *args);
    json_t *trace;
    json_t *state;
    void   *ctx;
} cp_flow_io_t;

/* PoP: cp_flow_trace @ agent/credential_pool.py:<seam-trace helper> */
void cp_flow_trace(cp_flow_io_t *io, const char *token) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) return;
    json_append(io->trace, json_string(token));
}

/* PoP: cp_flow_call @ agent/credential_pool.py:<seam dispatcher> */
json_t *cp_flow_call(cp_flow_io_t *io, const char *name, json_t *args) {
    if (io && io->call) return io->call(io, name, args);
    return json_null();
}

/* PoP: cp_flow_raised @ agent/credential_pool.py:<exception sentinel> */
bool cp_flow_raised(const json_t *result) {
    return result && result->type == JSON_OBJECT && json_obj_get(result, "__raise__") != NULL;
}

static json_t *seam(cp_flow_io_t *io, const char *name, json_t *args) {
    cp_flow_trace(io, name);
    json_t *r = cp_flow_call(io, name, args);
    if (args) json_free(args);
    return r ? r : json_null();
}

/* ── Python-faithful helpers ───────────────────────────────────────────── */
static bool cp_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

static int cp_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *cp_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && cp_is_ws((unsigned char)p[a])) a++;
    while (b > a && cp_is_ws((unsigned char)p[b - 1])) b--;
    char *out = malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, p + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *cp_lower(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)p[i]);
    out[n] = '\0';
    return out;
}

static char *cp_rstrip_slash(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    while (n && p[n - 1] == '/') n--;
    char *out = malloc(n + 1);
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

static bool cp_startswith(const char *s, const char *pre) {
    if (!s || !pre) return false;
    return strncmp(s, pre, strlen(pre)) == 0;
}

static char *cp_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return strdup(buf);
    }
    char *ser = json_serialize(v);
    return ser ? ser : strdup("");
}

static json_t *cp_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}

static const char *cp_get_str(const json_t *obj, const char *key, const char *def) {
    json_t *v = cp_get(obj, key);
    if (v && v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return def;
}

/* entry attribute access: dataclass field, then _EXTRA_KEYS via extra. */
static json_t *cp_attr(const json_t *entry, const char *name) {
    json_t *v = cp_get(entry, name);
    if (v) return v;
    json_t *extra = cp_get(entry, "extra");
    return cp_get(extra, name);
}

static const char *cp_attr_str(const json_t *entry, const char *name, const char *def) {
    json_t *v = cp_attr(entry, name);
    if (v && v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return def;
}

/* runtime_api_key for the non-nous subset the harness feeds (str(access_token or "")). */
static char *cp_runtime_api_key(const json_t *entry) {
    json_t *at = cp_get(entry, "access_token");
    return cp_pystr(cp_truthy(at) ? at : NULL);
}

static json_t *cp_state_get(cp_flow_io_t *io, const char *key) { return cp_get(io->state, key); }
static void cp_state_set(cp_flow_io_t *io, const char *key, json_t *val) {
    if (io && io->state && io->state->type == JSON_OBJECT) json_set(io->state, key, val);
    else if (val) json_free(val);
}
static const char *cp_provider(cp_flow_io_t *io) { return cp_get_str(io->state, "provider", ""); }

static void cp_log(cp_flow_io_t *io, const char *level) {
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *r = seam(io, name, NULL);
    json_free(r);
}

/* ── ISO-8601 -> epoch (aware subset) + _parse_absolute_timestamp ──────── */
static int64_t cp_days_from_civil(int64_t y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
static bool cp_read_digits(const char **p, int n, int *val) {
    int v = 0;
    for (int i = 0; i < n; i++) {
        char c = (*p)[i];
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
    }
    *p += n; *val = v;
    return true;
}
static bool cp_parse_iso_epoch(const char *s, double *out) {
    if (!s) return false;
    const char *p = s;
    while (cp_is_ws((unsigned char)*p)) p++;
    int y, mo, d, h, mi, sec;
    if (!cp_read_digits(&p, 4, &y)) return false;
    if (*p != '-') return false; p++;
    if (!cp_read_digits(&p, 2, &mo)) return false;
    if (*p != '-') return false; p++;
    if (!cp_read_digits(&p, 2, &d)) return false;
    if (*p == 'T' || *p == 't' || *p == ' ') p++; else return false;
    if (!cp_read_digits(&p, 2, &h)) return false;
    if (*p != ':') return false; p++;
    if (!cp_read_digits(&p, 2, &mi)) return false;
    if (*p != ':') return false; p++;
    if (!cp_read_digits(&p, 2, &sec)) return false;
    double frac = 0.0;
    if (*p == '.') {
        p++; double scale = 0.1; int digits = 0;
        while (*p >= '0' && *p <= '9') {
            if (digits < 6) { frac += (*p - '0') * scale; scale /= 10.0; }
            digits++; p++;
        }
        if (digits == 0) return false;
    }
    int off_sec = 0;
    if (*p == 'Z' || *p == 'z') p++;
    else if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1; p++;
        int oh, om = 0;
        if (!cp_read_digits(&p, 2, &oh)) return false;
        if (*p == ':') p++;
        if (*p >= '0' && *p <= '9') { if (!cp_read_digits(&p, 2, &om)) return false; }
        off_sec = sign * (oh * 3600 + om * 60);
    }
    while (cp_is_ws((unsigned char)*p)) p++;
    if (*p != '\0') return false;
    int64_t days = cp_days_from_civil(y, mo, d);
    *out = (double)(days * 86400 + h * 3600 + mi * 60 + sec - off_sec) + frac;
    return true;
}

/* Internal port of agent/credential_pool_timestamps.py:_parse_absolute_timestamp
 * (a pure dependency of the credential_pool.py flows; not a claimed gap here). */
static bool cp_parse_absolute_timestamp(const json_t *value, double *out) {
    if (!value || value->type == JSON_NULL) return false;
    if (value->type == JSON_STRING && (!value->str_val || value->str_val[0] == '\0')) return false;
    if (value->type == JSON_NUMBER) {
        double numeric = value->num_val;
        if (numeric <= 0) return false;
        *out = numeric > 1e12 ? numeric / 1000.0 : numeric;
        return true;
    }
    if (value->type == JSON_BOOL) return false;
    if (value->type == JSON_STRING) {
        char *raw = cp_strip(value->str_val);
        if (!raw[0]) { free(raw); return false; }
        char *end = NULL;
        double numeric = strtod(raw, &end);
        if (end != raw && *end == '\0') {
            *out = numeric > 1e12 ? numeric / 1000.0 : numeric;
            free(raw);
            return true;
        }
        bool ok = cp_parse_iso_epoch(raw, out);
        free(raw);
        return ok;
    }
    return false;
}
/* ── pure helpers ──────────────────────────────────────────────────────── */

/* PoP: cp_is_terminal_auth_failure @ agent/credential_pool.py:_is_terminal_auth_failure */
bool cp_is_terminal_auth_failure(long status_code, const json_t *normalized_error) {
    json_t *raw = cp_get(normalized_error, "reason");
    char *reason = NULL;
    if (raw && raw->type == JSON_STRING) {
        char *s = cp_strip(raw->str_val);
        reason = cp_lower(s);
        free(s);
    } else {
        reason = strdup("");
    }
    bool terminal = strcmp(reason, CP_CREDENTIAL_PERSIST_FAILED_REASON) == 0;
    if (!terminal && status_code == 401) {
        for (int i = 0; CP_TERMINAL_AUTH_REASONS[i]; i++) {
            if (strcmp(reason, CP_TERMINAL_AUTH_REASONS[i]) == 0) { terminal = true; break; }
        }
    }
    free(reason);
    return terminal;
}

/* PoP: cp_is_sole_credential @ agent/credential_pool.py:_is_sole_credential */
bool cp_is_sole_credential(const json_t *entries) {
    long count = 0;
    if (entries && entries->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *e = json_get(entries, i);
            const char *st = cp_get_str(e, "last_status", NULL);
            if (!st || strcmp(st, CP_STATUS_DEAD) != 0) count++;
        }
    }
    return count <= 1;
}

/* PoP: cp_next_priority @ agent/credential_pool.py:_next_priority */
long cp_next_priority(const json_t *entries) {
    long max_p = -1;
    if (entries && entries->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *p = cp_get(json_get(entries, i), "priority");
            if (p && p->type == JSON_NUMBER && (long)p->num_val > max_p) max_p = (long)p->num_val;
        }
    }
    return max_p + 1;
}

/* PoP: cp_is_manual_source @ agent/credential_pool.py:_is_manual_source */
bool cp_is_manual_source(const char *source) {
    char *s = cp_strip(source ? source : "");
    char *n = cp_lower(s);
    free(s);
    bool r = strcmp(n, CP_SOURCE_MANUAL) == 0 || cp_startswith(n, "manual:");
    free(n);
    return r;
}

/* PoP: cp_exhausted_ttl @ agent/credential_pool.py:_exhausted_ttl */
long cp_exhausted_ttl(long error_code, bool sole_credential, const char *failure_reason) {
    if (error_code == 401) return CP_EXHAUSTED_TTL_401_SECONDS;
    long base = error_code == 429 ? CP_EXHAUSTED_TTL_429_SECONDS : CP_EXHAUSTED_TTL_DEFAULT_SECONDS;
    const char *fr = failure_reason ? failure_reason : "";
    if (strcmp(fr, CP_FAILURE_REASON_BILLING_UNVERIFIED) == 0 && error_code != 402) {
        return base < CP_EXHAUSTED_TTL_SOLE_CREDENTIAL_SECONDS ? base : CP_EXHAUSTED_TTL_SOLE_CREDENTIAL_SECONDS;
    }
    bool is_billing = error_code == 402 || strcmp(fr, CP_FAILURE_REASON_BILLING) == 0;
    if (sole_credential && !is_billing) {
        return base < CP_EXHAUSTED_TTL_SOLE_CREDENTIAL_SECONDS ? base : CP_EXHAUSTED_TTL_SOLE_CREDENTIAL_SECONDS;
    }
    return base;
}

/* PoP: cp_exhausted_until @ agent/credential_pool.py:_exhausted_until */
json_t *cp_exhausted_until(const json_t *entry, bool sole_credential) {
    const char *st = cp_get_str(entry, "last_status", NULL);
    if (!st || strcmp(st, CP_STATUS_EXHAUSTED) != 0) return json_null();
    double reset_at;
    if (cp_parse_absolute_timestamp(cp_get(entry, "last_error_reset_at"), &reset_at)) {
        return json_number(reset_at);
    }
    json_t *lsa = cp_get(entry, "last_status_at");
    if (cp_truthy(lsa)) {
        double at = lsa->type == JSON_NUMBER ? lsa->num_val : 0.0;
        json_t *codej = cp_get(entry, "last_error_code");
        long code = codej && codej->type == JSON_NUMBER ? (long)codej->num_val : 0;
        const char *fr = cp_attr_str(entry, "failure_reason", NULL);
        return json_number(at + (double)cp_exhausted_ttl(code, sole_credential, fr));
    }
    return json_null();
}

/* PoP: cp_singleton_predates_entry @ agent/credential_pool.py:_singleton_predates_entry */
bool cp_singleton_predates_entry(const json_t *state, const json_t *entry) {
    double entry_ts;
    if (!cp_parse_absolute_timestamp(cp_get(entry, "last_refresh"), &entry_ts)) return false;
    json_t *lr = (state && state->type == JSON_OBJECT) ? cp_get(state, "last_refresh") : NULL;
    double state_ts;
    if (!cp_parse_absolute_timestamp(lr, &state_ts)) return false;
    return state_ts < entry_ts;
}

/* PoP: cp_codex_principal_identity @ agent/credential_pool.py:_codex_principal_identity */
json_t *cp_codex_principal_identity(cp_flow_io_t *io, const json_t *access_token) {
    json_t *a = json_object();
    json_set(a, "token", access_token ? json_copy(access_token) : json_null());
    json_t *claims = seam(io, "_decode_jwt_claims", a);
    json_t *out = json_null();
    if (claims && claims->type == JSON_OBJECT) {
        json_t *auth = cp_get(claims, "https://api.openai.com/auth");
        json_t *acct = (auth && auth->type == JSON_OBJECT) ? cp_get(auth, "chatgpt_account_id") : NULL;
        json_t *sub = cp_get(claims, "sub");
        if (acct && acct->type == JSON_STRING && sub && sub->type == JSON_STRING) {
            char *as = cp_strip(acct->str_val);
            char *ss = cp_strip(sub->str_val);
            if (as[0] && ss[0]) {
                out = json_array();
                json_append(out, json_string(as));
                json_append(out, json_string(ss));
            }
            free(as); free(ss);
        }
    }
    json_free(claims);
    return out;
}

static bool cp_json_equal(const json_t *a, const json_t *b) {
    if (!a || !b) return a == b;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL: return true;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (json_len(a) != json_len(b)) return false;
            for (size_t i = 0; i < json_len(a); i++)
                if (!cp_json_equal(json_get(a, i), json_get(b, i))) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = cp_get(b, a->c.keys[i]);
                if (!bv || !cp_json_equal(a->c.items[i], bv)) return false;
            }
            return true;
    }
    return false;
}

/* PoP: cp_codex_entry_tracks_singleton @ agent/credential_pool.py:_codex_entry_tracks_singleton */
bool cp_codex_entry_tracks_singleton(cp_flow_io_t *io, const json_t *entry, const json_t *singleton_tokens) {
    const char *source = cp_get_str(entry, "source", "");
    if (strcmp(source, "device_code") == 0) return true;
    if (strcmp(source, CP_SOURCE_MANUAL_DEVICE_CODE) != 0) return false;
    json_t *entry_id = cp_codex_principal_identity(io, cp_get(entry, "access_token"));
    if (!entry_id || entry_id->type == JSON_NULL) { json_free(entry_id); return false; }
    json_t *st = cp_get(singleton_tokens, "access_token");
    json_t *single_id = cp_codex_principal_identity(io, st);
    bool r = single_id && single_id->type != JSON_NULL && cp_json_equal(entry_id, single_id);
    json_free(entry_id);
    json_free(single_id);
    return r;
}

/* PoP: cp_norm_url @ agent/credential_pool.py:_norm_url */
char *cp_norm_url(const json_t *url) {
    char *s = cp_pystr(cp_truthy(url) ? url : NULL);
    char *t = cp_strip(s);
    free(s);
    char *r = cp_rstrip_slash(t);
    free(t);
    return r;
}

/* PoP: cp_normalize_custom_pool_name @ agent/credential_pool.py:_normalize_custom_pool_name */
char *cp_normalize_custom_pool_name(const char *name) {
    char *s = cp_strip(name ? name : "");
    char *l = cp_lower(s);
    free(s);
    for (char *p = l; *p; p++) if (*p == ' ') *p = '-';
    return l;
}

/* PoP: cp_store_owns_pool_provider @ agent/credential_pool.py:_store_owns_pool_provider */
bool cp_store_owns_pool_provider(const json_t *auth_store, const char *provider) {
    json_t *pool = cp_get(auth_store, "credential_pool");
    json_t *entries = (pool && pool->type == JSON_OBJECT) ? cp_get(pool, provider) : NULL;
    return entries && entries->type == JSON_ARRAY && json_len(entries) > 0;
}

/* PoP: cp_retain_sources_not_in @ agent/credential_pool.py:_retain_sources_not_in */
bool cp_retain_sources_not_in(json_t *entries, const json_t *drop) {
    if (!entries || entries->type != JSON_ARRAY) return false;
    json_t *retained = json_array();
    for (size_t i = 0; i < json_len(entries); i++) {
        json_t *e = json_get(entries, i);
        const char *src = cp_get_str(e, "source", "");
        bool in_drop = false;
        if (drop && drop->type == JSON_ARRAY) {
            for (size_t k = 0; k < json_len(drop); k++) {
                json_t *d = json_get(drop, k);
                if (d && d->type == JSON_STRING && strcmp(d->str_val, src) == 0) { in_drop = true; break; }
            }
        }
        if (!in_drop) json_append(retained, json_copy(e));
    }
    bool changed = json_len(retained) != json_len(entries);
    if (changed) {
        /* entries[:] = retained — replace contents in place */
        for (size_t i = 0; i < entries->c.count; i++) json_free(entries->c.items[i]);
        free(entries->c.items);
        entries->c.items = NULL;
        entries->c.count = 0;
        for (size_t i = 0; i < json_len(retained); i++) json_append(entries, json_copy(json_get(retained, i)));
    }
    json_free(retained);
    return changed;
}

/* PoP: cp_normalize_pool_priorities @ agent/credential_pool.py:_normalize_pool_priorities */
bool cp_normalize_pool_priorities(const char *provider, json_t *entries) {
    if (strcmp(provider, "anthropic") != 0) return false;
    if (!entries || entries->type != JSON_ARRAY) return false;
    size_t n = json_len(entries);
    /* build index arrays for manual / seeded */
    size_t *manual = malloc(n * sizeof(size_t));
    size_t *seeded = malloc(n * sizeof(size_t));
    size_t nm = 0, ns = 0;
    for (size_t i = 0; i < n; i++) {
        const char *src = cp_get_str(json_get(entries, i), "source", "");
        if (cp_is_manual_source(src)) manual[nm++] = i;
        else seeded[ns++] = i;
    }
    /* stable sort manual by priority */
    for (size_t i = 1; i < nm; i++) {
        size_t key = manual[i];
        long kp = (long)json_get_num(json_get(entries, key), "priority", 0);
        size_t j = i;
        while (j > 0) {
            long pj = (long)json_get_num(json_get(entries, manual[j - 1]), "priority", 0);
            if (pj <= kp) break;
            manual[j] = manual[j - 1];
            j--;
        }
        manual[j] = key;
    }
    /* stable sort seeded by (rank, priority, label) */
    for (size_t i = 1; i < ns; i++) {
        size_t key = seeded[i];
        json_t *ke = json_get(entries, key);
        long kr = cp_anthropic_source_rank(cp_get_str(ke, "source", ""));
        long kp = (long)json_get_num(ke, "priority", 0);
        const char *kl = cp_get_str(ke, "label", "");
        size_t j = i;
        while (j > 0) {
            json_t *pe = json_get(entries, seeded[j - 1]);
            long pr = cp_anthropic_source_rank(cp_get_str(pe, "source", ""));
            long pp = (long)json_get_num(pe, "priority", 0);
            const char *pl = cp_get_str(pe, "label", "");
            bool greater = pr > kr || (pr == kr && pp > kp) ||
                           (pr == kr && pp == kp && strcmp(pl, kl) > 0);
            if (!greater) break;
            seeded[j] = seeded[j - 1];
            j--;
        }
        seeded[j] = key;
    }
    bool changed = false;
    long new_priority = 0;
    for (size_t i = 0; i < nm; i++, new_priority++) {
        json_t *e = json_get(entries, manual[i]);
        if ((long)json_get_num(e, "priority", 0) != new_priority) {
            json_set(e, "priority", json_number((double)new_priority));
            changed = true;
        }
    }
    for (size_t i = 0; i < ns; i++, new_priority++) {
        json_t *e = json_get(entries, seeded[i]);
        if ((long)json_get_num(e, "priority", 0) != new_priority) {
            json_set(e, "priority", json_number((double)new_priority));
            changed = true;
        }
    }
    free(manual);
    free(seeded);
    return changed;
}

/* PoP: cp_prune_stale_seeded_entries @ agent/credential_pool.py:_prune_stale_seeded_entries */
bool cp_prune_stale_seeded_entries(cp_flow_io_t *io, json_t *entries,
                                  const json_t *active_sources, bool prune_env_sources) {
    if (!entries || entries->type != JSON_ARRAY) return false;
    json_t *retained = json_array();
    for (size_t i = 0; i < json_len(entries); i++) {
        json_t *e = json_get(entries, i);
        const char *src = cp_get_str(e, "source", "");
        bool keep = cp_is_manual_source(src);
        if (!keep && active_sources && active_sources->type == JSON_ARRAY) {
            for (size_t k = 0; k < json_len(active_sources); k++) {
                json_t *a = json_get(active_sources, k);
                if (a && a->type == JSON_STRING && strcmp(a->str_val, src) == 0) { keep = true; break; }
            }
        }
        if (!keep) {
            bool prunable;
            if (cp_startswith(src, "env:")) {
                prunable = prune_env_sources;
            } else if (strcmp(src, "hermes_pkce") == 0) {
                prunable = true;
            } else {
                json_t *a = json_object();
                json_set(a, "source", json_string(src));
                json_set(a, "provider", json_string(cp_get_str(e, "provider", cp_provider(io))));
                json_t *r = seam(io, "is_borrowed_credential_source", a);
                prunable = cp_truthy(r);
                json_free(r);
            }
            keep = !prunable;
        }
        if (keep) json_append(retained, json_copy(e));
    }
    bool changed = json_len(retained) != json_len(entries);
    if (changed) {
        for (size_t i = 0; i < entries->c.count; i++) json_free(entries->c.items[i]);
        free(entries->c.items);
        entries->c.items = NULL;
        entries->c.count = 0;
        for (size_t i = 0; i < json_len(retained); i++) json_append(entries, json_copy(json_get(retained, i)));
    }
    json_free(retained);
    return changed;
}

/* PoP: cp_get_env_prefer_dotenv @ agent/credential_pool.py:get_env_prefer_dotenv */
char *cp_get_env_prefer_dotenv(cp_flow_io_t *io, const char *key) {
    json_t *env_file = seam(io, "load_env", NULL);
    json_t *rv = cp_get(env_file, key);
    char *raw = cp_strip(rv && rv->type == JSON_STRING ? rv->str_val : "");
    json_free(env_file);
    json_t *sa = json_object();
    json_set(sa, "key", json_string(key));
    json_set(sa, "default", json_string(""));
    json_t *sv = seam(io, "_get_secret", sa);
    char *scoped = cp_strip(cp_truthy(sv) && sv->type == JSON_STRING ? sv->str_val : "");
    json_free(sv);
    char *ret;
    if (cp_startswith(raw, "op://") && scoped[0]) {
        ret = strdup(scoped);
    } else {
        ret = strdup(raw[0] ? raw : scoped);
    }
    free(raw);
    free(scoped);
    return ret;
}

/* PoP: cp_env_payload @ agent/credential_pool.py:_env_payload */
json_t *cp_env_payload(cp_flow_io_t *io, const char *env_var, const char *token, const char *base_url) {
    json_t *payload = json_object();
    json_set(payload, "auth_type", json_string(CP_AUTH_TYPE_API_KEY));
    json_set(payload, "access_token", json_string(token ? token : ""));
    json_set(payload, "base_url", json_string(base_url ? base_url : ""));
    json_set(payload, "label", json_string(env_var ? env_var : ""));
    json_t *a = json_object();
    json_set(a, "env_var", json_string(env_var ? env_var : ""));
    json_t *sl = seam(io, "get_secret_source", a);
    char *secret_source = NULL;
    if (!cp_flow_raised(sl) && cp_truthy(sl)) {
        char *s = cp_pystr(sl);
        secret_source = cp_strip(s);
        free(s);
    }
    json_free(sl);
    if (secret_source && secret_source[0]) {
        json_set(payload, "secret_source", json_string(secret_source));
    }
    free(secret_source);
    return payload;
}

/* PoP: cp_env_key_var_candidates @ agent/credential_pool.py:_env_key_var_candidates */
json_t *cp_env_key_var_candidates(cp_flow_io_t *io, const json_t *env_vars, const json_t *entries) {
    json_t *names = json_array();
    if (env_vars && env_vars->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(env_vars); i++) json_append(names, json_copy(json_get(env_vars, i)));
        for (size_t i = 0; i < json_len(env_vars); i++) {
            const char *base = json_get(env_vars, i)->str_val;
            long n = 2;
            for (;;) {
                char *var = NULL;
                asprintf(&var, "%s_%ld", base, n);
                char *val = cp_get_env_prefer_dotenv(io, var);
                bool has = val && val[0];
                free(val);
                if (!has) { free(var); break; }
                json_append(names, json_string(var));
                free(var);
                n++;
            }
        }
    }
    if (entries && entries->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(entries); i++) {
            const char *src = cp_get_str(json_get(entries, i), "source", "");
            if (cp_startswith(src, "env:")) {
                const char *colon = strchr(src, ':');
                char *env_name = cp_strip(colon ? colon + 1 : "");
                if (env_name[0]) {
                    bool present = false;
                    for (size_t k = 0; k < json_len(names); k++) {
                        if (strcmp(json_get(names, k)->str_val, env_name) == 0) { present = true; break; }
                    }
                    if (!present) json_append(names, json_string(env_name));
                }
                free(env_name);
            }
        }
    }
    return names;
}
/* ── mutation primitives (pure over state) ─────────────────────────────── */

static json_t *cp_entries(cp_flow_io_t *io) {
    json_t *e = cp_state_get(io, "_entries");
    return (e && e->type == JSON_ARRAY) ? e : NULL;
}

/* PoP: cp_find @ agent/credential_pool.py:_find */
json_t *cp_find(cp_flow_io_t *io, const json_t *match) {
    json_t *entries = cp_entries(io);
    if (!entries) return NULL;
    const char *id = cp_get_str(match, "id", NULL);
    const char *key = cp_get_str(match, "runtime_api_key", NULL);
    for (size_t i = 0; i < json_len(entries); i++) {
        json_t *e = json_get(entries, i);
        if (id) {
            if (strcmp(cp_get_str(e, "id", ""), id) == 0) return e;
        } else if (key) {
            char *rk = cp_runtime_api_key(e);
            bool m = strcmp(rk, key) == 0;
            free(rk);
            if (m) return e;
        }
    }
    return NULL;
}

/* PoP: cp_replace_entry @ agent/credential_pool.py:_replace_entry */
void cp_replace_entry(cp_flow_io_t *io, const char *old_id, const json_t *new_entry) {
    json_t *entries = cp_entries(io);
    if (!entries) return;
    for (size_t i = 0; i < json_len(entries); i++) {
        json_t *e = json_get(entries, i);
        if (strcmp(cp_get_str(e, "id", ""), old_id) == 0) {
            json_free(entries->c.items[i]);
            entries->c.items[i] = json_copy(new_entry);
            return;
        }
    }
}

static json_t *cp_merge(const json_t *entry, const json_t *updates) {
    json_t *out = json_copy(entry ? entry : json_object());
    if (updates && updates->type == JSON_OBJECT) {
        for (size_t i = 0; i < updates->c.count; i++)
            json_set(out, updates->c.keys[i], json_copy(updates->c.items[i]));
    }
    return out;
}

/* PoP: cp_adopt @ agent/credential_pool.py:_adopt */
json_t *cp_adopt(cp_flow_io_t *io, const json_t *entry, const json_t *updates, bool persist) {
    json_t *updated = cp_merge(entry, updates);
    const char *id = cp_get_str(entry, "id", "");
    cp_replace_entry(io, id, updated);
    if (persist) {
        json_t *r = seam(io, "_persist", NULL);
        json_free(r);
        json_t *match = json_object();
        json_set(match, "id", json_string(id));
        json_t *live = cp_find(io, match);
        json_free(match);
        if (live) {
            json_free(updated);
            return json_copy(live);
        }
    }
    return updated;
}

/* PoP: cp_quarantine_sources @ agent/credential_pool.py:_quarantine_sources */
void cp_quarantine_sources(cp_flow_io_t *io, const json_t *entry, const json_t *sources) {
    json_t *entries = cp_entries(io);
    json_t *removed_ids = json_array();
    json_t *retained = json_array();
    if (entries) {
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *e = json_get(entries, i);
            const char *src = cp_get_str(e, "source", "");
            bool in_sources = false;
            if (sources && sources->type == JSON_ARRAY) {
                for (size_t k = 0; k < json_len(sources); k++) {
                    json_t *s = json_get(sources, k);
                    if (s && s->type == JSON_STRING && strcmp(s->str_val, src) == 0) { in_sources = true; break; }
                }
            }
            if (in_sources) json_append(removed_ids, json_string(cp_get_str(e, "id", "")));
            else json_append(retained, json_copy(e));
        }
        for (size_t i = 0; i < entries->c.count; i++) json_free(entries->c.items[i]);
        free(entries->c.items);
        entries->c.items = NULL;
        entries->c.count = 0;
        for (size_t i = 0; i < json_len(retained); i++) json_append(entries, json_copy(json_get(retained, i)));
    }
    json_free(retained);
    const char *cur = cp_get_str(io->state, "_current_id", NULL);
    if (cur && strcmp(cur, cp_get_str(entry, "id", "")) == 0) {
        cp_state_set(io, "_current_id", json_null());
    }
    json_t *a = json_object();
    json_set(a, "removed_ids", removed_ids);
    json_t *r = seam(io, "_persist", a);
    json_free(r);
}

/* ── exhaustion marking ────────────────────────────────────────────────── */

/* PoP: cp_mark_exhausted @ agent/credential_pool.py:_mark_exhausted */
json_t *cp_mark_exhausted(cp_flow_io_t *io, const json_t *entry, long status_code,
                          const json_t *error_context, bool persist, const char *failure_reason) {
    json_t *na = json_object();
    json_set(na, "error_context", error_context ? json_copy(error_context) : json_null());
    json_t *secrets = json_array();
    json_append(secrets, json_copy(cp_get(entry, "access_token")));
    json_append(secrets, json_copy(cp_get(entry, "refresh_token")));
    json_append(secrets, json_copy(cp_get(entry, "agent_key")));
    json_set(na, "known_secrets", secrets);
    json_t *normalized = seam(io, "_normalize_error_context", na);
    const char *last_status = cp_get_str(entry, "last_status", NULL);
    bool terminal = (last_status && strcmp(last_status, CP_STATUS_DEAD) == 0) ||
                    cp_is_terminal_auth_failure(status_code, normalized);
    json_t *updated_extra = json_copy(cp_truthy(cp_get(entry, "extra")) ? cp_get(entry, "extra") : json_object());
    if (failure_reason && *failure_reason) {
        json_set(updated_extra, "failure_reason", json_string(failure_reason));
    } else {
        json_obj_del(updated_extra, "failure_reason");
    }
    json_t *now = seam(io, "time.time", NULL);
    json_t *updates = json_object();
    json_set(updates, "last_status", json_string(terminal ? CP_STATUS_DEAD : CP_STATUS_EXHAUSTED));
    json_set(updates, "last_status_at", now);
    json_set(updates, "last_error_code", status_code ? json_number((double)status_code) : json_null());
    json_set(updates, "last_error_reason", json_copy(cp_get(normalized, "reason")));
    json_set(updates, "last_error_message", json_copy(cp_get(normalized, "message")));
    json_set(updates, "last_error_reset_at", json_copy(cp_get(normalized, "reset_at")));
    json_set(updates, "extra", updated_extra);
    json_free(normalized);
    json_t *adopted = cp_adopt(io, entry, updates, persist);
    json_free(updates);
    return adopted;
}

/* ── selection core ────────────────────────────────────────────────────── */

static bool cp_provider_in(const char *provider, const char *const *list) {
    for (int i = 0; list[i]; i++) if (strcmp(provider, list[i]) == 0) return true;
    return false;
}

/* PoP: cp_available_entries @ agent/credential_pool.py:_available_entries */
json_t *cp_available_entries(cp_flow_io_t *io, bool clear_expired, bool refresh, const char *model) {
    json_t *nowj = seam(io, "time.time", NULL);
    double now = nowj && nowj->type == JSON_NUMBER ? nowj->num_val : 0.0;
    json_free(nowj);
    bool cleared_any = false;
    json_t *entries_to_prune = json_array();
    json_t *available = json_array();
    json_t *pending_refresh = json_array();
    json_t *entries = cp_entries(io);
    bool sole = cp_is_sole_credential(entries);
    const char *provider = cp_provider(io);
    size_t n = entries ? json_len(entries) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *entry = json_copy(json_get(entries, i));
        const char *auth_type = cp_get_str(entry, "auth_type", "");
        if (strcmp(auth_type, CP_AUTH_TYPE_API_KEY) == 0) {
            char *rk = cp_runtime_api_key(entry);
            bool empty = !rk[0];
            free(rk);
            if (empty) { json_free(entry); continue; }
        }
        json_t *ra = json_object();
        json_set(ra, "entry", json_copy(entry));
        json_t *synced = seam(io, "_resync_stale_entry", ra);
        /* null script => unchanged: the live returns the same entry object */
        if (!synced || synced->type == JSON_NULL) {
            json_free(synced);
            synced = json_copy(entry);
        }
        if (!cp_json_equal(synced, entry)) {
            json_free(entry);
            entry = synced;
            cleared_any = true;
        } else {
            json_free(synced);
        }
        const char *last_status = cp_get_str(entry, "last_status", NULL);
        if (last_status && strcmp(last_status, CP_STATUS_DEAD) == 0) {
            if (cp_is_manual_source(cp_get_str(entry, "source", ""))) {
                json_t *da = cp_get(entry, "last_status_at");
                double dead_at = cp_truthy(da) && da->type == JSON_NUMBER ? da->num_val : 0.0;
                if (dead_at && now - dead_at > CP_DEAD_MANUAL_PRUNE_TTL_SECONDS) {
                    cp_log(io, "warning");
                    json_append(entries_to_prune, json_string(cp_get_str(entry, "id", "")));
                    cleared_any = true;
                }
            }
            json_free(entry);
            continue;
        }
        json_t *ma = json_object();
        json_set(ma, "entry", json_copy(entry));
        json_set(ma, "model", model ? json_string(model) : json_null());
        json_t *mc = seam(io, "model_cooldown_until", ma);
        bool blocked = mc && mc->type != JSON_NULL;
        json_free(mc);
        if (blocked) { json_free(entry); continue; }
        if (last_status && strcmp(last_status, CP_STATUS_EXHAUSTED) == 0) {
            json_t *until = cp_exhausted_until(entry, sole);
            bool cooldown_active = until && until->type != JSON_NULL && now < until->num_val;
            bool lift = false;
            if (cooldown_active && clear_expired) {
                json_t *qa = json_object();
                json_set(qa, "entry", json_copy(entry));
                json_t *q = seam(io, "_codex_quota_restored_upstream", qa);
                lift = cp_truthy(q);
                json_free(q);
            }
            json_free(until);
            if (cooldown_active && !lift) { json_free(entry); continue; }
            if (clear_expired) {
                json_t *fa = json_object();
                json_set(fa, "id", json_string(cp_get_str(entry, "id", "")));
                json_t *found = seam(io, "_find", fa);
                if (found && found->type != JSON_NULL) {
                    json_free(entry);
                    entry = found;
                } else {
                    json_free(found);
                }
                json_t *aa = json_object();
                json_set(aa, "entry", json_copy(entry));
                json_t *updates = json_object();
                json_set(updates, "last_status", json_string(CP_STATUS_OK));
                json_set(updates, "last_status_at", json_null());
                json_set(updates, "last_error_code", json_null());
                json_set(updates, "last_error_reason", json_null());
                json_set(updates, "last_error_message", json_null());
                json_set(updates, "last_error_reset_at", json_null());
                json_set(aa, "updates", updates);
                json_set(aa, "persist", json_bool(false));
                json_t *adopted = seam(io, "_adopt", aa);
                json_free(entry);
                entry = adopted;
                cleared_any = true;
            }
        }
        if (refresh) {
            json_t *ea = json_object();
            json_set(ea, "entry", json_copy(entry));
            json_t *needs = seam(io, "_entry_needs_refresh", ea);
            bool need = cp_truthy(needs);
            json_free(needs);
            if (need) {
                if (cp_provider_in(provider, CP_TOKENS_SINGLETON_PROVIDERS)) {
                    json_append(pending_refresh, json_string(cp_get_str(entry, "id", "")));
                    json_free(entry);
                    continue;
                }
                json_t *rfa = json_object();
                json_set(rfa, "entry", json_copy(entry));
                json_set(rfa, "force", json_bool(false));
                json_t *refreshed = seam(io, "_refresh_entry", rfa);
                if (!refreshed || refreshed->type == JSON_NULL) {
                    json_free(refreshed);
                    json_free(entry);
                    continue;
                }
                json_free(entry);
                entry = refreshed;
            }
        }
        if (strcmp(auth_type, CP_AUTH_TYPE_OAUTH) == 0) {
            json_t *at = cp_get(entry, "access_token");
            char *t = cp_strip(cp_truthy(at) && at->type == JSON_STRING ? at->str_val : "");
            bool empty = !t[0];
            free(t);
            if (empty) { json_free(entry); continue; }
        }
        json_append(available, json_string(cp_get_str(entry, "id", "")));
        json_free(entry);
    }
    if (json_len(entries_to_prune) > 0 && entries) {
        json_t *retained = json_array();
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *e = json_get(entries, i);
            const char *id = cp_get_str(e, "id", "");
            bool pruned = false;
            for (size_t k = 0; k < json_len(entries_to_prune); k++) {
                if (strcmp(json_get(entries_to_prune, k)->str_val, id) == 0) { pruned = true; break; }
            }
            if (!pruned) json_append(retained, json_copy(e));
        }
        for (size_t i = 0; i < entries->c.count; i++) json_free(entries->c.items[i]);
        free(entries->c.items);
        entries->c.items = NULL;
        entries->c.count = 0;
        for (size_t i = 0; i < json_len(retained); i++) json_append(entries, json_copy(json_get(retained, i)));
        json_free(retained);
    }
    if (cleared_any) {
        json_t *pa = json_object();
        json_set(pa, "removed_ids", json_copy(entries_to_prune));
        json_t *r = seam(io, "_persist", pa);
        json_free(r);
    }
    json_t *out = json_object();
    json_set(out, "available", available);
    json_set(out, "pending", pending_refresh);
    json_free(entries_to_prune);
    return out;
}

/* PoP: cp_lift_reopened_cooldowns @ agent/credential_pool.py:lift_reopened_cooldowns */
bool cp_lift_reopened_cooldowns(cp_flow_io_t *io, const char *model) {
    json_t *r = cp_available_entries(io, true, false, model);
    json_t *avail = cp_get(r, "available");
    bool has = avail && avail->type == JSON_ARRAY && json_len(avail) > 0;
    json_free(r);
    return has;
}

/* PoP: cp_reclaim @ agent/credential_pool.py:reclaim */
json_t *cp_reclaim(cp_flow_io_t *io, const char *credential_id, const char *model) {
    json_t *r1 = cp_available_entries(io, true, true, model);
    json_t *pending = cp_get(r1, "pending");
    bool in_pending = false;
    if (pending && pending->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(pending); i++)
            if (strcmp(json_get(pending, i)->str_val, credential_id) == 0) { in_pending = true; break; }
    }
    json_t *avail = json_copy(cp_get(r1, "available"));
    json_free(r1);
    if (in_pending) {
        json_t *pa = json_object();
        json_t *ids = json_array();
        json_append(ids, json_string(credential_id));
        json_set(pa, "ids", ids);
        json_t *pr = seam(io, "_refresh_pending_entries", pa);
        json_free(pr);
        json_t *r2 = cp_available_entries(io, true, true, model);
        json_free(avail);
        avail = json_copy(cp_get(r2, "available"));
        json_free(r2);
    }
    json_t *ret = json_null();
    if (avail && avail->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(avail); i++) {
            if (strcmp(json_get(avail, i)->str_val, credential_id) == 0) {
                ret = json_string(credential_id);
                break;
            }
        }
    }
    json_free(avail);
    return ret;
}

/* ── rotation ──────────────────────────────────────────────────────────── */

/* PoP: cp_identify_failed_entry @ agent/credential_pool.py:_identify_failed_entry */
json_t *cp_identify_failed_entry(cp_flow_io_t *io, const char *credential_id, const char *api_key_hint) {
    json_t *entry = NULL;
    if (credential_id && *credential_id) {
        json_t *m = json_object();
        json_set(m, "id", json_string(credential_id));
        entry = cp_find(io, m);
        json_free(m);
        if (entry && api_key_hint && *api_key_hint) {
            char *rk = cp_runtime_api_key(entry);
            bool mismatch = strcmp(rk, api_key_hint) != 0;
            free(rk);
            if (mismatch) {
                json_t *hm = json_object();
                json_set(hm, "runtime_api_key", json_string(api_key_hint));
                json_t *hint_entry = cp_find(io, hm);
                json_free(hm);
                if (hint_entry) cp_log(io, "info");
                entry = hint_entry;
            }
        }
    }
    if (!entry && api_key_hint && *api_key_hint) {
        json_t *hm = json_object();
        json_set(hm, "runtime_api_key", json_string(api_key_hint));
        entry = cp_find(io, hm);
        json_free(hm);
    }
    return entry ? json_string(cp_get_str(entry, "id", "")) : json_null();
}

/* PoP: cp_rotate_unmatched @ agent/credential_pool.py:_rotate_unmatched */
json_t *cp_rotate_unmatched(cp_flow_io_t *io) {
    long streak = (long)json_get_num(io->state, "_unmatched_rotation_streak", 0) + 1;
    cp_state_set(io, "_unmatched_rotation_streak", json_number((double)streak));
    json_t *av = cp_available_entries(io, false, false, NULL);
    long available_count = (long)json_len(cp_get(av, "available"));
    json_free(av);
    long cap = available_count > 1 ? available_count : 1;
    if (streak > cap) {
        cp_log(io, "warning");
        cp_state_set(io, "_unmatched_rotation_streak", json_number(0));
        cp_state_set(io, "_current_id", json_null());
        return json_null();
    }
    cp_log(io, "info");
    cp_state_set(io, "_current_id", json_null());
    json_t *sa = json_object();
    json_set(sa, "refresh", json_bool(false));
    json_t *sel = seam(io, "_select_unlocked", sa);
    /* _select_unlocked returns (next_entry, pending) */
    json_t *next = (sel && sel->type == JSON_ARRAY && json_len(sel) >= 1) ? json_copy(json_get(sel, 0)) : json_null();
    json_free(sel);
    json_t *av2 = cp_available_entries(io, false, false, NULL);
    long count2 = (long)json_len(cp_get(av2, "available"));
    json_free(av2);
    if (next && next->type != JSON_NULL && count2 == 1) {
        cp_state_set(io, "_unmatched_rotation_streak", json_number(0));
        cp_state_set(io, "_current_id", json_null());
        json_free(next);
        return json_null();
    }
    return next;
}

/* ── refresh trigger / recovery ────────────────────────────────────────── */

/* PoP: cp_entry_needs_refresh @ agent/credential_pool.py:_entry_needs_refresh */
bool cp_entry_needs_refresh(cp_flow_io_t *io, const json_t *entry) {
    if (strcmp(cp_get_str(entry, "auth_type", ""), CP_AUTH_TYPE_OAUTH) != 0) return false;
    const char *provider = cp_provider(io);
    bool plugin = false;
    if (strcmp(provider, "anthropic") != 0) {
        json_t *pa = json_object();
        json_set(pa, "provider", json_string(provider));
        json_t *hook = seam(io, "plugin_refresh_hook", pa);
        plugin = hook && hook->type != JSON_NULL;
        json_free(hook);
    }
    if (strcmp(provider, "anthropic") == 0 || plugin) {
        json_t *a = json_object();
        json_set(a, "entry", json_copy(entry));
        json_t *r = seam(io, "plugin_row_is_expiring", a);
        bool b = cp_truthy(r);
        json_free(r);
        return b;
    }
    if (strcmp(provider, "openai-codex") == 0) {
        json_t *a = json_object();
        json_set(a, "token", json_copy(cp_get(entry, "access_token")));
        json_set(a, "skew", json_number(CP_CODEX_ACCESS_TOKEN_REFRESH_SKEW_SECONDS));
        json_t *r = seam(io, "_codex_access_token_is_expiring", a);
        bool b = cp_truthy(r);
        json_free(r);
        return b;
    }
    if (strcmp(provider, "xai-oauth") == 0) {
        json_t *ska = json_object();
        json_set(ska, "token", json_copy(cp_get(entry, "access_token")));
        json_t *skew = seam(io, "_xai_proactive_refresh_skew_seconds", ska);
        json_t *a = json_object();
        json_set(a, "token", json_copy(cp_get(entry, "access_token")));
        json_set(a, "skew", skew);
        json_t *r = seam(io, "_xai_access_token_is_expiring", a);
        bool b = cp_truthy(r);
        json_free(r);
        return b;
    }
    return false;
}

/* PoP: cp_reset_cleared_after @ agent/credential_pool.py:_reset_cleared_after */
json_t *cp_reset_cleared_after(cp_flow_io_t *io, const json_t *entry) {
    json_t *a = json_object();
    json_set(a, "provider", json_string(cp_provider(io)));
    json_t *rows = seam(io, "read_credential_pool", a);
    if (cp_flow_raised(rows)) {
        cp_log(io, "debug");
        json_free(rows);
        return json_null();
    }
    json_t *row = NULL;
    if (rows && rows->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(rows); i++) {
            json_t *p = json_get(rows, i);
            if (p && p->type == JSON_OBJECT &&
                strcmp(cp_get_str(p, "id", ""), cp_get_str(entry, "id", "")) == 0) {
                row = p;
                break;
            }
        }
    }
    double cleared;
    bool ok = cp_parse_absolute_timestamp(cp_get(row, "status_cleared_at"), &cleared);
    json_free(rows);
    if (!ok) return json_null();
    json_t *lsa = cp_get(entry, "last_status_at");
    double last = cp_truthy(lsa) && lsa->type == JSON_NUMBER ? lsa->num_val : 0.0;
    if (cleared && cleared > last) return json_number(cleared);
    return json_null();
}

/* PoP: cp_resync_stale_entry @ agent/credential_pool.py:_resync_stale_entry */
json_t *cp_resync_stale_entry(cp_flow_io_t *io, const json_t *entry) {
    const char *st = cp_get_str(entry, "last_status", NULL);
    if (!st || (strcmp(st, CP_STATUS_EXHAUSTED) != 0 && strcmp(st, CP_STATUS_DEAD) != 0)) {
        return json_copy(entry);
    }
    json_t *cleared = cp_reset_cleared_after(io, entry);
    if (cleared && cleared->type != JSON_NULL) {
        json_t *updates = json_object();
        json_set(updates, "last_status", json_string(CP_STATUS_OK));
        json_set(updates, "last_status_at", json_null());
        json_set(updates, "last_error_code", json_null());
        json_set(updates, "last_error_reason", json_null());
        json_set(updates, "last_error_message", json_null());
        json_set(updates, "last_error_reset_at", json_null());
        json_set(updates, "status_cleared_at", cleared);
        json_t *adopted = cp_adopt(io, entry, updates, false);
        json_free(updates);
        return adopted;
    }
    json_free(cleared);
    const char *rs = cp_resync_source(cp_provider(io));
    if (!rs || strcmp(cp_get_str(entry, "source", ""), rs) != 0) return json_copy(entry);
    const char *provider = cp_provider(io);
    json_t *a = json_object();
    json_set(a, "entry", json_copy(entry));
    if (strcmp(provider, "anthropic") == 0) return seam(io, "_sync_anthropic_entry_from_credentials_file", a);
    if (strcmp(provider, "nous") == 0) return seam(io, "_sync_nous_entry_from_auth_store", a);
    return seam(io, "_sync_entry_from_auth_store", a);
}

/* PoP: cp_flow_refresh_pending_entries @ agent/credential_pool.py:_refresh_pending_entries */
void cp_flow_refresh_pending_entries(cp_flow_io_t *io, const json_t *pending) {
    if (!pending || pending->type != JSON_ARRAY) return;
    for (size_t i = 0; i < json_len(pending); i++) {
        if (i) {
            json_t *sa = json_object();
            json_set(sa, "seconds", json_number(CP_REFRESH_SWEEP_SPACING_SECONDS));
            json_t *sr = seam(io, "time.sleep", sa);
            json_free(sr);
        }
        json_t *a = json_object();
        json_set(a, "entry", json_copy(json_get(pending, i)));
        json_set(a, "force", json_bool(false));
        json_t *r = seam(io, "_refresh_entry", a);
        json_free(r);
    }
}

/* PoP: cp_try_refresh_current_unlocked @ agent/credential_pool.py:_try_refresh_current_unlocked */
json_t *cp_try_refresh_current_unlocked(cp_flow_io_t *io, const char *failed_key) {
    const char *cur = cp_get_str(io->state, "_current_id", NULL);
    json_t *entry = NULL;
    if (cur && *cur) {
        json_t *m = json_object();
        json_set(m, "id", json_string(cur));
        entry = cp_find(io, m);
        json_free(m);
    }
    if (!entry) return json_null();
    json_t *a = json_object();
    json_set(a, "entry", json_copy(entry));
    json_set(a, "force", json_bool(true));
    json_set(a, "failed_key", failed_key ? json_string(failed_key) : json_null());
    json_t *refreshed = seam(io, "_refresh_entry", a);
    if (refreshed && refreshed->type != JSON_NULL) {
        cp_state_set(io, "_current_id", json_string(cp_get_str(refreshed, "id", "")));
    }
    return refreshed;
}
/* ── refresh entry (single-use serialization flow) ─────────────────────── */

/* PoP: cp_refresh_entry @ agent/credential_pool.py:_refresh_entry */
json_t *cp_refresh_entry(cp_flow_io_t *io, const json_t *entry, bool force, const char *failed_key) {
    const char *auth_type = cp_get_str(entry, "auth_type", "");
    json_t *rt = cp_get(entry, "refresh_token");
    if (strcmp(auth_type, CP_AUTH_TYPE_OAUTH) != 0 || !cp_truthy(rt)) {
        if (force) {
            json_t *m = cp_mark_exhausted(io, entry, 0, NULL, true, NULL);
            json_free(m);
        }
        return json_null();
    }
    const char *provider = cp_provider(io);
    if (!cp_provider_in(provider, CP_SINGLE_USE_REFRESH_PROVIDERS)) {
        json_t *pa = json_object();
        json_set(pa, "provider", json_string(provider));
        json_t *hook = seam(io, "plugin_refresh_hook", pa);
        bool none = !hook || hook->type == JSON_NULL;
        json_free(hook);
        if (none) {
            json_t *a = json_object();
            json_set(a, "entry", json_copy(entry));
            json_set(a, "force", json_bool(force));
            json_set(a, "failed_key", failed_key ? json_string(failed_key) : json_null());
            return seam(io, "_refresh_entry_impl", a);
        }
    }
    /* single-use path: with _auth_store_lock(timeout_seconds=...) */
    json_t *to = seam(io, "_single_use_refresh_lock_timeout", NULL);
    json_t *la = json_object();
    json_set(la, "timeout", to);
    json_t *lr = seam(io, "_auth_store_lock.enter", la);
    json_free(lr);
    json_t *ret = json_null();
    if (strcmp(provider, "openai-codex") == 0) {
        json_t *a = json_object();
        json_set(a, "entry", json_copy(entry));
        json_t *synced = seam(io, "_sync_entry_from_auth_store", a);
        bool changed = !cp_json_equal(synced, entry);
        bool skip = false;
        if (changed && !force) {
            json_t *ea = json_object();
            json_set(ea, "entry", json_copy(synced));
            json_t *needs = seam(io, "_entry_needs_refresh", ea);
            skip = !cp_truthy(needs);
            json_free(needs);
        }
        if (changed && !force && skip) {
            ret = synced;
        } else {
            json_t *ia = json_object();
            json_set(ia, "entry", synced);
            json_set(ia, "force", json_bool(force));
            ret = seam(io, "_refresh_entry_impl", ia);
        }
    } else {
        json_t *a = json_object();
        json_set(a, "entry", json_copy(entry));
        json_t *synced = seam(io, "_sync_entry_from_pool_store", a);
        if (strcmp(provider, "anthropic") == 0 &&
            strcmp(cp_get_str(synced, "source", ""), "claude_code") == 0) {
            json_t *clr = seam(io, "_claude_code_credentials_lock.enter", NULL);
            json_free(clr);
            json_t *sa = json_object();
            json_set(sa, "entry", json_copy(synced));
            json_t *synced2 = seam(io, "_sync_anthropic_entry_from_credentials_file", sa);
            const char *rt2 = cp_get_str(synced2, "refresh_token", "");
            const char *rt0 = cp_get_str(entry, "refresh_token", "");
            if (strcmp(rt2, rt0) != 0) {
                ret = synced2;
                json_free(synced);
            } else {
                json_free(synced2);
                json_t *ia = json_object();
                json_set(ia, "entry", json_copy(synced));
                json_set(ia, "force", json_bool(force));
                ret = seam(io, "_refresh_entry_impl", ia);
                json_free(synced);
            }
            json_t *cle = seam(io, "_claude_code_credentials_lock.exit", NULL);
            json_free(cle);
        } else {
            const char *at_s = cp_get_str(synced, "access_token", "");
            const char *at_e = cp_get_str(entry, "access_token", "");
            const char *rt_s = cp_get_str(synced, "refresh_token", "");
            const char *rt_e = cp_get_str(entry, "refresh_token", "");
            if (strcmp(at_s, at_e) != 0 || strcmp(rt_s, rt_e) != 0) {
                ret = synced;
            } else {
                json_t *ia = json_object();
                json_set(ia, "entry", json_copy(synced));
                json_set(ia, "force", json_bool(force));
                ret = seam(io, "_refresh_entry_impl", ia);
                json_free(synced);
            }
        }
    }
    json_t *le = seam(io, "_auth_store_lock.exit", NULL);
    json_free(le);
    return ret;
}

/* ── failed-refresh recovery tree ──────────────────────────────────────── */

/* PoP: cp_mark_dead_refresh_grant @ agent/credential_pool.py:_mark_dead_refresh_grant */
void cp_mark_dead_refresh_grant(cp_flow_io_t *io, const json_t *entry, const json_t *exc) {
    json_t *m = json_object();
    json_set(m, "id", json_string(cp_get_str(entry, "id", "")));
    json_t *current = cp_find(io, m);
    json_free(m);
    if (!current) return;
    const char *st = cp_get_str(current, "last_status", NULL);
    if (st && strcmp(st, CP_STATUS_DEAD) == 0) return;
    json_t *code = cp_get(exc, "code");
    char *reason = cp_truthy(code) ? cp_pystr(code) : strdup("invalid_grant");
    json_t *ra = json_object();
    json_set(ra, "text", json_string(cp_get_str(exc, "message", "")));
    json_t *secrets = json_array();
    json_append(secrets, json_copy(cp_get(current, "access_token")));
    json_append(secrets, json_copy(cp_get(current, "refresh_token")));
    json_append(secrets, json_copy(cp_get(current, "agent_key")));
    json_set(ra, "known_secrets", secrets);
    json_t *message = seam(io, "redact_error_text", ra);
    json_t *now = seam(io, "time.time", NULL);
    json_t *updates = json_object();
    json_set(updates, "last_status", json_string(CP_STATUS_DEAD));
    json_set(updates, "last_status_at", now);
    json_set(updates, "last_error_code", json_null());
    json_set(updates, "last_error_reason", json_string(reason));
    json_set(updates, "last_error_message", message);
    json_t *adopted = cp_adopt(io, current, updates, true);
    json_free(adopted);
    json_free(updates);
    free(reason);
}

/* PoP: cp_fail_closed_unpersisted_rotation @ agent/credential_pool.py:_fail_closed_unpersisted_rotation */
void cp_fail_closed_unpersisted_rotation(cp_flow_io_t *io, const json_t *entry,
                                         const json_t *exc, const char *store) {
    cp_log(io, "error");
    json_t *spa = json_object();
    json_set(spa, "source", json_string(cp_get_str(entry, "source", "")));
    json_t *source_path = seam(io, "spent_rotation_source_path", spa);
    if (cp_flow_raised(source_path)) {
        cp_log(io, "debug");
        json_free(source_path);
    } else {
        json_t *ma = json_object();
        json_set(ma, "access_token", json_copy(cp_get(entry, "access_token")));
        json_set(ma, "refresh_token", json_copy(cp_get(entry, "refresh_token")));
        json_set(ma, "source_path", source_path);
        json_t *mr = seam(io, "mark_rotation_consumed_uncommitted", ma);
        if (cp_flow_raised(mr)) cp_log(io, "debug");
        json_free(mr);
    }
    char *msg = NULL;
    asprintf(&msg, "rotated credential was not durably written to %s: %s",
             store ? store : "", cp_get_str(exc, "message", ""));
    json_t *ctx = json_object();
    json_set(ctx, "reason", json_string(CP_CREDENTIAL_PERSIST_FAILED_REASON));
    json_set(ctx, "message", json_string(msg));
    json_t *m = cp_mark_exhausted(io, entry, 0, ctx, true, NULL);
    json_free(m);
    json_free(ctx);
    free(msg);
}

static json_t *cp_mark_ok_updates(void) {
    json_t *u = json_object();
    json_set(u, "last_status", json_string(CP_STATUS_OK));
    json_set(u, "last_status_at", json_null());
    json_set(u, "last_error_code", json_null());
    json_set(u, "last_error_reason", json_null());
    json_set(u, "last_error_message", json_null());
    json_set(u, "last_error_reset_at", json_null());
    return u;
}

/* PoP: cp_recover_failed_refresh @ agent/credential_pool.py:_recover_failed_refresh */
json_t *cp_recover_failed_refresh(cp_flow_io_t *io, const json_t *entry, const json_t *exc) {
    const char *provider = cp_provider(io);
    const char *source = cp_get_str(entry, "source", "");
    const char *rt_e = cp_get_str(entry, "refresh_token", "");
    if (strcmp(provider, "anthropic") == 0) {
        if (strcmp(source, "claude_code") == 0) {
            json_t *a = json_object();
            json_set(a, "entry", json_copy(entry));
            json_t *synced = seam(io, "_sync_anthropic_entry_from_credentials_file", a);
            const char *rt_s = cp_get_str(synced, "refresh_token", "");
            if (strcmp(rt_s, rt_e) != 0) {
                cp_log(io, "debug");
                json_t *pa = json_object();
                json_set(pa, "refresh_token", json_string(rt_s));
                /* use_json = synced.source.endswith("hermes_pkce") */
                const char *ss = cp_get_str(synced, "source", "");
                size_t sl = strlen(ss), pl = strlen("hermes_pkce");
                json_set(pa, "use_json", json_bool(sl >= pl && strcmp(ss + sl - pl, "hermes_pkce") == 0));
                json_t *refreshed = seam(io, "refresh_anthropic_oauth_pure", pa);
                if (cp_flow_raised(refreshed)) {
                    cp_log(io, "debug");
                    json_free(refreshed);
                } else {
                    json_t *ca = json_object();
                    json_set(ca, "entry", json_copy(synced));
                    json_set(ca, "refreshed", json_copy(refreshed));
                    json_t *commit = seam(io, "_commit_anthropic_rotation", ca);
                    if (cp_flow_raised(commit)) {
                        const char *t = cp_get_str(commit, "__raise__", "");
                        if (strcmp(t, "_RefreshDone") == 0) {
                            json_t *result = json_copy(cp_get(commit, "result"));
                            json_free(commit);
                            json_free(refreshed);
                            json_free(synced);
                            return result ? result : json_null();
                        }
                        cp_log(io, "debug");
                        json_free(commit);
                    } else {
                        json_free(commit);
                        json_t *updates = json_object();
                        json_set(updates, "access_token", json_copy(cp_get(refreshed, "access_token")));
                        json_set(updates, "refresh_token", json_copy(cp_get(refreshed, "refresh_token")));
                        json_set(updates, "expires_at_ms", json_copy(cp_get(refreshed, "expires_at_ms")));
                        json_set(updates, "last_status", json_string(CP_STATUS_OK));
                        json_set(updates, "last_status_at", json_null());
                        json_set(updates, "last_error_code", json_null());
                        json_t *adopted = cp_adopt(io, synced, updates, true);
                        json_free(updates);
                        json_free(refreshed);
                        json_free(synced);
                        return adopted;
                    }
                    json_free(refreshed);
                }
            } else {
                bool needs = cp_entry_needs_refresh(io, synced);
                if (!needs) {
                    cp_log(io, "debug");
                    return synced;
                }
            }
            json_free(synced);
        } else {
            json_t *a = json_object();
            json_set(a, "entry", json_copy(entry));
            json_t *synced = seam(io, "_sync_entry_from_pool_store", a);
            if (strcmp(cp_get_str(synced, "refresh_token", ""), rt_e) != 0) {
                cp_log(io, "debug");
                json_t *u = cp_mark_ok_updates();
                json_t *adopted = cp_adopt(io, synced, u, true);
                json_free(u);
                json_free(synced);
                return adopted;
            }
            json_free(synced);
        }
        json_t *ta = json_object();
        json_set(ta, "exc", json_copy(exc));
        json_t *terminal = seam(io, "is_terminal_anthropic_refresh_error", ta);
        if (cp_truthy(terminal)) {
            cp_log(io, "warning");
            cp_mark_dead_refresh_grant(io, entry, exc);
            json_free(terminal);
            return json_null();
        }
        json_free(terminal);
    } else if (cp_provider_in(provider, CP_TOKENS_SINGLETON_PROVIDERS)) {
        json_t *a = json_object();
        json_set(a, "entry", json_copy(entry));
        json_t *synced = seam(io, "_sync_entry_from_auth_store", a);
        if (strcmp(cp_get_str(synced, "refresh_token", ""), rt_e) != 0) {
            cp_log(io, "debug");
            json_t *u = cp_mark_ok_updates();
            json_t *adopted = cp_adopt(io, synced, u, true);
            json_free(u);
            json_free(synced);
            return adopted;
        }
        json_free(synced);
        const char *term_fn = strcmp(provider, "openai-codex") == 0
            ? "_is_terminal_codex_oauth_refresh_error" : "_is_terminal_xai_oauth_refresh_error";
        json_t *ta = json_object();
        json_set(ta, "exc", json_copy(exc));
        json_t *terminal = seam(io, term_fn, ta);
        if (cp_truthy(terminal)) {
            cp_log(io, "warning");
            json_t *ca = json_object();
            json_set(ca, "entry", json_copy(entry));
            json_set(ca, "exc", json_copy(exc));
            json_t *cr = seam(io, "_clear_terminal_tokens_state", ca);
            json_free(cr);
            json_t *sources = json_array();
            json_append(sources, json_string("device_code"));
            cp_quarantine_sources(io, entry, sources);
            json_free(sources);
            cp_mark_dead_refresh_grant(io, entry, exc);
            json_free(terminal);
            return json_null();
        }
        json_free(terminal);
    } else if (strcmp(provider, "nous") == 0) {
        json_t *a = json_object();
        json_set(a, "entry", json_copy(entry));
        json_t *synced = seam(io, "_sync_nous_entry_from_auth_store", a);
        if (strcmp(cp_get_str(synced, "refresh_token", ""), rt_e) != 0) {
            cp_log(io, "debug");
            json_t *u = cp_mark_ok_updates();
            json_t *updated = cp_adopt(io, synced, u, true);
            json_free(u);
            json_free(synced);
            json_t *sa = json_object();
            json_set(sa, "entry", json_copy(updated));
            json_t *sr = seam(io, "_sync_device_code_entry_to_auth_store", sa);
            json_free(sr);
            return updated;
        }
        json_free(synced);
        if (strcmp(cp_get_str(exc, "type", ""), "TimeoutError") == 0) {
            cp_log(io, "debug");
            return json_copy(entry);
        }
        json_t *ta = json_object();
        json_set(ta, "exc", json_copy(exc));
        json_t *terminal = seam(io, "_is_terminal_nous_refresh_error", ta);
        if (cp_truthy(terminal)) {
            cp_log(io, "warning");
            json_t *ca = json_object();
            json_set(ca, "entry", json_copy(entry));
            json_set(ca, "exc", json_copy(exc));
            json_t *cr = seam(io, "_clear_terminal_nous_state", ca);
            json_free(cr);
            json_t *sources = json_array();
            json_append(sources, json_string("device_code"));
            json_append(sources, json_string("manual:device_code"));
            cp_quarantine_sources(io, entry, sources);
            json_free(sources);
            cp_mark_dead_refresh_grant(io, entry, exc);
            json_free(terminal);
            return json_null();
        }
        json_free(terminal);
    } else {
        json_t *pa = json_object();
        json_set(pa, "provider", json_string(provider));
        json_t *hook = seam(io, "plugin_refresh_hook", pa);
        bool has_hook = hook && hook->type != JSON_NULL;
        json_free(hook);
        if (has_hook) {
            json_t *ra = json_object();
            json_set(ra, "entry", json_copy(entry));
            json_set(ra, "exc", json_copy(exc));
            json_t *r = seam(io, "recover_failed_plugin_refresh", ra);
            bool handled = r && r->type == JSON_ARRAY && json_len(r) >= 1 && cp_truthy(json_get(r, 0));
            if (handled) {
                json_t *result = json_copy(json_get(r, 1));
                json_free(r);
                return result ? result : json_null();
            }
            json_free(r);
        }
    }
    json_t *m = cp_mark_exhausted(io, entry, 0, NULL, true, NULL);
    json_free(m);
    return json_null();
}
/* ── seeding: upsert + seeder ──────────────────────────────────────────── */

static bool cp_str_in(const char *s, const char *const *list) {
    for (int i = 0; list[i]; i++) if (strcmp(s, list[i]) == 0) return true;
    return false;
}

static const char *CP_FIELD_NAMES[] = {
    "provider", "id", "label", "auth_type", "priority", "source", "access_token",
    "refresh_token", "last_status", "last_status_at", "last_error_code",
    "last_error_reason", "last_error_message", "last_error_reset_at",
    "status_cleared_at", "base_url", "expires_at", "expires_at_ms", "last_refresh",
    "inference_base_url", "agent_key", "agent_key_expires_at", "request_count",
    "model_cooldowns", "extra", NULL,
};
static const char *CP_EXTRA_KEYS[] = {
    "token_type", "scope", "client_id", "portal_base_url", "obtained_at",
    "expires_in", "agent_key_id", "agent_key_expires_in", "agent_key_reused",
    "agent_key_obtained_at", "tls", "secret_source", "secret_fingerprint",
    "auth_method", "account_tier", "anon_token", "user_id", "org_id",
    "failure_reason", NULL,
};
static const char *CP_CLEAR_STATUS_KEYS[] = {
    "last_status", "last_status_at", "last_error_code", "last_error_reason",
    "last_error_message", "last_error_reset_at", NULL,
};

static void cp_array_replace(json_t *arr, json_t *retained) {
    for (size_t i = 0; i < arr->c.count; i++) json_free(arr->c.items[i]);
    free(arr->c.items);
    arr->c.items = NULL;
    arr->c.count = 0;
    for (size_t i = 0; i < json_len(retained); i++) json_append(arr, json_copy(json_get(retained, i)));
}

/* PoP: cp_upsert_entry @ agent/credential_pool.py:_upsert_entry */
bool cp_upsert_entry(cp_flow_io_t *io, json_t *entries, const char *provider,
                     const char *source, json_t *payload) {
    if (!entries || entries->type != JSON_ARRAY) return false;
    /* matching_indices */
    size_t n = json_len(entries);
    long existing_idx = -1;
    bool *duplicate = calloc(n ? n : 1, sizeof(bool));
    bool any_dup = false;
    for (size_t i = 0; i < n; i++) {
        if (strcmp(cp_get_str(json_get(entries, i), "source", ""), source) == 0) {
            if (existing_idx < 0) existing_idx = (long)i;
            else { duplicate[i] = true; any_dup = true; }
        }
    }
    if (any_dup) {
        json_t *retained = json_array();
        for (size_t i = 0; i < n; i++)
            if (!duplicate[i]) json_append(retained, json_copy(json_get(entries, i)));
        cp_array_replace(entries, retained);
        json_free(retained);
        /* recompute existing_idx after compaction */
        existing_idx = -1;
        for (size_t i = 0; i < json_len(entries); i++) {
            if (strcmp(cp_get_str(json_get(entries, i), "source", ""), source) == 0) {
                existing_idx = (long)i;
                break;
            }
        }
    }
    free(duplicate);
    if (existing_idx < 0) {
        if (!cp_get(payload, "id")) {
            json_t *r = seam(io, "uuid_hex6", NULL);
            json_set(payload, "id", r);
        }
        if (!cp_get(payload, "priority")) {
            json_set(payload, "priority", json_number((double)cp_next_priority(entries)));
        }
        if (!cp_get(payload, "label")) {
            json_t *lbl = cp_get(payload, "label");
            json_set(payload, "label", json_string(cp_truthy(lbl) ? cp_get_str(payload, "label", source) : source));
        }
        json_t *fa = json_object();
        json_set(fa, "provider", json_string(provider));
        json_set(fa, "payload", json_copy(payload));
        json_t *entry = seam(io, "from_dict", fa);
        json_append(entries, entry);
        return true;
    }
    json_t *existing = json_get(entries, (size_t)existing_idx);
    json_t *field_updates = json_object();
    json_t *extra_updates = json_object();
    json_t *incoming_token = cp_get(payload, "access_token");
    const char *existing_at = cp_get_str(existing, "access_token", "");
    bool token_changed = incoming_token && incoming_token->type != JSON_NULL &&
                         !(incoming_token->type == JSON_STRING && strcmp(incoming_token->str_val, existing_at) == 0);
    if (token_changed && !existing_at[0]) {
        json_t *extra = cp_get(existing, "extra");
        json_t *kf = cp_get(extra, "secret_fingerprint");
        if (kf && kf->type == JSON_STRING && kf->str_val[0]) {
            json_t *fa = json_object();
            json_set(fa, "token", json_copy(incoming_token));
            json_t *fp = seam(io, "fingerprint_secret_value", fa);
            char *fps = cp_pystr(fp);
            json_free(fp);
            token_changed = strcmp(fps, kf->str_val) != 0;
            free(fps);
        }
    }
    if (payload && payload->type == JSON_OBJECT) {
        for (size_t i = 0; i < payload->c.count; i++) {
            const char *key = payload->c.keys[i];
            json_t *value = payload->c.items[i];
            if (strcmp(key, "id") == 0 || strcmp(key, "priority") == 0 ||
                !value || value->type == JSON_NULL ||
                (strcmp(key, "label") == 0 && cp_truthy(cp_get(existing, "label")))) {
                continue;
            }
            if (cp_str_in(key, CP_FIELD_NAMES)) {
                json_t *cur = cp_get(existing, key);
                if (!cp_json_equal(cur, value)) json_set(field_updates, key, json_copy(value));
            } else if (cp_str_in(key, CP_EXTRA_KEYS)) {
                json_t *cur = cp_get(cp_get(existing, "extra"), key);
                if (!cp_json_equal(cur, value)) json_set(extra_updates, key, json_copy(value));
            }
        }
    }
    if (token_changed && cp_truthy(cp_get(existing, "last_status"))) {
        for (int i = 0; CP_CLEAR_STATUS_KEYS[i]; i++)
            json_set(field_updates, CP_CLEAR_STATUS_KEYS[i], json_null());
    }
    bool has_updates = json_len(field_updates) > 0 || json_len(extra_updates) > 0;
    bool ret = any_dup;
    if (has_updates) {
        if (json_len(extra_updates) > 0) {
            json_t *merged_extra = json_copy(cp_truthy(cp_get(existing, "extra")) ? cp_get(existing, "extra") : json_object());
            for (size_t i = 0; i < extra_updates->c.count; i++)
                json_set(merged_extra, extra_updates->c.keys[i], json_copy(extra_updates->c.items[i]));
            json_set(field_updates, "extra", merged_extra);
        }
        json_t *updated = cp_merge(existing, field_updates);
        json_free(entries->c.items[existing_idx]);
        entries->c.items[existing_idx] = json_copy(updated);
        json_t *ea = json_object();
        json_set(ea, "entry", json_copy(existing));
        json_t *d1 = seam(io, "to_dict", ea);
        json_t *ua = json_object();
        json_set(ua, "entry", json_copy(updated));
        json_t *d2 = seam(io, "to_dict", ua);
        bool differ = !cp_json_equal(d1, d2);
        json_free(d1);
        json_free(d2);
        json_free(updated);
        ret = any_dup || differ;
    }
    json_free(field_updates);
    json_free(extra_updates);
    return ret;
}

/* PoP: cp_seeder_upsert @ agent/credential_pool.py:_Seeder.upsert */
bool cp_seeder_upsert(cp_flow_io_t *io, json_t *seeder, const char *source, json_t *payload) {
    const char *provider = cp_get_str(seeder, "provider", "");
    json_t *sa = json_object();
    json_set(sa, "provider", json_string(provider));
    json_set(sa, "source", json_string(source));
    json_t *sup = seam(io, "is_suppressed", sa);
    bool suppressed = cp_truthy(sup);
    json_free(sup);
    if (suppressed) return false;
    json_t *active = cp_get(seeder, "active_sources");
    if (!active || active->type != JSON_ARRAY) {
        active = json_array();
        json_set(seeder, "active_sources", active);
    }
    bool present = false;
    for (size_t i = 0; i < json_len(active); i++)
        if (strcmp(json_get(active, i)->str_val, source) == 0) { present = true; break; }
    if (!present) json_append(active, json_string(source));
    json_t *merged = json_object();
    json_set(merged, "source", json_string(source));
    if (payload && payload->type == JSON_OBJECT)
        for (size_t i = 0; i < payload->c.count; i++)
            json_set(merged, payload->c.keys[i], json_copy(payload->c.items[i]));
    json_t *entries = cp_get(seeder, "entries");
    bool ingested = cp_upsert_entry(io, entries, provider, source, merged);
    json_free(merged);
    if (ingested) json_set(seeder, "changed", json_bool(true));
    return ingested;
}
