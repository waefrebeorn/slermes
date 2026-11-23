/*
 * session_policy_flow2.c -- gateway/session_policy.py FLOW cores, second wave
 * (universal-seam convention, twin of gateway_run_flow2 / session_policy_flow).
 *
 * Ports 20 UNCLAIMED gateway/session_policy.py functions (PoP-grep verified;
 * the 9 flow1 claims -- session_pinned, created_surface, profile_target,
 * present_sections, terminal_uses_host_cwd, launch_skills, _valid_client_env,
 * _stored_cwd_is_absolute, restore_policy -- are SEAMED, never re-claimed):
 *   route/pick:  model_route, _held_pick, entry_serves, bound_route_holds,
 *                _without_auto_pick, _keeps_profile_route, _with_entry_pick
 *   pin/config:  pin_reasoning_effort, unpin_reasoning, profile_config
 *   key:         _key_slot, launch_key, route_launch_key
 *   workspace:   workspace_ready, bind_turn_workspace, unbind_turn_workspace
 *   hooks:       accept_launch_hooks, refresh_profile_hooks,
 *                register_worker_hooks
 *   posture:     _in_coding_workspace
 *
 * Every live I/O (route identity resolvers, config loaders, persistence
 * scope, coding-workspace probe, shell-hook / webhook registrars, terminal
 * override table accessors, the policy.config() read, Path probes, logging)
 * enters through sp2_seams_t.call(ctx, name, args); args is OWNED by sp2_call
 * (freed after the borrowed call); results are NEW json_t with NULL == Python
 * None and {"__raise__":K,"msg":M} == a raised exception. BaseException kinds
 * (SystemExit/KeyboardInterrupt/CancelledError/GeneratorExit) pass through
 * every `except Exception` site; all other kinds are Exception-family (the
 * fixture domain scripts only Exception subclasses + the base trio, so no
 * partial exception-parent table is needed). Fail-closed: the harness
 * delivers a RuntimeError raise spec for an unscripted seam.
 *
 * Claimed siblings are seams: present_sections, profile_target,
 * terminal_uses_host_cwd. Ported siblings compose inline (model_route into
 * _held_pick / bound_route_holds / _with_entry_pick; _key_slot into
 * launch_key; bound_route_holds + launch_key into route_launch_key;
 * unbind_turn_workspace into bind_turn_workspace). _follow (foreign module
 * gateway/session_policy_refresh) is seamed and its copy/pop contract is
 * transcribed at the call site so the mutated config state matches live.
 *
 * Modeling notes (differential oracle tests/oracle_session_policy_flow2.py):
 *  - Mutable STATE: pin_reasoning_effort / unpin_reasoning mutate in.config
 *    (out.state.config); bind/unbind_turn_workspace mutate in.overrides
 *    (out.state.overrides, the tools.terminal_tool._task_env_overrides table).
 *    All other funcs echo out.state = null.
 *  - str() positions accept str/int/bool/null ONLY (no floats/containers:
 *    integral-float vs int identity and Python container repr are outside the
 *    fixture domain). .strip()/.lower() are ASCII-whitespace/ASCII-case domain.
 *  - json.loads positions (policy.request_json / terminal_json / config_json /
 *    kanban_json) accept STRICT-valid JSON strings only (libjson json_parse is
 *    lenient where Python json.loads is strict; invalid-JSON fixtures are
 *    excluded). A parsed non-dict reaching .get raises the live AttributeError.
 *  - Paths are POSIX strings; profile_config joins profile_dir / 'config.yaml'
 *    like PurePosixPath (collapse //, drop "." and empty parts, keep "..",
 *    exactly-two leading slashes preserved, empty base == ".").
 *  - AuthorityRefusal refusals carry {__raise__, msg(=reason), reason, field,
 *    detail}; field/detail run the live _one_line normalization (whitespace
 *    collapse, non-printable drop, 80/160 bounds, empty -> null). Scripted
 *    raise specs propagate VERBATIM (the oracle identity-matches them).
 *  - `and`/`or`/`return call(...)` positions return the OPERAND verbatim
 *    (workspace_ready, bound_route_holds's same_route, _in_coding_workspace's
 *    in_focus_workspace), not a coerced bool.
 *  - logger seams record {fmt, args?[, exc_info]}; the two except-Exception
 *    sites use logger.debug(fmt, cwd, exc_info=True) (_in_coding_workspace)
 *    and logger.warning(fmt, exc_info=True) (refresh_profile_hooks).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include "libjson/json.h"

typedef struct sp2_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} sp2_seams_t;

/* -- universal-seam helpers -- */
static json_t *sp2_call(const sp2_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool sp2_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *sp2_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING && k->str_val) ? k->str_val : "";
}
static json_t *sp2_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static bool sp2_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
static bool sp2_truth(const json_t *v) {
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
static const json_t *sp2_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *sp2_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool sp2_is_null_node(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool sp2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool sp2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool sp2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static json_t *sp2_args(void) { return json_object(); }
static void sp2_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void sp2_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void sp2_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
/* Python type name for a JSON node (integral numbers read as int). */
static const char *sp2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (isfinite(d) && d == (double)(long long)d) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}
/* Python scalar == (bool is an int subclass: True == 1). Containers compare
 * by deep element equality only where the live code compares tuples/arrays. */
static bool sp2_scalar_eq(const json_t *a, const json_t *b) {
    bool an = sp2_is_null_node(a), bn = sp2_is_null_node(b);
    if (an || bn) return an && bn;
    bool anum = (a->type == JSON_NUMBER || a->type == JSON_BOOL);
    bool bnum = (b->type == JSON_NUMBER || b->type == JSON_BOOL);
    if (anum && bnum) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return false;
}
static bool sp2_array_eq(const json_t *a, const json_t *b) {
    if (!sp2_is_arr(a) || !sp2_is_arr(b)) return false;
    if (json_len(a) != json_len(b)) return false;
    for (size_t i = 0; i < json_len(a); i++)
        if (!sp2_scalar_eq(json_get(a, i), json_get(b, i))) return false;
    return true;
}
/* -- Python value helpers (scalar str() domain) -- */
static char *sp2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (isfinite(d) && d == (double)(long long)d && d < 9.2e18 && d > -9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                snprintf(buf, sizeof(buf), "%.17g", d);
            return strdup(buf);
        }
        default: {
            char *s = json_serialize(v);
            return s ? s : strdup("");
        }
    }
}
/* str(value or '') -- falsy collapses to the empty string. malloc'd. */
static char *sp2_str_or_empty(const json_t *v) {
    if (!sp2_truth(v)) return strdup("");
    return sp2_pystr(v);
}
/* ASCII whitespace strip (fixture domain restricted to ASCII ws). */
static char *sp2_strip(const char *s) {
    if (!s) s = "";
    while (*s && (*s == ' ' || (*s >= '\t' && *s <= '\r'))) s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || (s[n-1] >= '\t' && s[n-1] <= '\r'))) n--;
    char *out = malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
static char *sp2_lower(const char *s) {
    char *out = strdup(s ? s : "");
    if (out) for (char *p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
    return out;
}
static bool sp2_startswith(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
/* str.strip().lower() composed (malloc'd). */
static char *sp2_strip_lower(const json_t *v) {
    char *s = sp2_str_or_empty(v);
    char *t = sp2_strip(s);
    free(s);
    char *l = sp2_lower(t);
    free(t);
    return l;
}
/* -- AttributeError / TypeError emission helpers -- */
static json_t *sp2_attr_err(const json_t *v, const char *attr) {
    char m[160];
    snprintf(m, sizeof(m), "'%s' object has no attribute '%s'", sp2_type_name(v), attr);
    return sp2_raise("AttributeError", m);
}
static json_t *sp2_item_assign_err(const json_t *v) {
    char m[160];
    snprintf(m, sizeof(m), "'%s' object does not support item assignment", sp2_type_name(v));
    return sp2_raise("TypeError", m);
}
static json_t *sp2_not_mapping_err(const json_t *v) {
    char m[160];
    snprintf(m, sizeof(m), "'%s' object is not a mapping", sp2_type_name(v));
    return sp2_raise("TypeError", m);
}
static json_t *sp2_not_subscript_err(const json_t *v) {
    char m[160];
    snprintf(m, sizeof(m), "'%s' object is not subscriptable", sp2_type_name(v));
    return sp2_raise("TypeError", m);
}
/* -- logging seams (RecLogger twin): {fmt, args?[, exc_info]} -- */
static json_t *sp2_log_debug_exc(const sp2_seams_t *s, const char *fmt, const char *cwd) {
    json_t *a = sp2_args();
    sp2_s(a, "fmt", fmt);
    json_t *arr = json_array();
    json_append(arr, json_string(cwd ? cwd : ""));
    json_set(a, "args", arr);
    sp2_b(a, "exc_info", true);
    return sp2_call(s, "logger.debug", a);
}
static json_t *sp2_log_warning_exc(const sp2_seams_t *s, const char *fmt) {
    json_t *a = sp2_args();
    sp2_s(a, "fmt", fmt);
    sp2_b(a, "exc_info", true);
    return sp2_call(s, "logger.warning", a);
}
/* -- AuthorityRefusal (real class twin) --
 * _one_line: whitespace collapse (Python str.split White_Space incl \x1c-\x1f
 * for the ASCII domain), non-printable drop, length bound, empty -> NULL. */
static bool sp2_py_ws(unsigned char ch) {
    return ch == ' ' || (ch >= '\t' && ch <= '\r') || (ch >= 0x1c && ch <= 0x1f);
}
static char *sp2_one_line(const char *value, size_t limit) {
    if (!value) return NULL;
    size_t n = strlen(value);
    char *tmp = malloc(n + 1);
    if (!tmp) return NULL;
    size_t k = 0;
    bool pending_space = false, started = false;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)value[i];
        if (sp2_py_ws(ch)) { if (started) pending_space = true; continue; }
        if (pending_space) { tmp[k++] = ' '; pending_space = false; }
        started = true;
        tmp[k++] = (char)ch;
    }
    tmp[k] = '\0';
    size_t w = 0;
    for (size_t i = 0; i < k; i++) {
        unsigned char ch = (unsigned char)tmp[i];
        if (ch < 0x20 || (ch >= 0x7f && ch <= 0x9f)) continue;
        tmp[w++] = tmp[i];
    }
    tmp[w] = '\0';
    if (w == 0) { free(tmp); return NULL; }
    if (w > limit) w = limit;
    char *out = malloc(w + 1);
    if (!out) { free(tmp); return NULL; }
    memcpy(out, tmp, w);
    out[w] = '\0';
    free(tmp);
    return out;
}
static json_t *sp2_ar_raise(const char *reason, const char *field, const char *detail) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string("AuthorityRefusal"));
    json_set(r, "msg", json_string(reason ? reason : ""));
    json_set(r, "reason", json_string(reason ? reason : ""));
    char *f = sp2_one_line(field, 80);
    json_set(r, "field", f ? json_string(f) : json_null());
    free(f);
    char *d = sp2_one_line(detail, 160);
    json_set(r, "detail", d ? json_string(d) : json_null());
    free(d);
    return r;
}
/* -- PurePosixPath normalization + join (profile_dir / 'config.yaml') --
 * Collapse empty + "." parts, keep "..", leading run: exactly 2 -> "//",
 * >=3 or 1 -> "/", 0 -> relative. Empty base normalizes to ".". */
static char *sp2_posix_norm(const char *s) {
    if (!s) s = "";
    size_t lead = 0;
    while (s[lead] == '/') lead++;
    /* split remaining on '/', drop empty + "." parts, keep ".." */
    char *out = malloc(strlen(s) + 3);
    if (!out) return NULL;
    size_t k = 0;
    if (lead == 2) { out[k++] = '/'; out[k++] = '/'; }
    else if (lead >= 1) { out[k++] = '/'; }
    const char *p = s + lead;
    bool first = true;
    while (*p) {
        const char *q = p;
        while (*q && *q != '/') q++;
        size_t len = (size_t)(q - p);
        if (len == 1 && p[0] == '.') { /* drop */ }
        else if (len > 0) {
            if (!first && k > 0 && out[k-1] != '/') out[k++] = '/';
            /* relative first part: no leading sep; absolute already has one */
            if (lead == 0 && first) { /* no sep */ }
            memcpy(out + k, p, len); k += len;
            first = false;
        }
        p = (*q == '/') ? q + 1 : q;
    }
    if (k == 0) { out[k++] = '.'; }
    out[k] = '\0';
    return out;
}
/* PurePosixPath(base) / "config.yaml" then str(). The component is a simple
 * name (no separators), so join == norm(base) + sep + name with the root
 * rules; an empty/"." base yields the bare name. */
static char *sp2_posix_join_name(const char *base, const char *name) {
    char *nb = sp2_posix_norm(base);
    if (!nb) return NULL;
    size_t n = strlen(nb);
    bool root = (n > 0 && nb[n-1] == '/');
    bool dot = (strcmp(nb, ".") == 0);
    size_t cap = n + strlen(name) + 2;
    char *out = malloc(cap);
    if (!out) { free(nb); return NULL; }
    if (dot) snprintf(out, cap, "%s", name);
    else if (root) snprintf(out, cap, "%s%s", nb, name);
    else snprintf(out, cap, "%s/%s", nb, name);
    free(nb);
    return out;
}
/* ── Python-faithful json.JSONDecodeError message ──
 * Recompute CPython's "Expecting value: line L column C (char N)" /
 * "Extra data: ..." from the decoded text (libjson's own error has no
 * position). A prefix-scan locates the first complete value; a non-whitespace
 * tail is "Extra data", otherwise the failure is "Expecting value". */
static void sp2_jerr_msg(const char *text, char *buf, size_t bufsz) {
    if (!text) text = "";
    size_t n = strlen(text), pos = 0;
    while (pos < n && (text[pos] == ' ' || text[pos] == '\t' ||
                       text[pos] == '\n' || text[pos] == '\r')) pos++;
    size_t errpos = pos;
    const char *kind = "Expecting value";
    if (pos < n) {
        size_t best = 0;
        for (size_t j = pos + 1; j <= n; j++) {
            char *tmp = malloc(j + 1);
            if (!tmp) break;
            memcpy(tmp, text, j); tmp[j] = '\0';
            char *pe = NULL;
            json_t *parsed = json_parse(tmp, &pe);
            free(pe);
            bool ok = (parsed != NULL);
            json_free(parsed);
            free(tmp);
            if (ok) { best = j; break; }
        }
        if (best) {
            size_t k = best;
            while (k < n && (text[k] == ' ' || text[k] == '\t' ||
                             text[k] == '\n' || text[k] == '\r')) k++;
            if (k < n) { errpos = k; kind = "Extra data"; }
        }
    }
    size_t line = 1, col = 1;
    for (size_t i = 0; i < errpos; i++) {
        if (text[i] == '\n') { line++; col = 1; } else col++;
    }
    snprintf(buf, bufsz, "%s: line %zu column %zu (char %zu)", kind, line, col, errpos);
}
/* json.loads for the strict-valid-JSON string domain. Returns a NEW node
 * (JSON_NULL node for "null"); NULL input / parse failure -> the live
 * TypeError / JSONDecodeError (with CPython's exact message). */
static json_t *sp2_jloads(const json_t *v) {
    if (!sp2_is_str(v)) {
        char m[160];
        snprintf(m, sizeof(m),
                 "the JSON object must be str, bytes or bytearray, not %s",
                 sp2_type_name(v));
        return sp2_raise("TypeError", m);
    }
    char *err = NULL;
    json_t *r = json_parse(v->str_val ? v->str_val : "", &err);
    free(err);
    if (!r) {
        char mbuf[192];
        sp2_jerr_msg(v->str_val ? v->str_val : "", mbuf, sizeof mbuf);
        return sp2_raise("JSONDecodeError", mbuf);
    }
    return r;
}
/* SURFACES[source] (module constant); NULL == KeyError (unknown label). */
static const char *sp2_surface(const char *label) {
    if (!label) return NULL;
    if (strcmp(label, "cli") == 0) return "cli";
    if (strcmp(label, "tui") == 0) return "tui";
    if (strcmp(label, "gui") == 0) return "desktop";
    if (strcmp(label, "acp") == 0) return "acp";
    if (strcmp(label, "tool") == 0) return "cli";
    if (strcmp(label, "oneshot") == 0) return "cli";
    return NULL;
}
/* SELECTION_KEYS (module constant, in live order). */
static const char *SP2_SELECTION_KEYS[] = {
    "default", "provider", "base_url", "api_mode", "selection_profile",
    "api_key_from", "key_env", "api_key_env"};
#define SP2_N_SELECTION (sizeof(SP2_SELECTION_KEYS)/sizeof(SP2_SELECTION_KEYS[0]))
static bool sp2_is_selection_key(const char *k) {
    for (size_t i = 0; i < SP2_N_SELECTION; i++)
        if (strcmp(SP2_SELECTION_KEYS[i], k) == 0) return true;
    return false;
}

/* -- unpack / subscript helpers (2-array + null fixture domain) -- */
static json_t *sp2_unpack2(const json_t *r, const json_t **a, const json_t **b) {
    if (sp2_is_null_node(r))
        return sp2_raise("TypeError", "cannot unpack non-iterable NoneType object");
    if (sp2_is_arr(r)) {
        size_t n = json_len(r);
        char m[96];
        if (n < 2) {
            snprintf(m, sizeof(m), "not enough values to unpack (expected 2, got %zu)", n);
            return sp2_raise("ValueError", m);
        }
        if (n > 2) {
            snprintf(m, sizeof(m), "too many values to unpack (expected 2, got %zu)", n);
            return sp2_raise("ValueError", m);
        }
        *a = json_get(r, 0); *b = json_get(r, 1);
        return NULL;
    }
    char m[128];
    snprintf(m, sizeof(m), "cannot unpack non-iterable %s object", sp2_type_name(r));
    return sp2_raise("TypeError", m);
}
static json_t *sp2_index0(const json_t *r, const json_t **out) {
    if (sp2_is_null_node(r)) return sp2_not_subscript_err(r);
    if (sp2_is_arr(r)) {
        if (json_len(r) == 0) return sp2_raise("IndexError", "list index out of range");
        *out = json_get(r, 0);
        return NULL;
    }
    return sp2_not_subscript_err(r);
}

/* -- 1. model_route -- */
/* PoP: sp2_model_route @ gateway/session_policy.py:model_route */
static json_t *sp2_model_route(const json_t *block, const sp2_seams_t *s) {
    const json_t *b = sp2_is_obj(block) ? block : NULL;
    char *prov = sp2_strip_lower(sp2_oget(b, "provider"));
    json_t *a = sp2_args();
    sp2_j(a, "base_url", sp2_oget(b, "base_url"));
    json_t *ep = sp2_call(s, "normalize_route_base_url", a);
    if (sp2_raised(ep)) { free(prov); return ep; }
    json_t *ret = json_array();
    json_append(ret, json_string(prov));
    free(prov);
    json_append(ret, ep ? ep : json_null());
    return ret;
}

/* -- 2. _held_pick -- */
/* PoP: sp2_held_pick @ gateway/session_policy.py:_held_pick */
static json_t *sp2_held_pick(const json_t *frozen, const json_t *live, const sp2_seams_t *s) {
    json_t *fr = sp2_model_route(frozen, s);
    if (sp2_raised(fr)) return fr;
    json_t *lr = sp2_model_route(live, s);
    if (sp2_raised(lr)) { json_free(fr); return lr; }
    bool equal = sp2_array_eq(fr, lr);
    json_free(fr); json_free(lr);
    if (!equal) return json_copy(frozen);
    if (!sp2_is_obj(live)) return sp2_attr_err(live, "items");
    if (!sp2_is_obj(frozen)) {
        char m[128];
        snprintf(m, sizeof(m), "argument of type '%s' is not a container or iterable",
                 sp2_type_name(frozen));
        return sp2_raise("TypeError", m);
    }
    json_t *out = json_object();
    for (size_t i = 0; i < live->c.count; i++) {
        const char *k = live->c.keys[i];
        if (!sp2_is_selection_key(k)) json_set(out, k, json_copy(live->c.items[i]));
    }
    for (size_t i = 0; i < SP2_N_SELECTION; i++) {
        const json_t *v = sp2_oget(frozen, SP2_SELECTION_KEYS[i]);
        if (v) json_set(out, SP2_SELECTION_KEYS[i], json_copy(v));
    }
    return out;
}

/* -- 3. entry_serves -- */
/* PoP: sp2_entry_serves @ gateway/session_policy.py:entry_serves */
static json_t *sp2_entry_serves(const json_t *block, const json_t *config, const sp2_seams_t *s) {
    const json_t *b = sp2_is_obj(block) ? block : NULL;
    json_t *a = sp2_args();
    sp2_j(a, "base_url", sp2_oget(b, "base_url"));
    json_t *url = sp2_call(s, "normalize_route_base_url", a);
    if (sp2_raised(url)) return url;
    if (!sp2_truth(url)) { json_free(url); return json_bool(true); }
    char *prov = sp2_strip(sp2_str_or_empty(sp2_oget(b, "provider")));
    json_t *entries = json_array();
    if (prov[0]) {
        json_t *na = sp2_args();
        sp2_s(na, "provider", prov);
        sp2_j(na, "config", config);
        json_t *ne = sp2_call(s, "_named_entry", na);
        if (sp2_raised(ne)) { free(prov); json_free(url); json_free(entries); return ne; }
        json_append(entries, ne ? ne : json_null());
    }
    free(prov);
    const json_t *source = sp2_oget(b, "api_key_from");
    if (sp2_is_arr(source) && json_len(source) > 1) {
        const json_t *alias = config;
        for (size_t i = 0; i + 1 < json_len(source); i++) {
            const char *pk = sp2_sval(json_get(source, i));
            alias = (sp2_is_obj(alias) && pk) ? sp2_oget(alias, pk) : NULL;
        }
        json_append(entries, alias ? json_copy(alias) : json_null());
    }
    for (size_t i = 0; i < json_len(entries); i++) {
        const json_t *entry = json_get(entries, i);
        if (!sp2_is_obj(entry)) continue;
        const json_t *ebu = sp2_oget(entry, "base_url");
        char *es = sp2_strip(sp2_str_or_empty(ebu));
        bool qualifies = es[0] != '\0';
        free(es);
        if (!qualifies) continue;
        json_t *na = sp2_args();
        sp2_j(na, "base_url", ebu);
        json_t *n = sp2_call(s, "normalize_route_base_url", na);
        if (sp2_raised(n)) { json_free(url); json_free(entries); return n; }
        bool eq = sp2_scalar_eq(n, url);
        json_free(n);
        if (!eq) { json_free(url); json_free(entries); return json_bool(false); }
    }
    json_free(url); json_free(entries);
    return json_bool(true);
}

/* -- 4. bound_route_holds -- */
/* PoP: sp2_bound_route_holds @ gateway/session_policy.py:bound_route_holds */
static json_t *sp2_bound_route_holds(const json_t *bound, const json_t *bound_config,
                                     const json_t *block, const json_t *config,
                                     const sp2_seams_t *s) {
    const json_t *bd = sp2_is_obj(bound) ? bound : NULL;
    json_t *a1 = sp2_args();
    sp2_j(a1, "block", bd);
    sp2_j(a1, "config", bound_config);
    json_t *rr1 = sp2_call(s, "resolved_route", a1);
    if (sp2_raised(rr1)) return rr1;
    const json_t *identity = NULL, *url = NULL;
    json_t *ue = sp2_unpack2(rr1, &identity, &url);
    if (ue) { json_free(rr1); return ue; }
    json_t *a2 = sp2_args();
    sp2_j(a2, "block", block);
    sp2_j(a2, "config", config);
    json_t *rr2 = sp2_call(s, "resolved_route", a2);
    if (sp2_raised(rr2)) { json_free(rr1); return rr2; }
    const json_t *id2 = NULL;
    json_t *ie = sp2_index0(rr2, &id2);
    if (ie) { json_free(rr1); json_free(rr2); return ie; }
    bool is_auto = (sp2_is_str(identity) && strcmp(identity->str_val, "auto") == 0)
                || (sp2_is_str(id2) && strcmp(id2->str_val, "auto") == 0);
    if (is_auto) {
        json_free(rr1); json_free(rr2);
        json_t *m1 = sp2_model_route(bound, s);
        if (sp2_raised(m1)) return m1;
        json_t *m2 = sp2_model_route(block, s);
        if (sp2_raised(m2)) { json_free(m1); return m2; }
        bool eq = sp2_array_eq(m1, m2);
        json_free(m1); json_free(m2);
        return json_bool(eq);
    }
    json_t *tmp = NULL;
    const json_t *argbound = bd;
    if (sp2_truth(url)) {
        tmp = bd ? json_copy(bd) : json_object();
        json_set(tmp, "base_url", json_copy(url));
        argbound = tmp;
    }
    json_t *ab = sp2_args();
    sp2_j(ab, "bound", argbound);
    sp2_j(ab, "block", block);
    sp2_j(ab, "config", config);
    json_free(tmp);
    json_free(rr1); json_free(rr2);
    json_t *sr = sp2_call(s, "same_route", ab);
    if (sp2_raised(sr)) return sr;
    return sr ? sr : json_null();
}

/* -- 5. _without_auto_pick -- */
/* PoP: sp2_without_auto_pick @ gateway/session_policy.py:_without_auto_pick */
static json_t *sp2_without_auto_pick(const json_t *params) {
    if (!sp2_is_obj(params)) return sp2_attr_err(params, "get");
    const json_t *p = sp2_oget(params, "provider");
    char *pl = sp2_strip_lower(p);
    bool is_auto = sp2_is_str(p) && strcmp(pl, "auto") == 0;
    free(pl);
    json_t *out = json_copy(params);
    if (is_auto) json_obj_del(out, "provider");
    return out;
}

/* -- 6. _keeps_profile_route -- */
/* PoP: sp2_keeps_profile_route @ gateway/session_policy.py:_keeps_profile_route */
static json_t *sp2_keeps_profile_route(const json_t *picked, const json_t *config,
                                       const sp2_seams_t *s) {
    json_t *a = sp2_args();
    sp2_j(a, "config", config);
    json_t *hr = sp2_call(s, "profile_route_identity", a);
    if (sp2_raised(hr)) return hr;
    bool held_auto = sp2_is_str(hr) && strcmp(hr->str_val, "auto") == 0;
    char *ps = sp2_is_str(picked) ? sp2_strip(picked->str_val) : NULL;
    bool pick_ok = sp2_is_str(picked) && ps && ps[0] != '\0';
    free(ps);
    if (held_auto || !pick_ok) { json_free(hr); return json_bool(false); }
    json_t *blk = json_object();
    json_set(blk, "provider", json_copy(picked));
    json_t *a2 = sp2_args();
    sp2_j(a2, "block", blk);
    json_free(blk);
    sp2_j(a2, "config", config);
    json_t *rr = sp2_call(s, "resolved_route", a2);
    if (sp2_raised(rr)) { json_free(hr); return rr; }
    const json_t *id0 = NULL;
    json_t *ie = sp2_index0(rr, &id0);
    if (ie) { json_free(hr); json_free(rr); return ie; }
    bool eq = sp2_scalar_eq(id0, hr);
    json_free(hr); json_free(rr);
    return json_bool(eq);
}

/* -- 7. _with_entry_pick -- */
/* PoP: sp2_with_entry_pick @ gateway/session_policy.py:_with_entry_pick */
static json_t *sp2_with_entry_pick(const json_t *params, const json_t *config,
                                   const sp2_seams_t *s) {
    if (!sp2_is_obj(params)) return sp2_attr_err(params, "get");
    const json_t *picked = sp2_oget(params, "provider");
    if (!sp2_is_str(picked)) return json_copy(params);
    json_t *a = sp2_args();
    sp2_j(a, "config", config);
    json_t *identity = sp2_call(s, "profile_route_identity", a);
    if (sp2_raised(identity)) return identity;
    char *pl = sp2_strip_lower(picked);
    if (strcmp(pl, "custom") == 0) {
        free(pl);
        const json_t *url = sp2_oget(params, "base_url");
        char *model = sp2_str_or_empty(sp2_oget(params, "model"));
        json_t *mr = sp2_model_route(sp2_oget(config, "model"), s);
        if (sp2_raised(mr)) { free(model); json_free(identity); return mr; }
        json_t *ka = sp2_args();
        sp2_j(ka, "url", json_get(mr, 1));
        json_t *own = sp2_call(s, "_route_key", ka);
        json_free(mr);
        if (sp2_raised(own)) { free(model); json_free(identity); return own; }
        bool cond = false;
        bool ident_custom = sp2_is_str(identity) && strcmp(identity->str_val, "custom") == 0;
        if (ident_custom && sp2_truth(own)) {
            if (!sp2_is_null_node(url)) {
                json_t *ka2 = sp2_args();
                sp2_j(ka2, "url", url);
                json_t *rk = sp2_call(s, "_route_key", ka2);
                if (sp2_raised(rk)) { free(model); json_free(identity); json_free(own); return rk; }
                cond = sp2_scalar_eq(rk, own);
                json_free(rk);
            } else {
                char *mt = sp2_strip(model);
                char *ml = sp2_lower(mt);
                free(mt);
                json_t *pa = sp2_args();
                sp2_j(pa, "config", config);
                json_t *pt = sp2_call(s, "profile_target", pa);
                if (sp2_raised(pt)) { free(ml); free(model); json_free(identity); json_free(own); return pt; }
                const json_t *p0 = NULL;
                json_t *ie = sp2_index0(pt, &p0);
                if (ie) { free(ml); free(model); json_free(identity); json_free(own); json_free(pt); return ie; }
                if (!sp2_is_str(p0)) {
                    json_t *ae = sp2_attr_err(p0, "lower");
                    free(ml); free(model); json_free(identity); json_free(own); json_free(pt); return ae;
                }
                char *p0l = sp2_lower(p0->str_val);
                cond = strcmp(ml, p0l) == 0;
                free(p0l);
                json_free(pt);
                if (!cond) {
                    json_t *ra = sp2_args();
                    sp2_j(ra, "url", NULL);
                    sp2_s(ra, "model", model);
                    sp2_j(ra, "config", config);
                    json_t *rc = sp2_call(s, "routed_custom_identity", ra);
                    if (sp2_raised(rc)) { free(ml); free(model); json_free(identity); json_free(own); return rc; }
                    cond = !sp2_truth(rc);
                    json_free(rc);
                }
                free(ml);
            }
        }
        json_free(own);
        if (cond) { free(model); json_free(identity); return json_copy(params); }
        if (!sp2_is_str(identity)) {
            json_t *ae = sp2_attr_err(identity, "startswith");
            free(model); json_free(identity); return ae;
        }
        bool ident_prefix = sp2_startswith(identity->str_val, "custom:");
        const char *held = ident_prefix ? identity->str_val : NULL;
        json_t *ha = sp2_args();
        sp2_j(ha, "url", url);
        sp2_s(ha, "model", model);
        sp2_j(ha, "config", config);
        sp2_s(ha, "held", held);
        json_t *healed = sp2_call(s, "routed_custom_identity", ha);
        if (sp2_raised(healed)) { free(model); json_free(identity); return healed; }
        if (!sp2_truth(healed)) {
            json_free(healed);
            healed = (ident_prefix && sp2_is_null_node(url)) ? json_copy(identity) : NULL;
        }
        if (sp2_truth(healed) && !sp2_scalar_eq(healed, identity)
                && sp2_is_null_node(url) && json_has(params, "api_key")) {
            char *hs = sp2_pystr(healed);
            size_t cap = strlen(hs) + 128;
            char *detail = malloc(cap);
            if (detail)
                snprintf(detail, cap,
                         "%s was chosen by model; name it with --provider or --base-url to use a launch key", hs);
            free(hs);
            json_t *ar = sp2_ar_raise("invalid_params", "api_key", detail);
            free(detail);
            json_free(healed); free(model); json_free(identity);
            return ar;
        }
        if (sp2_truth(healed)) {
            json_t *out = json_copy(params);
            json_set(out, "provider", healed);
            free(model); json_free(identity);
            return out;
        }
        json_free(healed);
        free(model); json_free(identity);
        return json_copy(params);
    }
    free(pl);
    bool ident_auto = sp2_is_str(identity) && strcmp(identity->str_val, "auto") == 0;
    if (ident_auto) { json_free(identity); return json_copy(params); }
    json_t *ca = sp2_args();
    sp2_j(ca, "identity", identity);
    sp2_j(ca, "config", config);
    json_t *cek = sp2_call(s, "configured_entry_key", ca);
    if (sp2_raised(cek)) { json_free(identity); return cek; }
    const json_t *cek_or_none = sp2_truth(cek) ? cek : NULL;
    char *pl2 = sp2_strip_lower(picked);
    bool neq = !cek_or_none
            || !(sp2_is_str(cek_or_none) && strcmp(pl2, cek_or_none->str_val) == 0);
    free(pl2);
    json_free(cek);
    if (neq) { json_free(identity); return json_copy(params); }
    json_t *out = json_copy(params);
    json_set(out, "provider", json_copy(identity));
    json_free(identity);
    return out;
}

/* -- 8. pin_reasoning_effort (mutates config state) -- */
/* PoP: sp2_pin_reasoning_effort @ gateway/session_policy.py:pin_reasoning_effort */
static json_t *sp2_pin_reasoning_effort(json_t *config, const json_t *effort, bool session) {
    if (!sp2_is_obj(config)) return sp2_attr_err(config, "setdefault");
    json_t *agent = json_obj_get(config, "agent");
    if (!agent) { agent = json_object(); json_set(config, "agent", agent); }
    if (!sp2_is_obj(agent)) return sp2_item_assign_err(agent);
    json_set(agent, "reasoning_effort", effort ? json_copy(effort) : json_null());
    json_obj_del(agent, "reasoning_overrides");
    if (session) {
        const json_t *pins = sp2_oget(config, "session_pins");
        const json_t *base = sp2_truth(pins) ? pins : NULL;
        if (base && !sp2_is_obj(base)) return sp2_not_mapping_err(base);
        json_t *np = base ? json_copy(base) : json_object();
        json_set(np, "reasoning", json_bool(true));
        json_set(config, "session_pins", np);
    }
    return NULL;
}

/* -- 9. unpin_reasoning (mutates config state; _follow seamed + contract) -- */
/* PoP: sp2_unpin_reasoning @ gateway/session_policy.py:unpin_reasoning */
static json_t *sp2_unpin_reasoning(json_t *config, const json_t *profile, const sp2_seams_t *s) {
    if (!sp2_is_obj(config)) return sp2_attr_err(config, "setdefault");
    if (!sp2_is_obj(profile)) return sp2_attr_err(profile, "get");
    const json_t *pa = sp2_oget(profile, "agent");
    const json_t *followed = sp2_is_obj(pa) ? pa : NULL;
    json_t *agent = json_obj_get(config, "agent");
    if (!agent) { agent = json_object(); json_set(config, "agent", agent); }
    static const char *keys[] = {"reasoning_effort", "reasoning_overrides"};
    for (int i = 0; i < 2; i++) {
        const char *key = keys[i];
        json_t *a = sp2_args();
        sp2_j(a, "agent", agent);
        json_set(a, "followed", followed ? json_copy(followed) : json_object());
        sp2_s(a, "key", key);
        json_t *r = sp2_call(s, "_follow", a);
        if (sp2_raised(r)) return r;
        json_free(r);
        bool in_live = followed && json_has(followed, key);
        if (!sp2_is_obj(agent)) {
            if (in_live) return sp2_item_assign_err(agent);
            return sp2_attr_err(agent, "pop");
        }
        if (in_live) json_set(agent, key, json_copy(json_obj_get(followed, key)));
        else json_obj_del(agent, key);
    }
    const json_t *pins = sp2_oget(config, "session_pins");
    const json_t *base = sp2_truth(pins) ? pins : NULL;
    if (base && !sp2_is_obj(base)) return sp2_not_mapping_err(base);
    json_t *np = base ? json_copy(base) : json_object();
    json_set(np, "reasoning", json_bool(false));
    json_set(config, "session_pins", np);
    return NULL;
}

/* -- 10. profile_config -- */
/* PoP: sp2_profile_config @ gateway/session_policy.py:profile_config */
static json_t *sp2_profile_config(const json_t *authority, const sp2_seams_t *s) {
    json_t *a = sp2_args();
    sp2_j(a, "authority", authority);
    json_t *ap = sp2_call(s, "authority_persistence", a);
    if (sp2_raised(ap)) return ap;
    if (sp2_is_null_node(ap)) { json_free(ap); return sp2_attr_err(NULL, "scope"); }
    const json_t *pd = sp2_oget(ap, "profile_dir");
    if (!sp2_is_str(pd)) {
        char m[200];
        snprintf(m, sizeof(m),
                 "argument should be a str or an os.PathLike object where __fspath__ returns a str, not '%s'",
                 sp2_type_name(pd));
        json_free(ap);
        return sp2_raise("TypeError", m);
    }
    char *path = sp2_posix_join_name(pd->str_val, "config.yaml");
    json_free(ap);
    json_t *la = sp2_args();
    sp2_s(la, "path", path);
    free(path);
    json_t *lc = sp2_call(s, "_load_gateway_config", la);
    if (sp2_raised(lc)) return lc;
    json_t *psa = sp2_args();
    sp2_j(psa, "config", lc);
    json_free(lc);
    json_t *ps = sp2_call(s, "present_sections", psa);
    if (sp2_raised(ps)) return ps;
    return ps ? ps : json_null();
}

/* -- 11. _key_slot -- */
/* PoP: sp2_key_slot @ gateway/session_policy.py:_key_slot */
static json_t *sp2_key_slot(const json_t *ref) {
    if (!sp2_is_str(ref)) return sp2_attr_err(ref, "split");
    const char *s = ref->str_val ? ref->str_val : "";
    const char *h = strchr(s, '#');
    size_t n = h ? (size_t)(h - s) : strlen(s);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n); out[n] = '\0';
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* -- 12. launch_key -- */
/* PoP: sp2_launch_key @ gateway/session_policy.py:launch_key */
static json_t *sp2_launch_key(const json_t *authority, const json_t *policy, const sp2_seams_t *s) {
    (void)s;
    const json_t *cref = sp2_oget(policy, "credential_ref");
    if (sp2_is_null_node(cref)) return NULL;
    json_t *slot = sp2_key_slot(cref);
    if (sp2_raised(slot)) return slot;
    const json_t *keys = sp2_oget(authority, "_local_launch_keys");
    if (keys && !sp2_is_obj(keys)) { json_free(slot); return sp2_attr_err(keys, "get"); }
    const json_t *value = keys ? sp2_oget(keys, sp2_sval(slot)) : NULL;
    json_free(slot);
    if (sp2_is_null_node(value)) return sp2_ar_raise("launch_credentials_unavailable", NULL, NULL);
    return json_copy(value);
}

/* -- 13. route_launch_key -- */
/* PoP: sp2_route_launch_key @ gateway/session_policy.py:route_launch_key */
static json_t *sp2_route_launch_key(const json_t *authority, const json_t *policy,
                                    const json_t *config, const sp2_seams_t *s) {
    json_t *parsed = sp2_jloads(sp2_oget(policy, "config_json"));
    if (sp2_raised(parsed)) return parsed;
    json_t *psa = sp2_args();
    sp2_j(psa, "config", parsed);
    json_free(parsed);
    json_t *bound = sp2_call(s, "present_sections", psa);
    if (sp2_raised(bound)) return bound;
    if (!sp2_is_obj(bound)) { json_t *e = sp2_attr_err(bound, "get"); json_free(bound); return e; }
    if (!sp2_is_obj(config)) { json_free(bound); return sp2_attr_err(config, "get"); }
    const json_t *bm = sp2_oget(bound, "model");
    const json_t *cm = sp2_oget(config, "model");
    json_t *holds = sp2_bound_route_holds(bm, bound, cm, config, s);
    if (sp2_raised(holds)) { json_free(bound); return holds; }
    bool h = sp2_truth(holds);
    json_free(holds);
    if (!h) { json_free(bound); return NULL; }
    json_t *lk = sp2_launch_key(authority, policy, s);
    json_free(bound);
    return lk;
}

/* -- 14. workspace_ready -- */
/* PoP: sp2_workspace_ready @ gateway/session_policy.py:workspace_ready */
static json_t *sp2_workspace_ready(const json_t *policy, const sp2_seams_t *s) {
    const json_t *cwd = sp2_oget(policy, "cwd");
    if (!sp2_is_str(cwd)) {
        char m[200];
        snprintf(m, sizeof(m),
                 "argument should be a str or an os.PathLike object where __fspath__ returns a str, not '%s'",
                 sp2_type_name(cwd));
        return sp2_raise("TypeError", m);
    }
    json_t *a = sp2_args();
    sp2_s(a, "path", cwd->str_val);
    json_t *ia = sp2_call(s, "Path.is_absolute", a);
    if (sp2_raised(ia)) return ia;
    if (!sp2_truth(ia)) return ia ? ia : json_null();
    json_t *a2 = sp2_args();
    sp2_s(a2, "path", cwd->str_val);
    json_t *id = sp2_call(s, "Path.is_dir", a2);
    if (sp2_raised(id)) return id;
    return id ? id : json_null();
}

/* -- 15/16. bind/unbind_turn_workspace (mutate overrides state) -- */
static json_t *sp2_unbind_turn_workspace(const char *task_id, json_t *overrides, const sp2_seams_t *s);

/* PoP: sp2_bind_turn_workspace @ gateway/session_policy.py:bind_turn_workspace */
static json_t *sp2_bind_turn_workspace(const json_t *policy, const char *task_id,
                                       json_t *overrides, const sp2_seams_t *s) {
    if (sp2_is_null_node(policy) || !task_id || !task_id[0]) return NULL;
    json_t *tj = sp2_jloads(sp2_oget(policy, "terminal_json"));
    if (sp2_raised(tj)) return tj;
    if (!sp2_is_obj(tj)) { json_t *e = sp2_attr_err(tj, "get"); json_free(tj); return e; }
    char *backend = sp2_strip_lower(sp2_oget(tj, "TERMINAL_ENV"));
    bool docker = strcmp(backend, "docker") == 0;
    free(backend);
    if (!docker) {
        json_free(tj);
        return sp2_unbind_turn_workspace(task_id, overrides, s);
    }
    json_t *ta = sp2_args();
    sp2_j(ta, "terminal", tj);
    json_t *tu = sp2_call(s, "terminal_uses_host_cwd", ta);
    if (sp2_raised(tu)) { json_free(tj); return tu; }
    bool uses = sp2_truth(tu);
    json_free(tu);
    json_free(tj);
    if (!uses) return sp2_unbind_turn_workspace(task_id, overrides, s);
    json_t *ra = sp2_args();
    sp2_s(ra, "task_id", task_id);
    json_t *cur = sp2_call(s, "resolve_task_overrides", ra);
    if (sp2_raised(cur)) return cur;
    if (!sp2_is_obj(cur)) { json_t *e = sp2_attr_err(cur, "get"); json_free(cur); return e; }
    const json_t *cwd = sp2_oget(policy, "cwd");
    const json_t *csrc = sp2_oget(cur, "cwd_source");
    bool same = sp2_scalar_eq(sp2_oget(cur, "cwd"), cwd)
             && sp2_is_str(csrc) && strcmp(csrc->str_val, "session") == 0;
    if (same) { json_free(cur); return NULL; }
    json_t *wa = sp2_args();
    sp2_j(wa, "cwd", cwd);
    json_t *wk = sp2_call(s, "workspace_key_for", wa);
    if (sp2_raised(wk)) { json_free(cur); return wk; }
    json_t *nov = json_copy(cur);
    json_free(cur);
    json_set(nov, "cwd", cwd ? json_copy(cwd) : json_null());
    json_set(nov, "cwd_source", json_string("session"));
    json_set(nov, "workspace_key", wk ? wk : json_null());
    json_t *rega = sp2_args();
    sp2_s(rega, "task_id", task_id);
    sp2_j(rega, "overrides", nov);
    json_free(nov);
    json_t *rr = sp2_call(s, "register_task_env_overrides", rega);
    if (sp2_raised(rr)) return rr;
    json_free(rr);
    return NULL;
}

/* PoP: sp2_unbind_turn_workspace @ gateway/session_policy.py:unbind_turn_workspace */
static json_t *sp2_unbind_turn_workspace(const char *task_id, json_t *overrides, const sp2_seams_t *s) {
    if (!task_id || !task_id[0]) return NULL;
    json_t *ka = sp2_args();
    sp2_s(ka, "task_id", task_id);
    json_t *key = sp2_call(s, "_qualify_task_key", ka);
    if (sp2_raised(key)) return key;
    const char *ks = sp2_sval(key);
    const json_t *cur = (ks && sp2_is_obj(overrides)) ? json_obj_get(overrides, ks) : NULL;
    json_t *ret = NULL;
    if (!sp2_truth(cur)) cur = NULL;
    if (cur && !sp2_is_obj(cur)) {
        char m[128];
        snprintf(m, sizeof(m), "argument of type '%s' is not a container or iterable", sp2_type_name(cur));
        ret = sp2_raise("TypeError", m);
        goto out;
    }
    if (!cur || !json_has(cur, "workspace_key")) goto out;
    {
        json_t *cwd_copy = json_copy(sp2_oget(cur, "cwd"));
        json_obj_del(overrides, ks);
        json_t *ga = sp2_args();
        sp2_s(ga, "task_id", task_id);
        json_t *gc = sp2_call(s, "get_session_cwd", ga);
        if (sp2_raised(gc)) { json_free(cwd_copy); ret = gc; goto out; }
        bool eq = sp2_scalar_eq(gc, cwd_copy);
        json_free(gc); json_free(cwd_copy);
        if (eq) {
            json_t *ca = sp2_args();
            sp2_s(ca, "task_id", task_id);
            json_t *cr = sp2_call(s, "clear_session_cwd", ca);
            if (sp2_raised(cr)) { ret = cr; goto out; }
            json_free(cr);
        }
    }
out:
    json_free(key);
    return ret;
}

/* -- 17. accept_launch_hooks -- */
/* PoP: sp2_accept_launch_hooks @ gateway/session_policy.py:accept_launch_hooks */
static json_t *sp2_accept_launch_hooks(const json_t *policy, const sp2_seams_t *s) {
    json_t *rj = sp2_jloads(sp2_oget(policy, "request_json"));
    if (sp2_raised(rj)) return rj;
    if (!sp2_is_obj(rj)) { json_t *e = sp2_attr_err(rj, "get"); json_free(rj); return e; }
    const json_t *ah = sp2_oget(rj, "accept_hooks");
    const json_t *iuc = sp2_oget(policy, "ignore_user_config");
    bool go = sp2_truth(ah) && !sp2_truth(iuc);
    json_free(rj);
    if (!go) return NULL;
    json_t *lc = sp2_call(s, "_load_gateway_config", sp2_args());
    if (sp2_raised(lc)) return lc;
    json_t *aa = sp2_args();
    sp2_j(aa, "config", lc);
    json_free(lc);
    json_t *r = sp2_call(s, "accept_for_launch", aa);
    if (sp2_raised(r)) return r;
    json_free(r);
    return NULL;
}

/* -- 18. refresh_profile_hooks -- */
/* PoP: sp2_refresh_profile_hooks @ gateway/session_policy.py:refresh_profile_hooks */
static json_t *sp2_refresh_profile_hooks(const json_t *policy, const sp2_seams_t *s) {
    if (sp2_is_null_node(policy)) return NULL;
    const json_t *iuc = sp2_oget(policy, "ignore_user_config");
    if (sp2_truth(iuc)) return NULL;
    json_t *a = sp2_args();
    sp2_s(a, "loader", "_load_gateway_config");
    json_t *r = sp2_call(s, "sync_from_config", a);
    if (sp2_raised(r)) {
        if (sp2_is_base_exc(sp2_kind(r))) return r;
        json_free(r);
        json_t *lw = sp2_log_warning_exc(s, "shell-hook re-sync failed; keeping the registered hooks");
        if (sp2_raised(lw)) return lw;
        json_free(lw);
    }
    return NULL;
}

/* -- 19. register_worker_hooks -- */
/* PoP: sp2_register_worker_hooks @ gateway/session_policy.py:register_worker_hooks */
static json_t *sp2_register_worker_hooks(const json_t *policy, const sp2_seams_t *s) {
    const json_t *iuc = sp2_oget(policy, "ignore_user_config");
    if (sp2_truth(iuc)) return NULL;
    const json_t *kj = sp2_oget(policy, "kanban_json");
    json_t *nullstr = json_string("null");
    json_t *kanban = sp2_jloads(sp2_truth(kj) ? kj : nullstr);
    json_free(nullstr);
    if (sp2_raised(kanban)) return kanban;
    if (!sp2_truth(kanban)) { json_free(kanban); kanban = json_object(); }
    /* LIVE evaluates left-to-right on one line:
     *   json.loads(request_json).get('accept_hooks') is True
     *   or kanban.get('accept_hooks') is True
     * So request_json is parsed (and its .get checked) BEFORE the kanban type
     * check / .get; a malformed request_json raises JSONDecodeError first, and
     * a non-dict kanban is only touched when the request side is False. */
    json_t *rj = sp2_jloads(sp2_oget(policy, "request_json"));
    if (sp2_raised(rj)) { json_free(kanban); return rj; }
    if (!sp2_is_obj(rj)) { json_t *e = sp2_attr_err(rj, "get"); json_free(rj); json_free(kanban); return e; }
    const json_t *ahr = sp2_oget(rj, "accept_hooks");
    bool accept = (ahr && ahr->type == JSON_BOOL && ahr->bool_val);
    json_free(rj);
    if (!accept) {
        if (!sp2_is_obj(kanban)) { json_t *e = sp2_attr_err(kanban, "get"); json_free(kanban); return e; }
        const json_t *ahk = sp2_oget(kanban, "accept_hooks");
        accept = (ahk && ahk->type == JSON_BOOL && ahk->bool_val);
    }
    json_free(kanban);
    json_t *cfg = sp2_call(s, "policy.config", sp2_args());
    if (sp2_raised(cfg)) return cfg;
    json_t *r1a = sp2_args();
    sp2_j(r1a, "config", cfg);
    sp2_b(r1a, "accept_hooks", accept);
    json_t *r1 = sp2_call(s, "shell_hooks.register_from_config", r1a);
    if (sp2_raised(r1)) { json_free(cfg); return r1; }
    json_free(r1);
    json_t *r2a = sp2_args();
    sp2_j(r2a, "config", cfg);
    json_free(cfg);
    json_t *r2 = sp2_call(s, "outbound_webhooks.register_from_config", r2a);
    if (sp2_raised(r2)) return r2;
    json_free(r2);
    return NULL;
}

/* -- 20. _in_coding_workspace -- */
/* PoP: sp2_in_coding_workspace @ gateway/session_policy.py:_in_coding_workspace */
static json_t *sp2_in_coding_workspace(const json_t *config, const char *source,
                                       const char *cwd, const sp2_seams_t *s) {
    const char *surf = sp2_surface(source);
    if (!surf) {
        json_t *ld = sp2_log_debug_exc(s,
            "coding posture detection failed for %s; keeping the profile default", cwd);
        if (sp2_raised(ld)) return ld;
        json_free(ld);
        return json_bool(false);
    }
    json_t *a = sp2_args();
    sp2_s(a, "platform", surf);
    sp2_s(a, "cwd", cwd);
    sp2_j(a, "config", config);
    json_t *r = sp2_call(s, "in_focus_workspace", a);
    if (sp2_raised(r)) {
        if (sp2_is_base_exc(sp2_kind(r))) return r;
        json_free(r);
        json_t *ld = sp2_log_debug_exc(s,
            "coding posture detection failed for %s; keeping the profile default", cwd);
        if (sp2_raised(ld)) return ld;
        json_free(ld);
        return json_bool(false);
    }
    return r ? r : json_null();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Third-wave remainder funcs (session_policy_flow2 extension). PoP-annotated
 * at gateway/session_policy.py:<fn>. Same libjson-only / static sp2_* / universal
 * seam / ordered-trace / BaseException-passthrough / fail-closed contract.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* strict Python `is True` (not ==) */
static bool sp2_is_true(const json_t *v) {
    return v && v->type == JSON_BOOL && v->bool_val;
}

/* -- 21. session_pinned -- */
/* PoP: sp2_session_pinned @ gateway/session_policy.py:session_pinned */
static json_t *sp2_session_pinned(const json_t *config, const char *field,
                                  const json_t *launched) {
    const json_t *pins = sp2_oget(config, "session_pins");
    if (sp2_is_obj(pins)) {
        const json_t *v = json_obj_get(pins, field);
        if (v && v->type == JSON_BOOL) return json_copy(v);
    }
    return launched ? json_copy(launched) : json_null();
}

/* -- 22. created_surface -- */
/* PoP: sp2_created_surface @ gateway/session_policy.py:created_surface */
static json_t *sp2_created_surface(const json_t *label) {
    if (sp2_is_null_node(label)) return json_null();
    const char *s = sp2_sval(label);
    if (!s) return json_copy(label);   /* non-str, non-None label: kept as provenance */
    /* _NOT_A_SURFACE = {None, '', 'local', 'unknown'} */
    if (strcmp(s, "") == 0 || strcmp(s, "local") == 0 || strcmp(s, "unknown") == 0)
        return json_null();
    /* STORED_AS_LOCAL = keys(SURFACES) - {tool, oneshot} = cli/tui/gui/acp */
    if (strcmp(s, "cli") == 0 || strcmp(s, "tui") == 0 ||
        strcmp(s, "gui") == 0 || strcmp(s, "acp") == 0)
        return json_string(sp2_surface(s));  /* gui -> desktop */
    return json_copy(label);                  /* tool/oneshot/other kept as provenance */
}

/* -- 23. profile_target -- */
/* PoP: sp2_profile_target @ gateway/session_policy.py:profile_target */
static json_t *sp2_profile_target(const json_t *config) {
    json_t *out = json_array();
    const json_t *model = sp2_oget(config, "model");
    if (sp2_is_obj(model)) {
        char *praw = sp2_str_or_empty(json_obj_get(model, "provider"));
        char *pstrip = sp2_strip(praw);
        free(praw);
        char *plow = sp2_lower(pstrip);
        bool is_auto = (strcmp(plow, "auto") == 0);
        free(plow);
        char *draw = sp2_str_or_empty(json_obj_get(model, "default"));
        char *dstrip = sp2_strip(draw);
        free(draw);
        json_append(out, json_string(dstrip));
        json_append(out, json_string(is_auto ? "" : pstrip));
        free(dstrip);
        free(pstrip);
    } else {
        char *mstrip = sp2_strip(sp2_sval(model) ? sp2_sval(model) : "");
        json_append(out, json_string(mstrip));
        json_append(out, json_string(""));
        free(mstrip);
    }
    return out;
}

/* -- 24. present_sections -- */
/* PoP: sp2_present_sections @ gateway/session_policy.py:present_sections */
static json_t *sp2_present_sections(const json_t *config) {
    json_t *out = json_object();
    if (sp2_is_obj(config)) {
        for (size_t i = 0; i < config->c.count; i++) {
            const json_t *v = config->c.items[i];
            if (sp2_is_null_node(v)) continue;  /* drop keys whose value is None */
            json_set(out, config->c.keys[i], json_copy(v));
        }
    }
    const json_t *m = json_obj_get(out, "model");
    if (sp2_is_str(m)) {
        char *t = sp2_strip(m->str_val ? m->str_val : "");
        if (*t) {
            json_t *d = json_object();
            json_set(d, "default", json_copy(m));
            json_set(out, "model", d);
        } else {
            json_set(out, "model", json_object());
        }
        free(t);
    }
    return out;
}

/* -- 25. terminal_uses_host_cwd -- */
/* PoP: sp2_terminal_uses_host_cwd @ gateway/session_policy.py:terminal_uses_host_cwd */
static json_t *sp2_terminal_uses_host_cwd(const json_t *terminal) {
    const json_t *be = sp2_oget(terminal, "TERMINAL_ENV");
    char *bes = sp2_truth(be) ? sp2_pystr(be) : strdup("local");
    char *be_st = sp2_strip(bes);
    free(bes);
    char *be_lo = sp2_lower(be_st);
    free(be_st);
    const json_t *mo = sp2_oget(terminal, "TERMINAL_DOCKER_MOUNT_CWD_TO_WORKSPACE");
    char *mos = sp2_truth(mo) ? sp2_pystr(mo) : strdup("");
    char *mo_st = sp2_strip(mos);
    free(mos);
    char *mo_lo = sp2_lower(mo_st);
    free(mo_st);
    bool mounts = (strcmp(mo_lo, "1") == 0 || strcmp(mo_lo, "true") == 0 ||
                   strcmp(mo_lo, "yes") == 0 || strcmp(mo_lo, "on") == 0);
    bool local = (strcmp(be_lo, "local") == 0);
    bool docker = (strcmp(be_lo, "docker") == 0) && mounts;
    free(be_lo);
    free(mo_lo);
    return json_bool(local || docker);
}

/* -- 26. _valid_client_env -- */
/* PoP: sp2_valid_client_env @ gateway/session_policy.py:_valid_client_env */
static json_t *sp2_valid_client_env(const json_t *value, const char *kind) {
    if (sp2_is_null_node(value)) return json_bool(true);
    if (!sp2_is_str(value)) return json_bool(false);
    char *err = NULL;
    json_t *env = json_parse(value->str_val ? value->str_val : "", &err);
    free(err);
    if (!env) return json_bool(false);            /* invalid JSON -> ValueError -> False */
    if (!sp2_is_obj(env)) { json_free(env); return json_bool(false); }
    bool listkind = (kind && strcmp(kind, "list") == 0);
    bool ok = true;
    for (size_t i = 0; i < env->c.count && ok; i++) {
        json_t *v = env->c.items[i];
        if (listkind) {
            if (!sp2_is_arr(v)) { ok = false; break; }
            for (size_t j = 0; j < v->c.count; j++)
                if (!sp2_is_str(v->c.items[j])) { ok = false; break; }
        } else {
            if (!sp2_is_str(v)) ok = false;
        }
    }
    json_free(env);
    return json_bool(ok);
}

/* -- live-request helper: json.loads(request_json).get(key, False) -- */
static json_t *sp2_request_flag(const char *key, const json_t *request_json) {
    json_t *r = sp2_jloads(request_json);
    if (sp2_raised(r)) return r;
    const json_t *v = json_obj_get(r, key);  /* dict domain (live .get) */
    json_t *out = v ? json_copy(v) : json_bool(false);
    json_free(r);
    return out;
}

/* -- 27. client_env -- */
/* PoP: sp2_client_env @ gateway/session_policy.py:client_env */
static json_t *sp2_client_env(const json_t *client_env_json) {
    if (sp2_is_null_node(client_env_json)) return json_null();
    return sp2_jloads(client_env_json);
}

/* -- 28. ignore_rules -- */
/* PoP: sp2_ignore_rules @ gateway/session_policy.py:ignore_rules */
static json_t *sp2_ignore_rules(bool safe_mode, const json_t *request_json) {
    if (safe_mode) return json_bool(true);
    return sp2_request_flag("ignore_rules", request_json);
}

/* -- 29. yolo -- */
/* PoP: sp2_yolo @ gateway/session_policy.py:yolo */
static json_t *sp2_yolo(const json_t *request_json) {
    return sp2_request_flag("yolo", request_json);
}

/* -- 30. pass_session_id -- */
/* PoP: sp2_pass_session_id @ gateway/session_policy.py:pass_session_id */
static json_t *sp2_pass_session_id(const json_t *request_json) {
    return sp2_request_flag("pass_session_id", request_json);
}

/* -- 31. verbose -- */
/* PoP: sp2_verbose @ gateway/session_policy.py:verbose */
static json_t *sp2_verbose(const json_t *request_json) {
    return sp2_request_flag("verbose", request_json);
}

/* -- 32. follows_profile -- */
/* PoP: sp2_follows_profile @ gateway/session_policy.py:follows_profile */
static json_t *sp2_follows_profile(const json_t *request_json, const json_t *config_json) {
    json_t *r = sp2_jloads(request_json);
    if (sp2_raised(r)) return r;
    bool a = sp2_is_true(json_obj_get(r, "follow_profile_config"));
    json_free(r);
    if (a) return json_bool(true);
    json_t *c = sp2_jloads(config_json);
    if (sp2_raised(c)) return c;
    bool b = sp2_is_true(json_obj_get(c, "follow_adopted"));
    json_free(c);
    return json_bool(b);
}

/* -- config()-based property helper (seam "policy.config") -- */
static json_t *sp2_config_model_field(const sp2_seams_t *s, const char *field) {
    json_t *cfg = sp2_call(s, "policy.config", sp2_args());
    if (sp2_raised(cfg)) return cfg;
    const json_t *model = sp2_oget(cfg, "model");
    json_t *out;
    if (sp2_is_obj(model)) {
        const json_t *v = json_obj_get(model, field);
        out = v ? json_copy(v) : json_null();
    } else if (sp2_is_null_node(model)) {
        out = json_null();  /* .get('model', {}) -> {} -> .get(field) -> None */
    } else {
        out = sp2_attr_err(model, "get");  /* out-of-domain: non-dict model */
    }
    json_free(cfg);
    return out;
}

/* -- 33. checkpoints_enabled -- */
/* PoP: sp2_checkpoints_enabled @ gateway/session_policy.py:checkpoints_enabled */
static json_t *sp2_checkpoints_enabled(const sp2_seams_t *s) {
    json_t *cfg = sp2_call(s, "policy.config", sp2_args());
    if (sp2_raised(cfg)) return cfg;
    const json_t *section = sp2_oget(cfg, "checkpoints");
    json_t *out;
    if (sp2_is_obj(section)) {
        const json_t *v = json_obj_get(section, "enabled");
        out = json_bool(v ? sp2_truth(v) : false);
    } else {
        out = json_bool(sp2_truth(section));  /* absent -> {} -> False; None -> False */
    }
    json_free(cfg);
    return out;
}

/* -- 34. provider -- */
/* PoP: sp2_provider @ gateway/session_policy.py:provider */
static json_t *sp2_provider(const sp2_seams_t *s) {
    return sp2_config_model_field(s, "provider");
}

/* -- 35. base_url -- */
/* PoP: sp2_base_url @ gateway/session_policy.py:base_url */
static json_t *sp2_base_url(const sp2_seams_t *s) {
    return sp2_config_model_field(s, "base_url");
}
