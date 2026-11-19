/* port_tui_dispatch_flow.c — FLOW + leaf port of hermes_cli/cli_tui_mixin.py
 * TUI command dispatch: the Enter routing chain (overlay -> backslash
 * continuation -> burst -> submit), the submit chain (inline command ->
 * busy routing -> pending queue), the busy_input_mode steer/queue/interrupt
 * decision tree with its error fallbacks, the modal-overlay Enter priority
 * chain, clarify freetext/choice locking, the Ctrl+C / Ctrl+Q priority
 * chains (voice -> foreground UI -> blocking overlays -> interrupt/force
 * exit -> clear-or-exit), modal ESC handling, Ctrl+D delete-vs-exit,
 * connection field navigation, model-picker ESC, the paste-collapse
 * threshold predicate and the closed-loop error suppressor.
 *
 * All terminal / agent / queue / config / i18n I/O is INJECTED through the
 * ctd_io_t universal seam dispatcher: every live call goes through
 * ctd_seam(io, name, args); the harness dispatcher appends nothing itself —
 * ctd_seam records {ev, args, ret} into io->trace, returns the scripted
 * result (caller-owned), and models a raised exception as
 * {"__raise__": {type, message, ...}}. An "import_raise" script entry on an
 * "import:<module>" key models a failed in-function import: the dispatcher
 * returns {"__raise__": spec, "__untrace__": true} and ctd_import_check
 * raises WITHOUT a trace token (the live import statement records nothing).
 *
 * Built-in doubles (NOT scripted, identical behaviour in the oracle):
 *   buffer.reset / buffer.delete      — prompt_toolkit Buffer semantics
 *   app.invalidate / app.exit         — prompt_toolkit Application no-ops
 *   pending_input.put / interrupt_queue.put / sudo_response_queue.put
 *                                     — queue.Queue append into the state list
 *   gateway_command_wait.set          — threading.Event set flag
 *
 * io->state is a json OBJECT mirroring the CLI instance + module globals:
 *   buf {text, cursor_position}, app_is_running, agent (null or a spec with
 *   has_steer / has_redirect / _supports_active_turn_redirect flags),
 *   _agent_running, _should_exit, _detach_on_exit, _last_ctrl_c_time,
 *   _attached_images, _pending_input, _interrupt_queue, _clarify_state,
 *   _clarify_freetext, _clarify_prefill, _clarify_multi_base, _sudo_state
 *   (response_queue rides as a list), _secret_state, _approval_state,
 *   _slash_confirm_state, _connection_state, _model_picker_state,
 *   _command_palette_state, _gateway_command_wait {id, set}, busy_input_mode,
 *   config, _tui_multiline_shortcuts, CLI_CONFIG, _hermes_home, _accent.
 *
 * Ownership: seam args are OWNED by ctd_seam (moved into the trace event or
 * freed); seam results are caller-owned (json_free). io->trace / io->state
 * are harness-owned. Every public ctd_* function returns a NEW reference
 * (json null / bool / string) or a propagated {"__raise__": spec} sentinel.
 *
 * Fixture constraints (documented representation limits): scripted numbers
 * are integral (libjson has no int/float identity); _clarify_multi_base and
 * slash-confirm choice values are strings; config threshold values are
 * numbers; connection drafts/targets are pre-seeded objects.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

/* ── seam dispatcher + trace + state ───────────────────────────────────── */
typedef struct ctd_io_t {
    json_t *(*call)(struct ctd_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* ARRAY of {ev, args, ret} events (harness-owned) */
    json_t *state;   /* OBJECT mirror (harness-owned) */
    void   *ctx;
} ctd_io_t;

/* live module constants (byte-pinned to hermes_cli/cli_render.py via cli.py) */
#define CTD_DIM "\x1b[2;3m"
#define CTD_RST "\x1b[0m"
/* agent/onboarding.py BUSY_INPUT_FLAG */
#define CTD_BUSY_FLAG "busy_input_prompt"
/* errno.EIO */
#define CTD_EIO 5

static bool ctd_truthy(const json_t *v) {
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

static bool ctd_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *ctd_raise_spec(const json_t *r) {
    return ctd_raised(r) ? json_obj_get(r, "__raise__") : NULL;
}

static void ctd_record(ctd_io_t *io, const char *name, json_t *args, json_t *ret) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) {
        json_free(args); json_free(ret); return;
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? ret : json_null());
    json_append(io->trace, ev);
}

/* args OWNED; result caller-owned. NULL result == Python None. */
static json_t *ctd_seam(ctd_io_t *io, const char *name, json_t *args) {
    json_t *res = (io && io->call) ? io->call(io, name, args) : json_null();
    bool untrace = res && res->type == JSON_OBJECT &&
                   json_is_true(json_obj_get(res, "__untrace__"));
    if (untrace) {
        if (ctd_raised(res)) {
            json_t *out = json_object();
            json_set(out, "__raise__", json_copy(ctd_raise_spec(res)));
            json_free(res); json_free(args);
            return out;
        }
        json_free(res); json_free(args);
        return NULL;
    }
    ctd_record(io, name, args, res ? json_copy(res) : json_null());
    return res;
}

/* In-function import check: NULL on success (records nothing), raise
 * sentinel on a scripted import failure. */
static json_t *ctd_import_check(ctd_io_t *io, const char *key) {
    json_t *res = (io && io->call) ? io->call(io, key, NULL) : NULL;
    if (ctd_raised(res)) {
        json_t *out = json_object();
        json_set(out, "__raise__", json_copy(ctd_raise_spec(res)));
        json_free(res);
        return out;
    }
    json_free(res);
    return NULL;
}

/* ── state accessors ───────────────────────────────────────────────────── */
static json_t *ctd_st(ctd_io_t *io, const char *k) {
    return (io && io->state) ? json_obj_get(io->state, k) : NULL;
}
static void ctd_st_set(ctd_io_t *io, const char *k, json_t *v) {
    if (io && io->state) json_set(io->state, k, v); else json_free(v);
}
static const char *ctd_st_str(ctd_io_t *io, const char *k, const char *def) {
    json_t *v = ctd_st(io, k);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}
static double ctd_st_num(ctd_io_t *io, const char *k, double def) {
    json_t *v = ctd_st(io, k);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}
static json_t *ctd_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}
static void ctd_array_clear(json_t *arr) {
    if (!arr || arr->type != JSON_ARRAY) return;
    for (size_t i = 0; i < arr->c.count; i++) json_free(arr->c.items[i]);
    arr->c.count = 0;
}

/* ── UTF-8 / Python string semantics ───────────────────────────────────── */
static unsigned long ctd_cp(const char *s, size_t *adv) {
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
static size_t ctd_utf8_len(const char *s) {
    size_t n = 0, adv;
    if (!s) return 0;
    while (*s) { ctd_cp(s, &adv); s += adv; n++; }
    return n;
}
/* first n code points, malloc'd (Python text[:n]) */
static char *ctd_utf8_prefix(const char *s, size_t n) {
    if (!s) s = "";
    const char *p = s; size_t adv, i = 0;
    while (*p && i < n) { ctd_cp(p, &adv); p += adv; i++; }
    size_t len = (size_t)(p - s);
    char *out = (char *)malloc(len + 1);
    memcpy(out, s, len); out[len] = '\0';
    return out;
}
static bool ctd_isspace_cp(unsigned long cp) {
    return (cp >= 0x09 && cp <= 0x0d) || cp == 0x20 || cp == 0x85 ||
           cp == 0xa0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200a) ||
           cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f ||
           cp == 0x3000;
}
/* Python str.strip() (Unicode whitespace), malloc'd */
static char *ctd_strip(const char *s) {
    if (!s) s = "";
    size_t len = strlen(s), start = 0, adv;
    while (start < len) {
        unsigned long cp = ctd_cp(s + start, &adv);
        if (!ctd_isspace_cp(cp)) break;
        start += adv;
    }
    size_t pos = start, last = start;
    while (pos < len) {
        unsigned long cp = ctd_cp(s + pos, &adv);
        pos += adv;
        if (!ctd_isspace_cp(cp)) last = pos;
    }
    char *out = (char *)malloc(last - start + 1);
    memcpy(out, s + start, last - start); out[last - start] = '\0';
    return out;
}
/* Python % for positive modulus (negative operands wrap positive) */
static long long ctd_pymod(long long a, long long m) {
    long long r = a % m;
    if (r < 0) r += m;
    return r;
}

/* ── string builder ────────────────────────────────────────────────────── */
typedef struct { char *p; size_t len, cap; } ctd_sb;
static void ctd_sb_init(ctd_sb *b) {
    b->cap = 64; b->len = 0;
    b->p = (char *)malloc(b->cap); b->p[0] = '\0';
}
static void ctd_sb_puts(ctd_sb *b, const char *s) {
    if (!s) return;
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        while (b->len + n + 1 > b->cap) b->cap *= 2;
        b->p = (char *)realloc(b->p, b->cap);
    }
    memcpy(b->p + b->len, s, n); b->len += n; b->p[b->len] = '\0';
}
static char *ctd_sb_take(ctd_sb *b) { return b->p; }

/* Python str() of a scalar json value (f-string interpolation) */
static char *ctd_node_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return strdup(buf);
    }
    return strdup(""); /* containers never reach an f-string in the fixtures */
}

/* Python repr() of a str (KeyError str() is repr of its argument) */
static char *ctd_py_repr(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    ctd_sb b; ctd_sb_init(&b);
    char qs[2] = { q, '\0' };
    ctd_sb_puts(&b, qs);
    for (const unsigned char *u = (const unsigned char *)s; *u; u++) {
        unsigned char c = *u;
        if (c == '\\')                    ctd_sb_puts(&b, "\\\\");
        else if (c == (unsigned char)q) { char e[3] = { '\\', (char)q, '\0' }; ctd_sb_puts(&b, e); }
        else if (c == '\n')               ctd_sb_puts(&b, "\\n");
        else if (c == '\r')               ctd_sb_puts(&b, "\\r");
        else if (c == '\t')               ctd_sb_puts(&b, "\\t");
        else if (c == '\b')               ctd_sb_puts(&b, "\\b");
        else if (c == '\f')               ctd_sb_puts(&b, "\\f");
        else if (c < 0x20 || c == 0x7f) { char e[5]; snprintf(e, sizeof e, "\\x%02x", c); ctd_sb_puts(&b, e); }
        else                            { char ch[2] = { (char)c, '\0' }; ctd_sb_puts(&b, ch); }
    }
    ctd_sb_puts(&b, qs);
    return ctd_sb_take(&b);
}

/* str() of the scripted exception (KeyError quotes its argument) */
static char *ctd_exc_str(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    if (strcmp(type, "KeyError") == 0) return ctd_py_repr(msg);
    return strdup(msg);
}

/* ── Python json.dumps(..., ensure_ascii=False) ────────────────────────── */
static void ctd_py_dump_str(const char *s, ctd_sb *b) {
    ctd_sb_puts(b, "\"");
    for (const unsigned char *u = (const unsigned char *)s; *u; u++) {
        unsigned char c = *u;
        if (c == '"')       ctd_sb_puts(b, "\\\"");
        else if (c == '\\') ctd_sb_puts(b, "\\\\");
        else if (c == '\n') ctd_sb_puts(b, "\\n");
        else if (c == '\r') ctd_sb_puts(b, "\\r");
        else if (c == '\t') ctd_sb_puts(b, "\\t");
        else if (c == '\b') ctd_sb_puts(b, "\\b");
        else if (c == '\f') ctd_sb_puts(b, "\\f");
        else if (c < 0x20) { char e[7]; snprintf(e, sizeof e, "\\u%04x", c); ctd_sb_puts(b, e); }
        else               { char ch[2] = { (char)c, '\0' }; ctd_sb_puts(b, ch); }
    }
    ctd_sb_puts(b, "\"");
}
static void ctd_py_dumps_rec(const json_t *v, ctd_sb *b) {
    if (!v || v->type == JSON_NULL) { ctd_sb_puts(b, "null"); return; }
    if (v->type == JSON_BOOL) { ctd_sb_puts(b, v->bool_val ? "true" : "false"); return; }
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        ctd_sb_puts(b, buf);
        return;
    }
    if (v->type == JSON_STRING) { ctd_py_dump_str(v->str_val ? v->str_val : "", b); return; }
    if (v->type == JSON_ARRAY) {
        ctd_sb_puts(b, "[");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) ctd_sb_puts(b, ", ");
            ctd_py_dumps_rec(v->c.items[i], b);
        }
        ctd_sb_puts(b, "]");
        return;
    }
    if (v->type == JSON_OBJECT) {
        ctd_sb_puts(b, "{");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) ctd_sb_puts(b, ", ");
            ctd_py_dump_str(v->c.keys[i], b);
            ctd_sb_puts(b, ": ");
            ctd_py_dumps_rec(v->c.items[i], b);
        }
        ctd_sb_puts(b, "}");
        return;
    }
}
static char *ctd_py_dumps(const json_t *v) {
    ctd_sb b; ctd_sb_init(&b);
    ctd_py_dumps_rec(v, &b);
    return ctd_sb_take(&b);
}

/* ── built-in doubles (buffer / app / queues / event) ──────────────────── */
static json_t *ctd_buf(ctd_io_t *io) {
    json_t *b = ctd_st(io, "buf");
    return (b && b->type == JSON_OBJECT) ? b : NULL;
}
static const char *ctd_buf_text(ctd_io_t *io) {
    json_t *b = ctd_buf(io);
    return b ? json_get_str(b, "text", "") : "";
}
static void ctd_buf_reset(ctd_io_t *io, bool append_history) {
    json_t *args = json_object();
    json_set(args, "append_to_history", json_bool(append_history));
    ctd_record(io, "buffer.reset", args, json_null());
    json_t *b = ctd_buf(io);
    if (b) {
        json_set(b, "text", json_string(""));
        json_set(b, "cursor_position", json_number(0));
    }
}
static void ctd_buf_delete(ctd_io_t *io) {
    ctd_record(io, "buffer.delete", json_object(), json_null());
    json_t *b = ctd_buf(io);
    if (!b) return;
    const char *t = json_get_str(b, "text", "");
    double cur = json_get_num(b, "cursor_position", 0);
    size_t n = ctd_utf8_len(t);
    size_t ci = (size_t)(cur < 0 ? 0 : cur);
    if (ci >= n) return;
    const char *p = t; size_t adv, start = 0, end = 0;
    for (size_t i = 0; i <= ci && *p; i++) {
        ctd_cp(p, &adv);
        if (i == ci) { start = (size_t)(p - t); end = start + adv; }
        p += adv;
    }
    size_t len = strlen(t), cut = end - start;
    char *out = (char *)malloc(len - cut + 1);
    memcpy(out, t, start);
    memcpy(out + start, t + end, len - end);
    out[len - cut] = '\0';
    json_set(b, "text", json_string(out));
    free(out);
}
static void ctd_app_invalidate(ctd_io_t *io) {
    ctd_record(io, "app.invalidate", json_object(), json_null());
}
static void ctd_app_exit(ctd_io_t *io) {
    ctd_record(io, "app.exit", json_object(), json_null());
}
static void ctd_event_set(ctd_io_t *io) {
    ctd_record(io, "gateway_command_wait.set", json_object(), json_null());
    json_t *ev = ctd_st(io, "_gateway_command_wait");
    if (ev && ev->type == JSON_OBJECT) json_set(ev, "set", json_bool(true));
}
static void ctd_queue_put(ctd_io_t *io, const char *qkey, const char *evname,
                          const json_t *payload) {
    json_t *args = json_object();
    json_set(args, "payload", ctd_copy_or_null(payload));
    ctd_record(io, evname, args, json_null());
    json_t *q = ctd_st(io, qkey);
    if (q && q->type == JSON_ARRAY) json_append(q, ctd_copy_or_null(payload));
}
static void ctd_sudo_queue_put(ctd_io_t *io, const char *text) {
    json_t *args = json_object();
    json_set(args, "text", json_string(text));
    ctd_record(io, "sudo_response_queue.put", args, json_null());
    json_t *sudo = ctd_st(io, "_sudo_state");
    if (sudo && sudo->type == JSON_OBJECT) {
        json_t *q = json_obj_get(sudo, "response_queue");
        if (q && q->type == JSON_ARRAY) json_append(q, json_string(text));
    }
}

/* ── i18n / print helpers (seamed) ─────────────────────────────────────── */
static json_t *ctd_t(ctd_io_t *io, const char *key, json_t *kwargs) {
    json_t *args = kwargs ? kwargs : json_object();
    json_set(args, "key", json_string(key));
    return ctd_seam(io, "t", args);
}
/* module leaf _tn: <key>_one for exactly one, <key>_other otherwise */
static json_t *ctd_tn(ctd_io_t *io, const char *key, long long count) {
    char kbuf[256];
    snprintf(kbuf, sizeof kbuf, "%s_%s", key, count == 1 ? "one" : "other");
    json_t *kw = json_object();
    json_set(kw, "count", json_number((double)count));
    return ctd_t(io, kbuf, kw);
}
static json_t *ctd_cprint(ctd_io_t *io, const char *msg) {
    json_t *args = json_object();
    json_set(args, "text", json_string(msg));
    return ctd_seam(io, "_cprint", args);
}
/* print("\n" + t(key)) — t runs BEFORE print (trace order) */
static json_t *ctd_print_line(ctd_io_t *io, const char *key) {
    json_t *tv = ctd_t(io, key, NULL);
    if (ctd_raised(tv)) return tv;
    char *ts = ctd_node_str(tv);
    json_free(tv);
    ctd_sb b; ctd_sb_init(&b);
    ctd_sb_puts(&b, "\n");
    ctd_sb_puts(&b, ts);
    free(ts);
    char *msg = ctd_sb_take(&b);
    json_t *args = json_object();
    json_set(args, "text", json_string(msg));
    free(msg);
    return ctd_seam(io, "print", args);
}
/* text[:80] + ("..." if len(text) > 80 else "") — code point slice */
static char *ctd_preview80(const char *text) {
    char *pre = ctd_utf8_prefix(text, 80);
    if (ctd_utf8_len(text) > 80) {
        size_t n = strlen(pre);
        char *out = (char *)malloc(n + 4);
        memcpy(out, pre, n);
        memcpy(out + n, "...", 4);
        free(pre);
        return out;
    }
    return pre;
}
/* Path(home) / "config.yaml" -> posix string */
static char *ctd_path_join(const char *home, const char *child) {
    if (!home) home = "";
    size_t hl = strlen(home);
    while (hl > 0 && home[hl - 1] == '/') hl--;
    ctd_sb b; ctd_sb_init(&b);
    if (hl > 0) {
        char *h = (char *)malloc(hl + 1);
        memcpy(h, home, hl); h[hl] = '\0';
        ctd_sb_puts(&b, h);
        free(h);
        ctd_sb_puts(&b, "/");
    }
    ctd_sb_puts(&b, child);
    return ctd_sb_take(&b);
}
/* _tui_set_filter (real staticmethod in the oracle; inline helper here) */
static void ctd_set_filter(json_t *st, const char *value) {
    json_set(st, "filter", json_string(value));
    json_set(st, "selected", json_number(0));
    json_set(st, "_scroll_offset", json_number(0));
}

/* ── Enter routing chain ───────────────────────────────────────────────── */

/* PoP: ctd_handle_enter_flow @ hermes_cli/cli_tui_mixin.py:_tui_handle_enter */
json_t *ctd_handle_enter_flow(ctd_io_t *io) {
    /* from cli import _apply_backslash_line_continuation, _is_backslash_line_continuation */
    json_t *imp = ctd_import_check(io, "import:cli");
    if (imp) return imp;
    json_t *r = ctd_seam(io, "self._tui_enter_overlay", json_object());
    if (ctd_raised(r)) return r;
    bool overlay = ctd_truthy(r);
    json_free(r);
    if (overlay) return json_null();
    char *raw_text = strdup(ctd_buf_text(io));
    json_t *buf = ctd_buf(io);
    double cursor = buf ? json_get_num(buf, "cursor_position", 0) : 0;
    bool shortcuts = ctd_truthy(ctd_st(io, "_tui_multiline_shortcuts"));
    if (shortcuts && cursor == (double)ctd_utf8_len(raw_text)) {
        json_t *args = json_object();
        json_set(args, "text", json_string(raw_text));
        json_t *iscont = ctd_seam(io, "_is_backslash_line_continuation", args);
        if (ctd_raised(iscont)) { free(raw_text); return iscont; }
        bool cont = ctd_truthy(iscont);
        json_free(iscont);
        if (cont) {
            json_t *a2 = json_object();
            json_set(a2, "text", json_string(raw_text));
            json_t *continued = ctd_seam(io, "_apply_backslash_line_continuation", a2);
            if (ctd_raised(continued)) { free(raw_text); return continued; }
            char *cs = ctd_node_str(continued);
            json_free(continued);
            buf = ctd_buf(io);
            if (buf) {
                json_set(buf, "text", json_string(cs));
                json_set(buf, "cursor_position", json_number((double)ctd_utf8_len(cs)));
            }
            free(cs);
            free(raw_text);
            ctd_app_invalidate(io);
            return json_null();
        }
    }
    free(raw_text);
    /* from hermes_cli.cli_enter_burst import handle_burst_enter */
    imp = ctd_import_check(io, "import:hermes_cli.cli_enter_burst");
    if (imp) return imp;
    json_t *args = json_object();
    json_set(args, "text", json_string(ctd_buf_text(io)));
    r = ctd_seam(io, "handle_burst_enter", args);
    if (ctd_raised(r)) return r;
    bool burst = ctd_truthy(r);
    json_free(r);
    if (!burst) {
        json_t *a2 = json_object();
        json_set(a2, "text", json_string(ctd_buf_text(io)));
        r = ctd_seam(io, "self._tui_submit_buffer", a2);
        if (ctd_raised(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* PoP: ctd_submit_buffer_flow @ hermes_cli/cli_tui_mixin.py:_tui_submit_buffer */
json_t *ctd_submit_buffer_flow(ctd_io_t *io) {
    /* from cli import _looks_like_slash_command */
    json_t *imp = ctd_import_check(io, "import:cli");
    if (imp) return imp;
    char *text = ctd_strip(ctd_buf_text(io));
    json_t *attached = ctd_st(io, "_attached_images");
    bool has_images = ctd_truthy(attached);
    if (!(text[0] != '\0' || has_images)) { free(text); return json_null(); }
    json_t *args = json_object();
    json_set(args, "text", json_string(text));
    json_set(args, "has_images", json_bool(has_images));
    json_t *r = ctd_seam(io, "self._tui_enter_inline_command", args);
    if (ctd_raised(r)) { free(text); return r; }
    bool inline_handled = ctd_truthy(r);
    json_free(r);
    if (inline_handled) { free(text); return json_null(); }
    /* snapshot + clear attached images; bundle (text, images) when present */
    json_t *images = attached ? json_copy(attached) : json_array();
    if (attached) ctd_array_clear(attached);
    ctd_app_invalidate(io);
    json_t *payload;
    if (ctd_truthy(images)) {
        payload = json_array();
        json_append(payload, json_string(text));
        json_append(payload, json_copy(images));
    } else {
        payload = json_string(text);
    }
    bool local = false;
    if (text[0] != '\0') {
        json_t *a2 = json_object();
        json_set(a2, "text", json_string(text));
        r = ctd_seam(io, "_looks_like_slash_command", a2);
        if (ctd_raised(r)) { free(text); json_free(images); json_free(payload); return r; }
        local = ctd_truthy(r);
        json_free(r);
        if (!local) local = strncmp(text, "!", 1) == 0;
    }
    if (ctd_truthy(ctd_st(io, "_agent_running")) && !local) {
        json_t *a3 = json_object();
        json_set(a3, "text", json_string(text));
        json_set(a3, "images", json_copy(images));
        json_set(a3, "payload", json_copy(payload));
        r = ctd_seam(io, "self._tui_enter_while_busy", a3);
        if (ctd_raised(r)) { free(text); json_free(images); json_free(payload); return r; }
        json_free(r);
    } else {
        ctd_queue_put(io, "_pending_input", "pending_input.put", payload);
    }
    json_t *a4 = json_object();
    json_set(a4, "text", json_string(ctd_buf_text(io)));
    r = ctd_seam(io, "self._inline_pastes", a4);
    if (ctd_raised(r)) { free(text); json_free(images); json_free(payload); return r; }
    json_free(r);
    ctd_buf_reset(io, true);
    free(text);
    json_free(images);
    json_free(payload);
    return json_null();
}

/* PoP: ctd_enter_inline_command_flow @ hermes_cli/cli_tui_mixin.py:_tui_enter_inline_command */
json_t *ctd_enter_inline_command_flow(ctd_io_t *io, const char *text, bool has_images) {
    json_t *args = json_object();
    json_set(args, "text", json_string(text));
    json_set(args, "has_images", json_bool(has_images));
    json_t *r = ctd_seam(io, "self._should_handle_model_command_inline", args);
    if (ctd_raised(r)) return r;
    bool model = ctd_truthy(r);
    json_free(r);
    bool handled = false;
    if (model) {
        json_t *a2 = json_object();
        json_set(a2, "text", json_string(text));
        r = ctd_seam(io, "self.process_command", a2);
        if (ctd_raised(r)) return r;
        bool ok = ctd_truthy(r);
        json_free(r);
        if (!ok) {
            ctd_st_set(io, "_should_exit", json_bool(true));
            if (ctd_truthy(ctd_st(io, "app_is_running"))) ctd_app_exit(io);
        }
        handled = true;
    } else {
        /* short-circuit or-chain: steer || background || stop */
        const char *names[3] = {
            "self._should_handle_steer_command_inline",
            "self._should_handle_background_command_inline",
            "self._should_handle_stop_command_inline"
        };
        for (int i = 0; i < 3 && !handled; i++) {
            json_t *a2 = json_object();
            json_set(a2, "text", json_string(text));
            json_set(a2, "has_images", json_bool(has_images));
            r = ctd_seam(io, names[i], a2);
            if (ctd_raised(r)) return r;
            handled = ctd_truthy(r);
            json_free(r);
        }
        if (handled) {
            json_t *a3 = json_object();
            json_set(a3, "text", json_string(text));
            r = ctd_seam(io, "self.process_command", a3);
            if (ctd_raised(r)) return r;
            json_free(r);
        }
    }
    if (!handled) return json_bool(false);
    ctd_buf_reset(io, true);
    ctd_app_invalidate(io);
    return json_bool(true);
}

/* PoP: ctd_enter_while_busy_flow @ hermes_cli/cli_tui_mixin.py:_tui_enter_while_busy */
json_t *ctd_enter_while_busy_flow(ctd_io_t *io, const char *text,
                                  const json_t *images, const json_t *payload,
                                  const char *mode) {
    /* from cli import CLI_CONFIG, _ACCENT, _DIM, _RST, _cprint, _hermes_home */
    json_t *imp = ctd_import_check(io, "import:cli");
    if (imp) return imp;
    if (!text) text = "";
    const char *accent = ctd_st_str(io, "_accent", "\x1b[1;38;2;255;215;0m");
    char eff_buf[64];
    snprintf(eff_buf, sizeof eff_buf, "%s",
             (mode && mode[0] != '\0') ? mode : ctd_st_str(io, "busy_input_mode", "queue"));
    const char *eff = eff_buf;
    bool redirected_bool = false;
    json_t *r;
    if (strcmp(eff, "steer") == 0) {
        if (ctd_truthy(images) || text[0] == '\0') {
            eff = "queue";
        } else {
            json_t *accepted = json_bool(false);
            json_t *agent = ctd_st(io, "agent");
            if (agent && agent->type != JSON_NULL &&
                json_is_true(json_obj_get(agent, "has_steer"))) {
                json_t *args = json_object();
                json_set(args, "text", json_string(text));
                json_free(accepted);
                accepted = ctd_seam(io, "agent.steer", args);
                if (ctd_raised(accepted)) {
                    /* except Exception as exc: _cprint(...error=exc...); accepted = False */
                    char *es = ctd_exc_str(ctd_raise_spec(accepted));
                    json_free(accepted);
                    json_t *kw = json_object();
                    json_set(kw, "error", json_string(es));
                    free(es);
                    json_t *tv = ctd_t(io, "cli.tui.steer_failed_queued", kw);
                    if (ctd_raised(tv)) return tv;
                    char *ts = ctd_node_str(tv);
                    json_free(tv);
                    ctd_sb b; ctd_sb_init(&b);
                    ctd_sb_puts(&b, "  ");
                    ctd_sb_puts(&b, CTD_DIM);
                    ctd_sb_puts(&b, ts);
                    ctd_sb_puts(&b, CTD_RST);
                    free(ts);
                    char *msg = ctd_sb_take(&b);
                    r = ctd_cprint(io, msg);
                    free(msg);
                    if (ctd_raised(r)) return r;
                    json_free(r);
                    accepted = json_bool(false);
                }
            }
            if (accepted && accepted->type == JSON_STRING &&
                strcmp(accepted->str_val, "voice_stopped") == 0) {
                json_free(accepted);
                return json_null(); /* TUI-26: consumed as a voice stop */
            }
            if (ctd_truthy(accepted)) {
                char *preview = ctd_preview80(text);
                json_t *kw = json_object();
                json_set(kw, "preview", json_string(preview));
                free(preview);
                json_t *tv = ctd_t(io, "cli.tui.steered", kw);
                if (ctd_raised(tv)) { json_free(accepted); return tv; }
                char *ts = ctd_node_str(tv);
                json_free(tv);
                ctd_sb b; ctd_sb_init(&b);
                ctd_sb_puts(&b, "  ");
                ctd_sb_puts(&b, accent);
                ctd_sb_puts(&b, ts);
                ctd_sb_puts(&b, CTD_RST);
                free(ts);
                char *msg = ctd_sb_take(&b);
                r = ctd_cprint(io, msg);
                free(msg);
                if (ctd_raised(r)) { json_free(accepted); return r; }
                json_free(r);
            } else {
                eff = "queue";
            }
            json_free(accepted);
        }
    }
    if (strcmp(eff, "queue") == 0) {
        bool enqueued = false;
        if (text[0] != '\0' && !ctd_truthy(images)) {
            r = ctd_seam(io, "self._gateway_queue_store", json_object());
            if (ctd_raised(r)) return r;
            bool has_store = r && r->type != JSON_NULL;
            json_free(r);
            if (has_store) {
                json_t *args = json_object();
                json_set(args, "text", json_string(text));
                r = ctd_seam(io, "self._queue_enqueue", args);
                if (ctd_raised(r)) return r;
                json_free(r);
                enqueued = true;
            }
        }
        if (!enqueued) {
            ctd_queue_put(io, "_pending_input", "pending_input.put", payload);
            char *preview;
            if (text[0] != '\0') {
                preview = strdup(text);
            } else {
                json_t *tv = ctd_tn(io, "cli.tui.images_attached_preview",
                                    (long long)(images ? json_len(images) : 0));
                if (ctd_raised(tv)) return tv;
                preview = ctd_node_str(tv);
                json_free(tv);
            }
            char *p80 = ctd_preview80(preview);
            free(preview);
            json_t *kw = json_object();
            json_set(kw, "preview", json_string(p80));
            free(p80);
            json_t *tv = ctd_t(io, "cli.tui.queued_next_turn", kw);
            if (ctd_raised(tv)) return tv;
            char *ts = ctd_node_str(tv);
            json_free(tv);
            ctd_sb b; ctd_sb_init(&b);
            ctd_sb_puts(&b, "  ");
            ctd_sb_puts(&b, ts);
            free(ts);
            char *msg = ctd_sb_take(&b);
            r = ctd_cprint(io, msg);
            free(msg);
            if (ctd_raised(r)) return r;
            json_free(r);
        }
    } else if (strcmp(eff, "interrupt") == 0) {
        json_t *redirected = json_bool(false);
        if (!ctd_truthy(images) && text[0] != '\0') {
            json_t *agent = ctd_st(io, "agent");
            if (agent && agent->type != JSON_NULL &&
                json_is_true(json_obj_get(agent, "_supports_active_turn_redirect")) &&
                json_is_true(json_obj_get(agent, "has_redirect"))) {
                json_t *args = json_object();
                json_set(args, "text", json_string(text));
                json_free(redirected);
                redirected = ctd_seam(io, "agent.redirect", args);
                if (ctd_raised(redirected)) {
                    json_free(redirected);
                    redirected = json_bool(false);
                }
            }
        }
        if (redirected && redirected->type == JSON_STRING &&
            strcmp(redirected->str_val, "voice_stopped") == 0) {
            json_free(redirected);
            return json_null(); /* TUI-26 */
        }
        redirected_bool = ctd_truthy(redirected);
        if (redirected_bool) {
            char *preview = ctd_preview80(text);
            json_t *kw = json_object();
            json_set(kw, "preview", json_string(preview));
            free(preview);
            json_t *tv = ctd_t(io, "cli.tui.redirected_turn", kw);
            if (ctd_raised(tv)) { json_free(redirected); return tv; }
            char *ts = ctd_node_str(tv);
            json_free(tv);
            ctd_sb b; ctd_sb_init(&b);
            ctd_sb_puts(&b, "  ");
            ctd_sb_puts(&b, accent);
            ctd_sb_puts(&b, ts);
            ctd_sb_puts(&b, CTD_RST);
            free(ts);
            char *msg = ctd_sb_take(&b);
            r = ctd_cprint(io, msg);
            free(msg);
            if (ctd_raised(r)) { json_free(redirected); return r; }
            json_free(r);
        } else {
            ctd_queue_put(io, "_interrupt_queue", "interrupt_queue.put", payload);
        }
        json_free(redirected);
    }
    /* First-touch onboarding (guarded: can never break the input loop).
     * from agent.onboarding import BUSY_INPUT_FLAG, busy_input_hint_cli, is_seen, mark_seen */
    imp = ctd_import_check(io, "import:agent.onboarding");
    if (imp) { json_free(imp); return json_null(); } /* except Exception: pass */
    json_t *cfg = ctd_st(io, "CLI_CONFIG");
    json_t *args = json_object();
    json_set(args, "config", ctd_copy_or_null(cfg));
    json_set(args, "flag", json_string(CTD_BUSY_FLAG));
    json_t *seen = ctd_seam(io, "is_seen", args);
    if (ctd_raised(seen)) { json_free(seen); return json_null(); }
    bool not_seen = !ctd_truthy(seen);
    json_free(seen);
    if (not_seen) {
        const char *hint_mode = redirected_bool ? "redirect" : eff;
        json_t *a2 = json_object();
        json_set(a2, "mode", json_string(hint_mode));
        json_t *hv = ctd_seam(io, "busy_input_hint_cli", a2);
        if (ctd_raised(hv)) { json_free(hv); return json_null(); }
        char *hs = ctd_node_str(hv);
        json_free(hv);
        ctd_sb b; ctd_sb_init(&b);
        ctd_sb_puts(&b, "  ");
        ctd_sb_puts(&b, CTD_DIM);
        ctd_sb_puts(&b, hs);
        ctd_sb_puts(&b, CTD_RST);
        free(hs);
        char *msg = ctd_sb_take(&b);
        r = ctd_cprint(io, msg);
        free(msg);
        if (ctd_raised(r)) { json_free(r); return json_null(); }
        json_free(r);
        char *path = ctd_path_join(ctd_st_str(io, "_hermes_home", ""), "config.yaml");
        json_t *a3 = json_object();
        json_set(a3, "path", json_string(path));
        json_set(a3, "flag", json_string(CTD_BUSY_FLAG));
        free(path);
        r = ctd_seam(io, "mark_seen", a3);
        if (ctd_raised(r)) { json_free(r); return json_null(); }
        json_free(r);
        /* CLI_CONFIG.setdefault("onboarding", {}).setdefault("seen", {})[FLAG] = True */
        if (cfg && cfg->type == JSON_OBJECT) {
            json_t *ob = json_obj_get(cfg, "onboarding");
            if (!ob) { ob = json_object(); json_set(cfg, "onboarding", ob); }
            if (ob->type != JSON_OBJECT) return json_null(); /* AttributeError swallowed */
            json_t *sn = json_obj_get(ob, "seen");
            if (!sn) { sn = json_object(); json_set(ob, "seen", sn); }
            if (sn->type != JSON_OBJECT) return json_null();
            json_set(sn, CTD_BUSY_FLAG, json_bool(true));
        }
    }
    return json_null();
}

/* ── modal overlay Enter priority chain ────────────────────────────────── */

/* PoP: ctd_enter_overlay_flow @ hermes_cli/cli_tui_mixin.py:_tui_enter_overlay */
json_t *ctd_enter_overlay_flow(ctd_io_t *io) {
    /* from cli import _cprint */
    json_t *imp = ctd_import_check(io, "import:cli");
    if (imp) return imp;
    json_t *r;
    json_t *conn = ctd_st(io, "_connection_state");
    if (ctd_truthy(conn)) {
        json_t *fields = json_obj_get(conn, "fields");
        size_t nf = (fields && fields->type == JSON_ARRAY) ? json_len(fields) : 0;
        const char *phase = json_get_str(conn, "phase", "");
        double fidx = json_get_num(conn, "field_index", 0);
        if ((strcmp(phase, "form") == 0 || strcmp(phase, "failed") == 0) &&
            fidx < (double)nf) {
            char *value = strdup(ctd_buf_text(io));
            ctd_buf_reset(io, false);
            json_t *args = json_object();
            json_set(args, "value", json_string(value));
            free(value);
            r = ctd_seam(io, "self._connection_set_field", args);
            if (ctd_raised(r)) return r;
            json_free(r);
        } else {
            r = ctd_seam(io, "self._connection_submit", json_object());
            if (ctd_raised(r)) return r;
            json_free(r);
        }
        ctd_app_invalidate(io);
        return json_bool(true);
    }
    json_t *sudo = ctd_st(io, "_sudo_state");
    if (ctd_truthy(sudo)) {
        ctd_sudo_queue_put(io, ctd_buf_text(io));
        ctd_st_set(io, "_sudo_state", json_null());
        ctd_app_invalidate(io);
        return json_bool(true);
    }
    if (ctd_truthy(ctd_st(io, "_secret_state"))) {
        char *value = strdup(ctd_buf_text(io));
        ctd_buf_reset(io, false);
        json_t *args = json_object();
        json_set(args, "value", json_string(value));
        free(value);
        r = ctd_seam(io, "self._submit_secret_response", args);
        if (ctd_raised(r)) return r;
        json_free(r);
        ctd_app_invalidate(io);
        return json_bool(true);
    }
    if (ctd_truthy(ctd_st(io, "_approval_state"))) {
        r = ctd_seam(io, "self._handle_approval_selection", json_object());
        if (ctd_raised(r)) return r;
        json_free(r);
        ctd_app_invalidate(io);
        return json_bool(true);
    }
    json_t *sc = ctd_st(io, "_slash_confirm_state");
    if (ctd_truthy(sc)) {
        /* typed choice wins over the highlighted one */
        char *text = ctd_strip(ctd_buf_text(io));
        json_t *choices = json_obj_get(sc, "choices");
        if (!choices || choices->type != JSON_ARRAY) choices = NULL;
        size_t nch = choices ? json_len(choices) : 0;
        json_t *choice = NULL;
        if (text[0] != '\0') {
            json_t *args = json_object();
            json_set(args, "text", json_string(text));
            json_set(args, "choices", choices ? json_copy(choices) : json_array());
            choice = ctd_seam(io, "self._normalize_slash_confirm_choice", args);
            if (ctd_raised(choice)) { free(text); return choice; }
        }
        if (!choice || choice->type == JSON_NULL) {
            double sel = json_get_num(sc, "selected", 0);
            if (sel >= 0 && sel < (double)nch) {
                json_t *row = json_get(choices, (size_t)sel);
                json_t *v0 = (row && row->type == JSON_ARRAY) ? json_get(row, 0) : NULL;
                json_free(choice);
                choice = ctd_copy_or_null(v0);
            }
        }
        char *cs = ctd_truthy(choice) ? ctd_node_str(choice) : strdup("cancel");
        json_t *args = json_object();
        json_set(args, "choice", json_string(cs));
        free(cs);
        r = ctd_seam(io, "self._submit_slash_confirm_response", args);
        if (ctd_raised(r)) { free(text); json_free(choice); return r; }
        json_free(r);
        json_free(choice);
        free(text);
        ctd_buf_reset(io, false);
        ctd_app_invalidate(io);
        return json_bool(true);
    }
    if (ctd_truthy(ctd_st(io, "_model_picker_state"))) {
        /* try: import resolve_persist_behavior; handle selection; except: report+close */
        imp = ctd_import_check(io, "import:hermes_cli.model_switch");
        r = NULL;
        if (imp) {
            r = imp;
        } else {
            json_t *args = json_object();
            json_set(args, "arg0", json_bool(false));
            json_set(args, "arg1", json_bool(false));
            json_t *persist = ctd_seam(io, "resolve_persist_behavior", args);
            if (ctd_raised(persist)) {
                r = persist;
            } else {
                json_t *a2 = json_object();
                json_set(a2, "persist_global", persist ? persist : json_null());
                r = ctd_seam(io, "self._handle_model_picker_selection", a2);
                if (!ctd_raised(r)) { json_free(r); r = NULL; }
            }
        }
        if (r && ctd_raised(r)) {
            char *es = ctd_exc_str(ctd_raise_spec(r));
            json_t *kw = json_object();
            json_set(kw, "error", json_string(es));
            free(es);
            json_t *tv = ctd_t(io, "cli.tui.model_selection_failed", kw);
            if (ctd_raised(tv)) { json_free(r); return tv; }
            char *ts = ctd_node_str(tv);
            json_free(tv);
            ctd_sb b; ctd_sb_init(&b);
            ctd_sb_puts(&b, "  ");
            ctd_sb_puts(&b, ts);
            free(ts);
            char *msg = ctd_sb_take(&b);
            json_t *cr = ctd_cprint(io, msg);
            free(msg);
            if (ctd_raised(cr)) { json_free(r); return cr; }
            json_free(cr);
            json_t *cl = ctd_seam(io, "self._close_model_picker", json_object());
            if (ctd_raised(cl)) { json_free(r); return cl; }
            json_free(cl);
            json_free(r);
        }
        ctd_buf_reset(io, false);
        ctd_app_invalidate(io);
        return json_bool(true);
    }
    if (ctd_truthy(ctd_st(io, "_clarify_state")) &&
        ctd_truthy(ctd_st(io, "_clarify_freetext"))) {
        r = ctd_seam(io, "self._tui_enter_clarify_freetext", json_object());
        if (ctd_raised(r)) return r;
        json_free(r);
        return json_bool(true);
    }
    if (ctd_truthy(ctd_st(io, "_clarify_state"))) {
        r = ctd_seam(io, "self._tui_enter_clarify_choice", json_object());
        if (ctd_raised(r)) return r;
        json_free(r);
        return json_bool(true);
    }
    return json_bool(false);
}

/* PoP: ctd_enter_clarify_freetext_flow @ hermes_cli/cli_tui_mixin.py:_tui_enter_clarify_freetext */
json_t *ctd_enter_clarify_freetext_flow(ctd_io_t *io) {
    char *text = ctd_strip(ctd_buf_text(io));
    json_t *state = ctd_st(io, "_clarify_state");
    json_t *base = ctd_st(io, "_clarify_multi_base");
    json_t *answer = NULL, *meta = json_object(), *r;
    if (text[0] == '\0') {
        ctd_st_set(io, "_clarify_freetext", json_bool(false));
        ctd_st_set(io, "_clarify_prefill", json_string(""));
        ctd_st_set(io, "_clarify_multi_base", json_null());
        if (ctd_truthy(base)) {
            char *dumps = ctd_py_dumps(base);
            answer = json_string(dumps);
            free(dumps);
            json_set(meta, "kind", json_string("multi"));
            json_set(meta, "choices", json_copy(base));
            json_set(meta, "other_text", json_string(""));
        } else {
            answer = json_null();
            json_set(meta, "kind", json_string("skipped"));
        }
        json_t *args = json_object();
        json_set(args, "state", ctd_copy_or_null(state));
        json_set(args, "answer", answer);
        json_set(args, "meta", meta);
        r = ctd_seam(io, "self._clarify_batch_lock", args);
        if (ctd_raised(r)) { free(text); return r; }
        json_free(r);
        ctd_buf_reset(io, false);
        ctd_app_invalidate(io);
        free(text);
        return json_null();
    }
    /* multi-select "Other" appends the typed answer as a JSON array string */
    if (base && base->type != JSON_NULL) {
        json_t *arr = json_copy(base);
        json_append(arr, json_string(text));
        char *dumps = ctd_py_dumps(arr);
        json_free(arr);
        answer = json_string(dumps);
        free(dumps);
        json_set(meta, "kind", json_string("multi"));
        json_set(meta, "choices", json_copy(base));
        json_set(meta, "other_text", json_string(text));
        ctd_st_set(io, "_clarify_multi_base", json_null());
    } else {
        answer = json_string(text);
        json_set(meta, "kind", json_string("other"));
        json_set(meta, "other_text", json_string(text));
    }
    ctd_st_set(io, "_clarify_freetext", json_bool(false));
    ctd_st_set(io, "_clarify_prefill", json_string(""));
    json_t *args = json_object();
    json_set(args, "state", ctd_copy_or_null(state));
    json_set(args, "answer", answer);
    json_set(args, "meta", meta);
    r = ctd_seam(io, "self._clarify_batch_lock", args);
    if (ctd_raised(r)) { free(text); return r; }
    json_free(r);
    ctd_buf_reset(io, false);
    ctd_app_invalidate(io);
    free(text);
    return json_null();
}

/* PoP: ctd_enter_clarify_choice_flow @ hermes_cli/cli_tui_mixin.py:_tui_enter_clarify_choice */
json_t *ctd_enter_clarify_choice_flow(ctd_io_t *io) {
    json_t *args = json_object();
    json_set(args, "state", ctd_copy_or_null(ctd_st(io, "_clarify_state")));
    json_t *r = ctd_seam(io, "self._clarify_batch_enter", args);
    if (ctd_raised(r)) return r;
    json_free(r);
    /* editing an earlier "Other" answer: prefill the composer */
    const char *prefill = ctd_st_str(io, "_clarify_prefill", "");
    if (ctd_truthy(ctd_st(io, "_clarify_freetext")) && prefill[0] != '\0') {
        json_t *buf = ctd_buf(io);
        if (buf) {
            json_set(buf, "text", json_string(prefill));
            json_set(buf, "cursor_position", json_number((double)ctd_utf8_len(prefill)));
        }
        ctd_st_set(io, "_clarify_prefill", json_string(""));
    }
    ctd_app_invalidate(io);
    return json_null();
}

/* ── Ctrl+C / Ctrl+Q priority chains ───────────────────────────────────── */

/* PoP: ctd_handle_ctrl_c_flow @ hermes_cli/cli_tui_mixin.py:_tui_handle_ctrl_c */
json_t *ctd_handle_ctrl_c_flow(ctd_io_t *io) {
    json_t *now = ctd_seam(io, "time.time", json_object());
    if (ctd_raised(now)) return now;
    double now_v = (now && now->type == JSON_NUMBER) ? now->num_val : 0;
    json_free(now);
    json_t *r = ctd_seam(io, "self._tui_cancel_voice_recording", json_object());
    if (ctd_raised(r)) return r;
    bool cancelled = ctd_truthy(r);
    json_free(r);
    if (cancelled) return json_null();
    json_t *closers = json_array();
    json_append(closers, json_string("_slash_confirm_state"));
    json_append(closers, json_string("_model_picker_state"));
    json_append(closers, json_string("_command_palette_state"));
    json_append(closers, json_string("_gateway_command_wait"));
    json_t *args = json_object();
    json_set(args, "closers", closers);
    r = ctd_seam(io, "self._tui_cancel_foreground_ui", args);
    if (ctd_raised(r)) return r;
    cancelled = ctd_truthy(r);
    json_free(r);
    if (cancelled) return json_null();
    r = ctd_seam(io, "self._tui_clear_blocking_overlays", json_object());
    if (ctd_raised(r)) return r;
    bool overlay_cleared = ctd_truthy(r);
    json_free(r);
    bool agent_busy = ctd_truthy(ctd_st(io, "_agent_running")) &&
                      ctd_truthy(ctd_st(io, "agent"));
    if (overlay_cleared && !agent_busy) return json_null();
    bool has_content = ctd_buf_text(io)[0] != '\0' ||
                       ctd_truthy(ctd_st(io, "_attached_images"));
    if (agent_busy && !has_content) {
        double last = ctd_st_num(io, "_last_ctrl_c_time", 0);
        if (now_v - last < 2.0) {
            r = ctd_print_line(io, "cli.tui.force_exiting");
            if (ctd_raised(r)) return r;
            json_free(r);
            ctd_st_set(io, "_should_exit", json_bool(true));
            ctd_app_exit(io);
            return json_null();
        }
        ctd_st_set(io, "_last_ctrl_c_time", json_number(now_v));
        r = ctd_print_line(io, "cli.tui.interrupting_again_hint");
        if (ctd_raised(r)) return r;
        json_free(r);
        json_t *a2 = json_object();
        json_set(a2, "agent", ctd_copy_or_null(ctd_st(io, "agent")));
        r = ctd_seam(io, "request_hard_interrupt", a2);
        if (ctd_raised(r)) return r;
        json_free(r);
    } else {
        r = ctd_seam(io, "self._tui_clear_or_exit", json_object());
        if (ctd_raised(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* PoP: ctd_handle_ctrl_q_flow @ hermes_cli/cli_tui_mixin.py:_tui_handle_ctrl_q */
json_t *ctd_handle_ctrl_q_flow(ctd_io_t *io) {
    json_t *r = ctd_seam(io, "self._tui_cancel_voice_recording", json_object());
    if (ctd_raised(r)) return r;
    bool cancelled = ctd_truthy(r);
    json_free(r);
    if (cancelled) return json_null();
    json_t *closers = json_array();
    json_append(closers, json_string("_slash_confirm_state"));
    json_append(closers, json_string("_model_picker_state"));
    json_t *args = json_object();
    json_set(args, "closers", closers);
    r = ctd_seam(io, "self._tui_cancel_foreground_ui", args);
    if (ctd_raised(r)) return r;
    cancelled = ctd_truthy(r);
    json_free(r);
    if (cancelled) return json_null();
    r = ctd_seam(io, "self._tui_clear_blocking_overlays", json_object());
    if (ctd_raised(r)) return r;
    bool overlay_cleared = ctd_truthy(r);
    json_free(r);
    bool agent_busy = ctd_truthy(ctd_st(io, "_agent_running")) &&
                      ctd_truthy(ctd_st(io, "agent"));
    if (overlay_cleared && !agent_busy) return json_null();
    bool has_content = ctd_buf_text(io)[0] != '\0' ||
                       ctd_truthy(ctd_st(io, "_attached_images"));
    if (agent_busy && !has_content) {
        r = ctd_print_line(io, "cli.tui.interrupting");
        if (ctd_raised(r)) return r;
        json_free(r);
        json_t *a2 = json_object();
        json_set(a2, "agent", ctd_copy_or_null(ctd_st(io, "agent")));
        r = ctd_seam(io, "request_hard_interrupt", a2);
        if (ctd_raised(r)) return r;
        json_free(r);
    } else {
        r = ctd_seam(io, "self._tui_clear_or_exit", json_object());
        if (ctd_raised(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* PoP: ctd_handle_escape_modal_flow @ hermes_cli/cli_tui_mixin.py:_tui_handle_escape_modal */
json_t *ctd_handle_escape_modal_flow(ctd_io_t *io) {
    json_t *r;
    if (ctd_truthy(ctd_st(io, "_connection_state"))) {
        r = ctd_seam(io, "self._connection_cancel", json_object());
        if (ctd_raised(r)) return r;
        json_free(r);
        ctd_buf_reset(io, false);
        ctd_app_invalidate(io);
    } else if (ctd_truthy(ctd_st(io, "_secret_state"))) {
        r = ctd_seam(io, "self._cancel_secret_capture", json_object());
        if (ctd_raised(r)) return r;
        json_free(r);
        ctd_buf_reset(io, false);
        ctd_app_invalidate(io);
    } else if (ctd_truthy(ctd_st(io, "_sudo_state"))) {
        ctd_sudo_queue_put(io, "");
        ctd_st_set(io, "_sudo_state", json_null());
        ctd_app_invalidate(io);
    } else if (ctd_truthy(ctd_st(io, "_slash_confirm_state"))) {
        json_t *args = json_object();
        json_set(args, "choice", json_string("cancel"));
        r = ctd_seam(io, "self._submit_slash_confirm_response", args);
        if (ctd_raised(r)) return r;
        json_free(r);
        ctd_buf_reset(io, false);
        ctd_app_invalidate(io);
    }
    return json_null();
}

/* PoP: ctd_handle_ctrl_d_flow @ hermes_cli/cli_tui_mixin.py:_tui_handle_ctrl_d */
json_t *ctd_handle_ctrl_d_flow(ctd_io_t *io) {
    if (ctd_buf_text(io)[0] != '\0') {
        ctd_buf_delete(io);
    } else if (!ctd_truthy(ctd_st(io, "_attached_images"))) {
        ctd_st_set(io, "_should_exit", json_bool(true));
        ctd_st_set(io, "_detach_on_exit",
                   json_bool(ctd_truthy(ctd_st(io, "_agent_running"))));
        ctd_app_exit(io);
    }
    return json_null();
}

/* ── clear / cancel leaves of the Ctrl+C chain ─────────────────────────── */

/* PoP: ctd_clear_or_exit_flow @ hermes_cli/cli_tui_mixin.py:_tui_clear_or_exit */
json_t *ctd_clear_or_exit_flow(ctd_io_t *io) {
    json_t *attached = ctd_st(io, "_attached_images");
    if (ctd_buf_text(io)[0] != '\0' || ctd_truthy(attached)) {
        ctd_buf_reset(io, false);
        if (attached) ctd_array_clear(attached);
        ctd_app_invalidate(io);
    } else {
        ctd_st_set(io, "_should_exit", json_bool(true));
        ctd_app_exit(io);
    }
    return json_null();
}

/* PoP: ctd_clear_blocking_overlays_flow @ hermes_cli/cli_tui_mixin.py:_tui_clear_blocking_overlays */
json_t *ctd_clear_blocking_overlays_flow(ctd_io_t *io) {
    if (!(ctd_truthy(ctd_st(io, "_sudo_state")) ||
          ctd_truthy(ctd_st(io, "_secret_state")) ||
          ctd_truthy(ctd_st(io, "_approval_state")) ||
          ctd_truthy(ctd_st(io, "_clarify_state")) ||
          ctd_truthy(ctd_st(io, "_connection_state")))) {
        return json_bool(false);
    }
    json_t *r = ctd_seam(io, "self._clear_active_overlays_for_interrupt", json_object());
    if (ctd_raised(r)) return r;
    json_free(r);
    ctd_buf_reset(io, false);
    ctd_app_invalidate(io);
    return json_bool(true);
}

/* PoP: ctd_cancel_foreground_ui_flow @ hermes_cli/cli_tui_mixin.py:_tui_cancel_foreground_ui */
json_t *ctd_cancel_foreground_ui_flow(ctd_io_t *io, const json_t *closers) {
    if (closers && closers->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(closers); i++) {
            json_t *entry = json_get(closers, i);
            if (!entry || entry->type != JSON_OBJECT) continue;
            const char *attr = json_get_str(entry, "state_attr", "");
            const char *close = json_get_str(entry, "close", "");
            if (!ctd_truthy(ctd_st(io, attr))) continue;
            if (strcmp(close, "gateway_command_wait.set") == 0) {
                ctd_event_set(io);
            } else {
                json_t *cargs = json_obj_get(entry, "args");
                json_t *r = ctd_seam(io, close, cargs ? json_copy(cargs) : json_object());
                if (ctd_raised(r)) return r;
                json_free(r);
            }
            ctd_buf_reset(io, false);
            ctd_app_invalidate(io);
            return json_bool(true);
        }
    }
    return json_bool(false);
}

/* ── connection field navigation ───────────────────────────────────────── */

/* PoP: ctd_connection_move_flow @ hermes_cli/cli_tui_mixin.py:_tui_connection_move */
json_t *ctd_connection_move_flow(ctd_io_t *io, int delta) {
    json_t *conn = ctd_st(io, "_connection_state");
    if (!ctd_truthy(conn)) return json_null();
    const char *phase = json_get_str(conn, "phase", "");
    json_t *r;
    if (strcmp(phase, "form") == 0 || strcmp(phase, "failed") == 0) {
        json_t *fields = json_obj_get(conn, "fields");
        size_t nf = (fields && fields->type == JSON_ARRAY) ? json_len(fields) : 0;
        double cur = json_get_num(conn, "field_index", 0);
        if (cur < (double)nf) {
            json_t *field = json_get(fields, (size_t)cur);
            json_t *target = json_obj_get(conn, "target");
            const char *tname = (target && target->type == JSON_OBJECT)
                                ? json_get_str(target, "name", "") : "";
            const char *fname = (field && field->type == JSON_OBJECT)
                                ? json_get_str(field, "name", NULL) : NULL;
            json_t *drafts = json_obj_get(conn, "drafts");
            json_t *td = (drafts && drafts->type == JSON_OBJECT)
                         ? json_obj_get(drafts, tname) : NULL;
            if (td && td->type == JSON_OBJECT && fname)
                json_set(td, fname, json_string(ctd_buf_text(io)));
            double cand = cur + delta;
            if (cand < 0) cand = 0;
            if (cand > (double)nf) cand = (double)nf;
            json_set(conn, "field_index", json_number(cand));
        } else if (delta < 0 && nf > 0) {
            json_set(conn, "field_index", json_number((double)(nf - 1)));
        } else {
            double sel = json_get_num(conn, "selected", 0);
            json_set(conn, "selected",
                     json_number((double)ctd_pymod((long long)sel + delta, 2)));
        }
        r = ctd_seam(io, "self._connection_sync_input_buffer", json_object());
        if (ctd_raised(r)) return r;
        json_free(r);
    } else if (strcmp(phase, "authorized") == 0) {
        double sel = json_get_num(conn, "selected", 0);
        json_set(conn, "selected",
                 json_number((double)ctd_pymod((long long)sel + delta, 2)));
    }
    r = ctd_seam(io, "self._paint_now", json_object());
    if (ctd_raised(r)) return r;
    json_free(r);
    ctd_app_invalidate(io);
    return json_null();
}

/* PoP: ctd_model_picker_escape_flow @ hermes_cli/cli_tui_mixin.py:_tui_model_picker_escape */
json_t *ctd_model_picker_escape_flow(ctd_io_t *io) {
    json_t *st = ctd_st(io, "_model_picker_state");
    const char *stage = (st && st->type == JSON_OBJECT) ? json_get_str(st, "stage", "") : "";
    const char *filter = (st && st->type == JSON_OBJECT) ? json_get_str(st, "filter", "") : "";
    if (ctd_truthy(st) && strcmp(stage, "model") == 0 && filter[0] != '\0') {
        ctd_set_filter(st, "");
        ctd_app_invalidate(io);
        return json_null();
    }
    if (ctd_truthy(st) && strcmp(stage, "reasoning") == 0) {
        json_set(st, "stage", json_string("model"));
        json_set(st, "selected", json_number(0));
        json_set(st, "_scroll_offset", json_number(0));
        json_set(st, "switch_result", json_null());
        ctd_app_invalidate(io);
        return json_null();
    }
    json_t *r = ctd_seam(io, "self._close_model_picker", json_object());
    if (ctd_raised(r)) return r;
    json_free(r);
    ctd_buf_reset(io, false);
    ctd_app_invalidate(io);
    return json_null();
}

/* ── leaves ────────────────────────────────────────────────────────────── */

/* PoP: ctd_paste_over_threshold @ hermes_cli/cli_tui_mixin.py:_tui_paste_over_threshold */
json_t *ctd_paste_over_threshold(ctd_io_t *io, const char *text,
                                 long long line_count, const char *threshold_key) {
    json_t *cfg = ctd_st(io, "config");
    double threshold = (cfg && cfg->type == JSON_OBJECT)
                       ? json_get_num(cfg, threshold_key, 5) : 5;
    double char_threshold = (cfg && cfg->type == JSON_OBJECT)
                            ? json_get_num(cfg, "paste_collapse_char_threshold", 2000) : 2000;
    bool lines_hit = threshold > 0 && (double)line_count >= threshold;
    bool chars_hit = char_threshold > 0 && (double)ctd_utf8_len(text) >= char_threshold;
    return json_bool(lines_hit || chars_hit);
}

static bool ctd_oserror_family(const char *type) {
    static const char *fam[] = {
        "OSError", "PermissionError", "FileNotFoundError", "FileExistsError",
        "BlockingIOError", "ChildProcessError", "ConnectionError", "BrokenPipeError",
        "ConnectionAbortedError", "ConnectionRefusedError", "ConnectionResetError",
        "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "ProcessLookupError", "TimeoutError"
    };
    for (size_t i = 0; i < sizeof fam / sizeof fam[0]; i++)
        if (strcmp(type, fam[i]) == 0) return true;
    return false;
}

/* PoP: ctd_suppress_closed_loop_errors_flow @ hermes_cli/cli_tui_mixin.py:_tui_suppress_closed_loop_errors */
json_t *ctd_suppress_closed_loop_errors_flow(ctd_io_t *io, const json_t *context) {
    json_t *exc = (context && context->type == JSON_OBJECT)
                  ? json_obj_get(context, "exception") : NULL;
    bool suppress = false;
    if (exc && exc->type == JSON_OBJECT) {
        const char *type = json_get_str(exc, "type", "");
        char *es = ctd_exc_str(exc);
        if ((strcmp(type, "RuntimeError") == 0 || strcmp(type, "RecursionError") == 0) &&
            strstr(es, "Event loop is closed") != NULL) {
            suppress = true;
        } else if (strcmp(type, "KeyError") == 0 &&
                   strstr(es, "is not registered") != NULL) {
            suppress = true; /* selector registration failures (#6393) */
        } else if (ctd_oserror_family(type)) {
            json_t *en = json_obj_get(exc, "errno");
            if (en && en->type == JSON_NUMBER && en->num_val == (double)CTD_EIO)
                suppress = true; /* broken stdout on interrupt (#13710) */
        }
        free(es);
    }
    if (suppress) return json_null();
    /* loop.default_exception_handler(context) — exception normalized to a spec */
    json_t *ctx_copy = (context && context->type == JSON_OBJECT)
                       ? json_copy(context) : json_object();
    json_t *e2 = json_obj_get(ctx_copy, "exception");
    if (e2 && e2->type == JSON_OBJECT) {
        json_t *ne = json_object();
        json_set(ne, "type", json_string(json_get_str(e2, "type", "")));
        char *es = ctd_exc_str(e2);
        json_set(ne, "message", json_string(es));
        free(es);
        json_t *en = json_obj_get(e2, "errno");
        json_set(ne, "errno", en ? json_copy(en) : json_null());
        json_set(ctx_copy, "exception", ne);
    }
    json_t *args = json_object();
    json_set(args, "context", ctx_copy);
    json_t *r = ctd_seam(io, "loop.default_exception_handler", args);
    if (ctd_raised(r)) return r;
    json_free(r);
    return json_null();
}
