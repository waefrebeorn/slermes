/* cli_tui_mixin_flow2.c — FLOW port of the UNCLAIMED remainder of
 * hermes_cli/cli_tui_mixin.py (prompt/placeholder/hint display cluster, the
 * small fragment delegators, Tab completion gesture, palette open, stash
 * panel restore, the five key-handler factories and Ctrl+Z suspend).
 * Companion to cli_tui_mixin_flow.c (navigation/editor/stash) and
 * port_tui_dispatch_flow.c (Enter/Ctrl dispatch); functions claimed there
 * are SEAMED here (self._audio_level_bar, self._tui_restore_stash_payload)
 * or kept LIVE-bound when ported in THIS file (_get_tui_prompt_symbols,
 * _build_tui_style_dict). The real staticmethod _tui_set_filter (claimed in
 * flow1) stays LIVE in the oracle and is INLINED here (ctm2_set_filter).
 *
 * All terminal / skin / profile / i18n / stash / app I/O is INJECTED through
 * the ctm2_io_t universal seam dispatcher: every live call goes through
 * ctm2_seam(io, name, args); the harness dispatcher appends nothing itself —
 * ctm2_seam records {ev, args, ret} into io->trace, returns the scripted
 * result (caller-owned), and models a raised exception as {"__raise__":
 * {type, message, ...}}. An "import_raise" script entry on an
 * "import:<module>" key models a failed in-function import: the dispatcher
 * returns {"__raise__": spec, "__untrace__": true} and ctm2_import_check
 * raises WITHOUT a trace token (the live import statement records nothing).
 *
 * Built-in doubles (NOT scripted, identical behaviour in the oracle):
 *   buffer.reset / buffer.insert_text — prompt_toolkit Buffer semantics
 *   app.invalidate                    — prompt_toolkit Application no-op
 * Pinned constants (NOT scripted, identical on both sides):
 *   cli._DIM = "\x1b[2;3m", cli._RST = "\x1b[0m" (hermes_cli/cli_render.py)
 *   _TUI_MODAL_HINTS / _TUI_CONNECTION_HINT_KEYS class tables (byte-pinned)
 *
 * io->state is a json OBJECT mirroring the CLI instance attributes:
 *   buf {text, cursor_position, complete_state, suggestion},
 *   _voice_recording/_voice_processing/_voice_mode (bool), _voice_live_text,
 *   _sudo_state/_secret_state/_connection_state/_approval_state/
 *   _slash_confirm_state/_clarify_state/_model_picker_state/
 *   _command_palette_state (object|null), _clarify_freetext (bool),
 *   _clarify_deadline/_sudo_deadline/_secret_deadline/_approval_deadline/
 *   _slash_confirm_deadline (number|null), _command_running/
 *   _command_blocks_input (bool), _command_status, _agent_running (bool),
 *   _modern_layout (bool), busy_input_mode, _attached_images (list),
 *   _image_counter, _tui_style_base (object|null), _app (object|null,
 *   carries the assigned "style"), _prompt_stash (object|null, carries
 *   "panel_cursor"), _composer_placeholder.
 *
 * Ownership: seam args are OWNED by ctm2_seam (moved into the trace event or
 * freed); seam results are caller-owned (json_free). io->trace / io->state
 * are harness-owned. Every ctm2_* flow returns a NEW reference (json null /
 * bool / string / number / array / object) or a propagated {"__raise__":
 * spec} sentinel. json_set(obj, key, NULL) is a libjson NO-OP — explicit
 * json_null() is set for Python None everywhere.
 *
 * BaseException kinds (SystemExit / KeyboardInterrupt / GeneratorExit /
 * CancelledError) PASS THROUGH every except-Exception site; plain exceptions
 * are swallowed there exactly as live. Fail-closed: with no dispatcher
 * installed ctm2_seam returns a RuntimeError raise sentinel (never a silent
 * success); the oracle exits 1 on zero cases.
 *
 * Fixture constraints (documented representation limits): scripted numbers
 * are integral (libjson has no int/float identity; py type name of a number
 * is "int"); _clarify_state.selected_indices is a SORTED unique int list on
 * both sides (live set normalized by sorted()); style-dict values and
 * _format_image_attachment_badges results are str|null|number|bool; filter
 * strings are str|null; choices entries are lists (slash-confirm) or str
 * (clarify/approval); handler idx >= 0; complete_state is null or a
 * non-empty object (never {}); ctrl_z "platform" is explicit per case;
 * run_in_terminal is a pure scripted seam — the _suspend closure body
 * (os.write/os.kill) is process-level and OUT of the seam's scripted scope.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

/* ── seam dispatcher + trace + state ───────────────────────────────────── */
typedef struct ctm2_io_t {
    json_t *(*call)(struct ctm2_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* ARRAY of {ev, args, ret} events (harness-owned) */
    json_t *state;   /* OBJECT mirror (harness-owned) */
    void   *ctx;
} ctm2_io_t;

/* class constants (byte-pinned from the quarry class body) */
static const char *CTM2_MODAL_HINTS[][3] = {
    {"_sudo_state", "_sudo_deadline", "cli.tui.hint_sudo"},
    {"_secret_state", "_secret_deadline", "cli.tui.hint_secret"},
    {"_approval_state", "_approval_deadline", "cli.tui.hint_approval"},
    {"_slash_confirm_state", "_slash_confirm_deadline", "cli.tui.hint_slash_confirm"},
};
static const char *CTM2_CONN_HINT_KEYS[][2] = {
    {"form", "cli.tui.hint_connection_form"},
    {"failed", "cli.tui.hint_connection_form"},
    {"url", "cli.tui.hint_connection_url"},
    {"authorized", "cli.tui.hint_connection_authorized"},
    {"waiting", "cli.tui.hint_connection_waiting"},
};
#define CTM2_CONN_HINT_DEFAULT "cli.tui.hint_connection_default"
/* cli_render constants (pinned, not scripted) */
#define CTM2_DIM "\x1b[2;3m"
#define CTM2_RST "\x1b[0m"
/* prompt symbols / state icons (UTF-8 bytes, ASCII source) */
#define CTM2_PROMPT_DEFAULT "\xe2\x9d\xaf "          /* ❯ + space */
#define CTM2_ICON_RECORD   "\xe2\x97\x8f"            /* ● */
#define CTM2_ICON_PROCESS  "\xe2\x97\x89"            /* ◉ */
#define CTM2_ICON_SUDO     "\xf0\x9f\x94\x90"        /* 🔐 */
#define CTM2_ICON_SECRET   "\xf0\x9f\x94\x91"        /* 🔑 */
#define CTM2_ICON_CONN     "\xe2\x9a\x99"            /* ⚙ */
#define CTM2_ICON_WARN     "\xe2\x9a\xa0"            /* ⚠ */
#define CTM2_ICON_EDIT     "\xe2\x9c\x8e"            /* ✎ */
#define CTM2_ICON_AGENT    "\xe2\x98\xa4"            /* ☤ */
#define CTM2_ICON_MIC      "\xf0\x9f\x8e\xa4"        /* 🎤 */

static bool ctm2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
    case JSON_BOOL:   return v->bool_val;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count > 0;
    default:          return false; /* JSON_NULL */
    }
}

static bool ctm2_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static json_t *ctm2_raise_spec(const json_t *r) {
    return ctm2_raised(r) ? json_obj_get(r, "__raise__") : NULL;
}
static bool ctm2_is_base_exc(const char *type) {
    return type && (strcmp(type, "SystemExit") == 0 ||
                    strcmp(type, "KeyboardInterrupt") == 0 ||
                    strcmp(type, "GeneratorExit") == 0 ||
                    strcmp(type, "CancelledError") == 0);
}
/* true when a raised seam result must propagate through an except-Exception
 * site (BaseException kinds); false when live would swallow it. */
static bool ctm2_base_raised(const json_t *r) {
    if (!ctm2_raised(r)) return false;
    const json_t *spec = ctm2_raise_spec(r);
    return ctm2_is_base_exc(json_get_str(spec, "type", ""));
}

static void ctm2_record(ctm2_io_t *io, const char *name, json_t *args, json_t *ret) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) {
        json_free(args); json_free(ret); return;
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? ret : json_null());
    json_append(io->trace, ev);
}

/* args OWNED; result caller-owned. NULL result == Python None. Fail-closed
 * when no dispatcher is installed. */
static json_t *ctm2_seam(ctm2_io_t *io, const char *name, json_t *args) {
    if (!io || !io->call) {
        json_free(args);
        json_t *spec = json_object();
        json_set(spec, "type", json_string("RuntimeError"));
        json_set(spec, "message", json_string("no seam dispatcher installed"));
        json_t *out = json_object();
        json_set(out, "__raise__", spec);
        return out;
    }
    json_t *res = io->call(io, name, args);
    bool untrace = res && res->type == JSON_OBJECT &&
                   json_is_true(json_obj_get(res, "__untrace__"));
    if (untrace) {
        if (ctm2_raised(res)) {
            json_t *out = json_object();
            json_set(out, "__raise__", json_copy(ctm2_raise_spec(res)));
            json_free(res); json_free(args);
            return out;
        }
        json_free(res); json_free(args);
        return NULL;
    }
    ctm2_record(io, name, args, res ? json_copy(res) : json_null());
    return res;
}

/* In-function import check: NULL on success (records nothing), raise
 * sentinel on a scripted import failure. */
static json_t *ctm2_import_check(ctm2_io_t *io, const char *key) {
    if (!io || !io->call) return NULL;
    json_t *res = io->call(io, key, NULL);
    if (ctm2_raised(res)) {
        json_t *out = json_object();
        json_set(out, "__raise__", json_copy(ctm2_raise_spec(res)));
        json_free(res);
        return out;
    }
    json_free(res);
    return NULL;
}

/* ── raise-spec builders (unscripted live errors) ──────────────────────── */
static json_t *ctm2_raise_new(const char *type, const char *message) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type));
    json_set(spec, "message", json_string(message));
    json_t *out = json_object();
    json_set(out, "__raise__", spec);
    return out;
}

/* ── state accessors ───────────────────────────────────────────────────── */
static json_t *ctm2_st(ctm2_io_t *io, const char *k) {
    return (io && io->state) ? json_obj_get(io->state, k) : NULL;
}
static void ctm2_st_set(ctm2_io_t *io, const char *k, json_t *v) {
    if (io && io->state) json_set(io->state, k, v); else json_free(v);
}
/* direct dict subscript: absent key raises KeyError (str() == repr(key)) */
static json_t *ctm2_subscript(json_t *obj, const char *key, json_t **out) {
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    if (!v) {
        char buf[256];
        snprintf(buf, sizeof buf, "'%s'", key);
        *out = NULL;
        return ctm2_raise_new("KeyError", buf);
    }
    *out = v;
    return NULL;
}

/* ── UTF-8 / Python string semantics ───────────────────────────────────── */
static unsigned long ctm2_cp(const char *s, size_t *adv) {
    const unsigned char *u = (const unsigned char *)s;
    unsigned long cp; size_t n;
    if (u[0] < 0x80)                { cp = u[0];        n = 1; }
    else if ((u[0] & 0xE0) == 0xC0) { cp = u[0] & 0x1F; n = 2; }
    else if ((u[0] & 0xF0) == 0xE0) { cp = u[0] & 0x0F; n = 3; }
    else if ((u[0] & 0xF8) == 0xF0) { cp = u[0] & 0x07; n = 4; }
    else                            { cp = u[0];        n = 1; }
    for (size_t i = 1; i < n; i++) cp = (cp << 6) | (u[i] & 0x3F);
    *adv = n;
    return cp;
}
static size_t ctm2_utf8_len(const char *s) {
    size_t n = 0, adv;
    if (!s) return 0;
    while (*s) { ctm2_cp(s, &adv); s += adv; n++; }
    return n;
}
/* first n code points, malloc'd (Python text[:n]); n past end -> whole */
static char *ctm2_utf8_prefix(const char *s, size_t n) {
    if (!s) s = "";
    const char *p = s; size_t adv, i = 0;
    while (*p && i < n) { ctm2_cp(p, &adv); p += adv; i++; }
    size_t len = (size_t)(p - s);
    char *out = (char *)malloc(len + 1);
    memcpy(out, s, len); out[len] = '\0';
    return out;
}

/* Python str.isspace() White_Space set (probe-verified on py3.14) */
static bool ctm2_is_ws_cp(unsigned long cp) {
    if (cp >= 0x9 && cp <= 0xD) return true;
    if (cp >= 0x1C && cp <= 0x1F) return true;
    if (cp == 0x20 || cp == 0x85 || cp == 0xA0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200A) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F ||
        cp == 0x205F || cp == 0x3000) return true;
    return false;
}
/* str.rstrip() over code points (malloc'd) */
static char *ctm2_rstrip(const char *s) {
    if (!s) s = "";
    const char *p = s; size_t adv, end = 0, off = 0;
    while (*p) {
        unsigned long cp = ctm2_cp(p, &adv);
        off += adv;
        if (!ctm2_is_ws_cp(cp)) end = off;
        p += adv;
    }
    char *out = (char *)malloc(end + 1);
    memcpy(out, s, end); out[end] = '\0';
    return out;
}
/* str.strip() over code points (malloc'd) */
static char *ctm2_strip(const char *s) {
    if (!s) s = "";
    const char *p = s; size_t adv;
    while (*p) {
        unsigned long cp = ctm2_cp(p, &adv);
        if (!ctm2_is_ws_cp(cp)) break;
        p += adv;
    }
    char *rs = ctm2_rstrip(p);
    return rs;
}
/* str.split() (no args): whitespace runs, no empty tokens. malloc'd array
 * of malloc'd strings; *n = token count. */
static char **ctm2_split_ws(const char *s, size_t *n) {
    *n = 0;
    if (!s) s = "";
    size_t cap = 8;
    char **toks = (char **)malloc(cap * sizeof(char *));
    const char *p = s; size_t adv;
    while (*p) {
        while (*p) {
            unsigned long cp = ctm2_cp(p, &adv);
            if (!ctm2_is_ws_cp(cp)) break;
            p += adv;
        }
        if (!*p) break;
        const char *start = p;
        while (*p) {
            unsigned long cp = ctm2_cp(p, &adv);
            if (ctm2_is_ws_cp(cp)) break;
            p += adv;
        }
        size_t len = (size_t)(p - start);
        if (*n == cap) { cap *= 2; toks = (char **)realloc(toks, cap * sizeof(char *)); }
        char *t = (char *)malloc(len + 1);
        memcpy(t, start, len); t[len] = '\0';
        toks[(*n)++] = t;
    }
    return toks;
}
static void ctm2_free_toks(char **toks, size_t n) {
    for (size_t i = 0; i < n; i++) free(toks[i]);
    free(toks);
}

/* ── string builder ────────────────────────────────────────────────────── */
typedef struct { char *p; size_t len, cap; } ctm2_sb;
static void ctm2_sb_init(ctm2_sb *b) {
    b->cap = 64; b->len = 0;
    b->p = (char *)malloc(b->cap); b->p[0] = '\0';
}
static void ctm2_sb_puts(ctm2_sb *b, const char *s) {
    if (!s) return;
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        while (b->len + n + 1 > b->cap) b->cap *= 2;
        b->p = (char *)realloc(b->p, b->cap);
    }
    memcpy(b->p + b->len, s, n); b->len += n; b->p[b->len] = '\0';
}
static char *ctm2_sb_take(ctm2_sb *b) { return b->p; }

/* Python repr() of a str (KeyError str() is repr of its argument) */
static char *ctm2_py_repr(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    ctm2_sb b; ctm2_sb_init(&b);
    char qs[2] = { q, '\0' };
    ctm2_sb_puts(&b, qs);
    for (const unsigned char *u = (const unsigned char *)s; *u; u++) {
        unsigned char c = *u;
        if (c == '\\')                    ctm2_sb_puts(&b, "\\\\");
        else if (c == (unsigned char)q) { char e[3] = { '\\', (char)q, '\0' }; ctm2_sb_puts(&b, e); }
        else if (c == '\n')               ctm2_sb_puts(&b, "\\n");
        else if (c == '\r')               ctm2_sb_puts(&b, "\\r");
        else if (c == '\t')               ctm2_sb_puts(&b, "\\t");
        else if (c < 0x20 || c == 0x7f) { char e[5]; snprintf(e, sizeof e, "\\x%02x", c); ctm2_sb_puts(&b, e); }
        else                            { char ch[2] = { (char)c, '\0' }; ctm2_sb_puts(&b, ch); }
    }
    ctm2_sb_puts(&b, qs);
    return ctm2_sb_take(&b);
}
/* str() of a scripted exception (KeyError quotes its argument) */
static char *ctm2_exc_str(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    if (strcmp(type, "KeyError") == 0) return ctm2_py_repr(msg);
    return strdup(msg);
}

/* Python type name of a JSON value (integral-number contract -> "int") */
static const char *ctm2_py_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
    case JSON_NULL:   return "NoneType";
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return "int";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    default:          return "object";
    }
}
/* Python str() of a JSON value (f-string / str() positions). Lists/dicts
 * render with Python repr semantics (single-quoted strings, True/None). */
static char *ctm2_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
    case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER: {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return strdup(buf);
    }
    case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
    case JSON_ARRAY: {
        ctm2_sb b; ctm2_sb_init(&b);
        ctm2_sb_puts(&b, "[");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) ctm2_sb_puts(&b, ", ");
            char *r = ctm2_py_str(v->c.items[i]);
            /* str() of a nested str inside a container is repr() */
            if (v->c.items[i] && v->c.items[i]->type == JSON_STRING) {
                free(r);
                r = ctm2_py_repr(v->c.items[i]->str_val ? v->c.items[i]->str_val : "");
            }
            ctm2_sb_puts(&b, r); free(r);
        }
        ctm2_sb_puts(&b, "]");
        return ctm2_sb_take(&b);
    }
    case JSON_OBJECT: {
        ctm2_sb b; ctm2_sb_init(&b);
        ctm2_sb_puts(&b, "{");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) ctm2_sb_puts(&b, ", ");
            char *k = ctm2_py_repr(v->c.keys[i]);
            ctm2_sb_puts(&b, k); free(k);
            ctm2_sb_puts(&b, ": ");
            json_t *it = v->c.items[i];
            char *r = (it && it->type == JSON_STRING)
                          ? ctm2_py_repr(it->str_val ? it->str_val : "")
                          : ctm2_py_str(it);
            ctm2_sb_puts(&b, r); free(r);
        }
        ctm2_sb_puts(&b, "}");
        return ctm2_sb_take(&b);
    }
    default: return strdup("");
    }
}

/* len() of a JSON value; non-sized -> TypeError */
static long long ctm2_len(const json_t *v, json_t **err) {
    *err = NULL;
    if (v && (v->type == JSON_ARRAY || v->type == JSON_OBJECT))
        return (long long)v->c.count;
    if (v && v->type == JSON_STRING)
        return (long long)ctm2_utf8_len(v->str_val ? v->str_val : "");
    char buf[128];
    snprintf(buf, sizeof buf, "object of type '%s' has no len()", ctm2_py_type_name(v));
    *err = ctm2_raise_new("TypeError", buf);
    return 0;
}
/* float(v) for the deadline coercion; caller applies the `or 0` first */
static double ctm2_to_float(const json_t *v, json_t **err) {
    *err = NULL;
    if (v && v->type == JSON_NUMBER) return v->num_val;
    if (v && v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v && v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char *end = NULL;
        double d = strtod(s, &end);
        if (end != s) {
            while (*end && ctm2_is_ws_cp((unsigned char)*end)) end++;
            if (*end == '\0') return d;
        }
        char *r = ctm2_py_repr(s);
        char buf[512];
        snprintf(buf, sizeof buf, "could not convert string to float: %s", r);
        free(r);
        *err = ctm2_raise_new("ValueError", buf);
        return 0.0;
    }
    char buf[160];
    snprintf(buf, sizeof buf,
             "float() argument must be a string or a real number, not '%s'",
             ctm2_py_type_name(v));
    *err = ctm2_raise_new("TypeError", buf);
    return 0.0;
}
/* a - b with Python TypeError on non-number operands (bool IS int in Python) */
static double ctm2_num_of(const json_t *v, bool *ok) {
    if (v && v->type == JSON_NUMBER) { *ok = true; return v->num_val; }
    if (v && v->type == JSON_BOOL)   { *ok = true; return v->bool_val ? 1.0 : 0.0; }
    *ok = false; return 0.0;
}
static double ctm2_sub(const json_t *a, const json_t *b, json_t **err) {
    *err = NULL;
    bool oa = false, ob = false;
    double va = ctm2_num_of(a, &oa), vb = ctm2_num_of(b, &ob);
    if (oa && ob) return va - vb;
    char buf[192];
    snprintf(buf, sizeof buf, "unsupported operand type(s) for -: '%s' and '%s'",
             ctm2_py_type_name(a), ctm2_py_type_name(b));
    *err = ctm2_raise_new("TypeError", buf);
    return 0.0;
}
/* "prefix" + t_result with Python TypeError on a non-str right operand */
static char *ctm2_concat_str(const char *prefix, const json_t *r, json_t **err) {
    *err = NULL;
    if (r && r->type == JSON_STRING) {
        ctm2_sb b; ctm2_sb_init(&b);
        ctm2_sb_puts(&b, prefix);
        ctm2_sb_puts(&b, r->str_val ? r->str_val : "");
        return ctm2_sb_take(&b);
    }
    char buf[192];
    snprintf(buf, sizeof buf, "can only concatenate str (not \"%s\") to str",
             ctm2_py_type_name(r));
    *err = ctm2_raise_new("TypeError", buf);
    return NULL;
}

/* ── built-in doubles (buffer / app) ───────────────────────────────────── */
static json_t *ctm2_buf(ctm2_io_t *io) {
    json_t *b = ctm2_st(io, "buf");
    return (b && b->type == JSON_OBJECT) ? b : NULL;
}
static const char *ctm2_buf_text(ctm2_io_t *io) {
    json_t *b = ctm2_buf(io);
    return b ? json_get_str(b, "text", "") : "";
}
static void ctm2_buf_reset(ctm2_io_t *io, bool append_history) {
    json_t *args = json_object();
    json_set(args, "append_to_history", json_bool(append_history));
    ctm2_record(io, "buffer.reset", args, json_null());
    json_t *b = ctm2_buf(io);
    if (b) {
        json_set(b, "text", json_string(""));
        json_set(b, "cursor_position", json_number(0));
    }
}
/* prompt_toolkit Buffer.insert_text: insert at cursor, advance cursor */
static void ctm2_buf_insert_text(ctm2_io_t *io, const char *s) {
    json_t *args = json_object();
    json_set(args, "text", json_string(s));
    ctm2_record(io, "buffer.insert_text", args, json_null());
    json_t *b = ctm2_buf(io);
    if (!b) return;
    const char *t = json_get_str(b, "text", "");
    size_t n = ctm2_utf8_len(t);
    double cur = json_get_num(b, "cursor_position", 0);
    size_t ci = (size_t)(cur < 0 ? 0 : cur);
    if (ci > n) ci = n;
    const char *p = t; size_t adv, off = 0;
    for (size_t i = 0; i < ci && *p; i++) { ctm2_cp(p, &adv); p += adv; off += adv; }
    ctm2_sb bld; ctm2_sb_init(&bld);
    char *head = (char *)malloc(off + 1);
    memcpy(head, t, off); head[off] = '\0';
    ctm2_sb_puts(&bld, head); free(head);
    ctm2_sb_puts(&bld, s);
    ctm2_sb_puts(&bld, t + off);
    char *nt = ctm2_sb_take(&bld);
    json_set(b, "text", json_string(nt));
    json_set(b, "cursor_position", json_number((double)(ci + ctm2_utf8_len(s))));
    free(nt);
}
static void ctm2_app_invalidate(ctm2_io_t *io) {
    ctm2_record(io, "app.invalidate", json_object(), json_null());
}

/* ── i18n helper (seamed) ──────────────────────────────────────────────── */
static json_t *ctm2_t(ctm2_io_t *io, const char *key, json_t *kwargs) {
    json_t *args = kwargs ? kwargs : json_object();
    json_set(args, "key", json_string(key));
    return ctm2_seam(io, "t", args);
}
/* _tui_set_filter (real staticmethod claimed in flow1; pure mutator, no
 * seam — inlined exactly as flow1 does) */
static void ctm2_set_filter(json_t *st, const char *value) {
    if (!st || st->type != JSON_OBJECT) return;
    json_set(st, "filter", json_string(value));
    json_set(st, "selected", json_number(0));
    json_set(st, "_scroll_offset", json_number(0));
}
/* sorted-unique int-list symmetric-difference toggle of one value */
static void ctm2_indices_toggle(json_t *arr, long long val) {
    if (!arr || arr->type != JSON_ARRAY) return;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *it = arr->c.items[i];
        if (it && it->type == JSON_NUMBER && (long long)it->num_val == val) {
            json_free(it);
            for (size_t j = i; j + 1 < arr->c.count; j++) arr->c.items[j] = arr->c.items[j + 1];
            arr->c.count--;
            return;
        }
    }
    size_t pos = arr->c.count;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *it = arr->c.items[i];
        if (it && it->type == JSON_NUMBER && (long long)it->num_val > val) { pos = i; break; }
    }
    json_append(arr, json_null());
    json_free(arr->c.items[arr->c.count - 1]);
    for (size_t j = arr->c.count - 1; j > pos; j--) arr->c.items[j] = arr->c.items[j - 1];
    arr->c.items[pos] = json_number((double)val);
}
/* one (style, text) fragment as a 2-list */
static json_t *ctm2_frag(const char *style, const char *text) {
    json_t *f = json_array();
    json_append(f, json_string(style));
    json_append(f, json_string(text));
    return f;
}

/* ── prompt symbols / fragments ────────────────────────────────────────── */

/* PoP: ctm2_get_tui_prompt_symbols @ hermes_cli/cli_tui_mixin.py:_get_tui_prompt_symbols */
json_t *ctm2_get_tui_prompt_symbols(ctm2_io_t *io) {
    char *symbol = NULL;
    /* try: from hermes_cli.skin_engine import get_active_prompt_symbol
     *      symbol = get_active_prompt_symbol("❯ ")
     * except Exception: symbol = "❯ " */
    json_t *imp = ctm2_import_check(io, "import:hermes_cli.skin_engine");
    if (imp) {
        if (ctm2_base_raised(imp)) return imp;
        json_free(imp);
        symbol = strdup(CTM2_PROMPT_DEFAULT);
    } else {
        json_t *args = json_object();
        json_set(args, "default", json_string(CTM2_PROMPT_DEFAULT));
        json_t *r = ctm2_seam(io, "get_active_prompt_symbol", args);
        if (ctm2_raised(r)) {
            if (ctm2_base_raised(r)) return r;
            json_free(r);
            symbol = strdup(CTM2_PROMPT_DEFAULT);
        } else {
            /* symbol = (symbol or "❯ ").rstrip() + " " — a truthy non-str
             * has no .rstrip() and raises OUTSIDE the try (propagates). */
            if (r && r->type != JSON_NULL && r->type != JSON_STRING && ctm2_truthy(r)) {
                char buf[160];
                snprintf(buf, sizeof buf, "'%s' object has no attribute 'rstrip'",
                         ctm2_py_type_name(r));
                json_free(r);
                return ctm2_raise_new("AttributeError", buf);
            }
            const char *sv = (r && r->type == JSON_STRING && ctm2_truthy(r))
                                 ? r->str_val : CTM2_PROMPT_DEFAULT;
            char *rs = ctm2_rstrip(sv);
            ctm2_sb b; ctm2_sb_init(&b);
            ctm2_sb_puts(&b, rs);
            ctm2_sb_puts(&b, " ");
            symbol = ctm2_sb_take(&b);
            free(rs);
            json_free(r);
        }
    }
    /* try: from hermes_cli.profiles import get_active_profile_name
     *      profile = get_active_profile_name()
     *      if profile not in {"default","custom"}: symbol = f"{profile} {symbol}"
     * except Exception: pass */
    imp = ctm2_import_check(io, "import:hermes_cli.profiles");
    if (imp) {
        if (ctm2_base_raised(imp)) { free(symbol); return imp; }
        json_free(imp);
    } else {
        json_t *p = ctm2_seam(io, "get_active_profile_name", json_object());
        if (ctm2_raised(p)) {
            if (ctm2_base_raised(p)) { free(symbol); return p; }
            json_free(p);
        } else {
            const char *ps = (p && p->type == JSON_STRING) ? p->str_val : NULL;
            bool in_set = ps && (strcmp(ps, "default") == 0 || strcmp(ps, "custom") == 0);
            /* unhashable profile (list/dict) raises TypeError INSIDE the try
             * -> swallowed, no prepend */
            bool unhashable = p && (p->type == JSON_ARRAY || p->type == JSON_OBJECT);
            if (!in_set && !unhashable) {
                char *pstr = ctm2_py_str(p);
                ctm2_sb b; ctm2_sb_init(&b);
                ctm2_sb_puts(&b, pstr);
                ctm2_sb_puts(&b, " ");
                ctm2_sb_puts(&b, symbol);
                free(symbol);
                symbol = ctm2_sb_take(&b);
                free(pstr);
            }
            json_free(p);
        }
    }
    /* stripped = symbol.rstrip(); if not stripped: return ("❯ ", "❯ ") */
    char *stripped = ctm2_rstrip(symbol);
    if (!stripped[0]) {
        free(stripped); free(symbol);
        json_t *ret = json_array();
        json_append(ret, json_string(CTM2_PROMPT_DEFAULT));
        json_append(ret, json_string(CTM2_PROMPT_DEFAULT));
        return ret;
    }
    /* parts = stripped.split(); candidate = parts[-1] if parts else "" */
    size_t np = 0;
    char **parts = ctm2_split_ws(stripped, &np);
    const char *candidate = np ? parts[np - 1] : "";
    static const char *markers[] = {
        "\xe2\x9d\xaf", ">", "$", "#", "\xe2\x80\xba", "\xc2\xbb", "\xe2\x86\x92",
    };
    bool hit = false;
    for (size_t i = 0; i < sizeof(markers) / sizeof(markers[0]); i++) {
        if (strstr(candidate, markers[i])) { hit = true; break; }
    }
    json_t *ret = json_array();
    json_append(ret, json_string(symbol));
    if (hit) {
        char *cr = ctm2_rstrip(candidate);
        ctm2_sb b; ctm2_sb_init(&b);
        ctm2_sb_puts(&b, cr);
        ctm2_sb_puts(&b, " ");
        json_append(ret, json_string(ctm2_sb_take(&b)));
        free(cr);
    } else {
        json_append(ret, json_string(symbol));
    }
    ctm2_free_toks(parts, np);
    free(stripped); free(symbol);
    return ret;
}

/* the nested _state_fragment closure (compact / extra / state_suffix captured).
 * icon is the ORIGINAL value: the compact no-extra path does a real `icon + " "`
 * concatenation (TypeError on a non-str), the f-string paths str() it. */
static json_t *ctm2_state_fragment(bool compact, const char *style, const json_t *icon,
                                   const json_t *extra, const char *state_suffix,
                                   json_t **err) {
    *err = NULL;
    ctm2_sb b; ctm2_sb_init(&b);
    if (compact) {
        if (ctm2_truthy(extra)) {
            if (!extra || extra->type != JSON_STRING) {
                char buf[160];
                snprintf(buf, sizeof buf, "'%s' object has no attribute 'strip'",
                         ctm2_py_type_name(extra));
                *err = ctm2_raise_new("AttributeError", buf);
                free(ctm2_sb_take(&b));
                return NULL;
            }
            char *is = ctm2_py_str(icon);
            char *es = ctm2_strip(extra->str_val ? extra->str_val : "");
            ctm2_sb_puts(&b, is);
            ctm2_sb_puts(&b, " ");
            ctm2_sb_puts(&b, es);
            free(is); free(es);
            /* f"{text} {extra.strip()}".rstrip() */
            char *whole = ctm2_sb_take(&b);
            char *rs = ctm2_rstrip(whole);
            free(whole);
            ctm2_sb_init(&b);
            ctm2_sb_puts(&b, rs);
            free(rs);
            ctm2_sb_puts(&b, " ");
        } else {
            /* text = icon ; return [(style, text + " ")] */
            if (!icon || icon->type != JSON_STRING) {
                char buf[192];
                snprintf(buf, sizeof buf, "unsupported operand type(s) for +: '%s' and 'str'",
                         ctm2_py_type_name(icon));
                *err = ctm2_raise_new("TypeError", buf);
                free(ctm2_sb_take(&b));
                return NULL;
            }
            ctm2_sb_puts(&b, icon->str_val ? icon->str_val : "");
            ctm2_sb_puts(&b, " ");
        }
    } else if (ctm2_truthy(extra)) {
        char *is = ctm2_py_str(icon);
        char *es = ctm2_py_str(extra);
        ctm2_sb_puts(&b, is);
        ctm2_sb_puts(&b, " ");
        ctm2_sb_puts(&b, es);
        ctm2_sb_puts(&b, " ");
        ctm2_sb_puts(&b, state_suffix);
        free(is); free(es);
    } else {
        char *is = ctm2_py_str(icon);
        ctm2_sb_puts(&b, is);
        ctm2_sb_puts(&b, " ");
        ctm2_sb_puts(&b, state_suffix);
        free(is);
    }
    json_t *ret = json_array();
    json_append(ret, ctm2_frag(style, ctm2_sb_take(&b)));
    return ret;
}

/* PoP: ctm2_get_tui_prompt_fragments @ hermes_cli/cli_tui_mixin.py:_get_tui_prompt_fragments */
json_t *ctm2_get_tui_prompt_fragments(ctm2_io_t *io) {
    /* symbol, state_suffix = self._get_tui_prompt_symbols() (composed LIVE) */
    json_t *sym = ctm2_get_tui_prompt_symbols(io);
    if (ctm2_raised(sym)) return sym;
    /* sym is a 2-list [symbol, suffix] */
    json_t *s0 = json_get(sym, 0);
    json_t *s1 = json_get(sym, 1);
    char *symbol = strdup(s0 && s0->type == JSON_STRING && s0->str_val ? s0->str_val : "");
    char *suffix = strdup(s1 && s1->type == JSON_STRING && s1->str_val ? s1->str_val : "");
    json_free(sym);
    /* compact = self._use_minimal_tui_chrome(width=self._get_tui_terminal_width()) */
    json_t *w = ctm2_seam(io, "self._get_tui_terminal_width", json_object());
    if (ctm2_raised(w)) { free(symbol); free(suffix); return w; }
    json_t *cargs = json_object();
    json_set(cargs, "width", w ? w : json_null());   /* w moved */
    json_t *c = ctm2_seam(io, "self._use_minimal_tui_chrome", cargs);
    if (ctm2_raised(c)) { free(symbol); free(suffix); return c; }
    bool compact = ctm2_truthy(c);
    json_free(c);

    json_t *ret = NULL;
    json_t *err = NULL;
    const char *style = NULL;
    json_t *icon_node = NULL;  /* owned: fixed icon string or spinner frame value */
    json_t *extra = NULL;      /* owned when set */

    if (ctm2_truthy(ctm2_st(io, "_voice_recording"))) {
        style = "class:voice-recording"; icon_node = json_string(CTM2_ICON_RECORD);
        extra = ctm2_seam(io, "self._audio_level_bar", json_object());
        if (ctm2_raised(extra)) { ret = extra; extra = NULL; goto done; }
    } else if (ctm2_truthy(ctm2_st(io, "_voice_processing"))) {
        style = "class:voice-processing"; icon_node = json_string(CTM2_ICON_PROCESS);
    } else if (ctm2_truthy(ctm2_st(io, "_sudo_state"))) {
        style = "class:sudo-prompt"; icon_node = json_string(CTM2_ICON_SUDO);
    } else if (ctm2_truthy(ctm2_st(io, "_secret_state"))) {
        style = "class:sudo-prompt"; icon_node = json_string(CTM2_ICON_SECRET);
    } else if (ctm2_truthy(ctm2_st(io, "_connection_state"))) {
        style = "class:prompt-working"; icon_node = json_string(CTM2_ICON_CONN);
    } else if (ctm2_truthy(ctm2_st(io, "_approval_state")) ||
               ctm2_truthy(ctm2_st(io, "_slash_confirm_state"))) {
        style = "class:prompt-working"; icon_node = json_string(CTM2_ICON_WARN);
    } else if (ctm2_truthy(ctm2_st(io, "_clarify_freetext"))) {
        style = "class:clarify-selected"; icon_node = json_string(CTM2_ICON_EDIT);
    } else if (ctm2_truthy(ctm2_st(io, "_clarify_state"))) {
        style = "class:prompt-working"; icon_node = json_string("?");
    } else if (ctm2_truthy(ctm2_st(io, "_command_running"))) {
        style = "class:prompt-working";
        icon_node = ctm2_seam(io, "self._command_spinner_frame", json_object());
        if (ctm2_raised(icon_node)) { ret = icon_node; icon_node = NULL; goto done; }
    } else if (ctm2_truthy(ctm2_st(io, "_agent_running"))) {
        style = "class:prompt-working"; icon_node = json_string(CTM2_ICON_AGENT);
    } else if (ctm2_truthy(ctm2_st(io, "_voice_mode"))) {
        style = "class:voice-prompt"; icon_node = json_string(CTM2_ICON_MIC);
    } else {
        ret = json_array();
        json_append(ret, ctm2_frag("class:prompt", symbol));
        goto done;
    }
    ret = ctm2_state_fragment(compact, style, icon_node, extra, suffix, &err);
    if (err) { ret = err; }
done:
    json_free(extra);
    json_free(icon_node);
    free(symbol); free(suffix);
    return ret;
}

/* ── style dict / skin apply ───────────────────────────────────────────── */

/* the nested _remap_value closure; NULL return with *err set on a seam raise
 * or a non-str .split(); caller decides swallow vs propagate. */
static char *ctm2_remap_value(ctm2_io_t *io, const json_t *v, json_t **err) {
    *err = NULL;
    /* vv = v or "" ; if not vv: return vv */
    if (!ctm2_truthy(v)) return strdup("");
    if (v->type != JSON_STRING) {
        char buf[160];
        snprintf(buf, sizeof buf, "'%s' object has no attribute 'split'",
                 ctm2_py_type_name(v));
        *err = ctm2_raise_new("AttributeError", buf);
        return NULL;
    }
    size_t n = 0;
    char **toks = ctm2_split_ws(v->str_val ? v->str_val : "", &n);
    for (size_t i = 0; i < n; i++) {
        if (strncmp(toks[i], "bg:", 3) == 0) {
            ctm2_free_toks(toks, n);
            return strdup(v->str_val);
        }
    }
    ctm2_sb b; ctm2_sb_init(&b);
    for (size_t i = 0; i < n; i++) {
        if (i) ctm2_sb_puts(&b, " ");
        if (toks[i][0] == '#') {
            json_t *args = json_object();
            json_set(args, "token", json_string(toks[i]));
            json_t *r = ctm2_seam(io, "_maybe_remap_for_light_mode", args);
            if (ctm2_raised(r)) {
                *err = r;
                ctm2_free_toks(toks, n);
                free(ctm2_sb_take(&b));
                return NULL;
            }
            char *rs = ctm2_py_str(r);
            ctm2_sb_puts(&b, rs);
            free(rs);
            json_free(r);
        } else {
            ctm2_sb_puts(&b, toks[i]);
        }
    }
    ctm2_free_toks(toks, n);
    return ctm2_sb_take(&b);
}

/* PoP: ctm2_build_tui_style_dict @ hermes_cli/cli_tui_mixin.py:_build_tui_style_dict */
json_t *ctm2_build_tui_style_dict(ctm2_io_t *io) {
    /* from cli import _detect_light_mode, _maybe_remap_for_light_mode
     * (OUTSIDE the try -> import failure propagates) */
    json_t *imp = ctm2_import_check(io, "import:cli");
    if (imp) return imp;
    /* style_dict = dict(getattr(self, "_tui_style_base", {}) or {}) */
    json_t *sd = json_object();
    json_t *base = ctm2_st(io, "_tui_style_base");
    if (ctm2_truthy(base) && base->type == JSON_OBJECT) {
        for (size_t i = 0; i < base->c.count; i++)
            json_set(sd, base->c.keys[i], json_copy(base->c.items[i]));
    }
    /* try: skin import + style_dict.update(get_prompt_toolkit_style_overrides())
     * except Exception: pass */
    imp = ctm2_import_check(io, "import:hermes_cli.skin_engine");
    if (imp) {
        if (ctm2_base_raised(imp)) { json_free(sd); return imp; }
        json_free(imp);
    } else {
        json_t *ov = ctm2_seam(io, "get_prompt_toolkit_style_overrides", json_object());
        if (ctm2_raised(ov)) {
            if (ctm2_base_raised(ov)) { json_free(sd); return ov; }
            json_free(ov);
        } else {
            if (ov && ov->type == JSON_OBJECT) {
                for (size_t i = 0; i < ov->c.count; i++)
                    json_set(sd, ov->c.keys[i], json_copy(ov->c.items[i]));
            }
            /* non-dict update() raises TypeError -> swallowed */
            json_free(ov);
        }
    }
    /* try: if _detect_light_mode(): style_dict = {k: _remap_value(v or "") ...}
     * except Exception: pass  (comprehension is atomic: partial results drop) */
    json_t *lm = ctm2_seam(io, "_detect_light_mode", json_object());
    if (ctm2_raised(lm)) {
        if (ctm2_base_raised(lm)) { json_free(sd); return lm; }
        json_free(lm);
        return sd;
    }
    bool light = ctm2_truthy(lm);
    json_free(lm);
    if (light) {
        json_t *remapped = json_object();
        json_t *err = NULL;
        for (size_t i = 0; i < sd->c.count; i++) {
            char *rv = ctm2_remap_value(io, sd->c.items[i], &err);
            if (err) {
                if (ctm2_base_raised(err)) {
                    json_free(remapped); json_free(sd);
                    return err;
                }
                json_free(err);
                json_free(remapped);
                return sd;   /* swallowed: keep the pre-remap dict */
            }
            json_set(remapped, sd->c.keys[i], json_string(rv));
            free(rv);
        }
        json_free(sd);
        sd = remapped;
    }
    return sd;
}

/* PoP: ctm2_apply_tui_skin_style @ hermes_cli/cli_tui_mixin.py:_apply_tui_skin_style */
json_t *ctm2_apply_tui_skin_style(ctm2_io_t *io) {
    json_t *app = ctm2_st(io, "_app");
    json_t *base = ctm2_st(io, "_tui_style_base");
    if (!ctm2_truthy(app) || !ctm2_truthy(base)) return json_bool(false);
    /* self._app.style = PTStyle.from_dict(self._build_tui_style_dict()) */
    json_t *sd = ctm2_build_tui_style_dict(io);   /* composed LIVE */
    if (ctm2_raised(sd)) return sd;
    json_t *args = json_object();
    json_set(args, "style_dict", sd);   /* sd moved */
    json_t *style = ctm2_seam(io, "PTStyle.from_dict", args);
    if (ctm2_raised(style)) return style;
    if (app && app->type == JSON_OBJECT)
        json_set(app, "style", style ? style : json_null());
    else
        json_free(style);
    /* self._invalidate(min_interval=0.0) */
    json_t *ia = json_object();
    json_set(ia, "min_interval", json_number(0));
    json_t *r = ctm2_seam(io, "self._invalidate", ia);
    if (ctm2_raised(r)) return r;
    json_free(r);
    return json_bool(true);
}

/* PoP: ctm2_get_extra_tui_widgets @ hermes_cli/cli_tui_mixin.py:_get_extra_tui_widgets */
json_t *ctm2_get_extra_tui_widgets(ctm2_io_t *io) {
    (void)io;
    return json_array();
}

/* ── hint / placeholder / stash-panel display ──────────────────────────── */

/* PoP: ctm2_tui_hint_text @ hermes_cli/cli_tui_mixin.py:_tui_hint_text */
json_t *ctm2_tui_hint_text(ctm2_io_t *io) {
    json_t *conn = ctm2_st(io, "_connection_state");
    if (ctm2_truthy(conn)) {
        const char *phase = json_get_str(conn, "phase", NULL);
        const char *hk = CTM2_CONN_HINT_DEFAULT;
        if (phase) {
            for (size_t i = 0; i < sizeof(CTM2_CONN_HINT_KEYS) / sizeof(CTM2_CONN_HINT_KEYS[0]); i++) {
                if (strcmp(CTM2_CONN_HINT_KEYS[i][0], phase) == 0) { hk = CTM2_CONN_HINT_KEYS[i][1]; break; }
            }
        }
        json_t *ht = ctm2_t(io, hk, NULL);
        if (ctm2_raised(ht)) return ht;
        json_t *err = NULL;
        char *hint = ctm2_concat_str("  ", ht, &err);
        json_free(ht);
        if (err) return err;
        /* deadline = float(state.get("payload", {}).get("deadline_at") or 0)
         * — the {} default applies only when the key is ABSENT; a present
         * non-dict (incl. null) raises AttributeError on .get (no try here). */
        json_t *payload = json_obj_get(conn, "payload");
        json_t *da = NULL;
        if (!payload) {
            da = NULL;   /* default {} -> no deadline_at */
        } else if (payload->type == JSON_OBJECT) {
            da = json_obj_get(payload, "deadline_at");
        } else {
            char buf[160];
            snprintf(buf, sizeof buf, "'%s' object has no attribute 'get'",
                     ctm2_py_type_name(payload));
            free(hint);
            return ctm2_raise_new("AttributeError", buf);
        }
        double deadline = 0.0;
        if (ctm2_truthy(da)) {
            deadline = ctm2_to_float(da, &err);
            if (err) { free(hint); return err; }
        }
        char countdown[64];
        countdown[0] = '\0';
        if (deadline != 0.0) {
            json_t *now = ctm2_seam(io, "time.time", json_object());
            if (ctm2_raised(now)) { free(hint); return now; }
            json_t *dj = json_number(deadline);
            double diff = ctm2_sub(dj, now, &err);
            json_free(dj);
            if (err) { free(hint); json_free(now); return err; }
            json_free(now);
            long long secs = (long long)diff;   /* int() truncates toward 0 */
            if (secs < 0) secs = 0;
            snprintf(countdown, sizeof countdown, "  (%llds)", secs);
        }
        json_t *ret = json_array();
        json_append(ret, ctm2_frag("class:hint", hint));
        json_append(ret, ctm2_frag("class:clarify-countdown", countdown));
        free(hint);
        return ret;
    }
    /* for state_attr, deadline_attr, hint_key in self._TUI_MODAL_HINTS */
    for (size_t m = 0; m < sizeof(CTM2_MODAL_HINTS) / sizeof(CTM2_MODAL_HINTS[0]); m++) {
        const char *state_attr = CTM2_MODAL_HINTS[m][0];
        const char *deadline_attr = CTM2_MODAL_HINTS[m][1];
        const char *hk = CTM2_MODAL_HINTS[m][2];
        json_t *ms = ctm2_st(io, state_attr);
        if (!ctm2_truthy(ms)) continue;
        if (strcmp(state_attr, "_sudo_state") == 0) {
            /* (vault_save or {}).get("step") == "identifier" or vault_code */
            json_t *vs = json_obj_get(ms, "vault_save");
            json_t *step = NULL;
            if (ctm2_truthy(vs)) {
                if (vs->type != JSON_OBJECT) {
                    char buf[160];
                    snprintf(buf, sizeof buf, "'%s' object has no attribute 'get'",
                             ctm2_py_type_name(vs));
                    return ctm2_raise_new("AttributeError", buf);
                }
                step = json_obj_get(vs, "step");
            }
            bool ident = step && step->type == JSON_STRING &&
                         strcmp(step->str_val ? step->str_val : "", "identifier") == 0;
            if (ident || ctm2_truthy(json_obj_get(ms, "vault_code")))
                hk = "cli.tui.hint_vault_username";
        }
        json_t *ht = ctm2_t(io, hk, NULL);
        if (ctm2_raised(ht)) return ht;
        json_t *err = NULL;
        char *hint = ctm2_concat_str("  ", ht, &err);
        json_free(ht);
        if (err) return err;
        /* remaining = max(0, int(getattr(self, deadline_attr) - time.monotonic())) */
        json_t *dl = ctm2_st(io, deadline_attr);
        json_t *mono = ctm2_seam(io, "time.monotonic", json_object());
        if (ctm2_raised(mono)) { free(hint); return mono; }
        double diff = ctm2_sub(dl, mono, &err);
        if (err) { free(hint); json_free(mono); return err; }
        json_free(mono);
        long long secs = (long long)diff;
        if (secs < 0) secs = 0;
        char countdown[64];
        snprintf(countdown, sizeof countdown, "  (%llds)", secs);
        json_t *ret = json_array();
        json_append(ret, ctm2_frag("class:hint", hint));
        json_append(ret, ctm2_frag("class:clarify-countdown", countdown));
        free(hint);
        return ret;
    }
    json_t *cs = ctm2_st(io, "_clarify_state");
    if (ctm2_truthy(cs)) {
        /* None deadline = unlimited wait -> hide the countdown entirely */
        json_t *dl = ctm2_st(io, "_clarify_deadline");
        char countdown[64];
        countdown[0] = '\0';
        if (dl && dl->type != JSON_NULL) {
            json_t *mono = ctm2_seam(io, "time.monotonic", json_object());
            if (ctm2_raised(mono)) return mono;
            json_t *err = NULL;
            double diff = ctm2_sub(dl, mono, &err);
            if (err) { json_free(mono); return err; }
            json_free(mono);
            long long secs = (long long)diff;
            if (secs < 0) secs = 0;
            snprintf(countdown, sizeof countdown, "  (%llds)", secs);
        }
        const char *hk = ctm2_truthy(ctm2_st(io, "_clarify_freetext"))
                             ? "cli.tui.hint_clarify_freetext"
                             : "cli.tui.hint_clarify_batch";
        json_t *ht = ctm2_t(io, hk, NULL);
        if (ctm2_raised(ht)) return ht;
        json_t *err = NULL;
        char *hint = ctm2_concat_str("  ", ht, &err);
        json_free(ht);
        if (err) return err;
        json_t *ret = json_array();
        json_append(ret, ctm2_frag("class:hint", hint));
        json_append(ret, ctm2_frag("class:clarify-countdown", countdown));
        free(hint);
        return ret;
    }
    if (ctm2_truthy(ctm2_st(io, "_command_running"))) {
        json_t *frame = ctm2_seam(io, "self._command_spinner_frame", json_object());
        if (ctm2_raised(frame)) return frame;
        const char *hk = ctm2_truthy(ctm2_st(io, "_command_blocks_input"))
                             ? "cli.tui.hint_input_disabled"
                             : "cli.tui.hint_input_queues";
        json_t *detail = ctm2_t(io, hk, NULL);
        if (ctm2_raised(detail)) { json_free(frame); return detail; }
        json_t *kw = json_object();
        json_set(kw, "frame", frame);    /* frame moved */
        json_set(kw, "detail", detail);  /* detail moved */
        json_t *ht = ctm2_t(io, "cli.tui.hint_command_running", kw);
        if (ctm2_raised(ht)) return ht;
        json_t *err = NULL;
        char *hint = ctm2_concat_str("  ", ht, &err);
        json_free(ht);
        if (err) return err;
        json_t *ret = json_array();
        json_append(ret, ctm2_frag("class:hint", hint));
        free(hint);
        return ret;
    }
    return json_array();
}

/* PoP: ctm2_tui_placeholder_text @ hermes_cli/cli_tui_mixin.py:_tui_placeholder_text */
json_t *ctm2_tui_placeholder_text(ctm2_io_t *io) {
    if (ctm2_truthy(ctm2_st(io, "_voice_recording"))) {
        json_t *lt = ctm2_st(io, "_voice_live_text");
        if (ctm2_truthy(lt)) return json_copy(lt);
        json_t *label = ctm2_seam(io, "self._voice_record_key_label", json_object());
        if (ctm2_raised(label)) return label;
        json_t *kw = json_object();
        json_set(kw, "shortcut", label);   /* label moved */
        return ctm2_t(io, "cli.tui.placeholder_recording", kw);
    }
    if (ctm2_truthy(ctm2_st(io, "_voice_processing"))) {
        json_t *lt = ctm2_st(io, "_voice_live_text");
        if (ctm2_truthy(lt)) return json_copy(lt);
        return ctm2_t(io, "cli.tui.placeholder_transcribing", NULL);
    }
    json_t *sudo = ctm2_st(io, "_sudo_state");
    if (ctm2_truthy(sudo)) {
        json_t *vs = json_obj_get(sudo, "vault_save");
        json_t *step = NULL;
        if (ctm2_truthy(vs)) {
            if (vs->type != JSON_OBJECT) {
                char buf[160];
                snprintf(buf, sizeof buf, "'%s' object has no attribute 'get'",
                         ctm2_py_type_name(vs));
                return ctm2_raise_new("AttributeError", buf);
            }
            step = json_obj_get(vs, "step");
        }
        if (step && step->type == JSON_STRING &&
            strcmp(step->str_val ? step->str_val : "", "identifier") == 0)
            return ctm2_t(io, "cli.tui.placeholder_vault_username", NULL);
        if (ctm2_truthy(json_obj_get(sudo, "vault_code")))
            return ctm2_t(io, "cli.tui.placeholder_vault_code", NULL);
        return ctm2_t(io, "cli.tui.placeholder_password", NULL);
    }
    if (ctm2_truthy(ctm2_st(io, "_secret_state")))
        return ctm2_t(io, "cli.tui.placeholder_secret", NULL);
    if (ctm2_truthy(ctm2_st(io, "_approval_state")))
        return json_string("");
    if (ctm2_truthy(ctm2_st(io, "_slash_confirm_state")))
        return ctm2_t(io, "cli.tui.placeholder_choice", NULL);
    if (ctm2_truthy(ctm2_st(io, "_clarify_freetext")))
        return ctm2_t(io, "cli.tui.placeholder_answer", NULL);
    if (ctm2_truthy(ctm2_st(io, "_clarify_state")))
        return json_string("");
    if (ctm2_truthy(ctm2_st(io, "_command_running"))) {
        /* f"{frame} {self._command_status or t('cli.tui.placeholder_processing_command')}" */
        json_t *frame = ctm2_seam(io, "self._command_spinner_frame", json_object());
        if (ctm2_raised(frame)) return frame;
        json_t *status = ctm2_st(io, "_command_status");
        json_t *rhs = NULL;
        if (ctm2_truthy(status)) {
            rhs = json_copy(status);
        } else {
            rhs = ctm2_t(io, "cli.tui.placeholder_processing_command", NULL);
            if (ctm2_raised(rhs)) { json_free(frame); return rhs; }
        }
        char *fs = ctm2_py_str(frame);
        char *rs = ctm2_py_str(rhs);
        json_free(frame); json_free(rhs);
        ctm2_sb b; ctm2_sb_init(&b);
        ctm2_sb_puts(&b, fs);
        ctm2_sb_puts(&b, " ");
        ctm2_sb_puts(&b, rs);
        free(fs); free(rs);
        return json_string(ctm2_sb_take(&b));
    }
    if (ctm2_truthy(ctm2_st(io, "_agent_running"))) {
        if (ctm2_truthy(ctm2_st(io, "_modern_layout"))) {
            const char *mode = json_get_str(io->state, "busy_input_mode", "");
            char key[160];
            snprintf(key, sizeof key, "cli.modern.placeholder_busy_%s", mode);
            return ctm2_t(io, key, NULL);
        }
        return ctm2_t(io, "cli.tui.placeholder_busy", NULL);
    }
    if (ctm2_truthy(ctm2_st(io, "_voice_mode"))) {
        json_t *label = ctm2_seam(io, "self._voice_record_key_label", json_object());
        if (ctm2_raised(label)) return label;
        json_t *kw = json_object();
        json_set(kw, "shortcut", label);   /* label moved */
        return ctm2_t(io, "cli.tui.placeholder_voice_idle", kw);
    }
    /* try: _stash_hint = self._prompt_stash.placeholder_hint()
     * except Exception: _stash_hint = "" */
    json_t *ps = ctm2_st(io, "_prompt_stash");
    json_t *hint = NULL;
    if (!ps || ps->type == JSON_NULL) {
        /* None.placeholder_hint -> AttributeError, swallowed */
        hint = NULL;
    } else {
        hint = ctm2_seam(io, "prompt_stash.placeholder_hint", json_object());
        if (ctm2_raised(hint)) {
            if (ctm2_base_raised(hint)) return hint;
            json_free(hint);
            hint = NULL;
        }
    }
    if (ctm2_truthy(hint)) return hint;   /* ownership moves */
    json_free(hint);
    /* return getattr(self, "_composer_placeholder", "") or "" */
    json_t *cp = ctm2_st(io, "_composer_placeholder");
    if (ctm2_truthy(cp)) return json_copy(cp);
    return json_string("");
}

/* PoP: ctm2_get_stash_panel_display_fragments @ hermes_cli/cli_tui_mixin.py:_get_stash_panel_display_fragments */
json_t *ctm2_get_stash_panel_display_fragments(ctm2_io_t *io) {
    /* try: return self._render_stash_panel(_stash.panel_rows(),
     *      _stash.panel_cursor, self._get_tui_terminal_width())
     * except Exception: return [] */
    json_t *stash = ctm2_st(io, "_prompt_stash");
    if (!stash || stash->type == JSON_NULL)
        return json_array();   /* AttributeError swallowed */
    json_t *rows = ctm2_seam(io, "prompt_stash.panel_rows", json_object());
    if (ctm2_raised(rows)) {
        if (ctm2_base_raised(rows)) return rows;
        json_free(rows);
        return json_array();
    }
    json_t *cur = json_obj_get(stash, "panel_cursor");
    if (!cur) {
        /* missing attribute -> AttributeError swallowed */
        json_free(rows);
        return json_array();
    }
    json_t *w = ctm2_seam(io, "self._get_tui_terminal_width", json_object());
    if (ctm2_raised(w)) {
        json_free(rows);
        if (ctm2_base_raised(w)) return w;
        json_free(w);
        return json_array();
    }
    json_t *args = json_object();
    json_set(args, "rows", rows);            /* rows moved */
    json_set(args, "cursor", json_copy(cur));
    json_set(args, "width", w);              /* w moved */
    json_t *r = ctm2_seam(io, "self._render_stash_panel", args);
    if (ctm2_raised(r)) {
        if (ctm2_base_raised(r)) return r;
        json_free(r);
        return json_array();
    }
    return r;
}

/* ── small fragment delegators ─────────────────────────────────────────── */

/* PoP: ctm2_tui_image_bar_fragments @ hermes_cli/cli_tui_mixin.py:_tui_image_bar_fragments */
json_t *ctm2_tui_image_bar_fragments(ctm2_io_t *io) {
    /* from cli import _format_image_attachment_badges (outside try) */
    json_t *imp = ctm2_import_check(io, "import:cli");
    if (imp) return imp;
    json_t *imgs = ctm2_st(io, "_attached_images");
    if (!ctm2_truthy(imgs)) return json_array();
    json_t *counter = ctm2_st(io, "_image_counter");
    json_t *args = json_object();
    json_set(args, "images", json_copy(imgs));
    json_set(args, "image_counter", counter ? json_copy(counter) : json_null());
    json_t *badges = ctm2_seam(io, "_format_image_attachment_badges", args);
    if (ctm2_raised(badges)) return badges;
    char *bs = ctm2_py_str(badges);
    json_free(badges);
    ctm2_sb b; ctm2_sb_init(&b);
    ctm2_sb_puts(&b, " ");
    ctm2_sb_puts(&b, bs);
    ctm2_sb_puts(&b, " ");
    free(bs);
    json_t *ret = json_array();
    json_append(ret, ctm2_frag("class:image-badge", ctm2_sb_take(&b)));
    return ret;
}

/* PoP: ctm2_tui_voice_status_fragments @ hermes_cli/cli_tui_mixin.py:_tui_voice_status_fragments */
json_t *ctm2_tui_voice_status_fragments(ctm2_io_t *io) {
    return ctm2_seam(io, "self._get_voice_status_fragments", json_object());
}

/* PoP: ctm2_tui_spinner_text @ hermes_cli/cli_tui_mixin.py:_tui_spinner_text */
json_t *ctm2_tui_spinner_text(ctm2_io_t *io) {
    json_t *line = ctm2_seam(io, "self._render_spinner_text", json_object());
    if (ctm2_raised(line)) return line;
    if (!ctm2_truthy(line)) {
        json_free(line);
        return json_array();
    }
    json_t *ret = json_array();
    json_t *f = json_array();
    json_append(f, json_string("class:hint"));
    json_append(f, line);   /* line moved (original value, not str()) */
    json_append(ret, f);
    return ret;
}

/* PoP: ctm2_tui_spinner_height @ hermes_cli/cli_tui_mixin.py:_tui_spinner_height */
json_t *ctm2_tui_spinner_height(ctm2_io_t *io) {
    return ctm2_seam(io, "self._spinner_widget_height", json_object());
}

/* PoP: ctm2_tui_hint_height @ hermes_cli/cli_tui_mixin.py:_tui_hint_height */
json_t *ctm2_tui_hint_height(ctm2_io_t *io) {
    static const char *states[] = {
        "_sudo_state", "_secret_state", "_approval_state", "_connection_state",
        "_slash_confirm_state", "_clarify_state", "_command_running",
    };
    for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
        if (ctm2_truthy(ctm2_st(io, states[i]))) return json_number(1);
    }
    return ctm2_seam(io, "self._agent_spacer_height", json_object());
}

/* ── Tab completion gesture ────────────────────────────────────────────── */

/* PoP: ctm2_handle_tab @ hermes_cli/cli_tui_mixin.py:_tui_handle_tab */
json_t *ctm2_handle_tab(ctm2_io_t *io) {
    json_t *b = ctm2_buf(io);
    json_t *cs = b ? json_obj_get(b, "complete_state") : NULL;
    if (ctm2_truthy(cs)) {
        json_t *comp = json_obj_get(cs, "current_completion");
        if (!comp || comp->type == JSON_NULL) {
            /* menu open but nothing selected — select first then grab it */
            json_t *args = json_object();
            json_set(args, "index", json_number(0));
            json_t *r = ctm2_seam(io, "buffer.go_to_completion", args);
            if (ctm2_raised(r)) return r;
            json_free(r);
            cs = b ? json_obj_get(b, "complete_state") : NULL;
            comp = ctm2_truthy(cs) ? json_obj_get(cs, "current_completion") : NULL;
        }
        if (!comp || comp->type == JSON_NULL) return json_null();
        json_t *args = json_object();
        json_set(args, "completion", json_copy(comp));
        json_t *r = ctm2_seam(io, "buffer.apply_completion", args);
        if (ctm2_raised(r)) return r;
        json_free(r);
        return json_null();
    }
    json_t *sug = b ? json_obj_get(b, "suggestion") : NULL;
    const char *stext = (sug && sug->type == JSON_OBJECT) ? json_get_str(sug, "text", "") : "";
    if (ctm2_truthy(sug) && stext && stext[0]) {
        ctm2_buf_insert_text(io, stext);
        return json_null();
    }
    json_t *r = ctm2_seam(io, "buffer.start_completion", json_object());
    if (ctm2_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: ctm2_open_command_palette @ hermes_cli/cli_tui_mixin.py:_tui_open_command_palette */
json_t *ctm2_open_command_palette(ctm2_io_t *io) {
    json_t *r = ctm2_seam(io, "self._open_command_palette", json_object());
    if (ctm2_raised(r)) return r;
    json_free(r);
    ctm2_app_invalidate(io);
    return json_null();
}

/* PoP: ctm2_handle_stash_panel_restore @ hermes_cli/cli_tui_mixin.py:_tui_handle_stash_panel_restore */
json_t *ctm2_handle_stash_panel_restore(ctm2_io_t *io) {
    json_t *ps = ctm2_st(io, "_prompt_stash");
    if (!ps || ps->type == JSON_NULL)
        return ctm2_raise_new("AttributeError",
                              "'NoneType' object has no attribute 'restore_at_cursor'");
    /* argument seam fires BEFORE the claimed-sibling call seam */
    json_t *payload = ctm2_seam(io, "prompt_stash.restore_at_cursor", json_object());
    if (ctm2_raised(payload)) return payload;
    json_t *args = json_object();
    json_set(args, "payload", payload ? payload : json_null());   /* payload moved */
    json_t *r = ctm2_seam(io, "self._tui_restore_stash_payload", args);
    if (ctm2_raised(r)) return r;
    json_free(r);
    ctm2_app_invalidate(io);
    return json_null();
}

/* ── key-handler factories (factory + handler invocation fused) ────────── */

/* PoP: ctm2_make_clarify_number_handler @ hermes_cli/cli_tui_mixin.py:_tui_make_clarify_number_handler */
json_t *ctm2_make_clarify_number_handler(ctm2_io_t *io, long long idx) {
    json_t *state = ctm2_st(io, "_clarify_state");
    if (!ctm2_truthy(state) || ctm2_truthy(ctm2_st(io, "_clarify_freetext")))
        return json_null();
    json_t *choices = json_obj_get(state, "choices");
    long long n = (choices && choices->type == JSON_ARRAY) ? (long long)choices->c.count : 0;
    if (idx > n) return json_null();
    if (ctm2_truthy(json_obj_get(state, "multi_select"))) {
        json_t *indices = json_obj_get(state, "selected_indices");
        /* absent -> live mutates a fresh default set (no write-back) */
        if (indices && indices->type == JSON_ARRAY) ctm2_indices_toggle(indices, idx);
        ctm2_app_invalidate(io);
        return json_null();
    }
    if (idx == n) {
        /* "Other" -> freetext */
        ctm2_st_set(io, "_clarify_freetext", json_bool(true));
    } else {
        /* lock the numbered choice for the active question only */
        json_t *choice = choices->c.items[idx];
        json_t *args = json_object();
        json_set(args, "state", json_copy(state));
        json_set(args, "choice", json_copy(choice));
        json_t *r = ctm2_seam(io, "self._clarify_batch_lock", args);
        if (ctm2_raised(r)) return r;
        json_free(r);
    }
    ctm2_app_invalidate(io);
    return json_null();
}

/* PoP: ctm2_make_model_filter_char_handler @ hermes_cli/cli_tui_mixin.py:_tui_make_model_filter_char_handler */
json_t *ctm2_make_model_filter_char_handler(ctm2_io_t *io, const char *ch) {
    json_t *st = ctm2_st(io, "_model_picker_state");
    if (!ctm2_truthy(st)) return json_null();
    const char *stage = json_get_str(st, "stage", NULL);
    if (!stage || strcmp(stage, "model") != 0) return json_null();
    const char *filter = json_get_str(st, "filter", "");
    ctm2_sb b; ctm2_sb_init(&b);
    ctm2_sb_puts(&b, filter);
    ctm2_sb_puts(&b, ch);
    ctm2_set_filter(st, ctm2_sb_take(&b));
    ctm2_app_invalidate(io);
    return json_null();
}

/* PoP: ctm2_make_palette_char_handler @ hermes_cli/cli_tui_mixin.py:_tui_make_palette_char_handler */
json_t *ctm2_make_palette_char_handler(ctm2_io_t *io, const char *ch) {
    json_t *st = ctm2_st(io, "_command_palette_state");
    if (!ctm2_truthy(st)) return json_null();
    const char *filter = json_get_str(st, "filter", "");
    ctm2_sb b; ctm2_sb_init(&b);
    ctm2_sb_puts(&b, filter);
    ctm2_sb_puts(&b, ch);
    ctm2_set_filter(st, ctm2_sb_take(&b));
    ctm2_app_invalidate(io);
    return json_null();
}

/* PoP: ctm2_make_approval_number_handler @ hermes_cli/cli_tui_mixin.py:_tui_make_approval_number_handler */
json_t *ctm2_make_approval_number_handler(ctm2_io_t *io, long long idx) {
    json_t *st = ctm2_st(io, "_approval_state");
    if (!ctm2_truthy(st)) return json_null();
    json_t *choices = NULL;
    json_t *err = ctm2_subscript(st, "choices", &choices);
    if (err) return err;
    long long n = ctm2_len(choices, &err);
    if (err) return err;
    if (idx < n) {
        json_set(st, "selected", json_number((double)idx));
        json_t *r = ctm2_seam(io, "self._handle_approval_selection", json_object());
        if (ctm2_raised(r)) return r;
        json_free(r);
        ctm2_app_invalidate(io);
    }
    return json_null();
}

/* PoP: ctm2_make_slash_confirm_number_handler @ hermes_cli/cli_tui_mixin.py:_tui_make_slash_confirm_number_handler */
json_t *ctm2_make_slash_confirm_number_handler(ctm2_io_t *io, long long idx) {
    json_t *st = ctm2_st(io, "_slash_confirm_state");
    if (!ctm2_truthy(st)) return json_null();
    json_t *choices = json_obj_get(st, "choices");
    long long n = (choices && choices->type == JSON_ARRAY) ? (long long)choices->c.count : 0;
    if (idx < n) {
        /* self._slash_confirm_state["choices"][idx][0] */
        json_t *entry = choices->c.items[idx];
        json_t *val = NULL;
        if (entry && entry->type == JSON_ARRAY) {
            if (entry->c.count == 0)
                return ctm2_raise_new("IndexError", "list index out of range");
            val = json_copy(entry->c.items[0]);
        } else if (entry && entry->type == JSON_STRING) {
            const char *s = entry->str_val ? entry->str_val : "";
            if (!s[0])
                return ctm2_raise_new("IndexError", "string index out of range");
            val = json_string(ctm2_utf8_prefix(s, 1));
        } else {
            char buf[160];
            snprintf(buf, sizeof buf, "'%s' object is not subscriptable",
                     ctm2_py_type_name(entry));
            return ctm2_raise_new("TypeError", buf);
        }
        json_t *args = json_object();
        json_set(args, "value", val);   /* val moved */
        json_t *r = ctm2_seam(io, "self._submit_slash_confirm_response", args);
        if (ctm2_raised(r)) return r;
        json_free(r);
        ctm2_buf_reset(io, false);
        ctm2_app_invalidate(io);
    }
    return json_null();
}

/* ── Ctrl+Z suspend ────────────────────────────────────────────────────── */

/* PoP: ctm2_handle_ctrl_z @ hermes_cli/cli_tui_mixin.py:_tui_handle_ctrl_z */
json_t *ctm2_handle_ctrl_z(ctm2_io_t *io, const char *platform) {
    /* from cli import _DIM, _RST, _cprint (outside try) */
    json_t *imp = ctm2_import_check(io, "import:cli");
    if (imp) return imp;
    if (strcmp(platform, "win32") == 0) {
        json_t *tr = ctm2_t(io, "cli.tui.suspend_unsupported", NULL);
        if (ctm2_raised(tr)) return tr;
        char *ts = ctm2_py_str(tr);
        json_free(tr);
        ctm2_sb b; ctm2_sb_init(&b);
        ctm2_sb_puts(&b, "\n");
        ctm2_sb_puts(&b, CTM2_DIM);
        ctm2_sb_puts(&b, ts);
        ctm2_sb_puts(&b, CTM2_RST);
        free(ts);
        json_t *args = json_object();
        json_set(args, "text", json_string(ctm2_sb_take(&b)));
        json_t *r = ctm2_seam(io, "_cprint", args);
        if (ctm2_raised(r)) return r;
        json_free(r);
        ctm2_app_invalidate(io);
        return json_null();
    }
    /* import signal as _sig ; from prompt_toolkit.application import
     * run_in_terminal ; from hermes_cli.skin_engine import get_active_skin */
    imp = ctm2_import_check(io, "import:signal");
    if (imp) return imp;
    imp = ctm2_import_check(io, "import:prompt_toolkit.application");
    if (imp) return imp;
    imp = ctm2_import_check(io, "import:hermes_cli.skin_engine");
    if (imp) return imp;
    json_t *skin = ctm2_seam(io, "get_active_skin", json_object());
    if (ctm2_raised(skin)) return skin;
    if (!skin || skin->type != JSON_OBJECT) {
        char buf[160];
        snprintf(buf, sizeof buf, "'%s' object has no attribute 'get_branding'",
                 ctm2_py_type_name(skin));
        json_free(skin);
        return ctm2_raise_new("AttributeError", buf);
    }
    json_free(skin);
    json_t *bargs = json_object();
    json_set(bargs, "key", json_string("agent_name"));
    json_set(bargs, "default", json_string("Hermes Agent"));
    json_t *brand = ctm2_seam(io, "get_branding", bargs);
    if (ctm2_raised(brand)) return brand;
    /* msg = "\n" + t("cli.tui.suspended", agent_name=agent_name) — str + non-str
     * raises TypeError (no try on this path). */
    json_t *kw = json_object();
    json_set(kw, "agent_name", brand);   /* brand moved */
    json_t *tr = ctm2_t(io, "cli.tui.suspended", kw);
    if (ctm2_raised(tr)) return tr;
    json_t *err = NULL;
    char *msg = ctm2_concat_str("\n", tr, &err);
    json_free(tr);
    if (err) return err;
    free(msg);   /* msg feeds the _suspend closure; not traced */
    /* run_in_terminal(_suspend): pure scripted seam — the closure body
     * (os.write/os.kill) is process-level, out of the scripted scope. */
    json_t *args = json_object();
    json_set(args, "closure", json_string("_suspend"));
    json_t *r = ctm2_seam(io, "run_in_terminal", args);
    if (ctm2_raised(r)) return r;
    json_free(r);
    return json_null();
}