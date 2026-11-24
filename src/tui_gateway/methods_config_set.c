/*
 * methods_config_set.c — C11 port of upstream tui_gateway/methods_config_set.py
 * (gates w32_methodscon_ce0f5d_p1 and w32_methodscon_ce0f5d_p2).
 *
 * STATUS: verified parity for the pure decision core; the config/agent I/O
 *         around it stays in the caller (see SEAMS).
 *
 * PoP: w32_methodscon_ce0f5d_p1_normalize_word     @ tui_gateway/methods_config_set.py:_word
 * PoP: w32_methodscon_ce0f5d_p1_normalize_raw_word @ tui_gateway/methods_config_set.py:_raw_word
 * PoP: w32_methodscon_ce0f5d_p1_bool_word          @ tui_gateway/methods_config_set.py:_set_display_toggle(_BOOL_WORDS)
 * PoP: w32_methodscon_ce0f5d_p1_service_tier_word  @ agent/fast_mode.py:service_tier_word
 * PoP: w32_methodscon_ce0f5d_p1_fast_set           @ tui_gateway/methods_config_set.py:_set_fast
 * PoP: w32_methodscon_ce0f5d_p1_verbose_set        @ tui_gateway/methods_config_set.py:_set_verbose
 * PoP: w32_methodscon_ce0f5d_p1_word_set           @ tui_gateway/methods_config_set.py:_set_word/_word_setters
 * PoP: w32_methodscon_ce0f5d_p1_details_section    @ tui_gateway/methods_config_set.py:_set_details_section
 * PoP: w32_methodscon_ce0f5d_p1_resolve_key        @ tui_gateway/methods_config_set.py:config.set(_CONFIG_SETTERS)
 * PoP: w32_methodscon_ce0f5d_p2_err                @ tui_gateway/server.py:_err
 * PoP: w32_methodscon_ce0f5d_p2_ok                 @ tui_gateway/server.py:_ok
 * PoP: w32_methodscon_ce0f5d_p2_kv                 @ tui_gateway/methods_config_set.py:_kv
 * PoP: w32_methodscon_ce0f5d_p2_model_ok           @ tui_gateway/methods_config_set.py:_cfgset_model_ok
 * PoP: w32_methodscon_ce0f5d_p2_confirm_required   @ tui_gateway/methods_config_set.py:_cfgset_model_ok(confirm_required)
 * PoP: w32_methodscon_ce0f5d_p2_reasoning          @ tui_gateway/methods_config_set.py:_set_reasoning
 * PoP: w32_methodscon_ce0f5d_p2_toggle_set         @ tui_gateway/methods_config_set.py:_set_toggle/_toggle_setters
 * PoP: w32_methodscon_ce0f5d_p2_bool_set           @ tui_gateway/methods_config_set.py:_set_display_toggle/_set_daybreak
 * PoP: w32_methodscon_ce0f5d_p2_prompt_set         @ tui_gateway/methods_config_set.py:_set_prompt
 * PoP: w32_methodscon_ce0f5d_p2_is_stale_session   @ tui_gateway/methods_config_set.py:config.set(_SESSION_SCOPED_KEYS)
 *
 * SEAMS (both sides take the value and hand one back; nothing here performs
 * them): config file read/write (_load_cfg*, _save_cfg, _write_config_key(s),
 * _write_display_sections, _write_config_changes), the managed-key refusal, the
 * live agent/session objects (_apply_model_switch, session mutation,
 * _persist_live_session_runtime, _emit_*_session_info), os.environ writes,
 * filesystem probing (cwd), git branch probing, and the model-override
 * resolution inside _set_fast (resolve_fast_mode_overrides/ultrafast_offered).
 * Effects the pure code decides are returned as the "writes" list so the
 * differential oracle can compare them without a filesystem.
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w32_methodscon_ce0f5d_p1.h"
#include "noc_w32_methodscon_ce0f5d_p2.h"

/* ── Python value semantics ───────────────────────────────────────────── */

static int ms_truthy(const json_t *v) {
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

static int ms_is_string(const json_t *v) { return v && v->type == JSON_STRING; }

static const char *ms_str_or(const json_t *v, const char *def) {
    return ms_is_string(v) && v->str_val ? v->str_val : def;
}

/* ── growable buffer ─────────────────────────────────────────────────── */

typedef struct {
    char *p;
    size_t n;
    size_t cap;
} ms_sb;

static void ms_sb_init(ms_sb *b) {
    b->cap = 64;
    b->p = (char *)malloc(b->cap);
    b->n = 0;
    if (b->p) b->p[0] = '\0';
}

static void ms_sb_add(ms_sb *b, const char *s) {
    size_t l;
    char *q;
    if (!b->p || !s) return;
    l = strlen(s);
    if (b->n + l + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        while (b->n + l + 1 > cap) cap *= 2;
        q = (char *)realloc(b->p, cap);
        if (!q) return;
        b->p = q;
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, l + 1);
    b->n += l;
}

static char *ms_sb_take(ms_sb *b) {
    char *p = b->p;
    if (!p) return strdup("");
    return p;
}

static void ms_fmt_num(double d, char *out, size_t cap) {
    int p;
    out[0] = '\0';
    if (d == 0.0) {
        snprintf(out, cap, "0");
        return;
    }
    if (fabs(d) < 1e16 && d == floor(d)) {
        snprintf(out, cap, "%.0f", d);
        return;
    }
    for (p = 1; p <= 17; p++) {
        snprintf(out, cap, "%.*g", p, d);
        if (strtod(out, NULL) == d) break;
    }
}

static void ms_repr_into(ms_sb *b, const json_t *v);
static void ms_repr_string_into(ms_sb *b, const char *s);

static void ms_repr_into(ms_sb *b, const json_t *v) {
    char num[64];
    size_t i, n;
    if (!v || v->type == JSON_NULL) { ms_sb_add(b, "None"); return; }
    switch (v->type) {
    case JSON_BOOL:   ms_sb_add(b, v->bool_val ? "True" : "False"); return;
    case JSON_NUMBER: ms_fmt_num(v->num_val, num, sizeof(num)); ms_sb_add(b, num); return;
    case JSON_STRING: ms_repr_string_into(b, v->str_val ? v->str_val : ""); return;
    case JSON_ARRAY:
        ms_sb_add(b, "[");
        n = json_len(v);
        for (i = 0; i < n; i++) {
            if (i) ms_sb_add(b, ", ");
            ms_repr_into(b, json_get(v, i));
        }
        ms_sb_add(b, "]");
        return;
    case JSON_OBJECT:
        ms_sb_add(b, "{");
        n = json_object_size(v);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(v, i);
            if (i) ms_sb_add(b, ", ");
            ms_repr_string_into(b, k ? k : "");
            ms_sb_add(b, ": ");
            ms_repr_into(b, json_object_get_at(v, i));
        }
        ms_sb_add(b, "}");
        return;
    default:
        ms_sb_add(b, "None");
    }
}

static void ms_repr_string_into(ms_sb *b, const char *s) {
    char esc[3];
    size_t i;
    char quote = '\'';
    /* Python picks the quote that needs less escaping: a string holding ' but no
     * " is repr'd with double quotes (["''"] shows as "['']"). */
    if (strchr(s, '\'') && !strchr(s, '"')) quote = '"';
    ms_sb_add(b, quote == '\'' ? "'" : "\"");
    for (i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\' || c == (unsigned char)quote) {
            esc[0] = '\\';
            esc[1] = (char)c;
            esc[2] = '\0';
            ms_sb_add(b, esc);
        } else if (c == '\n') {
            ms_sb_add(b, "\\n");
        } else if (c == '\r') {
            ms_sb_add(b, "\\r");
        } else if (c == '\t') {
            ms_sb_add(b, "\\t");
        } else {
            esc[0] = (char)c;
            esc[1] = '\0';
            ms_sb_add(b, esc);
        }
    }
    ms_sb_add(b, quote == '\'' ? "'" : "\"");
}

/* Python str(): strings verbatim, everything else through repr(). */
static char *ms_py_str(const json_t *v) {
    ms_sb b;
    ms_sb_init(&b);
    if (ms_is_string(v)) ms_sb_add(&b, v->str_val ? v->str_val : "");
    else ms_repr_into(&b, v);
    return ms_sb_take(&b);
}

/* Python repr() of a C string (a JSON string's own text). */
static char *ms_repr_cstr(const char *s) {
    ms_sb b;
    ms_sb_init(&b);
    ms_repr_string_into(&b, s ? s : "");
    return ms_sb_take(&b);
}

static int ms_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* str.strip().lower() in place (ASCII, the alphabet config keys use). */
static void ms_lower_trim(char *s) {
    size_t i = 0, n, j;
    if (!s) return;
    while (s[i] && ms_space((unsigned char)s[i])) i++;
    n = strlen(s + i);
    memmove(s, s + i, n + 1);
    while (n > 0 && ms_space((unsigned char)s[n - 1])) s[--n] = '\0';
    for (j = 0; j < n; j++) s[j] = (char)tolower((unsigned char)s[j]);
}

static char *ms_word_of(const json_t *value) {
    char *s;
    if (!ms_truthy(value)) return strdup("");
    s = ms_py_str(value);
    ms_lower_trim(s);
    return s;
}

static char *ms_raw_word_of(const json_t *value) {
    char *s;
    if (!value || value->type == JSON_NULL) return strdup("");
    s = ms_py_str(value);
    ms_lower_trim(s);
    return s;
}

static char *ms_msg(const char *a, const char *b, const char *c, const char *d) {
    ms_sb s;
    ms_sb_init(&s);
    if (a) ms_sb_add(&s, a);
    if (b) ms_sb_add(&s, b);
    if (c) ms_sb_add(&s, c);
    if (d) ms_sb_add(&s, d);
    return ms_sb_take(&s);
}

/* ── envelope shaping (server.py _ok/_err, methods_config_set._kv) ────── */

static json_t *ms_ok_env(const json_t *rid, json_t *result) {
    json_t *o = json_object();
    json_set(o, "jsonrpc", json_string("2.0"));
    json_set(o, "id", rid ? json_copy(rid) : json_null());
    json_set(o, "result", result ? result : json_null());
    return o;
}

static json_t *ms_err_env(const json_t *rid, int code, const char *msg) {
    json_t *o = json_object();
    json_t *e = json_object();
    json_set(e, "code", json_number((double)code));
    json_set(e, "message", json_string(msg ? msg : ""));
    json_set(o, "jsonrpc", json_string("2.0"));
    json_set(o, "id", rid ? json_copy(rid) : json_null());
    json_set(o, "error", e);
    return o;
}

static void ms_merge_extra(json_t *dst, json_t *extra) {
    size_t i, n;
    if (!extra) return;
    if (extra->type == JSON_OBJECT) {
        n = json_object_size(extra);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(extra, i);
            json_t *v = json_object_get_at(extra, i);
            if (k && v) json_set(dst, k, json_copy(v));
        }
    }
    json_free(extra);
}

static json_t *ms_kv_env(const json_t *rid, const char *key, json_t *value, json_t *extra) {
    json_t *res = json_object();
    json_set(res, "key", json_string(key ? key : ""));
    json_set(res, "value", value ? value : json_null());
    ms_merge_extra(res, extra);
    return ms_ok_env(rid, res);
}

static json_t *ms_effects(json_t *env, json_t *writes) {
    json_t *o = json_object();
    json_set(o, "envelope", env ? env : json_null());
    json_set(o, "writes", writes ? writes : json_array());
    return o;
}

static void ms_write_set(json_t *writes, const char *key, json_t *value) {
    json_t *row = json_object();
    json_t *set = json_object();
    json_set(set, key ? key : "", value ? value : json_null());
    json_set(row, "set", set);
    json_append(writes, row);
}

static void ms_write_section(json_t *writes, json_t *sections, json_t *drop, json_t *fields) {
    json_t *row = json_object();
    json_set(row, "sections", sections ? sections : json_null());
    json_set(row, "drop", drop ? drop : json_array());
    json_set(row, "fields", fields ? fields : json_object());
    json_append(writes, row);
}

/* ── gate p1: validation / coercion / key-path parsing ───────────────── */

json_t *w32_methodscon_ce0f5d_p1_normalize_word(const json_t *value) {
    char *s;
    json_t *out;
    if (!ms_truthy(value)) return json_string("");
    s = ms_py_str(value);
    ms_lower_trim(s);
    out = json_string(s);
    free(s);
    return out;
}

json_t *w32_methodscon_ce0f5d_p1_normalize_raw_word(const json_t *value) {
    char *s = ms_raw_word_of(value);
    json_t *out = json_string(s);
    free(s);
    return out;
}

/* _BOOL_WORDS */
static const char *const MS_BOOL_KEYS[8] = {"1", "on", "true", "yes", "0", "off", "false", "no"};

static int ms_bool_index(const char *s) {
    int i;
    for (i = 0; i < 8; i++)
        if (!strcmp(s, MS_BOOL_KEYS[i])) return i;
    return -1;
}

json_t *w32_methodscon_ce0f5d_p1_bool_word(const json_t *value) {
    char *s = ms_py_str(value);
    int idx;
    ms_lower_trim(s);
    idx = ms_bool_index(s);
    free(s);
    return idx < 0 ? json_null() : json_bool(idx < 4 ? true : false);
}

/* agent.fast_mode.service_tier_word */
json_t *w32_methodscon_ce0f5d_p1_service_tier_word(const json_t *tier) {
    const char *s;
    if (!tier || tier->type == JSON_NULL) return json_string("normal");
    if (tier->type == JSON_STRING) {
        s = tier->str_val ? tier->str_val : "";
        if (!strcmp(s, "priority")) return json_string("fast");
        if (!s[0]) return json_string("normal");
        return json_string(s);
    }
    return json_copy(tier);
}

/* _FAST_WORDS */
static const char *const MS_FAST_WORDS[8][2] = {
    {"fast", "fast"}, {"on", "fast"}, {"normal", "normal"}, {"off", "normal"},
    {"auto", "auto"}, {"cold", "cold"}, {"ultrafast", "ultrafast"}, {"ultra", "ultrafast"},
};

json_t *w32_methodscon_ce0f5d_p1_fast_set(const json_t *rid, const json_t *value,
                                          const json_t *current_tier) {
    const char *tier_s = ms_str_or(current_tier, NULL);
    char *raw = ms_word_of(value);
    json_t *writes = json_array();
    json_t *env;
    const char *nv = NULL;
    size_t i;

    if (!strcmp(raw, "status")) {
        env = ms_kv_env(rid, "fast", w32_methodscon_ce0f5d_p1_service_tier_word(current_tier), NULL);
        free(raw);
        json_free(writes);
        return ms_effects(env, json_array());
    }
    for (i = 0; i < 8; i++)
        if (!strcmp(raw, MS_FAST_WORDS[i][0])) { nv = MS_FAST_WORDS[i][1]; break; }
    if (!nv && (!raw[0] || !strcmp(raw, "toggle"))) {
        /* STATIC_TIERS = {"priority", "ultrafast"} */
        nv = (tier_s && (!strcmp(tier_s, "priority") || !strcmp(tier_s, "ultrafast"))) ? "normal" : "fast";
    }
    if (!nv) {
        char *vs = ms_py_str(value);
        char *msg = ms_msg("unknown fast mode: ", vs, NULL, NULL);
        free(vs);
        free(raw);
        json_free(writes);
        return ms_effects(ms_err_env(rid, 4002, msg), json_array());
    }
    /* Route resolution is a seam; this is the branch every selected model
     * reaches once the overrides exist (write agent.service_tier, report nv). */
    ms_write_set(writes, "agent.service_tier", json_string(nv));
    env = ms_kv_env(rid, "fast", json_string(nv), NULL);
    free(raw);
    return ms_effects(env, writes);
}

json_t *w32_methodscon_ce0f5d_p1_verbose_set(const json_t *rid, const json_t *value,
                                             const json_t *current) {
    static const char *const CYCLE[4] = {"off", "new", "all", "verbose"};
    json_t *writes = json_array();
    json_t *env;
    const char *nv = NULL;
    char *raw = NULL;
    int i;

    if (ms_truthy(value) && !(ms_is_string(value) && !strcmp(value->str_val, "cycle"))) {
        raw = ms_py_str(value);
        ms_lower_trim(raw);
        for (i = 0; i < 4; i++)
            if (!strcmp(raw, CYCLE[i])) { nv = CYCLE[i]; break; }
        if (!nv) {
            char *vs = ms_py_str(value);
            char *msg = ms_msg("unknown verbose mode: ", vs, NULL, NULL);
            free(vs);
            free(raw);
            json_free(writes);
            return ms_effects(ms_err_env(rid, 4002, msg), json_array());
        }
    } else {
        int idx = 2; /* cycle.index(cur) if cur in cycle else 2 */
        const char *cur = ms_str_or(current, "");
        for (i = 0; i < 4; i++)
            if (!strcmp(cur, CYCLE[i])) { idx = i; break; }
        nv = CYCLE[(idx + 1) % 4];
    }
    free(raw);
    ms_write_set(writes, "display.tool_progress", json_string(nv));
    env = ms_kv_env(rid, "verbose", json_string(nv), NULL);
    return ms_effects(env, writes);
}

/* _word_setters specs */
enum { WS_SET = 0, WS_APPROVALS, WS_DETAILS_MODE, WS_THINKING, WS_THEME, WS_INDICATOR, WS_VOICE };

typedef struct {
    const char *key;
    int raw_norm; /* 1 -> _raw_word */
    const char *const *allowed;
    int nallowed;
    int kind;
} ms_word_spec;

static const char *const MS_ALLOW_BUSY[3] = {"queue", "steer", "interrupt"};
static const char *const MS_ALLOW_APPROVAL[3] = {"manual", "smart", "off"};
static const char *const MS_ALLOW_DETAILS[3] = {"hidden", "collapsed", "expanded"};
static const char *const MS_ALLOW_THINKING[3] = {"collapsed", "truncated", "full"};
static const char *const MS_ALLOW_THEME[3] = {"auto", "light", "dark"};
static const char *const MS_ALLOW_INDICATOR[4] = {"ascii", "emoji", "kaomoji", "unicode"};
static const char *const MS_ALLOW_VOICE[2] = {"chained", "gpt-live"};
static const char *const MS_DETAIL_SECTIONS[4] = {"thinking", "tools", "subagents", "activity"};

static const ms_word_spec MS_WORD_SPECS[7] = {
    {"busy", 0, MS_ALLOW_BUSY, 3, WS_SET},
    {"approvals.mode", 0, MS_ALLOW_APPROVAL, 3, WS_APPROVALS},
    {"details_mode", 0, MS_ALLOW_DETAILS, 3, WS_DETAILS_MODE},
    {"thinking_mode", 0, MS_ALLOW_THINKING, 3, WS_THINKING},
    {"theme", 0, MS_ALLOW_THEME, 3, WS_THEME},
    {"indicator", 1, MS_ALLOW_INDICATOR, 4, WS_INDICATOR},
    {"voice.voice_chat_mode", 0, MS_ALLOW_VOICE, 2, WS_VOICE},
};

static json_t *ms_word_error(const json_t *rid, const ms_word_spec *spec, const json_t *value,
                             const char *raw) {
    char *vs = ms_py_str(value);
    char *msg = NULL;
    switch (spec->kind) {
    case WS_SET:          msg = ms_msg("unknown busy mode: ", vs, NULL, NULL); break;
    case WS_APPROVALS:    msg = ms_msg("unknown approval mode: ", vs, "; pick one of manual|smart|off", NULL); break;
    case WS_DETAILS_MODE: msg = ms_msg("unknown details_mode: ", vs, NULL, NULL); break;
    case WS_THINKING:     msg = ms_msg("unknown thinking_mode: ", vs, NULL, NULL); break;
    case WS_THEME:        msg = ms_msg("unknown theme value: ", vs, " (use auto|light|dark)", NULL); break;
    case WS_INDICATOR: {
        /* {raw!r}: the normalised word in Python repr form. */
        char *repr = ms_repr_cstr(raw);
        msg = ms_msg("unknown indicator: ", repr, "; pick one of ascii|emoji|kaomoji|unicode", NULL);
        free(repr);
        break;
    }
    case WS_VOICE:        msg = ms_msg("unknown voice chat mode: ", vs, "; pick chained|gpt-live", NULL); break;
    }
    free(vs);
    return ms_err_env(rid, 4002, msg ? msg : "unknown value");
}

json_t *w32_methodscon_ce0f5d_p1_word_set(const json_t *rid, const json_t *key,
                                          const json_t *value) {
    const char *report_key = ms_str_or(key, "");
    const ms_word_spec *spec = NULL;
    char *raw = NULL;
    json_t *writes = json_array();
    json_t *env;
    size_t i;
    int accepted = 0;

    /* `approval_mode` is the legacy alias: it reports the real key. */
    if (!strcmp(report_key, "approval_mode")) report_key = "approvals.mode";
    for (i = 0; i < sizeof(MS_WORD_SPECS) / sizeof(MS_WORD_SPECS[0]); i++)
        if (!strcmp(report_key, MS_WORD_SPECS[i].key)) { spec = &MS_WORD_SPECS[i]; break; }
    if (!spec) {
        char *msg = ms_msg("unknown config key: ", report_key, NULL, NULL);
        json_free(writes);
        return ms_effects(ms_err_env(rid, 4002, msg), json_array());
    }
    raw = spec->raw_norm ? ms_raw_word_of(value) : ms_word_of(value);
    for (i = 0; i < (size_t)spec->nallowed; i++)
        if (!strcmp(raw, spec->allowed[i])) { accepted = 1; break; }
    if (!accepted) {
        env = ms_word_error(rid, spec, value, raw);
        free(raw);
        json_free(writes);
        return ms_effects(env, json_array());
    }
    switch (spec->kind) {
    case WS_SET:
        ms_write_set(writes, "display.busy_input_mode", json_string(raw));
        break;
    case WS_APPROVALS: {
        json_t *emit = json_object();
        ms_write_set(writes, "approvals.mode", json_string(raw));
        json_set(emit, "emit_all", json_bool(true));
        json_append(writes, emit);
        break;
    }
    case WS_DETAILS_MODE: {
        json_t *sections = json_object();
        json_t *fields = json_object();
        for (i = 0; i < 4; i++) json_set(sections, MS_DETAIL_SECTIONS[i], json_string(raw));
        json_set(fields, "details_mode", json_string(raw));
        ms_write_section(writes, sections, NULL, fields);
        break;
    }
    case WS_THINKING: {
        json_t *set = json_object();
        json_t *row = json_object();
        json_set(set, "display.thinking_mode", json_string(raw));
        json_set(set, "display.details_mode", json_string(!strcmp(raw, "full") ? "expanded" : "collapsed"));
        json_set(row, "set", set);
        json_append(writes, row);
        break;
    }
    case WS_THEME:
        ms_write_set(writes, "display.tui_theme", json_string(raw));
        break;
    case WS_INDICATOR:
        ms_write_set(writes, "display.tui_status_indicator", json_string(raw));
        break;
    case WS_VOICE:
        ms_write_set(writes, "voice.voice_chat_mode", json_string(raw));
        break;
    }
    env = ms_kv_env(rid, report_key, json_string(raw), NULL);
    free(raw);
    return ms_effects(env, writes);
}

static int ms_is_detail_section(const char *section) {
    size_t i;
    for (i = 0; i < 4; i++)
        if (!strcmp(section, MS_DETAIL_SECTIONS[i])) return 1;
    return 0;
}

json_t *w32_methodscon_ce0f5d_p1_details_section(const json_t *rid, const json_t *key,
                                                 const json_t *value) {
    const char *k = ms_str_or(key, "");
    const char *dot = strchr(k, '.');
    const char *section = dot ? dot + 1 : "";
    char *nv;
    json_t *writes = json_array();
    json_t *env;
    size_t i;

    if (!ms_is_detail_section(section)) {
        char *msg = ms_msg("unknown section: ", section, NULL, NULL);
        json_free(writes);
        return ms_effects(ms_err_env(rid, 4002, msg), json_array());
    }
    nv = ms_word_of(value);
    if (nv[0]) {
        int mode_ok = 0;
        for (i = 0; i < 3; i++)
            if (!strcmp(nv, MS_ALLOW_DETAILS[i])) mode_ok = 1;
        if (!mode_ok) {
            char *vs = ms_py_str(value);
            char *msg = ms_msg("unknown details_mode: ", vs, NULL, NULL);
            free(vs);
            free(nv);
            json_free(writes);
            return ms_effects(ms_err_env(rid, 4002, msg), json_array());
        }
        {
            json_t *sections = json_object();
            json_set(sections, section, json_string(nv));
            ms_write_section(writes, sections, NULL, NULL);
        }
    } else {
        json_t *drop = json_array();
        json_append(drop, json_string(section));
        ms_write_section(writes, NULL, drop, NULL);
    }
    env = ms_kv_env(rid, k, json_string(nv), NULL);
    free(nv);
    return ms_effects(env, writes);
}

/* _CONFIG_SETTERS key -> setter, plus the details_mode.<section> prefix and
 * the _DISPLAY_TOGGLE_KEYS set.  Values are the upstream function names. */
typedef struct {
    const char *key;
    const char *handler;
} ms_key_handler;

static const ms_key_handler MS_KEY_HANDLERS[26] = {
    {"model", "_set_model"},
    {"fast", "_set_fast"},
    {"busy", "_set_busy"},
    {"verbose", "_set_verbose"},
    {"focus", "_set_focus"},
    {"approval_mode", "_set_approval_mode"},
    {"approvals.mode", "_set_word"},
    {"yolo", "_set_yolo"},
    {"reasoning", "_set_reasoning"},
    {"details_mode", "_set_word"},
    {"thinking_mode", "_set_word"},
    {"density", "_set_toggle"},
    {"battery", "_set_toggle"},
    {"timestamps", "_set_toggle"},
    {"theme", "_set_word"},
    {"statusbar", "_set_toggle"},
    {"mouse", "_set_toggle"},
    {"indicator", "_set_word"},
    {"voice.voice_chat_mode", "_set_word"},
    {"cwd", "_set_cwd"},
    {"terminal.cwd", "_set_cwd"},
    {"workdir", "_set_cwd"},
    {"prompt", "_set_prompt"},
    {"personality", "_set_personality"},
    {"skin", "_set_skin"},
    {"daybreak", "_set_daybreak"},
};

static const char *const MS_DISPLAY_TOGGLES[3] = {
    "display.message_reactions", "display.in_app_tips", "display.in_app_tours",
};

json_t *w32_methodscon_ce0f5d_p1_resolve_key(const json_t *key) {
    const char *k = ms_str_or(key, "");
    json_t *o;
    size_t i;

    for (i = 0; i < sizeof(MS_KEY_HANDLERS) / sizeof(MS_KEY_HANDLERS[0]); i++) {
        if (!strcmp(k, MS_KEY_HANDLERS[i].key)) {
            o = json_object();
            json_set(o, "handler", json_string(MS_KEY_HANDLERS[i].handler));
            return o;
        }
    }
    if (!strncmp(k, "details_mode.", 13)) {
        o = json_object();
        json_set(o, "handler", json_string("_set_details_section"));
        return o;
    }
    for (i = 0; i < 3; i++) {
        if (!strcmp(k, MS_DISPLAY_TOGGLES[i])) {
            o = json_object();
            json_set(o, "handler", json_string("_set_display_toggle"));
            return o;
        }
    }
    o = json_object();
    json_set(o, "kind", json_string("error"));
    {
        char *msg = ms_msg("unknown config key: ", k, NULL, NULL);
        json_set(o, "message", json_string(msg));
        free(msg);
    }
    return o;
}

/* ── gate p2: payload shaping / precedence / predicates ─────────────── */

json_t *w32_methodscon_ce0f5d_p2_err(const json_t *rid, const json_t *code, const json_t *message) {
    int c = 0;
    if (code) {
        if (code->type == JSON_NUMBER) c = (int)code->num_val;
        else if (code->type == JSON_BOOL) c = code->bool_val ? 1 : 0;
    }
    return ms_err_env(rid, c, ms_str_or(message, ""));
}

json_t *w32_methodscon_ce0f5d_p2_ok(const json_t *rid, const json_t *result) {
    return ms_ok_env(rid, result ? json_copy(result) : json_null());
}

json_t *w32_methodscon_ce0f5d_p2_kv(const json_t *rid, const json_t *key, const json_t *value,
                                    const json_t *extra) {
    return ms_kv_env(rid, ms_str_or(key, ""), value ? json_copy(value) : json_null(),
                     extra ? json_copy(extra) : NULL);
}

json_t *w32_methodscon_ce0f5d_p2_model_ok(const json_t *rid, const json_t *key,
                                          const json_t *value, const json_t *warning,
                                          const json_t *confirm_message, const json_t *scope,
                                          const json_t *extra) {
    json_t *res = json_object();
    const json_t *rw = (warning && warning->type != JSON_NULL) ? warning : NULL;
    const json_t *rc = (confirm_message && confirm_message->type != JSON_NULL) ? confirm_message : NULL;
    const json_t *rs = (scope && scope->type != JSON_NULL) ? scope : NULL;
    json_set(res, "key", json_string(ms_str_or(key, "")));
    json_set(res, "value", value ? json_copy(value) : json_null());
    json_set(res, "warning", rw ? json_copy(rw) : json_string(""));
    json_set(res, "confirm_required", json_bool(ms_truthy(confirm_message) ? true : false));
    json_set(res, "confirm_message", rc ? json_copy(rc) : json_string(""));
    json_set(res, "scope", rs ? json_copy(rs) : json_string("session"));
    ms_merge_extra(res, extra ? json_copy(extra) : NULL);
    return ms_ok_env(rid, res);
}

json_t *w32_methodscon_ce0f5d_p2_confirm_required(const json_t *confirm_message) {
    return json_bool(ms_truthy(confirm_message) ? true : false);
}

/* _REASONING_DISPLAY_WORDS */
typedef struct {
    const char *w0, *w1, *w2;
    const char *reported;
    const char *field;
    int field_val;
    const char *thinking;
} ms_reason_row;

static const ms_reason_row MS_REASON_ROWS[4] = {
    {"show", "on", NULL, "show", "show_reasoning", 1, "expanded"},
    {"hide", "off", NULL, "hide", "show_reasoning", 0, "hidden"},
    {"full", "all", NULL, "full", "reasoning_full", 1, "expanded"},
    {"clamp", "collapse", "short", "clamp", "reasoning_full", 0, "collapsed"},
};

static const char *const MS_REASONING_EFFORTS[10] = {
    "none", "false", "disabled", "minimal", "low", "medium", "high", "xhigh", "max", "ultra",
};

json_t *w32_methodscon_ce0f5d_p2_reasoning(const json_t *rid, const json_t *value,
                                           const json_t *scope) {
    char *arg = ms_word_of(value);
    json_t *writes = json_array();
    json_t *env = NULL;
    size_t i;

    for (i = 0; i < 4 && !env; i++) {
        const ms_reason_row *r = &MS_REASON_ROWS[i];
        int hit = !strcmp(arg, r->w0) || (r->w1 && !strcmp(arg, r->w1)) ||
                  (r->w2 && !strcmp(arg, r->w2));
        if (!hit) continue;
        {
            json_t *sections = json_object();
            json_t *fields = json_object();
            json_set(sections, "thinking", json_string(r->thinking));
            json_set(fields, r->field, json_bool(r->field_val ? true : false));
            ms_write_section(writes, sections, NULL, fields);
        }
        env = ms_kv_env(rid, "reasoning", json_string(r->reported), NULL);
    }
    if (!env) {
        int ok = 0;
        for (i = 0; i < 10; i++)
            if (!strcmp(arg, MS_REASONING_EFFORTS[i])) ok = 1;
        if (!ok) {
            char *vs = ms_py_str(value);
            char *msg = ms_msg("unknown reasoning value: ", vs, NULL, NULL);
            free(vs);
            free(arg);
            json_free(writes);
            return ms_effects(ms_err_env(rid, 4002, msg), json_array());
        }
        /* The session is a seam: with no session the global config write is
         * the road taken whatever `scope` says. */
        (void)scope;
        ms_write_set(writes, "agent.reasoning_effort", json_string(arg));
        env = ms_kv_env(rid, "reasoning", json_string(arg), NULL);
    }
    free(arg);
    return ms_effects(env, writes);
}

/* _toggle_setters specs */
enum { TG_DENSITY = 0, TG_BATTERY, TG_TIMESTAMPS, TG_STATUSBAR, TG_MOUSE };

typedef struct {
    const char *key;
    int raw_norm;
    const char *cfg_key;
    int kind;
} ms_toggle_spec;

static const char *const MS_STATUS_NAMES[4] = {"on", "off", "top", "bottom"};

/* _MOUSE_TRACKING_ALIASES */
static const char *const MS_MOUSE_ALIASES[16][2] = {
    {"0", "off"}, {"1", "all"}, {"all", "all"}, {"any", "all"},
    {"button", "buttons"}, {"buttons", "buttons"}, {"click", "buttons"}, {"false", "off"},
    {"full", "all"}, {"no", "off"}, {"off", "off"}, {"on", "all"},
    {"scroll", "wheel"}, {"true", "all"}, {"wheel", "wheel"}, {"yes", "all"},
};

/* agent_factory._coerce_statusbar */
static const char *ms_coerce_statusbar(const json_t *raw, char *buf, size_t cap) {
    if (raw && raw->type == JSON_BOOL && !raw->bool_val) return "off";
    if (ms_is_string(raw)) {
        char *s = ms_py_str(raw);
        const char *picked = "top";
        ms_lower_trim(s);
        if (!strcmp(s, "off")) picked = "off";
        else if (!strcmp(s, "top")) picked = "top";
        else if (!strcmp(s, "bottom")) picked = "bottom";
        snprintf(buf, cap, "%s", picked);
        free(s);
        return buf;
    }
    return "top";
}

/* agent_factory._display_mouse_tracking */
static const char *ms_display_mouse_tracking(const json_t *display, char *buf, size_t cap) {
    const json_t *raw;
    if (!display || display->type != JSON_OBJECT) return "all";
    raw = json_obj_get(display, "mouse_tracking");
    if (!raw) raw = json_obj_get(display, "tui_mouse");
    if (ms_is_string(raw)) {
        char *s = ms_py_str(raw);
        size_t i;
        const char *v = "all";
        ms_lower_trim(s);
        for (i = 0; i < 16; i++)
            if (!strcmp(s, MS_MOUSE_ALIASES[i][0])) { v = MS_MOUSE_ALIASES[i][1]; break; }
        snprintf(buf, cap, "%s", v);
        free(s);
        return buf;
    }
    if (raw && ((raw->type == JSON_BOOL && !raw->bool_val) ||
                (raw->type == JSON_NUMBER && raw->num_val == 0.0)))
        return "off";
    return "all";
}

static json_t *ms_toggle_alias_value(int kind, const char *raw) {
    size_t i;
    if (kind == TG_DENSITY) {
        if (!strcmp(raw, "on")) return json_bool(true);
        if (!strcmp(raw, "off")) return json_bool(false);
        return NULL;
    }
    if (kind == TG_BATTERY) {
        if (!strcmp(raw, "on") || !strcmp(raw, "true") || !strcmp(raw, "yes")) return json_bool(true);
        if (!strcmp(raw, "off") || !strcmp(raw, "false") || !strcmp(raw, "no")) return json_bool(false);
        return NULL;
    }
    if (kind == TG_TIMESTAMPS) {
        int idx = ms_bool_index(raw);
        return idx < 0 ? json_null() : json_bool(idx < 4 ? true : false);
    }
    if (kind == TG_STATUSBAR) {
        for (i = 0; i < 4; i++)
            if (!strcmp(raw, MS_STATUS_NAMES[i])) return json_string(i == 0 ? "top" : raw);
        return NULL;
    }
    if (kind == TG_MOUSE) {
        for (i = 0; i < 16; i++)
            if (!strcmp(raw, MS_MOUSE_ALIASES[i][0])) return json_string(MS_MOUSE_ALIASES[i][1]);
        return NULL;
    }
    return NULL;
}

json_t *w32_methodscon_ce0f5d_p2_toggle_set(const json_t *rid, const json_t *key,
                                            const json_t *value, const json_t *display) {
    static const ms_toggle_spec SPECS[5] = {
        {"density", 0, "display.tui_compact", TG_DENSITY},
        {"battery", 0, "display.battery", TG_BATTERY},
        {"timestamps", 0, "display.timestamps", TG_TIMESTAMPS},
        {"statusbar", 0, "display.tui_statusbar", TG_STATUSBAR},
        {"mouse", 1, "display.mouse_tracking", TG_MOUSE},
    };
    const char *k = ms_str_or(key, "");
    const ms_toggle_spec *spec = NULL;
    const json_t *d = (display && display->type == JSON_OBJECT) ? display : NULL;
    char *raw;
    json_t *nv = NULL;
    json_t *writes = json_array();
    json_t *env;
    size_t i;

    for (i = 0; i < 5; i++)
        if (!strcmp(k, SPECS[i].key)) { spec = &SPECS[i]; break; }
    if (!spec) {
        char *msg = ms_msg("unknown config key: ", k, NULL, NULL);
        json_free(writes);
        return ms_effects(ms_err_env(rid, 4002, msg), json_array());
    }
    raw = spec->raw_norm ? ms_raw_word_of(value) : ms_word_of(value);
    if (!raw[0] || !strcmp(raw, "toggle")) {
        switch (spec->kind) {
        case TG_DENSITY:
            nv = json_bool(ms_truthy(d ? json_obj_get(d, "tui_compact") : NULL) ? false : true);
            break;
        case TG_BATTERY:
            nv = json_bool(ms_truthy(d ? json_obj_get(d, "battery") : NULL) ? false : true);
            break;
        case TG_TIMESTAMPS:
            nv = json_bool(ms_truthy(d ? json_obj_get(d, "timestamps") : NULL) ? false : true);
            break;
        case TG_STATUSBAR: {
            char buf[32];
            const json_t *cur = d ? json_obj_get(d, "tui_statusbar") : NULL;
            json_t *tmp = NULL;
            if (!cur) { tmp = json_string("top"); cur = tmp; }
            nv = json_string(!strcmp(ms_coerce_statusbar(cur, buf, sizeof(buf)), "off") ? "top" : "off");
            json_free(tmp);
            break;
        }
        case TG_MOUSE: {
            char buf[32];
            const char *cur = ms_display_mouse_tracking(d, buf, sizeof(buf));
            nv = json_string(!strcmp(cur, "off") ? "all" : "off");
            break;
        }
        }
    } else {
        nv = ms_toggle_alias_value(spec->kind, raw);
    }
    if (!nv || nv->type == JSON_NULL) {
        char *vs = ms_py_str(value);
        char *msg = ms_msg("unknown ", k, " value: ", vs);
        free(vs);
        free(raw);
        json_free(nv);
        json_free(writes);
        return ms_effects(ms_err_env(rid, 4002, msg), json_array());
    }
    {
        json_t *reported;
        if (spec->kind == TG_STATUSBAR || spec->kind == TG_MOUSE)
            reported = json_copy(nv);
        else
            reported = json_string(ms_truthy(nv) ? "on" : "off");
        ms_write_set(writes, spec->cfg_key, json_copy(nv));
        env = ms_kv_env(rid, k, reported, NULL);
    }
    json_free(nv);
    free(raw);
    return ms_effects(env, writes);
}

/* _set_display_toggle / _set_daybreak share the _BOOL_WORDS gate; cfg_key is
 * the key each writes ("agent.daybreak" for daybreak, the key itself otherwise). */
json_t *w32_methodscon_ce0f5d_p2_bool_set(const json_t *rid, const json_t *key,
                                          const json_t *value, const json_t *cfg_key) {
    const char *k = ms_str_or(key, "");
    const char *cfg = ms_str_or(cfg_key, k);
    json_t *on = w32_methodscon_ce0f5d_p1_bool_word(value);
    json_t *writes = json_array();
    json_t *env;
    if (!on || on->type == JSON_NULL) {
        char *msg = ms_msg(k, " takes true or false", NULL, NULL);
        json_free(on);
        json_free(writes);
        return ms_effects(ms_err_env(rid, 4002, msg), json_array());
    }
    ms_write_set(writes, cfg, json_copy(on));
    env = ms_kv_env(rid, k, on, NULL);
    return ms_effects(env, writes);
}

/* _set_prompt: the "clear" word is the only mapping -- every other value is
 * stored (and echoed) verbatim.  The config round-trip is the seam; the write
 * it decides is reported as {"save": {...}}. */
json_t *w32_methodscon_ce0f5d_p2_prompt_set(const json_t *rid, const json_t *value) {
    int clear = ms_is_string(value) && value->str_val && !strcmp(value->str_val, "clear");
    json_t *writes = json_array();
    json_t *save = json_object();
    json_t *row = json_object();
    json_t *env;
    if (!clear && value) json_set(save, "custom_prompt", json_copy(value));
    json_set(row, "save", save);
    json_append(writes, row);
    env = ms_kv_env(rid, "prompt", clear ? json_string("") : json_copy(value), NULL);
    return ms_effects(env, writes);
}

/* config.set's stale-session guard: session is None, a session_id was sent, the
 * key writes a wider scope without one, and scope is not explicitly global. */
json_t *w32_methodscon_ce0f5d_p2_is_stale_session(const json_t *key, const json_t *session_id_present,
                                                  const json_t *session_exists, const json_t *scope) {
    const char *k = ms_str_or(key, "");
    char *scope_w = ms_word_of(scope);
    int scoped = !strcmp(k, "model") || !strcmp(k, "fast") || !strcmp(k, "yolo") ||
                 !strcmp(k, "reasoning");
    int stale = !ms_truthy(session_exists) && ms_truthy(session_id_present) && scoped &&
                strcmp(scope_w, "global") != 0;
    free(scope_w);
    return json_bool(stale ? true : false);
}
