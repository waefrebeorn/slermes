/* port_model_switch_flow.c — hermes_cli/model_switch.py switching FLOW.
 *
 * Decision-sequence port: alias resolution ordering, PATH A / PATH B
 * routing, the validation chain, direct-alias credential decisions and the
 * config-write / rollback branches. All file/network/config/credential I/O
 * rides the injected universal seam (msf_call); the harness scripts results
 * and records the ordered trace the oracle diffs against LIVE
 * hermes_cli/model_switch.py driven under the same scripts.
 *
 * Representation contract: see include/port_model_switch_flow.h.
 * libjson-only (default oracle deps suffice).
 */
#include "port_model_switch_flow.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── string helpers (ASCII fixture domain; Python str semantics) ─────────── */

static char *msf_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static bool msf_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* str.strip() — both ends, ASCII whitespace fixture domain. */
static char *msf_strip(const char *s) {
    while (*s && msf_is_ws(*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && msf_is_ws(s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

/* str.lower() — ASCII fixture domain. */
static char *msf_lower(const char *s) {
    char *out = msf_strdup(s);
    if (out) for (char *p = out; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return out;
}

static char *msf_strip_lower(const char *s) {
    char *t = msf_strip(s);
    char *out = msf_lower(t ? t : "");
    free(t);
    return out;
}

/* str.lstrip(chars) / str.strip(chars) for an explicit character set. */
static char *msf_strip_chars(const char *s, const char *set, bool both) {
    size_t start = 0, n = strlen(s);
    while (start < n && strchr(set, s[start])) start++;
    size_t end = n;
    if (both) while (end > start && strchr(set, s[end - 1])) end--;
    char *out = (char *)malloc(end - start + 1);
    if (out) { memcpy(out, s + start, end - start); out[end - start] = '\0'; }
    return out;
}

/* str.removeprefix(p) — one leading occurrence only. */
static char *msf_removeprefix(const char *s, const char *p) {
    size_t np = strlen(p);
    if (np > 0 && strncmp(s, p, np) == 0) return msf_strdup(s + np);
    return msf_strdup(s);
}

static bool msf_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

static bool msf_starts_with(const char *s, const char *p) {
    return strncmp(s, p, strlen(p)) == 0;
}

/* ── JSON value helpers ──────────────────────────────────────────────────── */

static bool msf_is_none(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

/* Python truthiness for a JSON node (NULL pointer == None == falsy). */
static bool msf_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

/* String value of a node ("" for non-strings; fixture domain). */
static const char *msf_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static bool msf_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool msf_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }

/* str(value or "").strip() — _clean. Truthy non-strings (container repr)
 * are excluded from the fixture domain. */
static char *msf_clean(const json_t *v) {
    if (msf_truthy(v) && v->type == JSON_STRING) return msf_strip(v->str_val);
    return msf_strdup("");
}

/* str(x or "") for string/null fixture fields. */
static char *msf_str_or_empty(const json_t *v) {
    if (msf_truthy(v) && v->type == JSON_STRING) return msf_strdup(v->str_val);
    return msf_strdup("");
}

static bool msf_arr_has_str(const json_t *arr, const char *s) {
    if (!msf_is_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *it = arr->c.items[i];
        if (it && it->type == JSON_STRING && msf_str_eq(it->str_val, s)) return true;
    }
    return false;
}

/* Deep Python-style equality; NULL pointer and JSON_NULL both read None.
 * Object key order is irrelevant (dict ==). */
static bool msf_json_eq(const json_t *a, const json_t *b) {
    if (msf_is_none(a) && msf_is_none(b)) return true;
    if (msf_is_none(a) || msf_is_none(b)) return false;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return msf_str_eq(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!msf_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !msf_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

/* ── string builder ──────────────────────────────────────────────────────── */

typedef struct { char *buf; size_t len, cap; } msf_sb;

static void msf_sb_init(msf_sb *sb) { sb->buf = NULL; sb->len = 0; sb->cap = 0; }

static void msf_sb_append(msf_sb *sb, const char *s) {
    size_t n = strlen(s);
    if (sb->len + n + 1 > sb->cap) {
        size_t nc = sb->cap ? sb->cap * 2 : 128;
        while (nc < sb->len + n + 1) nc *= 2;
        char *nb = (char *)realloc(sb->buf, nc);
        if (!nb) return;
        sb->buf = nb; sb->cap = nc;
    }
    memcpy(sb->buf + sb->len, s, n + 1);
    sb->len += n;
}

static void msf_sb_appendf(msf_sb *sb, const char *fmt, ...) {
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    msf_sb_append(sb, tmp);
}

static char *msf_sb_take(msf_sb *sb) {
    if (!sb->buf) return msf_strdup("");
    return sb->buf;
}

/* json_string COPIES its input — hand off the builder buffer without leaking. */
static json_t *msf_sb_json(msf_sb *sb) {
    char *s = msf_sb_take(sb);
    json_t *j = json_string(s);
    free(s);
    return j;
}

/* ── exception plumbing ──────────────────────────────────────────────────── */

bool msf_exc_pending(const msf_ctx *ctx) { return ctx && ctx->exc != NULL; }

void msf_exc_clear(msf_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}

void msf_raise(msf_ctx *ctx, const char *cls) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
}

bool msf_exc_is(const msf_ctx *ctx, const char *cls) {
    return ctx && ctx->exc && msf_str_eq(json_get_str(ctx->exc, "__raise__", ""), cls);
}

bool msf_exc_is_exception(const msf_ctx *ctx) {
    if (!msf_exc_pending(ctx)) return false;
    const char *cls = json_get_str(ctx->exc, "__raise__", "");
    return !msf_str_eq(cls, "SystemExit") && !msf_str_eq(cls, "KeyboardInterrupt")
        && !msf_str_eq(cls, "GeneratorExit");
}

/* AmbiguousAliasError carries alias/provider/candidates on the marker so a
 * catching flow can render _ambiguous_alias_message exactly like live. */
static void msf_raise_ambiguous(msf_ctx *ctx, const char *alias, const char *provider,
                                const json_t *candidates) {
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string("AmbiguousAliasError"));
    json_set(ctx->exc, "alias", json_string(alias));
    json_set(ctx->exc, "provider", json_string(provider));
    json_set(ctx->exc, "candidates", json_copy(candidates));
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *msf_script_result(msf_ctx *ctx, const json_t *script) {
    /* Returns a NEW value (NULL == None); sets ctx->exc for a scripted raise. */
    if (!script || script->type == JSON_NULL) return NULL;
    if (script->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(script, "__raise__");
        if (raise && raise->type == JSON_STRING) {
            if (ctx->exc) json_free(ctx->exc);
            ctx->exc = json_copy(script);
            return NULL;
        }
    }
    return json_copy(script);
}

json_t *msf_call(msf_ctx *ctx, const char *name, json_t *args) {
    /* args ownership is transferred (NULL -> empty object). Result is NEW. */
    if (!args) args = json_object();
    json_t *ret = NULL;
    json_t *script = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *chosen = script;
    if (script && script->type == JSON_OBJECT) {
        json_t *seq = json_obj_get(script, "$seq");
        if (seq && seq->type == JSON_ARRAY) {
            double cur = json_get_num(ctx->counters, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= seq->c.count) {
                if (ctx->exc) json_free(ctx->exc);
                ctx->exc = json_object();
                json_set(ctx->exc, "__raise__", json_string("RuntimeError"));
                char msg[256];
                snprintf(msg, sizeof(msg), "script exhausted: %s", name);
                json_set(ctx->exc, "msg", json_string(msg));
                chosen = ctx->exc;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        }
    }
    if (chosen != ctx->exc) ret = msf_script_result(ctx, chosen);
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args);
    if (ctx->exc && chosen == ctx->exc) json_set(ev, "ret", json_copy(ctx->exc));
    else if (ctx->exc && chosen && chosen->type == JSON_OBJECT && json_obj_get(chosen, "__raise__"))
        json_set(ev, "ret", json_copy(ctx->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    return ret;
}

/* Arg builders: None-valued keys are dropped on BOTH sides ("" is kept). */
static void msf_arg_json(json_t *args, const char *key, json_t *val) {
    /* adopts val */
    if (msf_is_none(val)) { json_free(val); return; }
    json_set(args, key, val);
}

static void msf_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}

static void msf_arg_copy(json_t *args, const char *key, const json_t *val) {
    if (msf_is_none(val)) return;
    json_set(args, key, json_copy(val));
}

/* ── _Switch / ModelSwitchResult state doubles ───────────────────────────── */

typedef struct { const char *name; int kind; const char *sval; } msf_field_def;
/* kind: 0 = string, 1 = bool false, 2 = null, 3 = empty dict */

static const msf_field_def MSF_SWITCH_FIELDS[] = {
    {"raw_input", 0, ""}, {"current_provider", 0, ""}, {"current_model", 0, ""},
    {"current_base_url", 0, ""}, {"current_api_key", 0, ""}, {"is_global", 1, NULL},
    {"explicit_provider", 0, ""}, {"user_providers", 2, NULL}, {"custom_providers", 2, NULL},
    {"new_model", 0, ""}, {"target_provider", 0, ""}, {"resolved_alias", 0, ""},
    {"alias_endpoint_applied", 1, NULL}, {"provider_label", 0, ""}, {"api_key", 0, ""},
    {"base_url", 0, ""}, {"api_mode", 0, ""}, {"validation_headers", 3, NULL},
    {"suppress_ollama_headers", 1, NULL}, {"validation", 3, NULL},
};
#define MSF_SWITCH_NFIELDS (sizeof(MSF_SWITCH_FIELDS) / sizeof(MSF_SWITCH_FIELDS[0]))

static json_t *msf_default_node(const msf_field_def *f) {
    switch (f->kind) {
        case 1: return json_bool(false);
        case 2: return json_null();
        case 3: return json_object();
        default: return json_string(f->sval ? f->sval : "");
    }
}

json_t *msf_switch_new(const json_t *fields) {
    json_t *st = json_object();
    for (size_t i = 0; i < MSF_SWITCH_NFIELDS; i++) {
        const msf_field_def *f = &MSF_SWITCH_FIELDS[i];
        json_t *v = fields ? json_obj_get(fields, f->name) : NULL;
        json_set(st, f->name, v ? json_copy(v) : msf_default_node(f));
    }
    return st;
}

static const msf_field_def MSF_RESULT_FIELDS[] = {
    {"success", 1, NULL}, {"new_model", 0, ""}, {"target_provider", 0, ""},
    {"provider_changed", 1, NULL}, {"api_key", 0, ""}, {"base_url", 0, ""},
    {"api_mode", 0, ""}, {"request_overrides", 2, NULL}, {"error_message", 0, ""},
    {"warning_message", 0, ""}, {"usage_notice", 0, ""}, {"provider_label", 0, ""},
    {"resolved_via_alias", 0, ""}, {"capabilities", 2, NULL}, {"runtime_capabilities", 2, NULL},
    {"model_info", 2, NULL}, {"is_global", 1, NULL}, {"alias_endpoint_applied", 1, NULL},
};
#define MSF_RESULT_NFIELDS (sizeof(MSF_RESULT_FIELDS) / sizeof(MSF_RESULT_FIELDS[0]))

/* _Switch.fail(message, **fields) — full ModelSwitchResult default window. */
json_t *msf_fail(msf_ctx *ctx, const json_t *st, json_t *message, const json_t *fields) {
    (void)ctx;
    json_t *r = json_object();
    for (size_t i = 0; i < MSF_RESULT_NFIELDS; i++)
        json_set(r, MSF_RESULT_FIELDS[i].name, msf_default_node(&MSF_RESULT_FIELDS[i]));
    json_set(r, "success", json_bool(false));
    json_set(r, "is_global", json_copy(json_obj_get(st, "is_global")));
    json_set(r, "error_message", message ? message : json_string(""));
    if (fields) {
        for (size_t i = 0; i < fields->c.count; i++)
            json_set(r, fields->c.keys[i], json_copy(fields->c.items[i]));
    }
    return r;
}

json_t *msf_fail_on_target(msf_ctx *ctx, const json_t *st, json_t *message) {
    json_t *fields = json_object();
    json_set(fields, "target_provider", json_copy(json_obj_get(st, "target_provider")));
    json_set(fields, "provider_label", json_copy(json_obj_get(st, "provider_label")));
    json_t *r = msf_fail(ctx, st, message, fields);
    json_free(fields);
    return r;
}

bool msf_provider_changed(const json_t *st) {
    return !msf_str_eq(json_get_str(st, "target_provider", ""),
                       json_get_str(st, "current_provider", ""));
}

/* ── MODEL_ALIASES (live module constant, source order) ──────────────────── */

typedef struct { const char *key, *vendor, *family; } msf_model_alias;

static const msf_model_alias MSF_MODEL_ALIASES[] = {
    {"sonnet", "anthropic", "claude-sonnet"}, {"opus", "anthropic", "claude-opus"},
    {"haiku", "anthropic", "claude-haiku"}, {"claude", "anthropic", "claude"},
    {"gpt5", "openai", "gpt-5"}, {"gpt", "openai", "gpt"},
    {"codex", "openai", "codex"}, {"o3", "openai", "o3"}, {"o4", "openai", "o4"},
    {"gemini", "google", "gemini"}, {"deepseek", "deepseek", "deepseek-chat"},
    {"grok", "x-ai", "grok"}, {"llama", "meta-llama", "llama"}, {"qwen", "qwen", "qwen"},
    {"minimax", "minimax", "minimax"}, {"nemotron", "nvidia", "nemotron"},
    {"kimi", "moonshotai", "kimi"}, {"glm", "z-ai", "glm"}, {"step", "stepfun", "step"},
    {"mimo", "xiaomi", "mimo"}, {"trinity", "arcee-ai", "trinity"},
};
#define MSF_MODEL_ALIASES_N (sizeof(MSF_MODEL_ALIASES) / sizeof(MSF_MODEL_ALIASES[0]))

static const msf_model_alias *msf_model_alias_get(const char *key) {
    for (size_t i = 0; i < MSF_MODEL_ALIASES_N; i++)
        if (msf_str_eq(MSF_MODEL_ALIASES[i].key, key)) return &MSF_MODEL_ALIASES[i];
    return NULL;
}

/* ── internal pure helpers (claimed siblings stay REAL in the oracle) ────── */

/* _declared_item_id */
static json_t *msf_declared_item_id(const json_t *item) {
    json_t *mid = json_obj_get(item, "id");
    if (mid && mid->type == JSON_STRING) {
        char *s = msf_strip(mid->str_val);
        bool nonblank = s && s[0] != '\0';
        free(s);
        if (nonblank) return mid;
    }
    return json_obj_get(item, "name");
}

/* _declared_model_ids — ordered unique (case-insensitive) stripped ids. */
static json_t *msf_declared_model_ids(const json_t *value) {
    json_t *out = json_array();
    json_t *cands = json_array(); /* borrowed candidate nodes */
    if (value && value->type == JSON_STRING) {
        json_append(cands, (json_t *)value);
    } else if (msf_is_obj(value)) {
        for (size_t i = 0; i < value->c.count; i++) {
            const char *k = value->c.keys[i];
            if (msf_str_eq(k, "__explicit_model_allowlist__")
                || msf_str_eq(k, "__discovered_model_catalog__")) continue;
            json_t *kn = json_string(k);
            json_append(cands, kn); /* owned string nodes freed below */
        }
    } else if (msf_is_arr(value)) {
        for (size_t i = 0; i < value->c.count; i++) {
            json_t *item = value->c.items[i];
            json_append(cands, msf_is_obj(item) ? msf_declared_item_id(item) : item);
        }
    } else {
        json_free(cands);
        return out;
    }
    json_t *seen = json_array(); /* lowered ids already emitted */
    for (size_t i = 0; i < cands->c.count; i++) {
        json_t *c = cands->c.items[i];
        bool owned = msf_is_obj(value) && c && c->type == JSON_STRING;
        if (!c || c->type != JSON_STRING) { if (owned) json_free(c); continue; }
        char *mid = msf_strip(c->str_val);
        if (owned) json_free(c);
        if (mid && mid[0]) {
            char *low = msf_lower(mid);
            if (!msf_arr_has_str(seen, low)) {
                json_append(seen, json_string(low));
                json_append(out, json_string(mid));
            }
            free(low);
        }
        free(mid);
    }
    json_free(seen);
    /* cands holds BORROWED nodes (owned key strings were freed in the loop):
     * free the container only, never recurse into the items. */
    free(cands->c.items);
    free(cands);
    return out;
}

/* _ambiguous_alias_message(err) — err is the exception MARKER (alias /
 * provider / candidates fields), matching the live exception attributes. */
static char *msf_ambiguous_alias_message(const json_t *err) {
    const char *alias = json_get_str(err, "alias", "");
    const char *provider = json_get_str(err, "provider", "");
    json_t *cands = json_obj_get(err, "candidates");
    size_t total = msf_is_arr(cands) ? cands->c.count : 0;
    size_t shown = total < 10 ? total : 10;
    msf_sb sb; msf_sb_init(&sb);
    msf_sb_appendf(&sb, "'%s' matches %zu models on %s \xe2\x80\x94 not switching automatically:\n",
                   alias, total, provider);
    for (size_t i = 0; i < shown; i++)
        msf_sb_appendf(&sb, "  %zu. %s\n", i + 1, msf_jstr(cands->c.items[i]));
    /* join() puts no trailing newline after the last line; rebuild precisely */
    /* (the loop above appends \n per line; strip the final one) */
    if (shown > 0 && sb.len > 0) sb.buf[--sb.len] = '\0';
    if (total > shown)
        msf_sb_appendf(&sb, "\n  \xe2\x80\xa6 and %zu more", total - shown);
    msf_sb_append(&sb, "\nPick one with /model <exact-model-name>.");
    return msf_sb_take(&sb);
}

/* _bare_custom_provider_def — ProviderDef double as an attribute dict. */
static json_t *msf_bare_custom_provider_def(const char *current_base_url) {
    json_t *bn = json_string(current_base_url ? current_base_url : "");
    char *url = msf_clean(bn);
    json_free(bn);
    if (!url || !url[0]) { free(url); return NULL; }
    json_t *d = json_object();
    json_set(d, "id", json_string("custom"));
    json_set(d, "name", json_string("Custom endpoint"));
    json_set(d, "transport", json_string("openai_chat"));
    json_set(d, "api_key_env_vars", json_array());
    json_set(d, "base_url", json_string(url));
    json_set(d, "is_aggregator", json_bool(false));
    json_set(d, "auth_type", json_string("api_key"));
    json_set(d, "source", json_string("model-config"));
    free(url);
    return d;
}

/* _custom_entries — dict-shaped entries of a custom_providers list. */
static json_t *msf_custom_entries(const json_t *cp) {
    json_t *out = json_array();
    if (msf_is_arr(cp))
        for (size_t i = 0; i < cp->c.count; i++)
            if (msf_is_obj(cp->c.items[i])) json_append(out, json_copy(cp->c.items[i]));
    return out;
}

/* _entry_aliases — custom_provider_aliases seam (sorted-array canon). */
static json_t *msf_entry_aliases(msf_ctx *ctx, const json_t *entry) {
    char *name = msf_str_or_empty(json_obj_get(entry, "name"));
    char *pkey = msf_str_or_empty(json_obj_get(entry, "provider_key"));
    json_t *args = json_object();
    msf_arg_str(args, "name", name);
    msf_arg_str(args, "provider_key", pkey);
    free(name); free(pkey);
    return msf_call(ctx, "custom_provider_aliases", args);
}

/* ── _model_sort_key / _split_version_suffix (internal transcriptions) ───── */

typedef struct { bool is_str; double num; char *str; } msf_key_el;
typedef struct { msf_key_el *els; size_t n; } msf_sort_key;

static void msf_key_push(msf_sort_key *k, size_t *cap, bool is_str, double num, const char *s) {
    if (k->n + 1 > *cap) {
        *cap = *cap ? *cap * 2 : 8;
        k->els = (msf_key_el *)realloc(k->els, *cap * sizeof(msf_key_el));
    }
    k->els[k->n].is_str = is_str;
    k->els[k->n].num = num;
    k->els[k->n].str = is_str ? msf_strdup(s) : NULL;
    k->n++;
}

static void msf_key_free(msf_sort_key *k) {
    for (size_t i = 0; i < k->n; i++) free(k->els[i].str);
    free(k->els);
    k->els = NULL; k->n = 0;
}

/* float(run.rstrip(".")) with ValueError suppression; digit/dot runs only. */
static bool msf_parse_version(const char *run, double *out) {
    size_t n = strlen(run);
    while (n > 0 && run[n - 1] == '.') n--;
    if (n == 0) return false;
    char *tmp = (char *)malloc(n + 1);
    if (!tmp) return false;
    memcpy(tmp, run, n); tmp[n] = '\0';
    char *end = NULL;
    double v = strtod(tmp, &end);
    bool ok = end && end != tmp && *end == '\0';
    free(tmp);
    if (ok) *out = v;
    return ok;
}

/* _split_version_suffix(rest) -> (nums, rest[pos:]) */
static void msf_split_version_suffix(const char *rest, msf_sort_key *nums, size_t *numcap,
                                     char **out_suffix) {
    size_t pos = 0, len = strlen(rest);
    msf_sb run; msf_sb_init(&run);
    while (pos < len) {
        char ch = rest[pos];
        if (ch == '-' || ch == '_' || ch == '.') { pos++; continue; }
        bool is_v = ch == 'v' || ch == 'V';
        bool is_digit = ch >= '0' && ch <= '9';
        if (!is_v && !is_digit) break;
        if (is_v) pos++;
        while (pos < len && ((rest[pos] >= '0' && rest[pos] <= '9') || rest[pos] == '.')) {
            if (rest[pos] == '.' && strchr(run.buf ? run.buf : "", '.')) {
                double v;
                if (msf_parse_version(run.buf ? run.buf : "", &v))
                    msf_key_push(nums, numcap, false, v, NULL);
                run.len = 0; if (run.buf) run.buf[0] = '\0';
            } else {
                char piece[2] = {rest[pos], '\0'};
                msf_sb_append(&run, piece);
            }
            pos++;
        }
        double v;
        if (msf_parse_version(run.buf ? run.buf : "", &v))
            msf_key_push(nums, numcap, false, v, NULL);
        run.len = 0; if (run.buf) run.buf[0] = '\0';
        if (pos < len && rest[pos] != '-' && rest[pos] != '_') break;
    }
    free(run.buf);
    *out_suffix = msf_strdup(rest + pos);
}

/* _model_sort_key(model_id, prefix) — version_key + (rank, suffix) + date_key */
static msf_sort_key msf_model_sort_key(const char *model_id, const char *prefix) {
    size_t plen = strlen(prefix);
    size_t mlen = strlen(model_id);
    const char *tail = mlen >= plen ? model_id + plen : "";
    char *t1 = msf_removeprefix(tail, "/");
    char *t2 = msf_strip_chars(t1, "-", false);
    char *rest = msf_strip(t2);
    free(t1); free(t2);

    msf_sort_key nums = {NULL, 0}; size_t numcap = 0;
    char *suffix_buf = NULL;
    msf_split_version_suffix(rest, &nums, &numcap, &suffix_buf);
    free(rest);
    char *low = msf_lower(suffix_buf);
    free(suffix_buf);
    char *s1 = msf_strip_chars(low, "-_.", true);
    free(low);
    char *suffix = msf_strip(s1);
    free(s1);

    int rank = (msf_str_eq(suffix, "pro") || msf_str_eq(suffix, "max") || msf_str_eq(suffix, "plus")
                || msf_str_eq(suffix, "turbo") || msf_str_eq(suffix, "sol")
                || msf_str_eq(suffix, "astra")) ? 0 : 1;

    msf_sort_key key = {NULL, 0}; size_t cap = 0;
    double date_stamp = 0.0;
    for (size_t i = 0; i < nums.n; i++) {
        double n = nums.els[i].num;
        if (n < 19000101.0) msf_key_push(&key, &cap, false, -n, NULL);
        else if (n > date_stamp) date_stamp = n;
    }
    msf_key_push(&key, &cap, false, (double)rank, NULL);
    msf_key_push(&key, &cap, true, 0.0, suffix);
    if (date_stamp == 0.0) {
        msf_key_push(&key, &cap, false, 0.0, NULL);
        msf_key_push(&key, &cap, false, 0.0, NULL);
    } else {
        msf_key_push(&key, &cap, false, 1.0, NULL);
        msf_key_push(&key, &cap, false, -date_stamp, NULL);
    }
    free(suffix);
    msf_key_free(&nums);
    return key;
}

/* Python tuple ordering; a str-vs-number position pair raises TypeError. */
static int msf_key_cmp(const msf_sort_key *a, const msf_sort_key *b, msf_ctx *ctx) {
    size_t n = a->n < b->n ? a->n : b->n;
    for (size_t i = 0; i < n; i++) {
        const msf_key_el *x = &a->els[i];
        const msf_key_el *y = &b->els[i];
        if (x->is_str != y->is_str) { msf_raise(ctx, "TypeError"); return 0; }
        if (x->is_str) {
            int c = strcmp(x->str, y->str);
            if (c) return c;
        } else {
            if (x->num < y->num) return -1;
            if (x->num > y->num) return 1;
        }
    }
    if (a->n < b->n) return -1;
    if (a->n > b->n) return 1;
    return 0;
}

/* Stable sort (Python list.sort is stable; qsort is not). */
static void msf_sort_matches(json_t *matches, const char *prefix, msf_ctx *ctx) {
    size_t n = matches->c.count;
    if (n < 2) return;
    msf_sort_key *keys = (msf_sort_key *)calloc(n, sizeof(msf_sort_key));
    json_t **items = (json_t **)calloc(n, sizeof(json_t *));
    for (size_t i = 0; i < n; i++) {
        items[i] = matches->c.items[i];
        keys[i] = msf_model_sort_key(msf_jstr(items[i]), prefix);
    }
    for (size_t i = 1; i < n; i++) {
        json_t *it = items[i];
        msf_sort_key k = keys[i];
        size_t j = i;
        while (j > 0) {
            int c = msf_key_cmp(&keys[j - 1], &k, ctx);
            if (msf_exc_pending(ctx) || c <= 0) break;
            items[j] = items[j - 1];
            keys[j] = keys[j - 1];
            j--;
        }
        items[j] = it;
        keys[j] = k;
    }
    for (size_t i = 0; i < n; i++) matches->c.items[i] = items[i];
    for (size_t i = 0; i < n; i++) msf_key_free(&keys[i]);
    free(keys);
    free(items);
}

/* ── internal step helpers (gap siblings used only inside ported flows) ──── */

/* _current_provider_match(st, cfg_matches) */
static char *msf_current_provider_match(msf_ctx *ctx, const json_t *st, const json_t *cfg_matches) {
    const char *cur = json_get_str(st, "current_provider", "");
    if (json_obj_get(cfg_matches, cur)) return msf_strdup(cur);
    char *clean_cur = msf_clean(json_obj_get(st, "current_provider"));
    char *current = msf_lower(clean_cur);
    free(clean_cur);
    const json_t *providers = json_obj_get(st, "user_providers");
    char *hit = NULL;
    for (size_t i = 0; i < cfg_matches->c.count && !hit; i++) {
        const char *slug = cfg_matches->c.keys[i];
        json_t *p = msf_is_obj(providers) ? json_obj_get(providers, slug) : NULL;
        if (!msf_is_obj(p)) continue;
        json_t *entry = json_object();
        json_set(entry, "name", json_copy(json_obj_get(p, "name")));
        json_set(entry, "provider_key", json_string(slug));
        json_t *aliases = msf_entry_aliases(ctx, entry);
        json_free(entry);
        if (msf_exc_pending(ctx)) { json_free(aliases); break; }
        if (msf_arr_has_str(aliases, current)) hit = msf_strdup(slug);
        json_free(aliases);
    }
    free(current);
    return hit;
}

/* _entry_configured_key(cfg, read_env) — read_env is the os.environ lambda
 * (seam result stripped; absent script reads the "" get default). */
static char *msf_read_env(msf_ctx *ctx, const char *name) {
    json_t *args = json_object();
    msf_arg_str(args, "name", name);
    json_t *v = msf_call(ctx, "read_env", args);
    if (msf_exc_pending(ctx)) { json_free(v); return NULL; }
    char *out = msf_strip(msf_jstr(v));
    json_free(v);
    return out;
}

static char *msf_entry_configured_key(msf_ctx *ctx, const json_t *cfg) {
    char *key = msf_clean(json_obj_get(cfg, "api_key"));
    size_t klen = strlen(key);
    if (klen >= 3 && msf_starts_with(key, "${") && key[klen - 1] == '}') {
        /* live key[2:-1]: drop BOTH "${" and the trailing "}" (klen-3). */
        char *inner = (char *)malloc(klen - 3 + 1);
        memcpy(inner, key + 2, klen - 3);
        inner[klen - 3] = '\0';
        free(key);
        key = msf_read_env(ctx, inner);
        free(inner);
        if (!key) return NULL; /* exc pending */
    }
    if (!key[0]) {
        json_t *ke = json_obj_get(cfg, "key_env");
        json_t *ake = json_obj_get(cfg, "api_key_env");
        char *key_env = msf_clean(msf_truthy(ke) ? ke : ake);
        free(key);
        if (key_env[0]) {
            key = msf_read_env(ctx, key_env);
        } else {
            key = msf_strdup("");
        }
        free(key_env);
    }
    return key;
}

/* _resolve_named_custom_model_id (claimed pure sibling, internal here) */
static char *msf_resolve_named_custom_model_id(msf_ctx *ctx, const char *model_name,
                                               const char *target_provider, const json_t *cp) {
    char *pcl = msf_strip(target_provider ? target_provider : "");
    char *provider = msf_lower(pcl);
    free(pcl);
    if (!msf_starts_with(provider, "custom:") || !strchr(model_name, '/')) {
        free(provider);
        return msf_strdup(model_name);
    }
    const char *slash = strchr(model_name, '/');
    char *prefix_raw = (char *)malloc((size_t)(slash - model_name) + 1);
    memcpy(prefix_raw, model_name, (size_t)(slash - model_name));
    prefix_raw[slash - model_name] = '\0';
    char *prefix = msf_strip(prefix_raw);
    char *candidate = msf_strip(slash + 1);
    free(prefix_raw);
    if (!prefix[0] || !candidate[0]) {
        free(provider); free(prefix); free(candidate);
        return msf_strdup(model_name);
    }
    char *prefix_slug = msf_lower(prefix);
    char *want_slug = (char *)malloc(strlen(prefix_slug) + 8);
    sprintf(want_slug, "custom:%s", prefix_slug);
    char *result = NULL;
    json_t *entries = msf_custom_entries(cp);
    for (size_t i = 0; i < entries->c.count && !result; i++) {
        json_t *entry = entries->c.items[i];
        json_t *slugs = msf_entry_aliases(ctx, entry);
        if (msf_exc_pending(ctx)) { json_free(slugs); break; }
        if (msf_arr_has_str(slugs, provider) && msf_arr_has_str(slugs, want_slug)) {
            json_t *ids = msf_declared_model_ids(json_obj_get(entry, "models"));
            for (size_t j = 0; j < ids->c.count && !result; j++) {
                char *idl = msf_lower(msf_jstr(ids->c.items[j]));
                char *candl = msf_lower(candidate);
                if (msf_str_eq(idl, candl)) result = msf_strdup(msf_jstr(ids->c.items[j]));
                free(idl); free(candl);
            }
            json_free(ids);
        }
        json_free(slugs);
    }
    json_free(entries);
    free(provider); free(prefix); free(candidate); free(prefix_slug); free(want_slug);
    return result ? result : msf_strdup(model_name);
}

/* _names_known_provider(name, st) */
static bool msf_names_known_provider(msf_ctx *ctx, const char *name, const json_t *st) {
    json_t *args = json_object();
    msf_arg_str(args, "name", name);
    msf_arg_copy(args, "user_providers", json_obj_get(st, "user_providers"));
    msf_arg_copy(args, "custom_providers", json_obj_get(st, "custom_providers"));
    json_t *pdef = msf_call(ctx, "resolve_provider_full", args);
    if (msf_exc_pending(ctx)) { json_free(pdef); return false; }
    if (!msf_is_none(pdef)) { json_free(pdef); return true; }
    json_free(pdef);
    args = json_object();
    msf_arg_str(args, "name", name);
    json_set(args, "allow_network", json_bool(false));
    json_t *p = msf_call(ctx, "get_provider", args);
    if (msf_exc_pending(ctx)) {
        json_free(p);
        if (msf_exc_is_exception(ctx)) { msf_exc_clear(ctx); return false; }
        return false; /* BaseException propagates */
    }
    bool known = !msf_is_none(p);
    json_free(p);
    return known;
}

/* _convert_vendor_colon_slug(st) */
static void msf_convert_vendor_colon_slug(msf_ctx *ctx, json_t *st) {
    const char *raw = json_get_str(st, "raw_input", "");
    const char *colon = strchr(raw, ':');
    long colon_pos = colon ? (long)(colon - raw) : -1;
    char *cur_norm = msf_strip_lower(json_get_str(st, "current_provider", ""));
    bool skip = colon_pos <= 0 || strchr(raw, '/') || msf_starts_with(cur_norm, "custom")
                || msf_str_eq(cur_norm, "ollama");
    free(cur_norm);
    if (skip) return;
    char *left_raw = (char *)malloc((size_t)colon_pos + 1);
    memcpy(left_raw, raw, (size_t)colon_pos);
    left_raw[colon_pos] = '\0';
    char *left = msf_strip_lower(left_raw);
    char *right = msf_strip(raw + colon_pos + 1);
    free(left_raw);
    if (!left[0] || !right[0]) { free(left); free(right); return; }
    json_t *args = json_object();
    msf_arg_str(args, "provider", json_get_str(st, "current_provider", ""));
    json_t *agg = msf_call(ctx, "is_aggregator", args);
    if (msf_exc_pending(ctx)) { json_free(agg); free(left); free(right); return; }
    bool is_agg = msf_truthy(agg);
    json_free(agg);
    if (!is_agg && !msf_names_known_provider(ctx, left, st)) { free(left); free(right); return; }
    if (msf_exc_pending(ctx)) { free(left); free(right); return; }
    msf_sb sb; msf_sb_init(&sb);
    msf_sb_appendf(&sb, "%s/%s", left, right);
    json_set(st, "new_model", msf_sb_json(&sb));
    free(left); free(right);
}

/* _route_alias_fallback(st, key) — step b of PATH B. */
static json_t *msf_route_alias_fallback(msf_ctx *ctx, json_t *st, const char *key) {
    json_t *args = json_object();
    msf_arg_str(args, "current_provider", json_get_str(st, "current_provider", ""));
    msf_arg_copy(args, "user_providers", json_obj_get(st, "user_providers"));
    msf_arg_copy(args, "custom_providers", json_obj_get(st, "custom_providers"));
    json_t *authed = msf_call(ctx, "get_authenticated_provider_slugs", args);
    if (msf_exc_pending(ctx)) { json_free(authed); return NULL; }
    args = json_object();
    msf_arg_str(args, "raw_input", json_get_str(st, "raw_input", ""));
    msf_arg_copy(args, "authenticated_providers", authed);
    msf_arg_copy(args, "user_providers", json_obj_get(st, "user_providers"));
    msf_arg_copy(args, "custom_providers", json_obj_get(st, "custom_providers"));
    json_free(authed);
    json_t *fallback = msf_call(ctx, "resolve_alias_fallback", args);
    if (msf_exc_pending(ctx)) {
        json_free(fallback);
        if (msf_exc_is(ctx, "AmbiguousAliasError")) {
            char *msg = msf_ambiguous_alias_message(ctx->exc);
            msf_exc_clear(ctx);
            json_t *jm = json_string(msg);
            free(msg);
            return msf_fail(ctx, st, jm, NULL);
        }
        return NULL;
    }
    if (msf_is_none(fallback)) {
        json_free(fallback);
        const msf_model_alias *id = msf_model_alias_get(key);
        msf_sb sb; msf_sb_init(&sb);
        msf_sb_appendf(&sb, "Alias '%s' maps to %s/%s but no matching model was found in any "
                            "provider catalog. Try specifying the full model name.",
                       key, id ? id->vendor : "", id ? id->family : "");
        return msf_fail(ctx, st, msf_sb_json(&sb), NULL);
    }
    json_set(st, "target_provider", json_copy(json_get(fallback, 0)));
    json_set(st, "new_model", json_copy(json_get(fallback, 1)));
    json_set(st, "resolved_alias", json_copy(json_get(fallback, 2)));
    json_free(fallback);
    return NULL;
}

/* ── ported leaves ───────────────────────────────────────────────────────── */

/* PoP: msf_provider_identity @ hermes_cli/model_switch.py:_provider_identity */
json_t *msf_provider_identity(msf_ctx *ctx, const char *name, const json_t *user_providers,
                              const json_t *custom_providers) {
    json_t *pdef = NULL;
    if (name && name[0]) {
        json_t *args = json_object();
        msf_arg_str(args, "name", name);
        msf_arg_copy(args, "user_providers", user_providers);
        msf_arg_copy(args, "custom_providers", custom_providers);
        pdef = msf_call(ctx, "resolve_provider_full", args);
        if (msf_exc_pending(ctx)) { json_free(pdef); return NULL; }
    }
    if (!msf_is_none(pdef)) {
        json_t *id = json_string(json_get_str(pdef, "id", ""));
        json_free(pdef);
        return id;
    }
    json_free(pdef);
    json_t *args = json_object();
    msf_arg_str(args, "name", name ? name : "");
    return msf_call(ctx, "normalize_provider", args);
}

/* PoP: msf_external_process_catalog @ hermes_cli/model_switch.py:_external_process_catalog */
json_t *msf_external_process_catalog(msf_ctx *ctx, const char *provider) {
    json_t *args = json_object();
    msf_arg_str(args, "provider", provider);
    json_t *profile = msf_call(ctx, "get_provider_profile", args);
    if (msf_exc_pending(ctx)) { json_free(profile); return NULL; }
    json_t *out = json_array();
    json_t *catalog = json_array();
    json_t *aliases = json_object();
    if (!msf_is_none(profile) && msf_str_eq(json_get_str(profile, "auth_type", ""), "external_process")) {
        json_t *fm = json_obj_get(profile, "fallback_models");
        json_t *ma = json_obj_get(profile, "model_aliases");
        if (!msf_is_arr(fm) || !msf_is_obj(ma)) {
            msf_raise(ctx, "AttributeError");
            json_free(profile); json_free(catalog); json_free(aliases); json_free(out);
            return NULL;
        }
        for (size_t i = 0; i < fm->c.count; i++) json_append(catalog, json_copy(fm->c.items[i]));
        for (size_t i = 0; i < ma->c.count; i++) {
            char *lk = msf_lower(ma->c.keys[i]);
            json_set(aliases, lk, json_copy(ma->c.items[i]));
            free(lk);
        }
    }
    json_free(profile);
    json_append(out, catalog);
    json_append(out, aliases);
    return out;
}

/* PoP: msf_external_process_match @ hermes_cli/model_switch.py:_external_process_match */
json_t *msf_external_process_match(msf_ctx *ctx, const json_t *catalog, const json_t *aliases,
                                   const char *typed, const char *provider) {
    char *wanted = msf_strip_lower(typed);
    json_t *hit = json_obj_get(aliases, wanted);
    if (hit) {
        json_t *r = json_copy(hit);
        free(wanted);
        return r;
    }
    json_t *matches = json_array();
    const char *exact = NULL;
    if (msf_is_arr(catalog)) {
        for (size_t i = 0; i < catalog->c.count; i++) {
            const char *m = msf_jstr(catalog->c.items[i]);
            char *ml = msf_lower(m);
            if (!exact && msf_str_eq(ml, wanted)) exact = m;
            if (msf_starts_with(ml, wanted)) json_append(matches, json_string(m));
            free(ml);
        }
    }
    free(wanted);
    if (exact) {
        json_t *r = json_string(exact);
        json_free(matches);
        return r;
    }
    if (matches->c.count > 1) {
        char *w = msf_strip_lower(typed);
        msf_raise_ambiguous(ctx, w, provider, matches);
        free(w);
        json_free(matches);
        return NULL;
    }
    if (matches->c.count == 1) {
        json_t *r = json_copy(json_get(matches, 0));
        json_free(matches);
        return r;
    }
    json_free(matches);
    return NULL;
}

/* PoP: msf_aggregator_catalog_match @ hermes_cli/model_switch.py:_aggregator_catalog_match */
json_t *msf_aggregator_catalog_match(msf_ctx *ctx, const char *new_model, const json_t *catalog) {
    (void)ctx;
    char *wanted = msf_lower(new_model);
    const char *r1 = NULL;
    const char *r2 = NULL;
    if (msf_is_arr(catalog)) {
        for (size_t i = 0; i < catalog->c.count; i++) {
            const char *mid = msf_jstr(catalog->c.items[i]);
            char *ml = msf_lower(mid);
            if (!r1 && msf_str_eq(ml, wanted)) r1 = mid;
            if (!r2) {
                const char *slash = strchr(mid, '/');
                if (slash) {
                    char *bl = msf_lower(slash + 1);
                    if (msf_str_eq(bl, wanted)) r2 = mid;
                    free(bl);
                }
            }
            free(ml);
        }
    }
    free(wanted);
    /* next(...) or next(...) — an EMPTY first match falls through. */
    if (r1 && r1[0]) return json_string(r1);
    if (r2) return json_string(r2);
    return NULL;
}

/* PoP: msf_config_declares_model @ hermes_cli/model_switch.py:_config_declares_model */
json_t *msf_config_declares_model(msf_ctx *ctx, const char *new_model, const char *target_provider,
                                  const char *base_url, const json_t *user_providers,
                                  const json_t *custom_providers) {
    if (msf_truthy(user_providers)) {
        json_t *cfg = json_obj_get(user_providers, target_provider);
        if (!msf_is_none(cfg)) {
            json_t *args = json_object();
            msf_arg_copy(args, "cfg", cfg);
            json_t *enabled = msf_call(ctx, "is_provider_enabled", args);
            if (msf_exc_pending(ctx)) { json_free(enabled); return NULL; }
            bool en = msf_truthy(enabled);
            json_free(enabled);
            if (en) {
                json_t *models = json_obj_get(cfg, "models");
                json_t *ids = msf_declared_model_ids(models);
                bool hit = msf_arr_has_str(ids, new_model);
                json_free(ids);
                if (hit) return json_bool(true);
            }
        }
    }
    char *tp_lower = msf_lower(target_provider);
    json_t *entries = msf_custom_entries(custom_providers);
    bool found = false;
    for (size_t i = 0; i < entries->c.count && !found; i++) {
        json_t *entry = entries->c.items[i];
        json_t *slugs = msf_entry_aliases(ctx, entry);
        if (msf_exc_pending(ctx)) { json_free(slugs); break; }
        bool slug_hit = msf_arr_has_str(slugs, tp_lower);
        json_free(slugs);
        bool url_hit = msf_str_eq(json_get_str(entry, "base_url", ""), base_url);
        if (slug_hit || url_hit) {
            bool model_hit = msf_str_eq(json_get_str(entry, "model", ""), new_model);
            if (!model_hit) {
                json_t *models = json_obj_get(entry, "models");
                json_t *ids = msf_declared_model_ids(models);
                model_hit = msf_arr_has_str(ids, new_model);
                json_free(ids);
            }
            if (model_hit) found = true;
        }
    }
    json_free(entries);
    free(tp_lower);
    if (msf_exc_pending(ctx)) return NULL;
    return json_bool(found);
}

/* PoP: msf_unknown_provider_message @ hermes_cli/model_switch.py:_unknown_provider_message */
json_t *msf_unknown_provider_message(msf_ctx *ctx, const char *explicit_provider) {
    json_t *args = json_object();
    msf_arg_str(args, "provider", explicit_provider);
    json_t *moved = msf_call(ctx, "provider_install_hint", args);
    if (msf_exc_pending(ctx)) { json_free(moved); return NULL; }
    if (msf_truthy(moved)) {
        msf_sb sb; msf_sb_init(&sb);
        msf_sb_appendf(&sb, "Unknown provider '%s'. %s", explicit_provider, msf_jstr(moved));
        json_free(moved);
        return msf_sb_json(&sb);
    }
    json_free(moved);
    msf_sb sb; msf_sb_init(&sb);
    msf_sb_appendf(&sb, "Unknown provider '%s'. Check 'hermes model' for available "
                        "providers, or define it in config.yaml under 'providers:'.",
                   explicit_provider);
    json_t *issues = msf_call(ctx, "validate_config_structure", json_object());
    if (msf_exc_pending(ctx)) {
        json_free(issues);
        if (msf_exc_is_exception(ctx)) msf_exc_clear(ctx);
        else { free(sb.buf); return NULL; }
    } else if (msf_truthy(issues)) {
        msf_sb_append(&sb, "\n\nRun 'hermes doctor' \xe2\x80\x94 config issues detected:");
        size_t n = issues->c.count < 3 ? issues->c.count : 3;
        for (size_t i = 0; i < n; i++)
            msf_sb_appendf(&sb, "\n  \xe2\x80\xa2 %s",
                           json_get_str(issues->c.items[i], "message", ""));
    }
    json_free(issues);
    return msf_sb_json(&sb);
}

/* PoP: msf_aggregator_alias_error @ hermes_cli/model_switch.py:_aggregator_alias_error */
json_t *msf_aggregator_alias_error(msf_ctx *ctx, const char *explicit_provider, const char *target_provider,
                                   const char *current_provider, const json_t *user_providers,
                                   const json_t *custom_providers) {
    char *explicit_norm = msf_strip_lower(explicit_provider);
    const json_t *aliases = msf_is_obj(ctx->env) ? json_obj_get(ctx->env, "provider_aliases") : NULL;
    const json_t *aggs = msf_is_obj(ctx->env) ? json_obj_get(ctx->env, "aggregators") : NULL;
    const char *alias_target = msf_is_obj(aliases) ? json_get_str(aliases, explicit_norm, NULL) : NULL;
    bool guard = alias_target && alias_target[0]
                 && msf_str_eq(alias_target, target_provider)
                 && !msf_str_eq(target_provider, explicit_norm)
                 && msf_arr_has_str(aggs, target_provider);
    if (!guard) { free(explicit_norm); return json_string(""); }
    json_t *args = json_object();
    msf_arg_str(args, "current_provider", current_provider);
    msf_arg_copy(args, "user_providers", user_providers);
    msf_arg_copy(args, "custom_providers", custom_providers);
    json_t *authed = msf_call(ctx, "get_authenticated_provider_slugs", args);
    if (msf_exc_pending(ctx)) { json_free(authed); free(explicit_norm); return NULL; }
    if (msf_arr_has_str(authed, target_provider)) {
        json_free(authed); free(explicit_norm);
        return json_string("");
    }
    msf_sb suggestions; msf_sb_init(&suggestions);
    size_t nsugg = 0;
    if (msf_is_arr(authed)) {
        for (size_t i = 0; i < authed->c.count; i++) {
            const char *s = msf_jstr(authed->c.items[i]);
            if (msf_starts_with(s, explicit_norm) && !msf_str_eq(s, explicit_norm)) {
                if (nsugg) msf_sb_append(&suggestions, ", ");
                msf_sb_append(&suggestions, s);
                nsugg++;
            }
        }
    }
    json_free(authed);
    args = json_object();
    msf_arg_str(args, "provider", target_provider);
    json_t *label = msf_call(ctx, "get_label", args);
    if (msf_exc_pending(ctx)) {
        json_free(label); free(suggestions.buf); free(explicit_norm);
        return NULL;
    }
    msf_sb sb; msf_sb_init(&sb);
    /* Python f-string renders a None label as "None", not as an empty string. */
    const char *label_s = (label && label->type != JSON_NULL) ? msf_jstr(label) : "None";
    msf_sb_appendf(&sb, "Provider '%s' is an alias that routes through %s, which "
                        "has no credentials configured.", explicit_norm, label_s);
    if (nsugg) msf_sb_appendf(&sb, " Did you mean: %s?", suggestions.buf);
    json_free(label);
    free(suggestions.buf);
    free(explicit_norm);
    return msf_sb_json(&sb);
}

/* PoP: msf_resolve_alias @ hermes_cli/model_switch.py:resolve_alias */
json_t *msf_resolve_alias(msf_ctx *ctx, const char *raw_input, const char *current_provider,
                          const json_t *user_providers, const json_t *custom_providers) {
    char *key = msf_strip_lower(raw_input);
    json_t *result = NULL;
    json_t *args = NULL;
    json_t *current_id = NULL;
    json_t *pc = NULL;
    json_t *catalog = NULL;
    json_t *matches = NULL;
    char *prefix = NULL;
    char *r_prov = NULL, *r_model = NULL, *r_name = NULL;
    bool have_reverse = false;

    msf_call(ctx, "ensure_direct_aliases", json_object());
    if (msf_exc_pending(ctx)) goto out;

    const json_t *das = msf_is_obj(ctx->env) ? json_obj_get(ctx->env, "direct_aliases") : NULL;
    json_t *direct = msf_is_obj(das) ? json_obj_get(das, key) : NULL;
    if (direct) {
        result = json_array();
        json_append(result, json_string(json_get_str(direct, "provider", "")));
        json_append(result, json_string(json_get_str(direct, "model", "")));
        json_append(result, json_string(key));
        goto out;
    }

    /* Reverse lookup: prefer the alias served by the current provider. */
    current_id = msf_provider_identity(ctx, current_provider, user_providers, custom_providers);
    if (msf_exc_pending(ctx)) goto out;
    if (msf_is_obj(das)) {
        for (size_t i = 0; i < das->c.count; i++) {
            const char *alias_name = das->c.keys[i];
            json_t *da = das->c.items[i];
            char *ml = msf_lower(json_get_str(da, "model", ""));
            bool match = msf_str_eq(ml, key);
            free(ml);
            if (!match) continue;
            json_t *pid = msf_provider_identity(ctx, json_get_str(da, "provider", ""),
                                                user_providers, custom_providers);
            if (msf_exc_pending(ctx)) { json_free(pid); goto out; }
            bool same = msf_str_eq(msf_jstr(pid), msf_jstr(current_id));
            json_free(pid);
            if (same) {
                result = json_array();
                json_append(result, json_string(json_get_str(da, "provider", "")));
                json_append(result, json_string(json_get_str(da, "model", "")));
                json_append(result, json_string(alias_name));
                goto out;
            }
            if (!have_reverse) {
                r_prov = msf_strdup(json_get_str(da, "provider", ""));
                r_model = msf_strdup(json_get_str(da, "model", ""));
                r_name = msf_strdup(alias_name);
                have_reverse = true;
            }
        }
    }
    if (have_reverse) {
        result = json_array();
        json_append(result, json_string(r_prov));
        json_append(result, json_string(r_model));
        json_append(result, json_string(r_name));
        goto out;
    }

    pc = msf_external_process_catalog(ctx, current_provider);
    if (msf_exc_pending(ctx)) goto out;
    json_t *pcat = json_get(pc, 0);
    json_t *paliases = json_get(pc, 1);
    bool has_process = msf_is_arr(pcat) && pcat->c.count > 0;
    if (has_process) {
        json_t *declared = msf_external_process_match(ctx, pcat, paliases, key, current_provider);
        if (msf_exc_pending(ctx)) { json_free(declared); goto out; }
        if (declared) {
            result = json_array();
            json_append(result, json_string(current_provider));
            json_append(result, declared);
            json_append(result, json_string(key));
            goto out;
        }
    }

    const msf_model_alias *identity = msf_model_alias_get(key);
    if (!identity) goto out; /* result stays NULL */

    if (has_process) {
        json_t *declared = msf_external_process_match(ctx, pcat, paliases, identity->family,
                                                      current_provider);
        if (msf_exc_pending(ctx)) { json_free(declared); goto out; }
        if (msf_truthy(declared)) {
            result = json_array();
            json_append(result, json_string(current_provider));
            json_append(result, declared);
            json_append(result, json_string(key));
        } else {
            json_free(declared);
        }
        goto out;
    }

    args = json_object();
    msf_arg_str(args, "provider", current_provider);
    catalog = msf_call(ctx, "list_provider_models", args);
    if (msf_exc_pending(ctx)) goto out;
    if (!msf_is_arr(catalog)) { msf_raise(ctx, "TypeError"); goto out; }

    /* try: merge static_catalog entries the live catalog may be missing. */
    {
        json_t *seen = json_array();
        for (size_t i = 0; i < catalog->c.count; i++) {
            char *ml = msf_lower(msf_jstr(catalog->c.items[i]));
            json_append(seen, json_string(ml));
            free(ml);
        }
        args = json_object();
        msf_arg_str(args, "provider", current_provider);
        json_t *sc = msf_call(ctx, "static_catalog", args);
        if (msf_exc_pending(ctx)) {
            json_free(sc);
            if (msf_exc_is_exception(ctx)) msf_exc_clear(ctx);
            else { json_free(seen); goto out; }
        } else if (msf_is_arr(sc)) {
            for (size_t i = 0; i < sc->c.count; i++) {
                const char *m = msf_jstr(sc->c.items[i]);
                char *ml = msf_lower(m);
                if (!msf_arr_has_str(seen, ml)) json_append(catalog, json_string(m));
                free(ml);
            }
        }
        json_free(sc);
        json_free(seen);
    }

    {
        args = json_object();
        msf_arg_str(args, "provider", current_provider);
        json_t *agg = msf_call(ctx, "is_aggregator", args);
        if (msf_exc_pending(ctx)) { json_free(agg); goto out; }
        bool is_agg = msf_truthy(agg);
        json_free(agg);
        msf_sb pb; msf_sb_init(&pb);
        if (is_agg) msf_sb_appendf(&pb, "%s/%s", identity->vendor, identity->family);
        else msf_sb_append(&pb, identity->family);
        prefix = msf_sb_take(&pb);
    }
    {
        char *pl = msf_lower(prefix);
        matches = json_array();
        for (size_t i = 0; i < catalog->c.count; i++) {
            const char *mid = msf_jstr(catalog->c.items[i]);
            char *ml = msf_lower(mid);
            if (msf_starts_with(ml, pl)) json_append(matches, json_string(mid));
            free(ml);
        }
        free(pl);
    }
    if (matches->c.count == 0) goto out;
    msf_sort_matches(matches, prefix, ctx);
    if (msf_exc_pending(ctx)) goto out;
    if (matches->c.count > 1) {
        msf_raise_ambiguous(ctx, key, current_provider, matches);
        goto out;
    }
    result = json_array();
    json_append(result, json_string(current_provider));
    json_append(result, json_copy(json_get(matches, 0)));
    json_append(result, json_string(key));

out:
    free(key);
    free(prefix);
    free(r_prov); free(r_model); free(r_name);
    json_free(current_id);
    json_free(pc);
    json_free(catalog);
    json_free(matches);
    return result;
}

/* PoP: msf_apply_direct_alias_endpoint @ hermes_cli/model_switch.py:_apply_direct_alias_endpoint */
json_t *msf_apply_direct_alias_endpoint(msf_ctx *ctx, json_t *st, const json_t *da) {
    json_t *args = json_object();
    msf_arg_copy(args, "alias", da);
    json_t *akv = msf_call(ctx, "direct_alias_api_key", args);
    if (msf_exc_pending(ctx)) { json_free(akv); return NULL; }
    char *alias_key = msf_strdup(msf_jstr(akv));
    json_free(akv);

    args = json_object();
    msf_arg_str(args, "session_base_url", json_get_str(st, "base_url", ""));
    msf_arg_str(args, "alias_base_url", json_get_str(da, "base_url", ""));
    json_t *shv = msf_call(ctx, "may_reuse_session_credential", args);
    if (msf_exc_pending(ctx)) { json_free(shv); free(alias_key); return NULL; }
    bool same_host = msf_truthy(shv);
    json_free(shv);

    const char *da_base = json_get_str(da, "base_url", "");
    if (alias_key[0]) {
        json_set(st, "base_url", json_string(da_base));
        json_set(st, "api_key", json_string(alias_key));
    } else {
        const char *cur_key = json_get_str(st, "api_key", "");
        if (cur_key[0] && !msf_str_eq(cur_key, "no-key-required") && same_host) {
            json_set(st, "base_url", json_string(da_base));
        } else {
            json_t *runtime = NULL;
            args = json_object();
            msf_arg_copy(args, "alias", da);
            json_t *reqv = msf_call(ctx, "direct_alias_runtime_request", args);
            if (msf_exc_pending(ctx)) {
                json_free(reqv);
                if (msf_exc_is_exception(ctx)) { msf_exc_clear(ctx); runtime = json_object(); }
                else { free(alias_key); return NULL; }
            } else {
                args = json_object();
                msf_arg_str(args, "requested", msf_jstr(json_get(reqv, 0)));
                msf_arg_copy(args, "explicit_api_key", json_get(reqv, 1));
                msf_arg_str(args, "explicit_base_url", da_base);
                msf_arg_str(args, "target_model", json_get_str(st, "new_model", ""));
                json_free(reqv);
                runtime = msf_call(ctx, "resolve_runtime_provider", args);
                if (msf_exc_pending(ctx)) {
                    json_free(runtime);
                    if (msf_exc_is_exception(ctx)) { msf_exc_clear(ctx); runtime = json_object(); }
                    else { free(alias_key); return NULL; }
                } else if (msf_is_none(runtime)) {
                    json_free(runtime);
                    msf_raise(ctx, "AttributeError");
                    free(alias_key);
                    return NULL;
                }
            }
            const char *rb = json_get_str(runtime, "base_url", "");
            json_set(st, "base_url", json_string(rb[0] ? rb : da_base));
            const char *rk = json_get_str(runtime, "api_key", "");
            if (msf_str_eq(rk, "no-key-required")) rk = "";
            cur_key = json_get_str(st, "api_key", "");
            const char *chosen = rk[0] ? rk : (same_host ? cur_key : "");
            if (!chosen[0]) chosen = "no-key-required";
            json_set(st, "api_key", json_string(chosen));
            json_free(runtime);
        }
    }

    /* providers.ollama refinement (skipped when the alias declared a key). */
    char *tp = msf_strip_lower(json_get_str(st, "target_provider", ""));
    bool ollama = msf_str_eq(tp, "ollama");
    free(tp);
    if (!alias_key[0] && ollama) {
        json_t *oc = msf_call(ctx, "ollama_configured_base", json_object());
        if (msf_exc_pending(ctx)) { json_free(oc); free(alias_key); return NULL; }
        json_t *ocfg = json_get(oc, 0);
        const char *obase = msf_jstr(json_get(oc, 1));
        bool native = false;
        if (obase[0]) {
            args = json_object();
            msf_arg_str(args, "base_url", json_get_str(st, "base_url", ""));
            msf_arg_str(args, "configured_base", obase);
            json_t *sn = msf_call(ctx, "same_ollama_native_root", args);
            if (msf_exc_pending(ctx)) { json_free(sn); json_free(oc); free(alias_key); return NULL; }
            native = msf_truthy(sn);
            json_free(sn);
        }
        if (native) {
            char *ck = msf_entry_configured_key(ctx, ocfg);
            if (!ck) { json_free(oc); free(alias_key); return NULL; }
            if (ck[0]) json_set(st, "api_key", json_string(ck));
            free(ck);
        } else {
            json_set(st, "validation_headers", json_object());
            json_set(st, "suppress_ollama_headers", json_bool(true));
            json_set(st, "api_key", json_string("no-key-required"));
        }
        json_free(oc);
    }
    free(alias_key);
    if (!json_get_str(st, "api_key", "")[0])
        json_set(st, "api_key", json_string("no-key-required"));
    json_set(st, "api_mode", json_string(""));
    return NULL;
}

/* PoP: msf_route_explicit_provider @ hermes_cli/model_switch.py:_route_explicit_provider */
json_t *msf_route_explicit_provider(msf_ctx *ctx, json_t *st) {
    const char *ep = json_get_str(st, "explicit_provider", "");
    json_t *args = json_object();
    msf_arg_str(args, "name", ep);
    msf_arg_copy(args, "user_providers", json_obj_get(st, "user_providers"));
    msf_arg_copy(args, "custom_providers", json_obj_get(st, "custom_providers"));
    json_t *pdef = msf_call(ctx, "resolve_provider_full", args);
    if (msf_exc_pending(ctx)) { json_free(pdef); return NULL; }
    if (msf_is_none(pdef)) {
        char *epl = msf_strip_lower(ep);
        bool is_custom = msf_str_eq(epl, "custom");
        free(epl);
        if (is_custom) {
            json_free(pdef);
            pdef = msf_bare_custom_provider_def(json_get_str(st, "current_base_url", ""));
        }
    }
    if (msf_is_none(pdef)) {
        json_free(pdef);
        json_t *msg = msf_unknown_provider_message(ctx, ep);
        if (msf_exc_pending(ctx)) { json_free(msg); return NULL; }
        return msf_fail(ctx, st, msg, NULL);
    }
    json_t *pid = json_obj_get(pdef, "id");
    json_t *pname = json_obj_get(pdef, "name");
    if (!pid || !pname) {
        msf_raise(ctx, "AttributeError");
        json_free(pdef);
        return NULL;
    }
    json_set(st, "target_provider", json_copy(pid));
    json_set(st, "provider_label", json_copy(pname));

    if (msf_str_eq(json_get_str(st, "target_provider", ""), "moa")
        && !json_get_str(st, "new_model", "")[0]) {
        json_t *mp = msf_call(ctx, "moa_default_preset", json_object());
        if (msf_exc_pending(ctx)) { json_free(mp); json_free(pdef); return NULL; }
        json_set(st, "new_model", mp ? mp : json_null());
    }

    json_t *agg_err = msf_aggregator_alias_error(ctx, ep, json_get_str(st, "target_provider", ""),
                                                 json_get_str(st, "current_provider", ""),
                                                 json_obj_get(st, "user_providers"),
                                                 json_obj_get(st, "custom_providers"));
    if (msf_exc_pending(ctx)) { json_free(agg_err); json_free(pdef); return NULL; }
    if (msf_truthy(agg_err)) {
        json_free(pdef);
        return msf_fail_on_target(ctx, st, agg_err);
    }
    json_free(agg_err);

    if (!json_get_str(st, "new_model", "")[0]) {
        const char *pbase = json_get_str(pdef, "base_url", "");
        if (!pbase[0]) {
            msf_sb sb; msf_sb_init(&sb);
            msf_sb_appendf(&sb, "Provider '%s' has no base URL configured. "
                                "Specify a model: /model <model-name> --provider %s",
                           msf_jstr(pname), ep);
            json_free(pdef);
            return msf_fail_on_target(ctx, st, msf_sb_json(&sb));
        }
        args = json_object();
        msf_arg_str(args, "base_url", pbase);
        json_t *dm = msf_call(ctx, "auto_detect_local_model", args);
        if (msf_exc_pending(ctx)) { json_free(dm); json_free(pdef); return NULL; }
        json_set(st, "new_model", dm ? dm : json_null());
        if (!json_get_str(st, "new_model", "")[0]) {
            msf_sb sb; msf_sb_init(&sb);
            msf_sb_appendf(&sb, "No model detected on %s (%s). "
                                "Specify the model explicitly: /model <model-name> --provider %s",
                           msf_jstr(pname), pbase, ep);
            json_free(pdef);
            return msf_fail_on_target(ctx, st, msf_sb_json(&sb));
        }
    }

    json_t *alias_result = msf_resolve_alias(ctx, json_get_str(st, "new_model", ""),
                                             json_get_str(st, "target_provider", ""),
                                             json_obj_get(st, "user_providers"),
                                             json_obj_get(st, "custom_providers"));
    if (msf_exc_pending(ctx)) {
        json_free(alias_result);
        json_free(pdef);
        if (msf_exc_is(ctx, "AmbiguousAliasError")) {
            char *msg = msf_ambiguous_alias_message(ctx->exc);
            msf_exc_clear(ctx);
            json_t *fields = json_object();
            json_set(fields, "target_provider", json_copy(json_obj_get(st, "target_provider")));
            json_t *jm = json_string(msg);
            free(msg);
            json_t *r = msf_fail(ctx, st, jm, fields);
            json_free(fields);
            return r;
        }
        return NULL;
    }
    if (alias_result) {
        const char *alias_provider = msf_jstr(json_get(alias_result, 0));
        json_set(st, "new_model", json_copy(json_get(alias_result, 1)));
        const char *alias_name = msf_jstr(json_get(alias_result, 2));
        json_t *id1 = msf_provider_identity(ctx, alias_provider, json_obj_get(st, "user_providers"),
                                            json_obj_get(st, "custom_providers"));
        if (msf_exc_pending(ctx)) { json_free(id1); json_free(alias_result); json_free(pdef); return NULL; }
        json_t *id2 = msf_provider_identity(ctx, json_get_str(st, "target_provider", ""),
                                            json_obj_get(st, "user_providers"),
                                            json_obj_get(st, "custom_providers"));
        if (msf_exc_pending(ctx)) { json_free(id1); json_free(id2); json_free(alias_result); json_free(pdef); return NULL; }
        if (msf_str_eq(msf_jstr(id1), msf_jstr(id2)))
            json_set(st, "resolved_alias", json_string(alias_name));
        json_free(id1);
        json_free(id2);
        json_free(alias_result);
    }
    json_free(pdef);
    return NULL;
}

/* PoP: msf_route_configured_provider @ hermes_cli/model_switch.py:_route_configured_provider */
json_t *msf_route_configured_provider(msf_ctx *ctx, json_t *st) {
    json_t *args = json_object();
    msf_arg_str(args, "model_name", json_get_str(st, "new_model", ""));
    msf_arg_copy(args, "user_providers", json_obj_get(st, "user_providers"));
    msf_arg_copy(args, "custom_providers", json_obj_get(st, "custom_providers"));
    json_t *cm = msf_call(ctx, "configured_provider_matches", args);
    if (msf_exc_pending(ctx)) { json_free(cm); return NULL; }
    if (!msf_truthy(cm)) { json_free(cm); return json_bool(false); }

    char *slug = msf_current_provider_match(ctx, st, cm);
    if (msf_exc_pending(ctx)) { free(slug); json_free(cm); return NULL; }
    if (slug) {
        json_set(st, "new_model", json_copy(json_obj_get(cm, slug)));
        free(slug);
        json_free(cm);
        return json_bool(true);
    }

    size_t n = cm->c.count;
    const char **slugs = (const char **)calloc(n, sizeof(char *));
    for (size_t i = 0; i < n; i++) slugs[i] = cm->c.keys[i];
    /* sorted(cfg_matches) — unique keys, order-only sort. */
    for (size_t i = 1; i < n; i++) {
        const char *s = slugs[i];
        size_t j = i;
        while (j > 0 && strcmp(slugs[j - 1], s) > 0) { slugs[j] = slugs[j - 1]; j--; }
        slugs[j] = s;
    }
    if (n > 1) {
        msf_sb sb; msf_sb_init(&sb);
        msf_sb_appendf(&sb, "'%s' is declared by multiple configured providers (",
                       json_get_str(st, "new_model", ""));
        for (size_t i = 0; i < n; i++) {
            if (i) msf_sb_append(&sb, ", ");
            msf_sb_append(&sb, slugs[i]);
        }
        msf_sb_append(&sb, "). Re-run with --provider <slug> to choose which one to use.");
        free(slugs);
        json_free(cm);
        return msf_fail(ctx, st, msf_sb_json(&sb), NULL);
    }
    const char *tp = slugs[0];
    json_set(st, "target_provider", json_string(tp));
    json_set(st, "new_model", json_copy(json_obj_get(cm, tp)));
    free(slugs);

    const json_t *up = json_obj_get(st, "user_providers");
    if (msf_is_obj(up) && json_obj_get(up, json_get_str(st, "target_provider", ""))) {
        args = json_object();
        msf_arg_str(args, "target_provider", json_get_str(st, "target_provider", ""));
        msf_arg_copy(args, "user_providers", up);
        json_t *persisted = msf_call(ctx, "persisted_provider_id", args);
        if (msf_exc_pending(ctx)) { json_free(persisted); json_free(cm); return NULL; }
        const char *p = msf_jstr(persisted);
        if (!msf_str_eq(p, json_get_str(st, "target_provider", "")))
            json_set(st, "target_provider", json_string(p));
        else
            json_set(st, "explicit_provider", json_copy(json_obj_get(st, "target_provider")));
        json_free(persisted);
    }
    json_free(cm);
    return json_bool(true);
}

/* PoP: msf_route_from_model_input @ hermes_cli/model_switch.py:_route_from_model_input */
json_t *msf_route_from_model_input(msf_ctx *ctx, json_t *st) {
    const char *raw = json_get_str(st, "raw_input", "");
    const char *cur = json_get_str(st, "current_provider", "");
    json_t *moa_match = NULL;

    /* MoA preset probe — any Exception drops to plain alias resolution. */
    json_t *cfg = msf_call(ctx, "load_config", json_object());
    if (msf_exc_pending(ctx)) {
        json_free(cfg);
        if (!msf_exc_is_exception(ctx)) return NULL;
        msf_exc_clear(ctx);
    } else if (!msf_is_none(cfg)) {
        json_t *moa = json_obj_get(cfg, "moa");
        json_t *args = json_object();
        json_set(args, "cfg", msf_truthy(moa) ? json_copy(moa) : json_object());
        json_t *norm = msf_call(ctx, "normalize_moa_config", args);
        if (msf_exc_pending(ctx)) {
            json_free(norm);
            if (!msf_exc_is_exception(ctx)) { json_free(cfg); return NULL; }
            msf_exc_clear(ctx);
        } else {
            args = json_object();
            msf_arg_copy(args, "normalized", norm);
            msf_arg_str(args, "raw_input", raw);
            moa_match = msf_call(ctx, "exact_moa_preset_name", args);
            json_free(norm);
            if (msf_exc_pending(ctx)) {
                json_free(moa_match);
                moa_match = NULL;
                if (!msf_exc_is_exception(ctx)) { json_free(cfg); return NULL; }
                msf_exc_clear(ctx);
            }
        }
    }
    json_free(cfg);

    if (msf_truthy(moa_match)) {
        json_set(st, "target_provider", json_string("moa"));
        json_set(st, "new_model", moa_match);
        moa_match = NULL;
        json_set(st, "resolved_alias", json_string(""));
    } else {
        json_free(moa_match);
        json_t *alias_result = msf_resolve_alias(ctx, raw, cur, json_obj_get(st, "user_providers"),
                                                 json_obj_get(st, "custom_providers"));
        if (msf_exc_pending(ctx)) {
            json_free(alias_result);
            if (msf_exc_is(ctx, "AmbiguousAliasError")) {
                char *msg = msf_ambiguous_alias_message(ctx->exc);
                msf_exc_clear(ctx);
                json_t *jm = json_string(msg);
                free(msg);
                return msf_fail(ctx, st, jm, NULL);
            }
            return NULL;
        }
        if (alias_result) {
            json_set(st, "target_provider", json_copy(json_get(alias_result, 0)));
            json_set(st, "new_model", json_copy(json_get(alias_result, 1)));
            json_set(st, "resolved_alias", json_copy(json_get(alias_result, 2)));
            json_free(alias_result);
        } else {
            char *key = msf_strip_lower(raw);
            if (msf_model_alias_get(key)) {
                json_t *fail = msf_route_alias_fallback(ctx, st, key);
                free(key);
                if (fail) return fail;
                if (msf_exc_pending(ctx)) return NULL;
            } else {
                free(key);
                msf_convert_vendor_colon_slug(ctx, st);
                if (msf_exc_pending(ctx)) return NULL;
            }
        }
    }

    /* Step d: the current provider's live catalog owns a resolved slug. */
    bool resolved_in_current_catalog = false;
    json_t *args = json_object();
    msf_arg_str(args, "provider", json_get_str(st, "target_provider", ""));
    json_t *agg = msf_call(ctx, "is_aggregator", args);
    if (msf_exc_pending(ctx)) { json_free(agg); return NULL; }
    if (msf_truthy(agg) && !json_get_str(st, "resolved_alias", "")[0]) {
        args = json_object();
        msf_arg_str(args, "provider", json_get_str(st, "target_provider", ""));
        json_t *cat = msf_call(ctx, "list_provider_models", args);
        if (msf_exc_pending(ctx)) { json_free(cat); json_free(agg); return NULL; }
        if (msf_truthy(cat)) {
            json_t *matched = msf_aggregator_catalog_match(ctx, json_get_str(st, "new_model", ""), cat);
            if (matched) {
                json_set(st, "new_model", matched);
                resolved_in_current_catalog = true;
            }
        }
        json_free(cat);
    }
    json_free(agg);

    if (json_get_str(st, "resolved_alias", "")[0] || resolved_in_current_catalog
        || !msf_str_eq(json_get_str(st, "target_provider", ""), cur))
        return NULL;

    if (msf_str_eq(cur, "nous")) {
        args = json_object();
        msf_arg_str(args, "base_url", json_get_str(st, "current_base_url", ""));
        json_t *wh = msf_call(ctx, "route_is_welcome_host", args);
        if (msf_exc_pending(ctx)) { json_free(wh); return NULL; }
        const char *guest = json_get_str(ctx->env, "guest_model", "nous/welcome");
        if (msf_truthy(wh) && !msf_str_eq(json_get_str(st, "new_model", ""), guest)) {
            json_free(wh);
            msf_sb sb; msf_sb_init(&sb);
            msf_sb_appendf(&sb, "%s needs a Nous account or an API key. "
                                "Use /login to sign in, or /model to pick another provider.",
                           json_get_str(st, "new_model", ""));
            return msf_fail(ctx, st, msf_sb_json(&sb), NULL);
        }
        json_free(wh);
    }

    json_t *config_routed = msf_route_configured_provider(ctx, st);
    if (msf_exc_pending(ctx)) { json_free(config_routed); return NULL; }
    if (config_routed && config_routed->type == JSON_OBJECT) return config_routed;
    bool routed = config_routed && config_routed->type == JSON_BOOL && config_routed->bool_val;
    json_free(config_routed);

    bool is_custom = msf_str_eq(cur, "custom") || msf_str_eq(cur, "local")
                     || msf_starts_with(cur, "custom:");
    if (!is_custom) {
        args = json_object();
        const char *cb = json_get_str(st, "current_base_url", "");
        msf_arg_str(args, "url", cb[0] ? cb : "");
        json_t *hn = msf_call(ctx, "base_url_hostname", args);
        if (msf_exc_pending(ctx)) { json_free(hn); return NULL; }
        const char *host = msf_jstr(hn);
        is_custom = msf_str_eq(host, "localhost") || msf_str_eq(host, "127.0.0.1");
        json_free(hn);
    }
    if (!routed && !is_custom) {
        args = json_object();
        msf_arg_str(args, "model_name", json_get_str(st, "new_model", ""));
        msf_arg_str(args, "current_provider", cur);
        json_t *det = msf_call(ctx, "detect_provider_for_model", args);
        if (msf_exc_pending(ctx)) { json_free(det); return NULL; }
        if (msf_truthy(det)) {
            json_set(st, "target_provider", json_copy(json_get(det, 0)));
            json_set(st, "new_model", json_copy(json_get(det, 1)));
        }
        json_free(det);
    }
    return NULL;
}

/* PoP: msf_validate_switch @ hermes_cli/model_switch.py:_validate_switch */
json_t *msf_validate_switch(msf_ctx *ctx, json_t *st) {
    char *nm = msf_resolve_named_custom_model_id(ctx, json_get_str(st, "new_model", ""),
                                                 json_get_str(st, "target_provider", ""),
                                                 json_obj_get(st, "custom_providers"));
    if (msf_exc_pending(ctx)) { free(nm); return NULL; }
    json_set(st, "new_model", json_string(nm));
    free(nm);

    json_t *args = json_object();
    msf_arg_str(args, "model", json_get_str(st, "new_model", ""));
    msf_arg_str(args, "provider", json_get_str(st, "target_provider", ""));
    json_t *norm = msf_call(ctx, "normalize_model_for_provider", args);
    if (msf_exc_pending(ctx)) { json_free(norm); return NULL; }
    json_set(st, "new_model", norm ? norm : json_null());

    args = json_object();
    msf_arg_str(args, "model", json_get_str(st, "new_model", ""));
    json_t *nc = msf_call(ctx, "is_known_non_chat_model", args);
    if (msf_exc_pending(ctx)) { json_free(nc); return NULL; }
    if (msf_truthy(nc)) {
        json_free(nc);
        msf_sb sb; msf_sb_init(&sb);
        msf_sb_appendf(&sb, "`%s` is a generation model and cannot be used for chat. "
                            "Pick a chat model, or use image generation for image models.",
                       json_get_str(st, "new_model", ""));
        json_t *fields = json_object();
        json_set(fields, "new_model", json_copy(json_obj_get(st, "new_model")));
        json_set(fields, "target_provider", json_copy(json_obj_get(st, "target_provider")));
        json_set(fields, "provider_label", json_copy(json_obj_get(st, "provider_label")));
        json_t *r = msf_fail(ctx, st, msf_sb_json(&sb), fields);
        json_free(fields);
        return r;
    }
    json_free(nc);

    /* Validation headers: ollama suppression / carried headers / config. */
    json_t *headers = NULL;
    char *tpl = msf_strip_lower(json_get_str(st, "target_provider", ""));
    bool ollama = msf_str_eq(tpl, "ollama");
    free(tpl);
    const json_t *vh = json_obj_get(st, "validation_headers");
    if (ollama) {
        if (json_get_bool(st, "suppress_ollama_headers", false)) headers = json_object();
        else if (msf_truthy(vh)) headers = json_copy(vh);
        else {
            headers = msf_call(ctx, "get_ollama_request_headers", json_object());
            if (msf_exc_pending(ctx)) { json_free(headers); return NULL; }
        }
    } else if (msf_truthy(vh)) {
        headers = json_copy(vh);
    } else {
        const json_t *up = json_obj_get(st, "user_providers");
        json_t *ucfg = msf_truthy(up) ? json_obj_get(up, json_get_str(st, "target_provider", "")) : NULL;
        if (ucfg) {
            args = json_object();
            msf_arg_copy(args, "cfg", ucfg);
            headers = msf_call(ctx, "extra_headers_from_config", args);
            if (msf_exc_pending(ctx)) { json_free(headers); return NULL; }
        }
    }

    /* A user-config endpoint validates as custom whether bare or prefixed. */
    char *validate_as = msf_strdup(json_get_str(st, "target_provider", ""));
    char *val = msf_lower(validate_as);
    bool custom_prefixed = msf_starts_with(val, "custom");
    free(val);
    if (!custom_prefixed) {
        args = json_object();
        msf_arg_str(args, "name", validate_as);
        msf_arg_copy(args, "user_providers", json_obj_get(st, "user_providers"));
        msf_arg_copy(args, "custom_providers", json_obj_get(st, "custom_providers"));
        json_t *pdef = msf_call(ctx, "resolve_provider_full", args);
        if (msf_exc_pending(ctx)) {
            json_free(pdef); json_free(headers); free(validate_as);
            return NULL;
        }
        if (!msf_is_none(pdef) && msf_str_eq(json_get_str(pdef, "source", ""), "user-config")
            && json_get_str(pdef, "base_url", "")[0]) {
            char *nv = (char *)malloc(strlen(validate_as) + 8);
            sprintf(nv, "custom:%s", validate_as);
            free(validate_as);
            validate_as = nv;
        }
        json_free(pdef);
    }

    args = json_object();
    msf_arg_str(args, "model", json_get_str(st, "new_model", ""));
    msf_arg_str(args, "provider", validate_as);
    msf_arg_str(args, "api_key", json_get_str(st, "api_key", ""));
    msf_arg_str(args, "base_url", json_get_str(st, "base_url", ""));
    if (json_get_str(st, "api_mode", "")[0])
        msf_arg_str(args, "api_mode", json_get_str(st, "api_mode", ""));
    msf_arg_copy(args, "headers", headers);
    json_free(headers);
    json_t *validation = msf_call(ctx, "validate_requested_model", args);
    if (msf_exc_pending(ctx)) {
        json_free(validation);
        if (!msf_exc_is_exception(ctx)) { free(validate_as); return NULL; }
        msf_sb sb; msf_sb_init(&sb);
        msf_sb_appendf(&sb, "Could not validate `%s`: %s", json_get_str(st, "new_model", ""),
                       json_get_str(ctx->exc, "msg", ""));
        msf_exc_clear(ctx);
        validation = json_object();
        json_set(validation, "accepted", json_bool(false));
        json_set(validation, "persist", json_bool(false));
        json_set(validation, "recognized", json_bool(false));
        json_set(validation, "message", msf_sb_json(&sb));
    }

    if (!msf_truthy(json_obj_get(validation, "accepted"))) {
        json_t *declares = msf_config_declares_model(ctx, json_get_str(st, "new_model", ""),
                                                     json_get_str(st, "target_provider", ""),
                                                     json_get_str(st, "base_url", ""),
                                                     json_obj_get(st, "user_providers"),
                                                     json_obj_get(st, "custom_providers"));
        if (msf_exc_pending(ctx)) {
            json_free(declares); json_free(validation); free(validate_as);
            return NULL;
        }
        if (!msf_truthy(declares)) {
            json_free(declares);
            json_t *msg = json_obj_get(validation, "message");
            json_t *msg_copy = msg ? json_copy(msg) : json_string("Invalid model");
            json_t *fields = json_object();
            json_set(fields, "new_model", json_copy(json_obj_get(st, "new_model")));
            json_set(fields, "target_provider", json_copy(json_obj_get(st, "target_provider")));
            json_set(fields, "provider_label", json_copy(json_obj_get(st, "provider_label")));
            json_free(validation);
            free(validate_as);
            json_t *r = msf_fail(ctx, st, msg_copy, fields);
            json_free(fields);
            return r;
        }
        json_free(declares);
        json_t *old_msg = json_obj_get(validation, "message");
        json_t *kept = old_msg ? json_copy(old_msg) : json_string("");
        json_free(validation);
        validation = json_object();
        json_set(validation, "accepted", json_bool(true));
        json_set(validation, "persist", json_bool(true));
        json_set(validation, "recognized", json_bool(false));
        json_set(validation, "message", kept);
    }
    json_set(st, "validation", validation);
    free(validate_as);
    return NULL;
}

/* ── config-write decision flows ─────────────────────────────────────────── */

/* Live route_identity constants (plain tuples; transcribed in source order). */
static const char *MSF_ENDPOINT_CREDENTIAL_KEYS[] = {"api_key", "api"};
static const char *MSF_ENDPOINT_KEY_POINTER_KEYS[] = {"key_env", "api_key_env"};

/* PoP: msf_selection_route_changed @ hermes_cli/model_switch.py:selection_route_changed */
json_t *msf_selection_route_changed(msf_ctx *ctx, const json_t *model_cfg, const json_t *result) {
    char *mp = msf_str_or_empty(json_obj_get(model_cfg, "provider"));
    char *mpl = msf_strip_lower(mp);
    free(mp);
    char *rp = msf_str_or_empty(json_obj_get(result, "target_provider"));
    char *rpl = msf_strip_lower(rp);
    free(rp);
    bool diff = !msf_str_eq(mpl, rpl);
    free(mpl);
    free(rpl);
    if (diff) return json_bool(true);
    json_t *args = json_object();
    msf_arg_copy(args, "base_url", json_obj_get(model_cfg, "base_url"));
    json_t *a = msf_call(ctx, "normalize_route_base_url", args);
    if (msf_exc_pending(ctx)) { json_free(a); return NULL; }
    args = json_object();
    msf_arg_copy(args, "base_url", json_obj_get(result, "base_url"));
    json_t *b = msf_call(ctx, "normalize_route_base_url", args);
    if (msf_exc_pending(ctx)) { json_free(a); json_free(b); return NULL; }
    bool changed = !msf_json_eq(a, b);
    json_free(a);
    json_free(b);
    return json_bool(changed);
}

/* PoP: msf_model_selection_config_updates @ hermes_cli/model_switch.py:model_selection_config_updates */
json_t *msf_model_selection_config_updates(msf_ctx *ctx, const json_t *result,
                                           const json_t *current_model_cfg) {
    const json_t *model_cfg = msf_is_obj(current_model_cfg) ? current_model_cfg : NULL;
    json_t *updates = json_object();
    json_set(updates, "default", json_copy(json_obj_get(result, "new_model")));
    json_set(updates, "provider", json_copy(json_obj_get(result, "target_provider")));
    json_t *base = json_obj_get(result, "base_url");
    json_set(updates, "base_url", msf_truthy(base) ? json_copy(base) : json_null());
    json_t *mode = json_obj_get(result, "api_mode");
    json_set(updates, "api_mode", msf_truthy(mode) ? json_copy(mode) : json_null());

    if (model_cfg && json_obj_get(model_cfg, "context_length")) {
        json_t *cm = json_obj_get(model_cfg, "default");
        if (!msf_truthy(cm)) cm = json_obj_get(model_cfg, "model");
        json_t *args = json_object();
        msf_arg_copy(args, "configured_model", cm);
        msf_arg_copy(args, "model", json_obj_get(result, "new_model"));
        msf_arg_copy(args, "configured_base_url", json_obj_get(model_cfg, "base_url"));
        msf_arg_copy(args, "base_url", json_obj_get(result, "base_url"));
        msf_arg_copy(args, "configured_provider", json_obj_get(model_cfg, "provider"));
        msf_arg_copy(args, "provider", json_obj_get(result, "target_provider"));
        json_t *clear = msf_call(ctx, "should_clear_context_pin", args);
        if (msf_exc_pending(ctx)) { json_free(clear); json_free(updates); return NULL; }
        if (msf_truthy(clear)) json_set(updates, "context_length", json_null());
        json_free(clear);
    }

    char *tp = msf_str_or_empty(json_obj_get(result, "target_provider"));
    char *target = msf_strip_lower(tp);
    free(tp);
    json_t *rc = msf_selection_route_changed(ctx, model_cfg, result);
    if (msf_exc_pending(ctx)) { json_free(rc); free(target); json_free(updates); return NULL; }
    bool route_changed = msf_truthy(rc);
    json_free(rc);

    const char *stale[4];
    size_t nstale = 0;
    if (!msf_starts_with(target, "custom") || route_changed) {
        stale[nstale++] = MSF_ENDPOINT_CREDENTIAL_KEYS[0];
        stale[nstale++] = MSF_ENDPOINT_CREDENTIAL_KEYS[1];
    }
    if (route_changed) {
        stale[nstale++] = MSF_ENDPOINT_KEY_POINTER_KEYS[0];
        stale[nstale++] = MSF_ENDPOINT_KEY_POINTER_KEYS[1];
    }
    for (size_t i = 0; i < nstale; i++)
        if (model_cfg && json_obj_get(model_cfg, stale[i]))
            json_set(updates, stale[i], json_null());
    free(target);
    return updates;
}

/* PoP: msf_merge_selection_updates @ hermes_cli/model_switch.py:_merge_selection_updates */
json_t *msf_merge_selection_updates(msf_ctx *ctx, const json_t *model_cfg, const json_t *updates,
                                    bool mask_cleared) {
    (void)ctx;
    json_t *out = msf_is_obj(model_cfg) ? json_copy(model_cfg) : json_object();
    if (msf_is_obj(updates)) {
        for (size_t i = 0; i < updates->c.count; i++) {
            const char *key = updates->c.keys[i];
            json_t *value = updates->c.items[i];
            if (msf_is_none(value) && !(mask_cleared && json_obj_get(out, key)))
                json_obj_del(out, key);
            else
                json_set(out, key, json_copy(value));
        }
    }
    return out;
}

/* PoP: msf_apply_model_selection @ hermes_cli/model_switch.py:apply_model_selection */
json_t *msf_apply_model_selection(msf_ctx *ctx, const json_t *model_cfg, const json_t *result,
                                  bool mask_cleared) {
    json_t *updates = msf_model_selection_config_updates(ctx, result, model_cfg);
    if (msf_exc_pending(ctx)) { json_free(updates); return NULL; }
    json_t *merged = msf_merge_selection_updates(ctx, model_cfg, updates, mask_cleared);
    json_free(updates);
    return merged;
}

/* PoP: msf_persist_model_selection @ hermes_cli/model_switch.py:persist_model_selection */
json_t *msf_persist_model_selection(msf_ctx *ctx, const json_t *result, const char *config_path) {
    char *path = NULL;
    if (config_path && config_path[0]) {
        path = msf_strdup(config_path);
    } else {
        json_t *gp = msf_call(ctx, "get_config_path", json_object());
        if (msf_exc_pending(ctx)) { json_free(gp); return NULL; }
        path = msf_strdup(msf_jstr(gp));
        json_free(gp);
    }

    json_t *args = json_object();
    msf_arg_str(args, "path", path);
    json_t *raw1 = msf_call(ctx, "read_user_config_raw", args);
    if (msf_exc_pending(ctx)) { json_free(raw1); free(path); return NULL; }
    json_t *planned_from = json_copy(json_obj_get(raw1, "model"));
    json_free(raw1);

    json_t *updates = msf_model_selection_config_updates(ctx, result, planned_from);
    if (msf_exc_pending(ctx)) { json_free(updates); json_free(planned_from); free(path); return NULL; }

    json_t *sft = msf_call(ctx, "supports_file_tooling", json_object());
    if (msf_exc_pending(ctx)) {
        json_free(sft); json_free(updates); json_free(planned_from); free(path);
        return NULL;
    }
    if (!msf_truthy(sft)) {
        /* Remote Config: one keyed write; cleared keys the effective block
         * holds are masked with null, absent ones are unset. */
        json_free(sft);
        json_t *masked = msf_merge_selection_updates(ctx, planned_from, updates, true);
        if (msf_exc_pending(ctx)) {
            json_free(masked); json_free(updates); json_free(planned_from); free(path);
            return NULL;
        }
        json_t *set = json_object();
        json_t *unset = json_array();
        for (size_t i = 0; i < updates->c.count; i++) {
            const char *k = updates->c.keys[i];
            char *fk = (char *)malloc(strlen(k) + 7);
            sprintf(fk, "model.%s", k);
            json_t *mv = json_obj_get(masked, k);
            if (mv) json_set(set, fk, json_copy(mv));
            else json_append(unset, json_string(fk));
            free(fk);
        }
        args = json_object();
        msf_arg_str(args, "path", path);
        json_set(args, "set", set);
        json_set(args, "unset", unset);
        json_t *wc = msf_call(ctx, "write_changes", args);
        json_free(wc);
        json_free(masked);
        json_free(updates);
        json_free(planned_from);
        free(path);
        return NULL; /* success or pending exc */
    }
    json_free(sft);

    /* File backend: read -> merge -> replace under the (modeled) lock. */
    args = json_object();
    msf_arg_str(args, "path", path);
    json_t *config = msf_call(ctx, "read_user_config_raw", args);
    if (msf_exc_pending(ctx)) {
        json_free(config); json_free(updates); json_free(planned_from); free(path);
        return NULL;
    }
    json_t *model_cfg = json_obj_get(config, "model");
    if (!msf_json_eq(model_cfg, planned_from)) {
        json_free(updates);
        updates = msf_model_selection_config_updates(ctx, result, model_cfg);
        if (msf_exc_pending(ctx)) {
            json_free(updates); json_free(config); json_free(planned_from); free(path);
            return NULL;
        }
    }
    json_t *merged = msf_merge_selection_updates(ctx, model_cfg, updates, false);
    if (msf_exc_pending(ctx)) {
        json_free(merged); json_free(updates); json_free(config); json_free(planned_from); free(path);
        return NULL;
    }
    if (msf_is_obj(model_cfg)) {
        /* YAML anchor sharing: edit the model block IN PLACE. */
        while (model_cfg->c.count > 0) json_obj_del(model_cfg, model_cfg->c.keys[0]);
        for (size_t i = 0; i < merged->c.count; i++)
            json_set(model_cfg, merged->c.keys[i], json_copy(merged->c.items[i]));
    } else {
        json_set(config, "model", json_copy(merged));
    }
    json_free(merged);
    args = json_object();
    msf_arg_str(args, "path", path);
    msf_arg_copy(args, "config", config);
    json_t *ar = msf_call(ctx, "atomic_config_replace", args);
    json_free(ar);
    json_free(config);
    json_free(updates);
    json_free(planned_from);
    if (msf_exc_pending(ctx)) { free(path); return NULL; }

    /* owner-only chmod; only OSError / NotImplementedError are suppressed. */
    args = json_object();
    msf_arg_str(args, "path", path);
    json_set(args, "mode", json_number(384)); /* 0o600 */
    json_t *ch = msf_call(ctx, "chmod", args);
    json_free(ch);
    if (msf_exc_pending(ctx)) {
        if (msf_exc_is(ctx, "OSError") || msf_exc_is(ctx, "NotImplementedError")) msf_exc_clear(ctx);
        else { free(path); return NULL; }
    }
    free(path);
    return NULL;
}
