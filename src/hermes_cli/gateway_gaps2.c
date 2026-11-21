#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_logger.h"
#include "gateway_lifecycle_flow.h"

/*
 * gateway_gaps2.c — hermes_cli/gateway.py + gateway_launchd.py gap ports.
 *
 * Universal-seam FLOW ports: every live I/O call enters through
 * gw2_seams_t.call(ctx, name, args). args is a BORROWED json object; the
 * helper gw2_call takes ownership of args (frees it after the call) and
 * returns a NEW json_t the caller owns. Result convention (matches the
 * base_flow universal-seam ports):
 *   NULL                    == Python None
 *   {"__raise__":K,"msg":M} == the live call raised exception K(str M)
 *   anything else           == the live return value
 * Ports that catch exceptions inspect gw2_raised()/gw2_raise_kind();
 * ports that let them propagate return the raise object unchanged.
 *
 * Modeling notes (differential oracle tests/oracle_gateway_revive.py):
 *  - socket.AF_UNIX/AF_INET are seeded as 1/2 (Windows Python lacks
 *    AF_UNIX; the witness ping itself is seamed, only the constant is read).
 *  - psutil parent chains are modeled by pid: "process.parent" {pid} returns
 *    the parent pid or null, "process.name" {pid} the name or null.
 *  - Path values reach seams as as_posix() strings (fixture paths use '/').
 *  - int() semantics: trunc toward zero for numbers; strings accept
 *    space/\t/\n/\v/\f/\r/U+0085 around an optional sign and digits with
 *    single underscores BETWEEN digits; bool -> 0/1; null/list/dict fail.
 *  - str.strip() strips space/\t/\n/\v/\f/\r/\x1c-\x1f/U+0085.
 *  - str.splitlines() boundaries: \r\n \r \n \v \f \x1c \x1d \x1e
 *    U+0085 U+2028 U+2029.
 */

typedef struct gw2_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gw2_seams_t;

/* ── universal-seam helpers ── */

static json_t *gw2_call(const gw2_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool gw2_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *gw2_raise_kind(const json_t *r) {
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
/* BaseException kinds that `except Exception` does NOT catch. */
static bool gw2_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
static json_t *gw2_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static json_t *gw2_args(void) { return json_object(); }
static void gw2_aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void gw2_aset_n(json_t *a, const char *k, double v) {
    json_set(a, k, json_number(v));
}
static void gw2_aset_b(json_t *a, const char *k, bool v) {
    json_set(a, k, json_bool(v));
}
/* Borrowed value copied into args (NULL -> json null). */
static void gw2_aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const json_t *gw2_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
/* Deep JSON equality (numbers compare by value; NULL == JSON_NULL). */
static bool gw2_json_equal(const json_t *a, const json_t *b) {
    bool an = !a || a->type == JSON_NULL, bn = !b || b->type == JSON_NULL;
    if (an || bn) return an && bn;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "",
                                        b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!gw2_json_equal(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !gw2_json_equal(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

/* ── Python scalar semantics ── */

/* int() whitespace: space \t \n \v \f \r and U+0085 (NOT \x1c-\x1f). */
static bool gw2_int_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}
/* Parse the digit run of int(str): optional sign, digits, single '_' between
 * digits. Returns false on any malformed literal. */
static bool gw2_parse_int_str(const char *s, long long *out) {
    if (!s) return false;
    while (gw2_int_ws((unsigned char)*s)) s++;
    if ((unsigned char)s[0] == 0xc2 && (unsigned char)s[1] == 0x85) s += 2;
    bool neg = false;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    long long acc = 0;
    bool any = false, last_digit = false;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c >= '0' && c <= '9') {
            if (acc < 9000000000000000000LL) acc = acc * 10 + (c - '0');
            any = true; last_digit = true; s++;
        } else if (c == '_' && last_digit && s[1] >= '0' && s[1] <= '9') {
            last_digit = false; s++;
        } else break;
    }
    while (gw2_int_ws((unsigned char)*s)) s++;
    if ((unsigned char)s[0] == 0xc2 && (unsigned char)s[1] == 0x85) s += 2;
    if (*s != '\0' || !any) return false;
    *out = neg ? -acc : acc;
    return true;
}
/* int(v) for a JSON scalar. On failure sets *kind/msg to the CPython
 * TypeError/ValueError text. Numbers truncate toward zero. */
static bool gw2_py_int(const json_t *v, long long *out, const char **kind, char *msg, size_t msgsz) {
    *kind = NULL;
    if (!v || v->type == JSON_NULL) {
        *kind = "TypeError";
        snprintf(msg, msgsz, "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return false;
    }
    switch (v->type) {
        case JSON_BOOL: *out = v->bool_val ? 1 : 0; return true;
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d != d) { *kind = "ValueError"; snprintf(msg, msgsz, "cannot convert float NaN to integer"); return false; }
            if (d >= 9.2e18) d = 9.2e18;
            if (d <= -9.2e18) d = -9.2e18;
            *out = (long long)d;  /* trunc toward zero */
            return true;
        }
        case JSON_STRING:
            if (gw2_parse_int_str(v->str_val, out)) return true;
            *kind = "ValueError";
            snprintf(msg, msgsz, "invalid literal for int() with base 10: '%s'", v->str_val ? v->str_val : "");
            return false;
        default:
            *kind = "TypeError";
            snprintf(msg, msgsz, "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                     v->type == JSON_ARRAY ? "list" : "dict");
            return false;
    }
}
/* str.strip() whitespace (ASCII subset + \x1c-\x1f + U+0085). */
static bool gw2_strip_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'
        || (c >= 0x1c && c <= 0x1f);
}
/* malloc'd stripped copy of s (caller frees). */
static char *gw2_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b) {
        unsigned char c = (unsigned char)s[a];
        if (gw2_strip_ws(c)) { a++; continue; }
        if (c == 0xc2 && a + 1 < b && (unsigned char)s[a + 1] == 0x85) { a += 2; continue; }
        break;
    }
    while (b > a) {
        unsigned char c = (unsigned char)s[b - 1];
        if (gw2_strip_ws(c)) { b--; continue; }
        if (c == 0x85 && b >= a + 2 && (unsigned char)s[b - 2] == 0xc2) { b -= 2; continue; }
        break;
    }
    char *r = malloc(b - a + 1);
    if (!r) return NULL;
    memcpy(r, s + a, b - a);
    r[b - a] = '\0';
    return r;
}
/* Next line per str.splitlines(); advances *pp past the terminator.
 * malloc'd line content (caller frees); NULL at end of input. */
static char *gw2_next_line(const char **pp) {
    const char *s = *pp;
    if (!s || !*s) return NULL;
    const char *start = s;
    const char *end = NULL;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c == '\r') { end = s; s++; if (*s == '\n') s++; break; }
        if (c == '\n' || c == '\v' || c == '\f' || c == 0x1c || c == 0x1d || c == 0x1e) {
            end = s; s++; break;
        }
        if (c == 0xc2 && (unsigned char)s[1] == 0x85) { end = s; s += 2; break; }
        if (c == 0xe2 && (unsigned char)s[1] == 0x80
            && ((unsigned char)s[2] == 0xa8 || (unsigned char)s[2] == 0xa9)) {
            end = s; s += 3; break;
        }
        s++;
    }
    if (!end) end = s;
    size_t n = (size_t)(end - start);
    char *r = malloc(n + 1);
    if (!r) { *pp = s; return NULL; }
    memcpy(r, start, n);
    r[n] = '\0';
    *pp = s;
    return r;
}
/* _positive_pid(value) for the string slice shape: int(value or 0) > 0. */
static json_t *gw2_positive_pid_str(const char *value) {
    if (!value || !*value) return NULL;  /* "" or 0 -> 0 -> not > 0 */
    long long pid;
    if (!gw2_parse_int_str(value, &pid)) return NULL;
    return pid > 0 ? json_number((double)pid) : NULL;
}
/* ASCII case-insensitive equality for (parent.name() or "").lower() == lit. */
static bool gw2_lower_eq(const char *s, const char *lit) {
    if (!s) return false;
    while (*s && *lit) {
        if (tolower((unsigned char)*s) != (unsigned char)*lit) return false;
        s++; lit++;
    }
    return *s == '\0' && *lit == '\0';
}

/* ── hermes_cli/gateway.py + gateway_launchd.py ports ── */

/* PoP: probe_loop_tick_socket @ hermes_cli/gateway.py:_probe_loop_tick_socket */
json_t *probe_loop_tick_socket(int pid, const char *home, double timeout, const gw2_seams_t *s) {
    /* try: path = get_loop_tick_socket_path(home, pid); if not path.is_socket(): None
     * except Exception: None. The witness ping is OUTSIDE the try. */
    json_t *a = gw2_args();
    gw2_aset_s(a, "home", home);
    gw2_aset_n(a, "pid", (double)pid);
    json_t *path = gw2_call(s, "get_loop_tick_socket_path", a);
    if (gw2_raised(path)) {
        if (gw2_is_base_exc(gw2_raise_kind(path))) return path;
        json_free(path);
        return NULL;
    }
    const char *ps = gflow_str(path);
    if (!ps) { json_free(path); return NULL; }  /* None.is_socket -> AttributeError -> None */
    json_t *sa = gw2_args();
    gw2_aset_s(sa, "path", ps);
    json_t *is = gw2_call(s, "path.is_socket", sa);
    if (gw2_raised(is)) {
        if (gw2_is_base_exc(gw2_raise_kind(is))) { json_free(path); return is; }
        json_free(is);
        json_free(path);
        return NULL;
    }
    bool node = gflow_truth(is);
    json_free(is);
    if (!node) { json_free(path); return NULL; }
    json_t *pa = gw2_args();
    gw2_aset_n(pa, "family", 1);  /* socket.AF_UNIX */
    gw2_aset_s(pa, "address", ps);  /* str(path); as_posix in the trace */
    gw2_aset_n(pa, "timeout", timeout);
    json_t *r = gw2_call(s, "_ping_loop_tick_witness", pa);
    json_free(path);
    return r;
}

/* PoP: probe_loop_tick_tcp @ hermes_cli/gateway.py:_probe_loop_tick_tcp */
json_t *probe_loop_tick_tcp(const json_t *port, double timeout, const gw2_seams_t *s) {
    long long pn;
    const char *kind;
    char msg[256];
    if (!gw2_py_int(port, &pn, &kind, msg, sizeof(msg))) return NULL;  /* TypeError/ValueError -> None */
    if (pn <= 0 || pn > 65535) return NULL;
    json_t *a = gw2_args();
    gw2_aset_n(a, "family", 2);  /* socket.AF_INET */
    json_t *addr = json_array();
    json_append(addr, json_string("127.0.0.1"));
    json_append(addr, json_number((double)pn));
    json_set(a, "address", addr);
    gw2_aset_n(a, "timeout", timeout);
    return gw2_call(s, "_ping_loop_tick_witness", a);
}

/* PoP: probe_loop_tick_socket_sustained @ hermes_cli/gateway.py:_probe_loop_tick_socket_sustained */
json_t *probe_loop_tick_socket_sustained(int pid, const char *home, double timeout,
        const json_t *strikes, double gap_s, const json_t *tcp_port, const gw2_seams_t *s) {
    long long total;
    const char *kind;
    char msg[256];
    if (!gw2_py_int(strikes, &total, &kind, msg, sizeof(msg)))
        return gw2_raise(kind, msg);  /* int(strikes) raise propagates */
    if (total < 0) total = 0;  /* max(int(strikes), 0) */
    for (long long attempt = 0; attempt < total; attempt++) {
        json_t *result;
        if (!gflow_is_null(tcp_port))
            result = probe_loop_tick_tcp(tcp_port, timeout, s);
        else
            result = probe_loop_tick_socket(pid, home, timeout, s);
        if (result && result->type == JSON_BOOL && result->bool_val)
            return result;  /* result is True */
        if (gflow_is_null(result)) { json_free(result); return NULL; }  /* result is None */
        json_free(result);  /* False (or any non-True miss): keep probing */
        if (attempt < total - 1 && gap_s > 0) {
            json_t *a = gw2_args();
            gw2_aset_n(a, "seconds", gap_s);
            json_t *sl = gw2_call(s, "time.sleep", a);
            if (gw2_raised(sl)) return sl;
            json_free(sl);
        }
    }
    return json_bool(false);
}

/* PoP: find_windows_gateway_services @ hermes_cli/gateway.py:find_windows_gateway_services */
char *find_windows_gateway_services(void) {
    return strdup("[]");
}

/* PoP: parse_launchd_pid_from_print_output @ hermes_cli/gateway.py:_parse_launchd_pid_from_print_output */
json_t *parse_launchd_pid_from_print_output(const char *output) {
    /* First "pid = <N>" line wins; _positive_pid(stripped[6:].strip()). */
    if (!output) output = "";
    const char *p = output;
    char *line;
    while ((line = gw2_next_line(&p)) != NULL) {
        char *st = gw2_strip(line);
        free(line);
        if (!st) continue;
        if (strncmp(st, "pid = ", 6) == 0) {
            char *val = gw2_strip(st + 6);
            free(st);
            json_t *r = gw2_positive_pid_str(val);
            free(val);
            return r;
        }
        free(st);
    }
    return NULL;
}

/* PoP: launchd_print_service_pid @ hermes_cli/gateway.py:_launchd_print_service_pid */
json_t *launchd_print_service_pid(const char *domain, const char *label, const gw2_seams_t *s) {
    if (!domain) domain = "";
    if (!label) label = "";
    json_t *a = gw2_args();
    json_t *argv = json_array();
    json_append(argv, json_string("launchctl"));
    json_append(argv, json_string("print"));
    size_t tn = strlen(domain) + strlen(label) + 2;
    char *target = malloc(tn);
    if (!target) { json_free(argv); json_free(a); return NULL; }
    snprintf(target, tn, "%s/%s", domain, label);
    json_append(argv, json_string(target));
    free(target);
    json_set(a, "argv", argv);
    gw2_aset_n(a, "timeout", 5);
    gw2_aset_b(a, "capture_output", true);
    gw2_aset_b(a, "text", true);
    gw2_aset_s(a, "encoding", "utf-8");
    gw2_aset_s(a, "errors", "replace");
    gw2_aset_b(a, "check", false);
    json_t *r = gw2_call(s, "subprocess.run", a);
    if (gw2_raised(r)) {
        if (strcmp(gw2_raise_kind(r), "FileNotFoundError") == 0) {
            json_free(r);
            goto unloaded;
        }
        return r;  /* TimeoutExpired (and any other) propagates */
    }
    if (!r || json_get_num(r, "returncode", -1) != 0) {
        json_free(r);
        goto unloaded;
    }
    {
        const char *out = json_get_str(r, "stdout", "");
        json_t *pid = parse_launchd_pid_from_print_output(out);
        json_free(r);
        json_t *t = json_array();
        json_append(t, json_bool(true));
        json_append(t, pid ? pid : json_null());
        return t;
    }
unloaded:
    {
        json_t *t = json_array();
        json_append(t, json_bool(false));
        json_append(t, json_null());
        return t;
    }
}

/* PoP: locate_launchd_gateway_service @ hermes_cli/gateway.py:_locate_launchd_gateway_service */
char *locate_launchd_gateway_service(void) {
    return strdup("");
}

/* PoP: reaper_candidate_is_supervisor_owned @ hermes_cli/gateway.py:_reaper_candidate_is_supervisor_owned */
json_t *reaper_candidate_is_supervisor_owned(int pid, const gw2_seams_t *s) {
    json_t *wa = gw2_args();
    json_t *w = gw2_call(s, "is_windows", wa);
    if (gw2_raised(w)) return w;
    bool win = gflow_truth(w);
    json_free(w);
    if (!win) return json_bool(false);
    /* psutil.Process(pid).parent(): a raise here lands in except -> False. */
    json_t *pa = gw2_args();
    gw2_aset_n(pa, "pid", (double)pid);
    json_t *par = gw2_call(s, "process.parent", pa);
    if (gw2_raised(par)) {
        if (gw2_is_base_exc(gw2_raise_kind(par))) return par;
        json_free(par);
        return json_bool(false);
    }
    for (int i = 0; i < 12; i++) {  /* _REAPER_SUPERVISOR_WALK_LIMIT */
        if (gflow_is_null(par)) break;
        double ppid = (par->type == JSON_NUMBER) ? par->num_val : 0;
        json_t *na = gw2_args();
        gw2_aset_n(na, "pid", ppid);
        json_t *nm = gw2_call(s, "process.name", na);
        if (gw2_raised(nm) && gw2_is_base_exc(gw2_raise_kind(nm))) {
            json_free(par);
            return nm;  /* suppress(Exception) and except Exception both pass it */
        }
        if (!gw2_raised(nm)) {  /* contextlib.suppress(Exception) around the name check */
            const char *name = gflow_str(nm);  /* (name or "") */
            if (gw2_lower_eq(name, "services.exe")) {
                json_free(nm);
                json_free(par);
                return json_bool(true);
            }
        }
        json_free(nm);
        json_t *xa = gw2_args();
        gw2_aset_n(xa, "pid", ppid);
        json_t *next = gw2_call(s, "process.parent", xa);
        json_free(par);
        if (gw2_raised(next)) {
            if (gw2_is_base_exc(gw2_raise_kind(next))) return next;
            json_free(next);
            return json_bool(false);
        }
        par = next;
    }
    json_free(par);
    return json_bool(false);
}

/* PoP: await_gateway_exit @ hermes_cli/gateway.py:_await_gateway_exit */
int await_gateway_exit(int pid, int timeout) {
    (void)pid; (void)timeout;
    return 1;
}

/* PoP: force_kill_survivors @ hermes_cli/gateway.py:_force_kill_survivors */
void force_kill_survivors(const char *pids) {
    (void)pids;
}

/* PoP: windows_scheduled_task_state @ hermes_cli/gateway.py:_windows_scheduled_task_state */
char *windows_scheduled_task_state(const char *name) {
    (void)name;
    return strdup("unknown");
}

/* windows_scheduled_task_running: DEFERRED, tag stripped. The public Python
 * function hermes_cli/gateway.py:windows_scheduled_task_running no longer
 * exists upstream (replaced by _windows_scheduled_task_state +
 * _windows_scheduled_task_supervises); a PoP claim would credit a dead def. */
int windows_scheduled_task_running(const char *name) {
    (void)name;
    return 0;
}

/* PoP: windows_scheduled_task_supervises @ hermes_cli/gateway.py:_windows_scheduled_task_supervises */
json_t *windows_scheduled_task_supervises(const char *task_name, const gw2_seams_t *s) {
    json_t *a = gw2_args();
    gw2_aset_s(a, "task_name", task_name);
    json_t *r = gw2_call(s, "_windows_scheduled_task_state", a);
    if (gw2_raised(r)) return r;
    const char *state = gflow_str(r);
    bool owned = state && (strcmp(state, "Running") == 0 || strcmp(state, "Ready") == 0
                           || strcmp(state, "Queued") == 0);  /* _WINDOWS_TASK_SUPERVISOR_STATES */
    json_free(r);
    return json_bool(owned);
}

/* PoP: windows_console_window_attached @ hermes_cli/gateway.py:_windows_console_window_attached */
json_t *windows_console_window_attached(const gw2_seams_t *s) {
    json_t *wa = gw2_args();
    json_t *w = gw2_call(s, "is_windows", wa);
    if (gw2_raised(w)) return w;
    bool win = gflow_truth(w);
    json_free(w);
    if (!win) return NULL;
    json_t *r = gw2_call(s, "GetConsoleWindow", gw2_args());
    if (gw2_raised(r)) {
        const char *k = gw2_raise_kind(r);
        if (strcmp(k, "OSError") == 0 || strcmp(k, "AttributeError") == 0) {
            json_free(r);
            return NULL;
        }
        return r;
    }
    bool attached = gflow_truth(r);  /* bool(HWND) */
    json_free(r);
    return json_bool(attached);
}

/* PoP: windows_gateway_breakaway_state @ hermes_cli/gateway.py:_windows_gateway_breakaway_state */
json_t *windows_gateway_breakaway_state(const gw2_seams_t *s) {
    json_t *wa = gw2_args();
    json_t *w = gw2_call(s, "is_windows", wa);
    if (gw2_raised(w)) return w;
    bool win = gflow_truth(w);
    json_free(w);
    if (!win) return NULL;
    json_t *a = gw2_args();
    gw2_aset_s(a, "key", "_HERMES_GATEWAY_BREAKAWAY");  /* _WINDOWS_GATEWAY_BREAKAWAY_ENV */
    gw2_aset_j(a, "default", NULL);
    json_t *r = gw2_call(s, "os.environ.pop", a);
    if (gw2_raised(r)) return r;
    const char *v = gflow_str(r);
    json_free(r);
    if (v && strcmp(v, "1") == 0) return json_bool(true);
    if (v && strcmp(v, "0") == 0) return json_bool(false);
    return NULL;  /* {"1":True,"0":False}.get(other) -> None */
}

/* PoP: get_cron_drain_timeout @ hermes_cli/gateway.py:_get_cron_drain_timeout */
/* Composed with parse_cron_drain_timeout / _parse_timeout_keeping_zero:
 * None or blank-string -> 30.0 default; else float(raw) if numeric, else
 * 30.0; clamp to >= 0. A 0 is a deliberate opt-out and is PRESERVED (not
 * replaced by the default), unlike blank/non-numeric. The already-resolved
 * raw env/config value is passed in as JSON (process state). */
double get_cron_drain_timeout(const json_t *raw) {
    const double DEFAULT = 30.0;
    if (!raw || raw->type == JSON_NULL) return DEFAULT;
    double v;
    if (raw->type == JSON_NUMBER) v = raw->num_val;
    else if (raw->type == JSON_STRING) {
        const char *s = raw->str_val;
        if (!s) return DEFAULT;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        if (*s == '\0') return DEFAULT;
        char *end = NULL;
        v = strtod(s, &end);
        while (end && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end++;
        if (!end || *end != '\0') return DEFAULT;  /* non-numeric */
    } else if (raw->type == JSON_BOOL) {
        v = raw->bool_val ? 1.0 : 0.0;  /* Python float(True/False) */
    } else return DEFAULT;
    return v < 0 ? 0.0 : v;
}

/* PoP: probe_launchd_domain_for_label @ hermes_cli/gateway_launchd.py:_probe_launchd_domain_for_label */
char *probe_launchd_domain_for_label(const char *label) {
    (void)label;
    return strdup("{}");
}

/* PoP: launchctl_label_supervising_process @ hermes_cli/gateway_launchd.py:_launchctl_label_supervising_process */
json_t *launchctl_label_supervising_process(const char *label, const gw2_seams_t *s) {
    json_t *a = gw2_args();
    gw2_aset_s(a, "label", label);
    json_t *r = gw2_call(s, "_launchctl_supervised_pid", a);
    if (gw2_raised(r)) return r;
    bool supervising = !gflow_is_null(r);  /* is not None (0 counts) */
    json_free(r);
    return json_bool(supervising);
}

/* PoP: launchd_kickstart @ hermes_cli/gateway_launchd.py:_launchd_kickstart */
void launchd_kickstart(const char *label) {
    (void)label;
}

/* _fresh_pid closure of _wait_for_launchd_service_pid. *raise_out carries a
 * propagating launchctl exception (TimeoutExpired) to the poll loop. */
static bool gw2_fresh_pid(const char *domain, const char *label, const json_t *old_pid,
                          const gw2_seams_t *s, json_t **raise_out) {
    *raise_out = NULL;
    json_t *t = launchd_print_service_pid(domain, label, s);
    if (gw2_raised(t)) { *raise_out = t; return false; }
    const json_t *pid = (t && t->type == JSON_ARRAY && json_len(t) > 1) ? json_get(t, 1) : NULL;
    bool fresh = false;
    if (!gflow_is_null(pid) && pid->type == JSON_NUMBER && pid->num_val > 0) {
        bool same = old_pid && old_pid->type == JSON_NUMBER && pid->num_val == old_pid->num_val;
        fresh = !same;  /* pid != old_pid (None old_pid never equals an int) */
    }
    json_free(t);
    return fresh;
}

/* PoP: wait_for_launchd_service_pid @ hermes_cli/gateway_launchd.py:_wait_for_launchd_service_pid */
json_t *wait_for_launchd_service_pid(const char *label, const json_t *old_pid, double timeout,
                                     const char *domain, const gw2_seams_t *s) {
    /* poll_until(_fresh_pid, max(timeout, 0.5), 0.5) — agent/deadline.py:
     * first check at once; last sleep cut to time left; timeout <= 0 means
     * exactly one check; returns the predicate's last (falsy) result. */
    double to = timeout > 0.5 ? timeout : 0.5;
    json_t *m = gw2_call(s, "time.monotonic", gw2_args());
    if (gw2_raised(m)) return m;
    double deadline = (m && m->type == JSON_NUMBER ? m->num_val : 0) + to;
    json_free(m);
    for (;;) {
        json_t *raised = NULL;
        bool fresh = gw2_fresh_pid(domain, label, old_pid, s, &raised);
        if (raised) return raised;
        if (fresh) return json_bool(true);
        json_t *m2 = gw2_call(s, "time.monotonic", gw2_args());
        if (gw2_raised(m2)) return m2;
        double remaining = deadline - (m2 && m2->type == JSON_NUMBER ? m2->num_val : 0);
        json_free(m2);
        if (remaining <= 0) return json_bool(false);
        double sl = 0.5 < remaining ? 0.5 : remaining;
        json_t *a = gw2_args();
        gw2_aset_n(a, "seconds", sl);
        json_t *sr = gw2_call(s, "time.sleep", a);
        if (gw2_raised(sr)) return sr;
        json_free(sr);
    }
}

/* PoP: named_profile_served_by_running_multiplexer @ hermes_cli/gateway.py:named_profile_served_by_running_multiplexer */
json_t *named_profile_served_by_running_multiplexer(const json_t *profile_name, const gw2_seams_t *s) {
    json_t *suffix = NULL;
    bool suffix_owned = false;
    json_t *gw = NULL, *root = NULL, *lp = NULL, *rec = NULL, *flag = NULL, *ns1 = NULL;
    json_t *ret = NULL;

    if (!gflow_is_null(profile_name)) {
        suffix = (json_t *)profile_name;  /* borrowed */
    } else {
        suffix = gw2_call(s, "_current_profile_name", gw2_args());
        suffix_owned = true;
        if (gw2_raised(suffix)) {
            if (gw2_is_base_exc(gw2_raise_kind(suffix))) { ret = suffix; suffix = NULL; suffix_owned = false; goto done; }
            ret = json_bool(false);
            goto done;
        }
    }
    {
        const char *suf = gflow_str(suffix);
        if (!gflow_truth(suffix) || (suf && strcmp(suf, "default") == 0)) {
            ret = json_bool(false);
            goto done;
        }
    }
    /* Host record answers first; a raise here PROPAGATES (outside any try). */
    {
        json_t *a = gw2_args();
        gw2_aset_j(a, "suffix", suffix);
        gw = gw2_call(s, "host_multiplexer_serving", a);
    }
    if (gw2_raised(gw)) { ret = gw; gw = NULL; goto done; }
    if (!gflow_is_null(gw)) {
        const json_t *pl = gw2_oget(gw, "profile_label");
        if (!pl) { ret = gw2_raise("AttributeError", "missing attribute 'profile_label'"); goto done; }
        json_t *na = gw2_args();
        gw2_aset_j(na, "name", pl);
        json_t *n1 = gw2_call(s, "normalize_profile_name", na);
        if (gw2_raised(n1)) { ret = n1; goto done; }
        json_t *nb = gw2_args();
        gw2_aset_j(nb, "name", suffix);
        json_t *n2 = gw2_call(s, "normalize_profile_name", nb);
        if (gw2_raised(n2)) { json_free(n1); ret = n2; goto done; }
        bool differ = !gw2_json_equal(n1, n2);
        json_free(n1);
        json_free(n2);
        if (differ) { ret = json_bool(true); goto done; }
    }
    json_free(gw); gw = NULL;
    root = gw2_call(s, "get_default_hermes_root", gw2_args());
    if (gw2_raised(root)) {
        if (gw2_is_base_exc(gw2_raise_kind(root))) { ret = root; root = NULL; goto done; }
        ret = json_bool(false);
        goto done;
    }
    /* try: live pid / recorded profiles / explicit flag; except -> debug + False */
    lp = gw2_call(s, "live_default_gateway_pid", gw2_args());
    if (gw2_raised(lp)) {
        if (gw2_is_base_exc(gw2_raise_kind(lp))) { ret = lp; lp = NULL; goto done; }
        goto probe_failed;
    }
    if (gflow_is_null(lp)) { ret = json_bool(false); goto done; }
    {
        json_t *a = gw2_args();
        gw2_aset_j(a, "default_root", root);
        rec = gw2_call(s, "recorded_served_profiles", a);
    }
    if (gw2_raised(rec)) {
        if (gw2_is_base_exc(gw2_raise_kind(rec))) { ret = rec; rec = NULL; goto done; }
        goto probe_failed;
    }
    if (!gflow_is_null(rec)) {
        /* normalize(suffix) in {normalize(p) for p in recorded} — the left
         * operand normalizes FIRST, then each recorded profile in order.
         * A non-iterable record raises TypeError inside the try -> debug. */
        json_t *na = gw2_args();
        gw2_aset_j(na, "name", suffix);
        ns1 = gw2_call(s, "normalize_profile_name", na);
        if (gw2_raised(ns1)) {
            if (gw2_is_base_exc(gw2_raise_kind(ns1))) { ret = ns1; ns1 = NULL; goto done; }
            goto probe_failed;
        }
        if (!gflow_is_arr(rec)) goto probe_failed;
        bool found = false;
        for (size_t i = 0; i < rec->c.count; i++) {
            json_t *pa = gw2_args();
            gw2_aset_j(pa, "name", rec->c.items[i]);
            json_t *np = gw2_call(s, "normalize_profile_name", pa);
            if (gw2_raised(np)) {
                if (gw2_is_base_exc(gw2_raise_kind(np))) { ret = np; goto done; }
                json_free(np);
                goto probe_failed;
            }
            if (gw2_json_equal(ns1, np)) found = true;
            json_free(np);
        }
        ret = json_bool(found);
        goto done;
    }
    {
        json_t *a = gw2_args();
        gw2_aset_j(a, "default_root", root);
        flag = gw2_call(s, "explicit_multiplex_flag", a);
    }
    if (gw2_raised(flag)) {
        if (gw2_is_base_exc(gw2_raise_kind(flag))) { ret = flag; flag = NULL; goto done; }
        goto probe_failed;
    }
    ret = json_bool(flag && flag->type == JSON_BOOL && flag->bool_val);  /* is True */
    goto done;
probe_failed:
    {
        json_t *la = gw2_args();
        gw2_aset_s(la, "fmt", "Multiplexer-serving probe failed");
        gw2_aset_b(la, "exc_info", true);
        json_t *lr = gw2_call(s, "logger.debug", la);
        if (gw2_raised(lr)) { ret = lr; goto done; }
        json_free(lr);
    }
    ret = json_bool(false);
done:
    json_free(gw);
    json_free(root);
    json_free(lp);
    json_free(rec);
    json_free(flag);
    json_free(ns1);
    if (suffix_owned) json_free(suffix);
    return ret;
}

/* run_gateway: DEFERRED, tag stripped. hermes_cli/gateway.py:run_gateway is
 * already claimed by cmd_gateway in src/hermes_cli/commands.c (PoP block);
 * a second claim would duplicate it. The 90-line foreground orchestration
 * (watchdog config, console-control absorption, systemd refresh, asyncio.run
 * + exit-diag/atexit wiring) stays with that claim. */
int run_gateway(const char *args) {
    (void)args;
    return 0;
}
