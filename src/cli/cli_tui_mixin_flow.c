/* cli_tui_mixin_flow.c — FLOW + leaf port of hermes_cli/cli_tui_mixin.py
 * (navigation / editor / stash cluster). Companion to port_tui_dispatch_flow.c
 * (the Enter/Ctrl dispatch chain); the functions claimed there are SEAMED here
 * (self._tui_connection_move). This port closes the unclaimed navigation,
 * picker/palette/clarify/approval/slash-confirm selection handlers, the editor
 * gestures (double-ESC, clipboard paste, newline, history recall), the
 * wake-word startup guard, the ignored-terminal-sequence consumer, the stash
 * browse-panel handlers and payload restore, plus the module leaves
 * (_approval_choice_label, _agent_name, _tn, _num_prefix, _term_rows) and the
 * _tui_set_filter staticmethod / _audio_level_bar indicator.
 *
 * All terminal / agent / skin / i18n / stash I/O is INJECTED through the
 * ctm_io_t universal seam dispatcher: every live call goes through
 * ctm_seam(io, name, args); the harness dispatcher appends nothing itself —
 * ctm_seam records {ev, args, ret} into io->trace, returns the scripted result
 * (caller-owned), and models a raised exception as {"__raise__": {type,
 * message, ...}}. An "import_raise" script entry on an "import:<module>" key
 * models a failed in-function import: the dispatcher returns
 * {"__raise__": spec, "__untrace__": true} and ctm_import_check raises WITHOUT
 * a trace token (the live import statement records nothing).
 *
 * Built-in doubles (NOT scripted, identical behaviour in the oracle):
 *   buffer.reset / buffer.insert_text — prompt_toolkit Buffer semantics
 *   app.invalidate                    — prompt_toolkit Application no-op
 *   buffer.auto_up / buffer.auto_down are SCRIPTED seams (history recall effect
 *   is history-dependent; the script carries a "buf" state effect merged in
 *   place so the composer mirror stays attached).
 *
 * io->state is a json OBJECT mirroring the CLI instance attributes:
 *   buf {text, cursor_position}, _attached_images (list), _connection_state,
 *   _model_picker_state, _clarify_state, _clarify_freetext, _command_palette_state,
 *   _slash_confirm_state, _approval_state, _skip_paste_collapse,
 *   _voice_recorder {current_rms}, _prompt_stash (presence flag object).
 *
 * Ownership: seam args are OWNED by ctm_seam (moved into the trace event or
 * freed); seam results are caller-owned (json_free). io->trace / io->state are
 * harness-owned. Every ctm_* flow returns a NEW reference (json null / bool /
 * string / number) or a propagated {"__raise__": spec} sentinel.
 *
 * BaseException kinds (SystemExit / KeyboardInterrupt / GeneratorExit /
 * CancelledError) PASS THROUGH every except-Exception site; plain exceptions
 * are swallowed there exactly as live. Fail-closed: with no dispatcher
 * installed ctm_seam returns a RuntimeError raise sentinel (never a silent
 * success); the oracle exits 1 on zero cases.
 *
 * Fixture constraints (documented representation limits): scripted numbers are
 * integral (libjson has no int/float identity); _clarify_state.selected_indices
 * is a SORTED unique int list on both sides (live set normalized by sorted());
 * _audio_level_bar current_rms >= 0; restore_stash_payload payload is a 2-list
 * [text, images] or null; image ids are strings.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

/* ── seam dispatcher + trace + state ───────────────────────────────────── */
typedef struct ctm_io_t {
    json_t *(*call)(struct ctm_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* ARRAY of {ev, args, ret} events (harness-owned) */
    json_t *state;   /* OBJECT mirror (harness-owned) */
    void   *ctx;
} ctm_io_t;

/* module constant: approval-choice id -> catalog key (byte-pinned) */
static const char *CTM_APPROVAL_KEYS[][2] = {
    {"once", "cli.tui.approval_once"},
    {"session", "cli.tui.approval_session"},
    {"always", "cli.tui.approval_always"},
    {"deny", "cli.tui.approval_deny"},
    {"view", "cli.tui.approval_view"},
};
/* _audio_level_bar: " ▁▂▃▄▅▆▇" (space + U+2581..U+2587), UTF-8 bytes */
static const char *CTM_AUDIO_BAR[] = {
    " ", "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84",
    "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87",
};

static bool ctm_truthy(const json_t *v) {
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

static bool ctm_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static json_t *ctm_raise_spec(const json_t *r) {
    return ctm_raised(r) ? json_obj_get(r, "__raise__") : NULL;
}
static bool ctm_is_base_exc(const char *type) {
    return type && (strcmp(type, "SystemExit") == 0 ||
                    strcmp(type, "KeyboardInterrupt") == 0 ||
                    strcmp(type, "GeneratorExit") == 0 ||
                    strcmp(type, "CancelledError") == 0);
}
/* true when a raised seam result must propagate through an except-Exception
 * site (BaseException kinds); false when live would swallow it. */
static bool ctm_base_raised(const json_t *r) {
    if (!ctm_raised(r)) return false;
    const json_t *spec = ctm_raise_spec(r);
    return ctm_is_base_exc(json_get_str(spec, "type", ""));
}

static void ctm_record(ctm_io_t *io, const char *name, json_t *args, json_t *ret) {
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
static json_t *ctm_seam(ctm_io_t *io, const char *name, json_t *args) {
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
        if (ctm_raised(res)) {
            json_t *out = json_object();
            json_set(out, "__raise__", json_copy(ctm_raise_spec(res)));
            json_free(res); json_free(args);
            return out;
        }
        json_free(res); json_free(args);
        return NULL;
    }
    ctm_record(io, name, args, res ? json_copy(res) : json_null());
    return res;
}

/* In-function import check: NULL on success (records nothing), raise
 * sentinel on a scripted import failure. */
static json_t *ctm_import_check(ctm_io_t *io, const char *key) {
    if (!io || !io->call) return NULL;
    json_t *res = io->call(io, key, NULL);
    if (ctm_raised(res)) {
        json_t *out = json_object();
        json_set(out, "__raise__", json_copy(ctm_raise_spec(res)));
        json_free(res);
        return out;
    }
    json_free(res);
    return NULL;
}

/* ── raise-spec builders (unscripted live errors) ──────────────────────── */
static json_t *ctm_raise_new(const char *type, const char *message) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type));
    json_set(spec, "message", json_string(message));
    json_t *out = json_object();
    json_set(out, "__raise__", spec);
    return out;
}

/* ── state accessors ───────────────────────────────────────────────────── */
static json_t *ctm_st(ctm_io_t *io, const char *k) {
    return (io && io->state) ? json_obj_get(io->state, k) : NULL;
}
static void ctm_st_set(ctm_io_t *io, const char *k, json_t *v) {
    if (io && io->state) json_set(io->state, k, v); else json_free(v);
}
static json_t *ctm_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}
static void ctm_array_clear(json_t *arr) {
    if (!arr || arr->type != JSON_ARRAY) return;
    for (size_t i = 0; i < arr->c.count; i++) json_free(arr->c.items[i]);
    arr->c.count = 0;
}
/* direct dict subscript: absent key raises KeyError (str() == repr(key)) */
static json_t *ctm_subscript(json_t *obj, const char *key, json_t **out) {
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    if (!v) {
        char buf[256];
        snprintf(buf, sizeof buf, "'%s'", key);
        *out = NULL;
        return ctm_raise_new("KeyError", buf);
    }
    *out = v;
    return NULL;
}

/* ── UTF-8 / Python string semantics ───────────────────────────────────── */
static unsigned long ctm_cp(const char *s, size_t *adv) {
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
static size_t ctm_utf8_len(const char *s) {
    size_t n = 0, adv;
    if (!s) return 0;
    while (*s) { ctm_cp(s, &adv); s += adv; n++; }
    return n;
}
/* first n code points, malloc'd (Python text[:n]); n past end -> whole */
static char *ctm_utf8_prefix(const char *s, size_t n) {
    if (!s) s = "";
    const char *p = s; size_t adv, i = 0;
    while (*p && i < n) { ctm_cp(p, &adv); p += adv; i++; }
    size_t len = (size_t)(p - s);
    char *out = (char *)malloc(len + 1);
    memcpy(out, s, len); out[len] = '\0';
    return out;
}
/* Python % for positive modulus (negative operands wrap positive) */
static long long ctm_pymod(long long a, long long m) {
    long long r = a % m;
    if (r < 0) r += m;
    return r;
}

/* ── string builder ────────────────────────────────────────────────────── */
typedef struct { char *p; size_t len, cap; } ctm_sb;
static void ctm_sb_init(ctm_sb *b) {
    b->cap = 64; b->len = 0;
    b->p = (char *)malloc(b->cap); b->p[0] = '\0';
}
static void ctm_sb_puts(ctm_sb *b, const char *s) {
    if (!s) return;
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        while (b->len + n + 1 > b->cap) b->cap *= 2;
        b->p = (char *)realloc(b->p, b->cap);
    }
    memcpy(b->p + b->len, s, n); b->len += n; b->p[b->len] = '\0';
}
static char *ctm_sb_take(ctm_sb *b) { return b->p; }

/* Python repr() of a str (KeyError str() is repr of its argument) */
static char *ctm_py_repr(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    ctm_sb b; ctm_sb_init(&b);
    char qs[2] = { q, '\0' };
    ctm_sb_puts(&b, qs);
    for (const unsigned char *u = (const unsigned char *)s; *u; u++) {
        unsigned char c = *u;
        if (c == '\\')                    ctm_sb_puts(&b, "\\\\");
        else if (c == (unsigned char)q) { char e[3] = { '\\', (char)q, '\0' }; ctm_sb_puts(&b, e); }
        else if (c == '\n')               ctm_sb_puts(&b, "\\n");
        else if (c == '\r')               ctm_sb_puts(&b, "\\r");
        else if (c == '\t')               ctm_sb_puts(&b, "\\t");
        else if (c < 0x20 || c == 0x7f) { char e[5]; snprintf(e, sizeof e, "\\x%02x", c); ctm_sb_puts(&b, e); }
        else                            { char ch[2] = { (char)c, '\0' }; ctm_sb_puts(&b, ch); }
    }
    ctm_sb_puts(&b, qs);
    return ctm_sb_take(&b);
}
/* str() of a scripted exception (KeyError quotes its argument) */
static char *ctm_exc_str(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    if (strcmp(type, "KeyError") == 0) return ctm_py_repr(msg);
    return strdup(msg);
}

/* ── built-in doubles (buffer / app) ───────────────────────────────────── */
static json_t *ctm_buf(ctm_io_t *io) {
    json_t *b = ctm_st(io, "buf");
    return (b && b->type == JSON_OBJECT) ? b : NULL;
}
static const char *ctm_buf_text(ctm_io_t *io) {
    json_t *b = ctm_buf(io);
    return b ? json_get_str(b, "text", "") : "";
}
static double ctm_buf_cursor(ctm_io_t *io) {
    json_t *b = ctm_buf(io);
    return b ? json_get_num(b, "cursor_position", 0) : 0;
}
static void ctm_buf_reset(ctm_io_t *io, bool append_history) {
    json_t *args = json_object();
    json_set(args, "append_to_history", json_bool(append_history));
    ctm_record(io, "buffer.reset", args, json_null());
    json_t *b = ctm_buf(io);
    if (b) {
        json_set(b, "text", json_string(""));
        json_set(b, "cursor_position", json_number(0));
    }
}
/* prompt_toolkit Buffer.insert_text: insert at cursor, advance cursor */
static void ctm_buf_insert_text(ctm_io_t *io, const char *s) {
    json_t *args = json_object();
    json_set(args, "text", json_string(s));
    ctm_record(io, "buffer.insert_text", args, json_null());
    json_t *b = ctm_buf(io);
    if (!b) return;
    const char *t = json_get_str(b, "text", "");
    size_t n = ctm_utf8_len(t);
    double cur = json_get_num(b, "cursor_position", 0);
    size_t ci = (size_t)(cur < 0 ? 0 : cur);
    if (ci > n) ci = n;
    /* byte offset of code-point ci */
    const char *p = t; size_t adv, off = 0;
    for (size_t i = 0; i < ci && *p; i++) { ctm_cp(p, &adv); p += adv; off += adv; }
    ctm_sb bld; ctm_sb_init(&bld);
    char *head = (char *)malloc(off + 1);
    memcpy(head, t, off); head[off] = '\0';
    ctm_sb_puts(&bld, head); free(head);
    ctm_sb_puts(&bld, s);
    ctm_sb_puts(&bld, t + off);
    char *nt = ctm_sb_take(&bld);
    json_set(b, "text", json_string(nt));
    json_set(b, "cursor_position", json_number((double)(ci + ctm_utf8_len(s))));
    free(nt);
}
static void ctm_buf_set_text(ctm_io_t *io, const char *text) {
    json_t *b = ctm_buf(io);
    if (b) {
        json_set(b, "text", json_string(text));
        json_set(b, "cursor_position", json_number((double)ctm_utf8_len(text)));
    }
}
static void ctm_app_invalidate(ctm_io_t *io) {
    ctm_record(io, "app.invalidate", json_object(), json_null());
}

/* ── i18n helper (seamed) ──────────────────────────────────────────────── */
static json_t *ctm_t(ctm_io_t *io, const char *key, json_t *kwargs) {
    json_t *args = kwargs ? kwargs : json_object();
    json_set(args, "key", json_string(key));
    return ctm_seam(io, "t", args);
}

/* _tui_set_filter (real staticmethod; pure mutator, no seam) */
static void ctm_set_filter(json_t *st, const char *value) {
    if (!st || st->type != JSON_OBJECT) return;
    json_set(st, "filter", json_string(value));
    json_set(st, "selected", json_number(0));
    json_set(st, "_scroll_offset", json_number(0));
}
/* filter[:-1] over code points (malloc'd) */
static char *ctm_filter_backspace(const char *filter) {
    size_t n = ctm_utf8_len(filter);
    return ctm_utf8_prefix(filter, n > 0 ? n - 1 : 0);
}
/* sorted-unique int-list symmetric-difference toggle of one value */
static void ctm_indices_toggle(json_t *arr, long long val) {
    if (!arr || arr->type != JSON_ARRAY) return;
    /* remove if present */
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *it = arr->c.items[i];
        if (it && it->type == JSON_NUMBER && (long long)it->num_val == val) {
            json_free(it);
            for (size_t j = i; j + 1 < arr->c.count; j++) arr->c.items[j] = arr->c.items[j + 1];
            arr->c.count--;
            return;
        }
    }
    /* insert at sorted position */
    size_t pos = arr->c.count;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *it = arr->c.items[i];
        if (it && it->type == JSON_NUMBER && (long long)it->num_val > val) { pos = i; break; }
    }
    json_append(arr, json_null()); /* grow */
    json_free(arr->c.items[arr->c.count - 1]); /* drop placeholder before shift */
    for (size_t j = arr->c.count - 1; j > pos; j--) arr->c.items[j] = arr->c.items[j - 1];
    arr->c.items[pos] = json_number((double)val);
}

/* ── module leaves ─────────────────────────────────────────────────────── */

/* PoP: ctm_approval_choice_label @ hermes_cli/cli_tui_mixin.py:_approval_choice_label */
json_t *ctm_approval_choice_label(ctm_io_t *io, const char *choice) {
    const char *key = NULL;
    for (size_t i = 0; i < sizeof(CTM_APPROVAL_KEYS) / sizeof(CTM_APPROVAL_KEYS[0]); i++) {
        if (strcmp(CTM_APPROVAL_KEYS[i][0], choice) == 0) { key = CTM_APPROVAL_KEYS[i][1]; break; }
    }
    if (!key) return json_string(choice);
    return ctm_t(io, key, NULL);
}

/* PoP: ctm_agent_name @ hermes_cli/cli_tui_mixin.py:_agent_name */
json_t *ctm_agent_name(ctm_io_t *io) {
    /* try: from hermes_cli.skin_engine import get_active_skin
     *      return get_active_skin().get_branding("agent_name", "Hermes")
     * except Exception: return "Hermes" */
    json_t *imp = ctm_import_check(io, "import:hermes_cli.skin_engine");
    if (imp) {
        if (ctm_base_raised(imp)) return imp;
        json_free(imp); return json_string("Hermes");
    }
    json_t *skin = ctm_seam(io, "get_active_skin", json_object());
    if (ctm_raised(skin)) {
        if (ctm_base_raised(skin)) return skin;
        json_free(skin); return json_string("Hermes");
    }
    if (!skin || skin->type == JSON_NULL) {
        /* None.get_branding -> AttributeError, swallowed */
        json_free(skin); return json_string("Hermes");
    }
    json_free(skin);
    json_t *args = json_object();
    json_set(args, "key", json_string("agent_name"));
    json_set(args, "default", json_string("Hermes"));
    json_t *brand = ctm_seam(io, "get_branding", args);
    if (ctm_raised(brand)) {
        if (ctm_base_raised(brand)) return brand;
        json_free(brand); return json_string("Hermes");
    }
    return brand;
}

/* PoP: ctm_tn @ hermes_cli/cli_tui_mixin.py:_tn */
json_t *ctm_tn(ctm_io_t *io, const char *key, long long count, json_t *kwargs) {
    char kbuf[256];
    snprintf(kbuf, sizeof kbuf, "%s_%s", key, count == 1 ? "one" : "other");
    json_t *kw = kwargs ? kwargs : json_object();
    json_set(kw, "count", json_number((double)count));
    return ctm_t(io, kbuf, kw);
}

/* PoP: ctm_num_prefix @ hermes_cli/cli_tui_mixin.py:_num_prefix */
json_t *ctm_num_prefix(long long i) {
    char buf[32];
    if (i < 9) snprintf(buf, sizeof buf, "%lld", i + 1);
    else if (i == 9) snprintf(buf, sizeof buf, "0");
    else snprintf(buf, sizeof buf, " ");
    return json_string(buf);
}

/* PoP: ctm_term_rows @ hermes_cli/cli_tui_mixin.py:_term_rows */
json_t *ctm_term_rows(ctm_io_t *io) {
    json_t *args = json_object();
    json_t *fb = json_array();
    json_append(fb, json_number(100));
    json_append(fb, json_number(24));
    json_set(args, "fallback", fb);
    json_t *size = ctm_seam(io, "shutil.get_terminal_size", args);
    if (ctm_raised(size)) return size;
    if (!size || size->type == JSON_NULL) {
        json_free(size);
        return ctm_raise_new("AttributeError", "'NoneType' object has no attribute 'lines'");
    }
    json_t *lines = json_obj_get(size, "lines");
    if (!lines) {
        json_free(size);
        return ctm_raise_new("AttributeError",
            "'types.SimpleNamespace' object has no attribute 'lines'");
    }
    json_t *ret = json_copy(lines);
    json_free(size);
    return ret;
}

/* PoP: ctm_audio_level_bar @ hermes_cli/cli_tui_mixin.py:_audio_level_bar */
json_t *ctm_audio_level_bar(ctm_io_t *io) {
    json_t *rec = ctm_st(io, "_voice_recorder");
    if (!rec || rec->type == JSON_NULL) return json_string("");
    json_t *rms = (rec->type == JSON_OBJECT) ? json_obj_get(rec, "current_rms") : NULL;
    if (!rms) {
        return ctm_raise_new("AttributeError",
            "'types.SimpleNamespace' object has no attribute 'current_rms'");
    }
    long long r = (long long)rms->num_val;
    if (r > 8000) r = 8000;
    long long idx = (r * 7) / 8000;   /* r >= 0 domain; floor div */
    if (idx < 0) idx = 0;
    if (idx > 7) idx = 7;
    return json_string(CTM_AUDIO_BAR[idx]);
}

/* PoP: ctm_set_filter_flow @ hermes_cli/cli_tui_mixin.py:_tui_set_filter */
json_t *ctm_set_filter_flow(json_t *st, const char *value) {
    ctm_set_filter(st, value);
    return json_null();
}/* ── connection navigation ─────────────────────────────────────────────── */

/* PoP: ctm_connection_up @ hermes_cli/cli_tui_mixin.py:_tui_connection_up */
json_t *ctm_connection_up(ctm_io_t *io) {
    json_t *args = json_object();
    json_set(args, "delta", json_number(-1));
    json_t *r = ctm_seam(io, "self._tui_connection_move", args);
    if (ctm_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: ctm_connection_down @ hermes_cli/cli_tui_mixin.py:_tui_connection_down */
json_t *ctm_connection_down(ctm_io_t *io) {
    json_t *args = json_object();
    json_set(args, "delta", json_number(1));
    json_t *r = ctm_seam(io, "self._tui_connection_move", args);
    if (ctm_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: ctm_connection_side @ hermes_cli/cli_tui_mixin.py:_tui_connection_side */
json_t *ctm_connection_side(ctm_io_t *io) {
    json_t *state = ctm_st(io, "_connection_state");
    if (!ctm_truthy(state)) return json_null();
    json_t *fields = json_obj_get(state, "fields");
    size_t nfields = (fields && fields->type == JSON_ARRAY) ? fields->c.count : 0;
    const char *phase = json_get_str(state, "phase", "");
    double field_index = json_get_num(state, "field_index", 0);
    bool act = (strcmp(phase, "authorized") == 0) ||
               ((strcmp(phase, "form") == 0 || strcmp(phase, "failed") == 0) &&
                field_index >= (double)nfields);
    if (!act) return json_null();
    double selected = json_get_num(state, "selected", 0);
    double clamped = selected < 0 ? 0 : (selected > 1 ? 1 : selected);
    json_set(state, "selected", json_number(1 - clamped));
    json_t *r = ctm_seam(io, "self._paint_now", json_object());
    if (ctm_raised(r)) return r;
    json_free(r);
    ctm_app_invalidate(io);
    return json_null();
}

/* ── model picker navigation ───────────────────────────────────────────── */

/* PoP: ctm_model_picker_down @ hermes_cli/cli_tui_mixin.py:_tui_model_picker_down */
json_t *ctm_model_picker_down(ctm_io_t *io) {
    json_t *state = ctm_st(io, "_model_picker_state");
    if (!ctm_truthy(state)) return json_null();
    const char *stage = json_get_str(state, "stage", "");
    long long max_idx = 0;
    if (strcmp(stage, "provider") == 0) {
        json_t *prov = json_obj_get(state, "providers");
        max_idx = (prov && prov->type == JSON_ARRAY) ? (long long)prov->c.count : 0;
    } else if (strcmp(stage, "reasoning") == 0) {
        json_t *imp = ctm_import_check(io, "import:hermes_cli.cli_model_switch_mixin");
        if (imp) return imp;
        json_t *rows = ctm_seam(io, "_picker_reasoning_rows", json_object());
        if (ctm_raised(rows)) return rows;
        long long n = (rows && rows->type == JSON_ARRAY) ? (long long)rows->c.count : 0;
        json_free(rows);
        max_idx = n + 1;   /* + Back + Cancel */
    } else {
        json_t *fp = json_obj_get(state, "_filtered_pairs");
        if (fp && fp->type != JSON_NULL) {
            max_idx = (fp->type == JSON_ARRAY ? (long long)fp->c.count : 0) + 1;
        } else {
            json_t *ml = json_obj_get(state, "model_list");
            max_idx = (ml && ml->type == JSON_ARRAY ? (long long)ml->c.count : 0) + 1;
        }
    }
    double selected = json_get_num(state, "selected", 0);
    double next = selected + 1;
    if (next > (double)max_idx) next = (double)max_idx;
    json_set(state, "selected", json_number(next));
    ctm_app_invalidate(io);
    return json_null();
}

/* PoP: ctm_model_picker_up @ hermes_cli/cli_tui_mixin.py:_tui_model_picker_up */
json_t *ctm_model_picker_up(ctm_io_t *io) {
    json_t *state = ctm_st(io, "_model_picker_state");
    if (ctm_truthy(state)) {
        double selected = json_get_num(state, "selected", 0);
        double next = selected - 1;
        if (next < 0) next = 0;
        json_set(state, "selected", json_number(next));
        ctm_app_invalidate(io);
    }
    return json_null();
}

/* PoP: ctm_model_picker_filter_backspace @ hermes_cli/cli_tui_mixin.py:_tui_model_picker_filter_backspace */
json_t *ctm_model_picker_filter_backspace(ctm_io_t *io) {
    json_t *st = ctm_st(io, "_model_picker_state");
    if (ctm_truthy(st)) {
        const char *filter = json_get_str(st, "filter", "");
        char *nf = ctm_filter_backspace(filter);
        ctm_set_filter(st, nf);
        free(nf);
        ctm_app_invalidate(io);
    }
    return json_null();
}

/* ── clarify navigation ────────────────────────────────────────────────── */

/* PoP: ctm_clarify_toggle @ hermes_cli/cli_tui_mixin.py:_tui_clarify_toggle */
json_t *ctm_clarify_toggle(ctm_io_t *io) {
    json_t *state = ctm_st(io, "_clarify_state");
    if (ctm_truthy(state)) {
        json_t *sel = NULL;
        json_t *err = ctm_subscript(state, "selected", &sel);
        if (err) return err;
        long long val = (long long)sel->num_val;
        json_t *indices = json_obj_get(state, "selected_indices");
        /* absent -> live mutates a fresh default set (no write-back) */
        if (indices && indices->type == JSON_ARRAY) ctm_indices_toggle(indices, val);
        ctm_app_invalidate(io);
    }
    return json_null();
}

/* PoP: ctm_clarify_down @ hermes_cli/cli_tui_mixin.py:_tui_clarify_down */
json_t *ctm_clarify_down(ctm_io_t *io) {
    json_t *state = ctm_st(io, "_clarify_state");
    if (ctm_truthy(state)) {
        json_t *choices = json_obj_get(state, "choices");
        long long max_idx = (choices && choices->type == JSON_ARRAY) ? (long long)choices->c.count : 0;
        json_t *sel = NULL;
        json_t *err = ctm_subscript(state, "selected", &sel);
        if (err) return err;
        double next = sel->num_val + 1;
        if (next > (double)max_idx) next = (double)max_idx;
        json_set(state, "selected", json_number(next));
        ctm_app_invalidate(io);
    }
    return json_null();
}

/* PoP: ctm_clarify_up @ hermes_cli/cli_tui_mixin.py:_tui_clarify_up */
json_t *ctm_clarify_up(ctm_io_t *io) {
    json_t *state = ctm_st(io, "_clarify_state");
    if (ctm_truthy(state)) {
        json_t *sel = NULL;
        json_t *err = ctm_subscript(state, "selected", &sel);
        if (err) return err;
        double next = sel->num_val - 1;
        if (next < 0) next = 0;
        json_set(state, "selected", json_number(next));
        ctm_app_invalidate(io);
    }
    return json_null();
}

/* PoP: ctm_clarify_batch_step @ hermes_cli/cli_tui_mixin.py:_tui_clarify_batch_step */
json_t *ctm_clarify_batch_step(ctm_io_t *io, long long delta) {
    json_t *state = ctm_st(io, "_clarify_state");
    if (ctm_truthy(state)) {
        json_t *active = NULL, *questions = NULL;
        json_t *err = ctm_subscript(state, "active", &active);
        if (err) return err;
        err = ctm_subscript(state, "questions", &questions);
        if (err) return err;
        long long n = (questions->type == JSON_ARRAY) ? (long long)questions->c.count : 0;
        if (n == 0) return ctm_raise_new("ZeroDivisionError", "division by zero");
        long long newactive = ctm_pymod((long long)active->num_val + delta, n);
        json_t *args = json_object();
        json_set(args, "state", json_copy(state));
        json_set(args, "active", json_number((double)newactive));
        json_t *r = ctm_seam(io, "self._clarify_batch_set_active", args);
        if (ctm_raised(r)) return r;
        json_free(r);
        ctm_app_invalidate(io);
    }
    return json_null();
}

/* PoP: ctm_clarify_batch_tab @ hermes_cli/cli_tui_mixin.py:_tui_clarify_batch_tab */
json_t *ctm_clarify_batch_tab(ctm_io_t *io) {
    return ctm_clarify_batch_step(io, 1);
}

/* PoP: ctm_clarify_batch_backtab @ hermes_cli/cli_tui_mixin.py:_tui_clarify_batch_backtab */
json_t *ctm_clarify_batch_backtab(ctm_io_t *io) {
    return ctm_clarify_batch_step(io, -1);
}

/* ── command palette navigation ────────────────────────────────────────── */

/* PoP: ctm_command_palette_backspace @ hermes_cli/cli_tui_mixin.py:_tui_command_palette_backspace */
json_t *ctm_command_palette_backspace(ctm_io_t *io) {
    json_t *st = ctm_st(io, "_command_palette_state");
    if (ctm_truthy(st)) {
        const char *filter = json_get_str(st, "filter", "");
        char *nf = ctm_filter_backspace(filter);
        ctm_set_filter(st, nf);
        free(nf);
        ctm_app_invalidate(io);
    }
    return json_null();
}

/* PoP: ctm_command_palette_down @ hermes_cli/cli_tui_mixin.py:_tui_command_palette_down */
json_t *ctm_command_palette_down(ctm_io_t *io) {
    json_t *st = ctm_st(io, "_command_palette_state");
    if (ctm_truthy(st)) {
        /* dict.get(k, len(seam())) evaluates the seam EAGERLY even when k present */
        json_t *entries = ctm_seam(io, "self._command_palette_visible_entries", json_object());
        if (ctm_raised(entries)) return entries;
        long long n = (entries && entries->type == JSON_ARRAY) ? (long long)entries->c.count : 0;
        json_free(entries);
        json_t *vc = json_obj_get(st, "_visible_count");
        if (vc && vc->type == JSON_NUMBER) n = (long long)vc->num_val;
        double selected = json_get_num(st, "selected", 0);
        double cap = (double)(n - 1);
        if (cap < 0) cap = 0;
        double next = selected + 1;
        if (next > cap) next = cap;
        json_set(st, "selected", json_number(next));
        ctm_app_invalidate(io);
    }
    return json_null();
}

/* PoP: ctm_command_palette_up @ hermes_cli/cli_tui_mixin.py:_tui_command_palette_up */
json_t *ctm_command_palette_up(ctm_io_t *io) {
    json_t *st = ctm_st(io, "_command_palette_state");
    if (ctm_truthy(st)) {
        double selected = json_get_num(st, "selected", 0);
        double next = selected - 1;
        if (next < 0) next = 0;
        json_set(st, "selected", json_number(next));
        ctm_app_invalidate(io);
    }
    return json_null();
}

/* PoP: ctm_command_palette_enter @ hermes_cli/cli_tui_mixin.py:_tui_command_palette_enter */
json_t *ctm_command_palette_enter(ctm_io_t *io) {
    json_t *r = ctm_seam(io, "self._handle_command_palette_selection", json_object());
    if (ctm_raised(r)) return r;
    json_free(r);
    ctm_app_invalidate(io);
    return json_null();
}

/* PoP: ctm_command_palette_escape @ hermes_cli/cli_tui_mixin.py:_tui_command_palette_escape */
json_t *ctm_command_palette_escape(ctm_io_t *io) {
    json_t *r = ctm_seam(io, "self._close_command_palette", json_object());
    if (ctm_raised(r)) return r;
    json_free(r);
    ctm_app_invalidate(io);
    return json_null();
}

/* ── slash-confirm / approval navigation ───────────────────────────────── */

/* PoP: ctm_slash_confirm_down @ hermes_cli/cli_tui_mixin.py:_tui_slash_confirm_down */
json_t *ctm_slash_confirm_down(ctm_io_t *io) {
    json_t *st = ctm_st(io, "_slash_confirm_state");
    if (ctm_truthy(st)) {
        json_t *choices = json_obj_get(st, "choices");
        long long n = (choices && choices->type == JSON_ARRAY) ? (long long)choices->c.count : 0;
        double selected = json_get_num(st, "selected", 0);
        double cap = (double)(n - 1);
        double next = selected + 1;
        if (next > cap) next = cap;
        json_set(st, "selected", json_number(next));
        ctm_app_invalidate(io);
    }
    return json_null();
}

/* PoP: ctm_slash_confirm_up @ hermes_cli/cli_tui_mixin.py:_tui_slash_confirm_up */
json_t *ctm_slash_confirm_up(ctm_io_t *io) {
    json_t *st = ctm_st(io, "_slash_confirm_state");
    if (ctm_truthy(st)) {
        double selected = json_get_num(st, "selected", 0);
        double next = selected - 1;
        if (next < 0) next = 0;
        json_set(st, "selected", json_number(next));
        ctm_app_invalidate(io);
    }
    return json_null();
}

/* PoP: ctm_approval_down @ hermes_cli/cli_tui_mixin.py:_tui_approval_down */
json_t *ctm_approval_down(ctm_io_t *io) {
    json_t *st = ctm_st(io, "_approval_state");
    if (ctm_truthy(st)) {
        json_t *choices = NULL, *sel = NULL;
        json_t *err = ctm_subscript(st, "choices", &choices);
        if (err) return err;
        err = ctm_subscript(st, "selected", &sel);
        if (err) return err;
        long long n = (choices->type == JSON_ARRAY) ? (long long)choices->c.count : 0;
        double cap = (double)(n - 1);
        double next = sel->num_val + 1;
        if (next > cap) next = cap;
        json_set(st, "selected", json_number(next));
        ctm_app_invalidate(io);
    }
    return json_null();
}

/* PoP: ctm_approval_up @ hermes_cli/cli_tui_mixin.py:_tui_approval_up */
json_t *ctm_approval_up(ctm_io_t *io) {
    json_t *st = ctm_st(io, "_approval_state");
    if (ctm_truthy(st)) {
        json_t *sel = NULL;
        json_t *err = ctm_subscript(st, "selected", &sel);
        if (err) return err;
        double next = sel->num_val - 1;
        if (next < 0) next = 0;
        json_set(st, "selected", json_number(next));
        ctm_app_invalidate(io);
    }
    return json_null();
}/* ── editor gestures ───────────────────────────────────────────────────── */

/* PoP: ctm_handle_double_escape @ hermes_cli/cli_tui_mixin.py:_tui_handle_double_escape */
json_t *ctm_handle_double_escape(ctm_io_t *io) {
    const char *text = ctm_buf_text(io);
    json_t *images = ctm_st(io, "_attached_images");
    bool has_text = text && text[0] != '\0';
    bool has_images = ctm_truthy(images);
    if (!has_text && !has_images) return json_null();
    ctm_buf_reset(io, has_text);
    if (images && images->type == JSON_ARRAY) ctm_array_clear(images);
    ctm_app_invalidate(io);
    return json_null();
}

/* PoP: ctm_handle_alt_v @ hermes_cli/cli_tui_mixin.py:_tui_handle_alt_v */
json_t *ctm_handle_alt_v(ctm_io_t *io) {
    json_t *r = ctm_seam(io, "self._try_attach_clipboard_image", json_object());
    if (ctm_raised(r)) return r;
    bool ok = ctm_truthy(r);
    json_free(r);
    if (ok) ctm_app_invalidate(io);
    return json_null();
}

/* PoP: ctm_handle_ctrl_v @ hermes_cli/cli_tui_mixin.py:_tui_handle_ctrl_v */
json_t *ctm_handle_ctrl_v(ctm_io_t *io) {
    json_t *r = ctm_seam(io, "self._try_attach_clipboard_image", json_object());
    if (ctm_raised(r)) return r;
    bool ok = ctm_truthy(r);
    json_free(r);
    if (ok) ctm_app_invalidate(io);
    return json_null();
}

/* PoP: ctm_handle_ctrl_l @ hermes_cli/cli_tui_mixin.py:_tui_handle_ctrl_l */
json_t *ctm_handle_ctrl_l(ctm_io_t *io) {
    json_t *r = ctm_seam(io, "self._force_full_redraw", json_object());
    if (ctm_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: ctm_insert_newline @ hermes_cli/cli_tui_mixin.py:_tui_insert_newline */
json_t *ctm_insert_newline(ctm_io_t *io) {
    ctm_buf_insert_text(io, "\n");
    return json_null();
}

/* PoP: ctm_handle_open_in_editor @ hermes_cli/cli_tui_mixin.py:_tui_handle_open_in_editor */
json_t *ctm_handle_open_in_editor(ctm_io_t *io) {
    json_t *args = json_object();
    json_set(args, "text", json_string(ctm_buf_text(io)));
    json_t *r = ctm_seam(io, "self._open_external_editor", args);
    if (ctm_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: ctm_recall_without_recollapse @ hermes_cli/cli_tui_mixin.py:_tui_recall_without_recollapse */
json_t *ctm_recall_without_recollapse(ctm_io_t *io) {
    char *before = strdup(ctm_buf_text(io));
    ctm_st_set(io, "_skip_paste_collapse", json_bool(true));
    json_t *r = ctm_seam(io, "move", json_object());
    if (ctm_raised(r)) { free(before); return r; }
    json_free(r);
    const char *after = ctm_buf_text(io);
    if (strcmp(after, before) == 0) ctm_st_set(io, "_skip_paste_collapse", json_bool(false));
    free(before);
    return json_null();
}

/* PoP: ctm_history_up @ hermes_cli/cli_tui_mixin.py:_tui_history_up */
json_t *ctm_history_up(ctm_io_t *io, double arg) {
    char *before = strdup(ctm_buf_text(io));
    ctm_st_set(io, "_skip_paste_collapse", json_bool(true));
    json_t *args = json_object();
    json_set(args, "count", json_number(arg));
    json_t *r = ctm_seam(io, "buffer.auto_up", args);
    if (ctm_raised(r)) { free(before); return r; }
    json_free(r);
    const char *after = ctm_buf_text(io);
    if (strcmp(after, before) == 0) ctm_st_set(io, "_skip_paste_collapse", json_bool(false));
    free(before);
    return json_null();
}

/* PoP: ctm_history_down @ hermes_cli/cli_tui_mixin.py:_tui_history_down */
json_t *ctm_history_down(ctm_io_t *io, double arg) {
    char *before = strdup(ctm_buf_text(io));
    ctm_st_set(io, "_skip_paste_collapse", json_bool(true));
    json_t *args = json_object();
    json_set(args, "count", json_number(arg));
    json_t *r = ctm_seam(io, "buffer.auto_down", args);
    if (ctm_raised(r)) { free(before); return r; }
    json_free(r);
    const char *after = ctm_buf_text(io);
    if (strcmp(after, before) == 0) ctm_st_set(io, "_skip_paste_collapse", json_bool(false));
    free(before);
    return json_null();
}

/* ── misc flows ────────────────────────────────────────────────────────── */

/* PoP: ctm_wake_startup @ hermes_cli/cli_tui_mixin.py:_tui_wake_startup */
json_t *ctm_wake_startup(ctm_io_t *io) {
    /* from cli import logger  (OUTSIDE the try -> import failure propagates) */
    json_t *imp = ctm_import_check(io, "import:cli");
    if (imp) return imp;
    json_t *r = ctm_seam(io, "self._maybe_start_wake_word", json_object());
    if (ctm_raised(r)) {
        if (ctm_base_raised(r)) return r;
        /* except Exception as e: logger.debug("wake-word startup skipped: %s", e) */
        json_t *spec = ctm_raise_spec(r);
        char *es = ctm_exc_str(spec);
        json_t *earg = json_object();
        json_set(earg, "type", json_string(json_get_str(spec, "type", "RuntimeError")));
        json_set(earg, "message", json_string(es));
        free(es);
        json_t *en = json_obj_get(spec, "errno");
        json_set(earg, "errno", en ? json_copy(en) : json_null());
        json_t *args = json_object();
        json_set(args, "msg", json_string("wake-word startup skipped: %s"));
        json_t *al = json_array();
        json_append(al, earg);
        json_set(args, "args", al);
        json_free(r);
        json_t *lr = ctm_seam(io, "logger.debug", args);
        if (ctm_raised(lr)) return lr;
        json_free(lr);
        return json_null();
    }
    json_free(r);
    return json_null();
}

/* PoP: ctm_handle_ignored_terminal_sequence @ hermes_cli/cli_tui_mixin.py:_tui_handle_ignored_terminal_sequence */
json_t *ctm_handle_ignored_terminal_sequence(ctm_io_t *io, const json_t *key_sequence) {
    /* try: for press in getattr(event,"key_sequence",None) or (): ...
     * except Exception: pass */
    if (!key_sequence || key_sequence->type == JSON_NULL) return json_null();
    if (key_sequence->type == JSON_NUMBER || key_sequence->type == JSON_BOOL) {
        /* iterating a non-iterable raises TypeError, swallowed */
        return json_null();
    }
    if (key_sequence->type == JSON_ARRAY) {
        for (size_t i = 0; i < key_sequence->c.count; i++) {
            json_t *press = key_sequence->c.items[i];
            const char *data = (press && press->type == JSON_OBJECT)
                                   ? json_get_str(press, "data", NULL) : NULL;
            if (data && strcmp(data, "\x1b[I") == 0) {
                json_t *r = ctm_seam(io, "self._schedule_focus_regain_redraw", json_object());
                if (ctm_raised(r)) {
                    if (ctm_base_raised(r)) return r;
                    json_free(r); return json_null();   /* swallowed */
                }
                json_free(r);
                break;
            }
        }
        return json_null();
    }
    if (key_sequence->type == JSON_STRING) {
        /* a str is iterable: each char is a press with no .data attribute */
        const char *s = key_sequence->str_val ? key_sequence->str_val : "";
        for (const char *p = s; *p; p++) {
            /* getattr(char, "data", None) == None -> never matches */
            (void)p;
        }
        return json_null();
    }
    return json_null();
}

/* ── stash browse-panel + restore ──────────────────────────────────────── */

/* _prompt_stash is always present on the self double (None when inactive).
 * Live reads self._prompt_stash.<method> directly: a None stash raises
 * AttributeError "'NoneType' object has no attribute '<method>'". Methods are
 * scripted seams when the stash is a live object. */
static json_t *ctm_stash_guard(ctm_io_t *io, const char *method) {
    json_t *ps = ctm_st(io, "_prompt_stash");
    if (!ps || ps->type == JSON_NULL) {
        char buf[160];
        snprintf(buf, sizeof buf, "'NoneType' object has no attribute '%s'", method);
        return ctm_raise_new("AttributeError", buf);
    }
    return NULL;
}

/* PoP: ctm_stash_panel_up @ hermes_cli/cli_tui_mixin.py:_tui_handle_stash_panel_up */
json_t *ctm_stash_panel_up(ctm_io_t *io) {
    json_t *g = ctm_stash_guard(io, "move_cursor");
    if (g) return g;
    json_t *args = json_object();
    json_set(args, "delta", json_number(-1));
    json_t *r = ctm_seam(io, "prompt_stash.move_cursor", args);
    if (ctm_raised(r)) return r;
    json_free(r);
    ctm_app_invalidate(io);
    return json_null();
}

/* PoP: ctm_stash_panel_down @ hermes_cli/cli_tui_mixin.py:_tui_handle_stash_panel_down */
json_t *ctm_stash_panel_down(ctm_io_t *io) {
    json_t *g = ctm_stash_guard(io, "move_cursor");
    if (g) return g;
    json_t *args = json_object();
    json_set(args, "delta", json_number(1));
    json_t *r = ctm_seam(io, "prompt_stash.move_cursor", args);
    if (ctm_raised(r)) return r;
    json_free(r);
    ctm_app_invalidate(io);
    return json_null();
}

/* PoP: ctm_stash_panel_delete @ hermes_cli/cli_tui_mixin.py:_tui_handle_stash_panel_delete */
json_t *ctm_stash_panel_delete(ctm_io_t *io) {
    json_t *g = ctm_stash_guard(io, "delete_at_cursor");
    if (g) return g;
    json_t *r = ctm_seam(io, "prompt_stash.delete_at_cursor", json_object());
    if (ctm_raised(r)) return r;
    json_free(r);
    ctm_app_invalidate(io);
    return json_null();
}

/* PoP: ctm_stash_panel_close @ hermes_cli/cli_tui_mixin.py:_tui_handle_stash_panel_close */
json_t *ctm_stash_panel_close(ctm_io_t *io) {
    json_t *g = ctm_stash_guard(io, "close_panel");
    if (g) return g;
    json_t *r = ctm_seam(io, "prompt_stash.close_panel", json_object());
    if (ctm_raised(r)) return r;
    json_free(r);
    ctm_app_invalidate(io);
    return json_null();
}

/* PoP: ctm_restore_stash_payload @ hermes_cli/cli_tui_mixin.py:_tui_restore_stash_payload */
json_t *ctm_restore_stash_payload(ctm_io_t *io, const json_t *payload) {
    if (!ctm_truthy(payload)) return json_null();
    if (!payload || payload->type != JSON_ARRAY) {
        return ctm_raise_new("TypeError", "cannot unpack non-iterable value");
    }
    size_t n = payload->c.count;
    if (n != 2) {
        char buf[96];
        if (n < 2) snprintf(buf, sizeof buf, "not enough values to unpack (expected 2, got %zu)", n);
        else snprintf(buf, sizeof buf, "too many values to unpack (expected 2, got %zu)", n);
        return ctm_raise_new("ValueError", buf);
    }
    json_t *text = payload->c.items[0];
    json_t *images = payload->c.items[1];
    const char *ts = (text && text->type == JSON_STRING) ? text->str_val : "";
    ctm_buf_set_text(io, ts);
    json_t *attached = ctm_st(io, "_attached_images");
    if (ctm_truthy(images) && images->type == JSON_ARRAY &&
        attached && attached->type == JSON_ARRAY) {
        for (size_t i = 0; i < images->c.count; i++) {
            json_t *img = images->c.items[i];
            bool found = false;
            for (size_t j = 0; j < attached->c.count; j++) {
                json_t *a = attached->c.items[j];
                if (a && img && a->type == img->type) {
                    if (a->type == JSON_STRING &&
                        strcmp(a->str_val ? a->str_val : "", img->str_val ? img->str_val : "") == 0) {
                        found = true; break;
                    }
                }
            }
            if (!found) json_append(attached, json_copy(img));
        }
    }
    return json_null();
}