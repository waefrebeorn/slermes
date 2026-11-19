#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/_subprocess_compat.py: 6 missing functions ── */

/* ────────────────────────────────────────────────────────────────
 * tail_revive2 — faithful ports of the _subprocess_compat gap stubs.
 * Universal seam interface: json_t *fn(sc2_io_t *io, json_t *in);
 * sc2_seam() records {ev, args, ret} in call order; scripted results
 * {"__raise__": Type, "message": ...} model Python exceptions and are
 * propagated/swallowed exactly where the live code does. BaseException
 * kinds pass through every `except Exception` site. NULL == None.
 *
 * Seam map:
 *   psutil.Process(pid)                -> "psutil.Process" {pid}
 *   process.cmdline()                  -> "psutil.cmdline" {handle:"<proc>"}
 *   process.exe()                      -> "psutil.exe" {handle:"<proc>"}
 *     (the Process double is an opaque handle, recorded as the constant
 *      marker "<proc>"; `import psutil` is always available in the model)
 *   _process_start_time(pid)           -> "process_start_time" {pid}
 *   _process_command_is_hermes(pid)    -> "process_command_is_hermes" {pid}
 *   IS_WINDOWS is a per-case module-constant double, not a seam call.
 *
 * pid_is_hermes takes a TAGGED pid ({"kind","value"}) because isinstance
 * distinguishes int/bool/float/str/None and a parsed JSON number cannot
 * carry that identity. text_names_hermes lowercases ASCII only: the
 * tested prefixes ("hermes", ".hermes") contain no 'k', so the one
 * non-ASCII fold that produces an ASCII letter (U+212A -> 'k') can never
 * create a prefix match; splitting is Unicode-whitespace aware.
 * ──────────────────────────────────────────────────────────────── */

typedef struct sc2_io {
    json_t *(*call)(struct sc2_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} sc2_io_t;

static int sc2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int sc2_is_base_exc(const json_t *r) {
    const char *k = json_get_str(r, "__raise__", "");
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0 ||
           strcmp(k, "GeneratorExit") == 0 || strcmp(k, "CancelledError") == 0;
}

static json_t *sc2_seam(sc2_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static int sc2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

/* ── UTF-8 + re \s whitespace helpers ── */

static unsigned sc2_cp_next(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned cp;
    if (s[0] < 0x80) { *p += 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        *p += 2; return cp;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x0F) << 12) | ((unsigned)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *p += 3; return cp;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x07) << 18) | ((unsigned)(s[1] & 0x3F) << 12) |
             ((unsigned)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        *p += 4; return cp;
    }
    *p += 1; return 0xFFFD;
}

static int sc2_isspace_cp(unsigned cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F)) return 1;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    return cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

/* re.split(r"[\\/\s=,;\"']+", text.lower()) token prefix test. */
static int sc2_text_names_hermes(const char *text) {
    if (!text) text = "";
    const char *p = text;
    char tok[512];
    size_t tn = 0;
    int hit = 0;
    for (;;) {
        const char *q = p;
        unsigned cp = (*p) ? sc2_cp_next(&q) : 0;
        int sep = !*p || cp == '\\' || cp == '/' || cp == '=' || cp == ',' ||
                  cp == ';' || cp == '"' || cp == '\'' || sc2_isspace_cp(cp);
        if (sep) {
            if (tn > 0) {
                tok[tn] = '\0';
                if (strncmp(tok, "hermes", 6) == 0 || strncmp(tok, ".hermes", 7) == 0) {
                    hit = 1;
                    break;
                }
                tn = 0;
            }
            if (!*p) break;
        } else {
            /* ASCII lower; non-ASCII bytes pass through (see header) */
            while (p < q && tn + 1 < sizeof(tok)) {
                char c = *p++;
                tok[tn++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
            }
            if (p < q) { /* token overflow: keep scanning, drop the tail */
                p = q;
            }
        }
        p = q;
    }
    return hit;
}

/* PoP: harden_git_argv @ hermes_cli/_subprocess_compat.py:harden_git_argv */
char *harden_git_argv(void) { return strdup("[]"); }

/* PoP: resolve_node_command @ hermes_cli/_subprocess_compat.py:resolve_node_command */
char *resolve_node_command(void) { return strdup(""); }

/* PoP: text_names_hermes @ hermes_cli/_subprocess_compat.py:_text_names_hermes */
json_t *text_names_hermes(sc2_io_t *io, json_t *in) {
    (void)io;
    const char *text = json_get_str(in, "text", "");
    return json_bool(sc2_text_names_hermes(text) != 0);
}

/* PoP: process_command_is_hermes @ hermes_cli/_subprocess_compat.py:_process_command_is_hermes */
json_t *process_command_is_hermes(sc2_io_t *io, json_t *in) {
    /* try:
     *     import psutil
     *     process = psutil.Process(pid)
     *     command = " ".join(process.cmdline() or [])
     *     executable = process.exe() or ""
     *     return _text_names_hermes(f"{command} {executable}")
     * except Exception: return False */
    double pid = json_get_num(in, "pid", 0.0);
    json_t *args = json_object();
    json_set(args, "pid", json_number(pid));
    json_t *r = sc2_seam(io, "psutil.Process", args);
    json_free(args);
    if (sc2_is_raise(r)) {
        if (sc2_is_base_exc(r)) return r;
        json_free(r);
        return json_bool(0);
    }
    json_free(r);
    json_t *ha = json_object();
    json_set_hdr(ha, "handle", "<proc>");
    json_t *cl = sc2_seam(io, "psutil.cmdline", ha);
    if (sc2_is_raise(cl)) {
        json_free(ha);
        if (sc2_is_base_exc(cl)) return cl;
        json_free(cl);
        return json_bool(0);
    }
    /* " ".join(cmdline or []) — non-str items raise TypeError (swallowed) */
    char *command = NULL;
    size_t cap = 0, len = 0;
    int join_err = 0;
    if (cl && cl->type == JSON_ARRAY) {
        cap = 64;
        command = malloc(cap);
        if (command) command[0] = '\0';
        for (size_t i = 0; command && i < json_len(cl); i++) {
            json_t *it = json_get(cl, i);
            if (!it || it->type != JSON_STRING) { join_err = 1; break; }
            size_t il = strlen(it->str_val);
            size_t need = len + (i ? 1 : 0) + il + 1;
            if (need > cap) {
                while (cap < need) cap *= 2;
                char *nb = realloc(command, cap);
                if (!nb) { join_err = 1; break; }
                command = nb;
            }
            if (i) command[len++] = ' ';
            memcpy(command + len, it->str_val, il);
            len += il;
            command[len] = '\0';
        }
    }
    json_free(cl);
    if (join_err) {
        free(command);
        json_free(ha);
        return json_bool(0);  /* TypeError from join, caught by except Exception */
    }
    json_t *ex = sc2_seam(io, "psutil.exe", ha);
    json_free(ha);
    if (sc2_is_raise(ex)) {
        free(command);
        if (sc2_is_base_exc(ex)) return ex;
        json_free(ex);
        return json_bool(0);
    }
    const char *executable = (ex && ex->type == JSON_STRING) ? ex->str_val : "";
    /* f"{command} {executable}" */
    size_t cl_len = command ? len : 0;
    size_t ex_len = strlen(executable);
    char *text = malloc(cl_len + 1 + ex_len + 1);
    int hit = 0;
    if (text) {
        if (cl_len) memcpy(text, command, cl_len);
        text[cl_len] = ' ';
        memcpy(text + cl_len + 1, executable, ex_len + 1);
        hit = sc2_text_names_hermes(text);
        free(text);
    }
    free(command);
    json_free(ex);
    return json_bool(hit != 0);
}

/* PoP: pid_is_hermes @ hermes_cli/_subprocess_compat.py:pid_is_hermes */
json_t *pid_is_hermes(sc2_io_t *io, json_t *in) {
    /* if not isinstance(pid, int) or isinstance(pid, bool) or pid <= 0:
     *     return False */
    json_t *tag = json_obj_get(in, "pid");
    const char *kind = tag ? json_get_str(tag, "kind", "null") : "null";
    double pidv = tag ? json_get_num(tag, "value", 0.0) : 0.0;
    if (strcmp(kind, "int") != 0 || pidv <= 0.0) return json_bool(0);
    int is_windows = json_get_bool(in, "is_windows", false);
    json_t *exp = json_obj_get(in, "expected_start_time");
    int has_exp = !sc2_is_null(exp);
    int exp_is_num = has_exp && exp->type == JSON_NUMBER;
    double expv = exp_is_num ? exp->num_val : 0.0;
    json_t *args = json_object();
    json_set(args, "pid", json_number(pidv));
    if (!is_windows) {
        /* if expected_start_time is None: return True
         * try: return _process_start_time(pid) == expected_start_time
         * except Exception: return False */
        if (!has_exp) { json_free(args); return json_bool(1); }
        json_t *r = sc2_seam(io, "process_start_time", args);
        json_free(args);
        if (sc2_is_raise(r)) {
            if (sc2_is_base_exc(r)) return r;
            json_free(r);
            return json_bool(0);
        }
        int eq = exp_is_num && r && r->type == JSON_NUMBER && r->num_val == expv;
        json_free(r);
        return json_bool(eq != 0);
    }
    /* try: current_start_time = _process_start_time(pid)
     * except Exception: return False */
    json_t *r = sc2_seam(io, "process_start_time", args);
    if (sc2_is_raise(r)) {
        json_free(args);
        if (sc2_is_base_exc(r)) return r;
        json_free(r);
        return json_bool(0);
    }
    /* if current_start_time is None: return False */
    if (sc2_is_null(r)) { json_free(args); json_free(r); return json_bool(0); }
    /* if expected_start_time is not None and current != expected: return False */
    int cur_is_num = r->type == JSON_NUMBER;
    double cur = cur_is_num ? r->num_val : 0.0;
    json_free(r);
    if (has_exp && (!cur_is_num || !exp_is_num || cur != expv)) {
        json_free(args);
        return json_bool(0);
    }
    /* try: return _process_command_is_hermes(pid)
     * except Exception: return False */
    json_t *c = sc2_seam(io, "process_command_is_hermes", args);
    json_free(args);
    if (sc2_is_raise(c)) {
        if (sc2_is_base_exc(c)) return c;
        json_free(c);
        return json_bool(0);
    }
    if (c && c->type == JSON_BOOL) return c;
    int t = c && c->type != JSON_NULL &&
            ((c->type == JSON_NUMBER && c->num_val != 0.0) ||
             (c->type == JSON_STRING && c->str_val && c->str_val[0]) ||
             ((c->type == JSON_ARRAY || c->type == JSON_OBJECT) && c->c.count > 0));
    json_free(c);
    return json_bool(t != 0);
}

/* PoP: bounded_probe_run @ hermes_cli/_subprocess_compat.py:bounded_probe_run */
char *bounded_probe_run(void) { return strdup("{}"); }
