/* port_main_cli_flow.c — FLOW-core port of hermes_cli/main.py (CLI dispatch).
 * See include/port_main_cli_flow.h for the seam table and the representation
 * contract. libjson-only; no real I/O inside the decision core.
 */
#include "port_main_cli_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── seam dispatch ─────────────────────────────────────────────────────── */
json_t *mcl_call(mcl_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_append(io->trace, ev);
    if (!io->call) return json_null();
    return io->call(io, name, args);
}

bool mcl_raised(const json_t *result) {
    return result && result->type == JSON_OBJECT && json_obj_get(result, "__raise__") != NULL;
}

static json_t *raise_desc(const char *type, const char *message) {
    json_t *o = json_object();
    json_set_hdr(o, "__raise__", type);
    json_set_hdr(o, "message", message);
    return o;
}

/* sys.exit(code) / raise SystemExit(code): code keeps its JSON type. */
static json_t *exit_node(double code) {
    json_t *o = json_object();
    json_set_hdr(o, "__raise__", "SystemExit");
    json_set(o, "code", json_number(code));
    return o;
}
static json_t *exit_str_node(const char *code) {
    json_t *o = json_object();
    json_set_hdr(o, "__raise__", "SystemExit");
    json_set_hdr(o, "code", code);
    return o;
}

/* ── Python truthiness / type tests ────────────────────────────────────── */
static bool jtruth(const json_t *v) {
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
static bool j_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool j_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool j_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

/* ── exception classification ──────────────────────────────────────────── */
static const char *exc_type(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
static bool exc_is_systemexit(const json_t *r) {
    return strcmp(exc_type(r), "SystemExit") == 0;
}
/* `except Exception` catches everything except SystemExit/KeyboardInterrupt/
 * GeneratorExit (BaseException-but-not-Exception). */
static bool exc_is_exception(const json_t *r) {
    const char *t = exc_type(r);
    return strcmp(t, "SystemExit") != 0 && strcmp(t, "KeyboardInterrupt") != 0
        && strcmp(t, "GeneratorExit") != 0;
}
static bool exc_is_oserror(const json_t *r) {
    const char *t = exc_type(r);
    return strcmp(t, "OSError") == 0 || strcmp(t, "PermissionError") == 0
        || strcmp(t, "FileNotFoundError") == 0 || strcmp(t, "FileExistsError") == 0
        || strcmp(t, "BlockingIOError") == 0 || strcmp(t, "IsADirectoryError") == 0
        || strcmp(t, "NotADirectoryError") == 0 || strcmp(t, "InterruptedError") == 0
        || strcmp(t, "TimeoutError") == 0 || strcmp(t, "ProcessLookupError") == 0;
}
static bool exc_is_importerror(const json_t *r) {
    const char *t = exc_type(r);
    return strcmp(t, "ImportError") == 0 || strcmp(t, "ModuleNotFoundError") == 0;
}
/* str(exc) parity: OSError(errno, msg) -> "[Errno N] msg"; else msg. */
static char *exc_str(const json_t *r) {
    const char *msg = json_get_str(r, "message", "");
    json_t *en = json_obj_get(r, "errno");
    size_t cap = strlen(msg) + 48;
    char *out = malloc(cap);
    if (en && en->type == JSON_NUMBER)
        snprintf(out, cap, "[Errno %.0f] %s", en->num_val, msg);
    else
        snprintf(out, cap, "%s", msg);
    return out;
}
/* Normalize an escaping raise descriptor to the oracle's proj_exc shape:
 * SystemExit -> {__raise__, code}; others -> {__raise__, message=str(exc)}. */
static json_t *propagate(json_t *r) {
    if (!mcl_raised(r)) return r;
    if (exc_is_systemexit(r)) {
        json_t *code = json_obj_get(r, "code");
        json_t *o = json_object();
        json_set_hdr(o, "__raise__", "SystemExit");
        json_set(o, "code", code ? json_copy(code) : json_null());
        json_free(r);
        return o;
    }
    char *m = exc_str(r);
    json_t *o = raise_desc(exc_type(r), m);
    free(m);
    json_free(r);
    return o;
}
/* exc.code == 0 (Python: False == 0 is True; "0" == 0 is False). */
static bool exit_code_is_zero(const json_t *r) {
    json_t *code = json_obj_get(r, "code");
    if (!code) return false;
    if (code->type == JSON_NUMBER) return code->num_val == 0.0;
    if (code->type == JSON_BOOL) return !code->bool_val;
    return false;
}

/* ── string helpers (ASCII fixture scope; see header) ──────────────────── */
static char *dupn(const char *s, size_t n) {
    char *o = malloc(n + 1);
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}
static bool py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}
static char *py_strip(const char *s) {
    if (!s) return dupn("", 0);
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && py_isspace((unsigned char)s[a])) a++;
    while (b > a && py_isspace((unsigned char)s[b - 1])) b--;
    return dupn(s + a, b - a);
}
static char *py_strip_chars(const char *s, const char *set) {
    if (!s) return dupn("", 0);
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && strchr(set, (unsigned char)s[a])) a++;
    while (b > a && strchr(set, (unsigned char)s[b - 1])) b--;
    return dupn(s + a, b - a);
}
static char *ascii_lower(const char *s) {
    char *o = dupn(s ? s : "", strlen(s ? s : ""));
    for (char *p = o; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return o;
}
/* os.path.join for two POSIX components (oracle drives posixpath.join). */
static char *posix_join2(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    if (b[0] == '/') return dupn(b, strlen(b));
    if (!a[0]) return dupn(b, strlen(b));
    size_t la = strlen(a), lb = strlen(b);
    bool slash = a[la - 1] == '/';
    char *o = malloc(la + lb + 2);
    memcpy(o, a, la);
    size_t k = la;
    if (!slash) o[k++] = '/';
    memcpy(o + k, b, lb);
    o[k + lb] = '\0';
    return o;
}
/* Python str.splitlines boundaries (UTF-8 aware). Returns malloc'd array of
 * malloc'd lines; a trailing newline does NOT yield an empty final line. */
static char **splitlines(const char *text, size_t *count) {
    size_t cap = 8, n = 0;
    char **out = malloc(cap * sizeof(char *));
    const char *start = text;
    const char *p = text;
    while (*p) {
        size_t adv = 0;
        unsigned char c = (unsigned char)*p;
        if (c == '\r') {
            adv = (p[1] == '\n') ? 2 : 1;
        } else if (c == '\n' || c == '\v' || c == '\f' || (c >= 0x1c && c <= 0x1e)) {
            adv = 1;
        } else if (c == 0xc2 && (unsigned char)p[1] == 0x85) {
            adv = 2;                       /* U+0085 NEL */
        } else if (c == 0xe2 && (unsigned char)p[1] == 0x80
                   && ((unsigned char)p[2] == 0xa8 || (unsigned char)p[2] == 0xa9)) {
            adv = 3;                       /* U+2028 / U+2029 */
        }
        if (adv) {
            if (n == cap) { cap *= 2; out = realloc(out, cap * sizeof(char *)); }
            out[n++] = dupn(start, (size_t)(p - start));
            p += adv;
            start = p;
        } else {
            p++;
        }
    }
    if (p > start) {
        if (n == cap) { cap *= 2; out = realloc(out, cap * sizeof(char *)); }
        out[n++] = dupn(start, (size_t)(p - start));
    }
    *count = n;
    return out;
}
static void free_lines(char **lines, size_t n) {
    for (size_t i = 0; i < n; i++) free(lines[i]);
    free(lines);
}

/* ── injected env / print / args helpers ───────────────────────────────── */
/* os.environ.get(name) / get(name, default): state-resolved seam. */
static json_t *env_get(mcl_io_t *io, const char *name, const char *def) {
    json_t *a = json_object();
    json_set_hdr(a, "name", name);
    json_set(a, "default", def ? json_string(def) : json_null());
    json_t *r = mcl_call(io, "env.get", a);
    json_free(a);
    return r;
}
/* os.environ.pop(name, None): state-mutating seam. */
static json_t *env_pop(mcl_io_t *io, const char *name) {
    json_t *a = json_object();
    json_set_hdr(a, "name", name);
    json_set(a, "default", json_null());
    json_t *r = mcl_call(io, "env.pop", a);
    json_free(a);
    return r;
}
/* builtins.print(text, end=end, file=file) — effect seam. */
static json_t *do_print(mcl_io_t *io, const char *text, const char *end, const char *file) {
    /* Python's print(text) recorder folds the default newline into `text`
     * while keeping end="\n". Normalise every default-end call here. */
    char *folded = NULL;
    if (end && strcmp(end, "\n") == 0 && text && text[0]) {
        size_t tl = strlen(text);
        if (text[tl - 1] != '\n') {
            folded = (char *)malloc(tl + 2);
            if (folded) { memcpy(folded, text, tl); folded[tl] = '\n'; folded[tl + 1] = 0; text = folded; }
        }
    }
    json_t *a = json_object();
    json_set_hdr(a, "text", text ? text : "");
    json_set_hdr(a, "end", end);
    json_set_hdr(a, "file", file);
    json_t *r = mcl_call(io, "print", a);
    json_free(a);
    free(folded);
    return r;
}
/* {"args": <snapshot of in.args>} for sibling-call trace events. */
static json_t *args_snap(json_t *in) {
    json_t *a = json_object();
    json_t *args = json_obj_get(in, "args");
    json_set(a, "args", args ? json_copy(args) : json_object());
    return a;
}
static json_t *field_str(const char *key, const char *val) {
    json_t *a = json_object();
    json_set_hdr(a, key, val);
    return a;
}

/* ── _early_interface_config_path ──────────────────────────────────────── */
/* PoP: mcl_early_interface_config_path @ hermes_cli/main.py:_early_interface_config_path */
json_t *mcl_early_interface_config_path(mcl_io_t *io, json_t *in) {
    (void)in;
    json_t *home = env_get(io, "HERMES_HOME", NULL);
    if (mcl_raised(home)) return propagate(home);
    if (jtruth(home)) {
        char *j = posix_join2(jstr(home), "config.yaml");
        json_t *r = json_string(j);
        free(j);
        json_free(home);
        return r;
    }
    json_free(home);
    json_t *a = field_str("path", "~");
    json_t *ex = mcl_call(io, "expanduser", a);
    json_free(a);
    if (mcl_raised(ex)) return propagate(ex);
    const char *hs = jstr(ex);
    char *j1 = posix_join2(hs ? hs : "", ".hermes");
    char *j2 = posix_join2(j1, "config.yaml");
    free(j1);
    json_t *r = json_string(j2);
    free(j2);
    json_free(ex);
    return r;
}

/* ── _resolve_sudo_user_profile_env ────────────────────────────────────── */
/* PoP: mcl_resolve_sudo_user_profile_env @ hermes_cli/main.py:_resolve_sudo_user_profile_env */
json_t *mcl_resolve_sudo_user_profile_env(mcl_io_t *io, json_t *in) {
    const char *name = json_get_str(in, "name", "");
    if (strcmp(name, "default") == 0) return json_null();
    json_t *sh = mcl_call(io, "sudo_invoker_default_home", NULL);
    if (mcl_raised(sh)) return propagate(sh);
    if (!sh || sh->type == JSON_NULL) {
        json_free(sh);
        return json_null();
    }
    /* candidate = sudo_home / "profiles" / name  (FakePath str is posix) */
    char *j1 = posix_join2(jstr(sh), "profiles");
    char *cand = posix_join2(j1, name);
    free(j1);
    json_free(sh);
    json_t *a = field_str("path", cand);
    json_t *live = mcl_call(io, "named_profile_is_live", a);
    json_free(a);
    if (mcl_raised(live)) { free(cand); return propagate(live); }
    bool is_live = jtruth(live);
    json_free(live);
    if (!is_live) { free(cand); return json_null(); }
    json_t *r = json_string(cand);
    free(cand);
    return r;
}

/* ── _under_gateway_supervisor ─────────────────────────────────────────── */
/* PoP: mcl_under_gateway_supervisor @ hermes_cli/main.py:_under_gateway_supervisor */
json_t *mcl_under_gateway_supervisor(mcl_io_t *io, json_t *in) {
    json_t *v1 = env_get(io, "HERMES_SUPERVISED_CHILD", NULL);
    if (mcl_raised(v1)) return propagate(v1);
    if (jtruth(v1)) { json_free(v1); return json_bool(true); }
    json_free(v1);
    json_t *v2 = env_get(io, "HERMES_S6_SUPERVISED_CHILD", NULL);
    if (mcl_raised(v2)) return propagate(v2);
    if (jtruth(v2)) { json_free(v2); return json_bool(true); }
    json_free(v2);
    /* next((a for a in argv if not a.startswith("-")), None) == "gateway" */
    json_t *argv = json_obj_get(in, "argv");
    const char *first = NULL;
    if (j_is_arr(argv)) {
        for (size_t i = 0; i < json_len(argv); i++) {
            const char *s = jstr(json_get(argv, i));
            if (s && s[0] != '-') { first = s; break; }
        }
    }
    if (first && strcmp(first, "gateway") == 0) {
        json_t *inv = env_get(io, "INVOCATION_ID", NULL);
        if (mcl_raised(inv)) return propagate(inv);
        bool t = jtruth(inv);
        json_free(inv);
        if (t) return json_bool(true);
    }
    static const char *names[2] = {
        "HERMES_GATEWAY_EXTERNAL_SUPERVISOR", "HERMES_GATEWAY_PROFILE_PIN"
    };
    for (int i = 0; i < 2; i++) {
        json_t *v = env_get(io, names[i], "");
        if (mcl_raised(v)) return propagate(v);
        char *s = py_strip(jstr(v));
        char *lw = ascii_lower(s);
        free(s);
        bool hit = strcmp(lw, "1") == 0 || strcmp(lw, "true") == 0
                || strcmp(lw, "yes") == 0 || strcmp(lw, "on") == 0;
        free(lw);
        json_free(v);
        if (hit) return json_bool(true);   /* any() short-circuits */
    }
    return json_bool(false);
}

/* ── _s6_supervised_gateway_run ────────────────────────────────────────── */
/* PoP: mcl_s6_supervised_gateway_run @ hermes_cli/main.py:_s6_supervised_gateway_run */
json_t *mcl_s6_supervised_gateway_run(mcl_io_t *io, json_t *in) {
    json_t *argv = json_obj_get(in, "argv");
    /* words = [a for a in argv if not a.startswith("-")]; words[:2] == [gateway, run] */
    const char *w0 = NULL, *w1 = NULL;
    if (j_is_arr(argv)) {
        for (size_t i = 0; i < json_len(argv); i++) {
            const char *s = jstr(json_get(argv, i));
            if (s && s[0] != '-') {
                if (!w0) w0 = s;
                else if (!w1) { w1 = s; break; }
            }
        }
    }
    if (!w0 || !w1 || strcmp(w0, "gateway") != 0 || strcmp(w1, "run") != 0)
        return json_bool(false);
    json_t *ns = env_get(io, "HERMES_GATEWAY_NO_SUPERVISE", "");
    if (mcl_raised(ns)) return propagate(ns);
    char *lw = ascii_lower(jstr(ns));   /* .lower() only — NO strip here */
    bool no_sup = strcmp(lw, "1") == 0 || strcmp(lw, "true") == 0 || strcmp(lw, "yes") == 0;
    free(lw);
    json_free(ns);
    if (no_sup) return json_bool(false);
    json_t *s6 = mcl_call(io, "_s6_running", NULL);
    if (mcl_raised(s6)) return propagate(s6);
    bool running = jtruth(s6);
    json_free(s6);
    if (!running) return json_bool(false);
    json_t *a = json_object();
    json_set(a, "argv", argv ? json_copy(argv) : json_array());
    json_t *g = mcl_call(io, "gateway_run_no_supervise", a);
    json_free(a);
    if (mcl_raised(g)) return propagate(g);
    bool flag = jtruth(g);
    json_free(g);
    return json_bool(!flag);
}

/* ── explicit_cli_profile ──────────────────────────────────────────────── */
/* PoP: mcl_explicit_cli_profile @ hermes_cli/main.py:explicit_cli_profile */
json_t *mcl_explicit_cli_profile(mcl_io_t *io, json_t *in) {
    (void)io;
    json_t *v = json_obj_get(in, "explicit_cli_profile");
    return v ? json_copy(v) : json_null();
}

/* ── _read_packed_ref ──────────────────────────────────────────────────── */
/* PoP: mcl_read_packed_ref @ hermes_cli/main.py:_read_packed_ref */
json_t *mcl_read_packed_ref(mcl_io_t *io, json_t *in) {
    const char *cd = json_get_str(in, "common_dir", "");
    const char *ref = json_get_str(in, "ref", "");
    char *p = posix_join2(cd, "packed-refs");
    json_t *a = json_object();
    json_set_hdr(a, "path", p);
    json_set_hdr(a, "encoding", "utf-8-sig");
    json_set_hdr(a, "errors", "replace");
    json_t *r = mcl_call(io, "path.read_text", a);
    json_free(a);
    free(p);
    if (mcl_raised(r)) {
        if (exc_is_oserror(r)) { json_free(r); return json_null(); }
        return propagate(r);
    }
    const char *text = jstr(r);
    /* utf-8-sig strips a leading BOM; the scripted seam returns the decoded
     * string, so drop the BOM here to match Python's read_text. */
    if (text && (unsigned char)text[0] == 0xEF &&
            (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        text += 3;
    json_t *out = json_null();
    if (text) {
        size_t n = 0;
        char **lines = splitlines(text, &n);
        for (size_t i = 0; i < n; i++) {
            const char *line = lines[i];
            if (!line[0] || line[0] == '#' || line[0] == '^') continue;
            const char *sp = strchr(line, ' ');
            if (!sp) continue;                 /* len(parts) == 1 */
            char *p1 = py_strip(sp + 1);
            if (strcmp(p1, ref) == 0) {
                char *p0 = dupn(line, (size_t)(sp - line));
                char *p0s = py_strip(p0);
                out = json_string(p0s);
                free(p0);
                free(p0s);
                free(p1);
                break;
            }
            free(p1);
        }
        free_lines(lines, n);
    }
    json_free(r);
    return out;
}

/* ── _dotenv_has_provider_key ──────────────────────────────────────────── */
/* PoP: mcl_dotenv_has_provider_key @ hermes_cli/main.py:_dotenv_has_provider_key */
json_t *mcl_dotenv_has_provider_key(mcl_io_t *io, json_t *in) {
    const char *path = json_get_str(in, "env_file", "");
    json_t *a = field_str("path", path);
    json_t *ex = mcl_call(io, "path.exists", a);
    json_free(a);
    if (mcl_raised(ex)) return propagate(ex);   /* exists() is OUTSIDE the try */
    if (!jtruth(ex)) { json_free(ex); return json_bool(false); }
    json_free(ex);
    a = json_object();
    json_set_hdr(a, "path", path);
    json_set_hdr(a, "encoding", "utf-8-sig");
    json_t *r = mcl_call(io, "path.read_text", a);
    json_free(a);
    if (mcl_raised(r)) {
        if (exc_is_exception(r)) { json_free(r); return json_bool(false); }
        return propagate(r);
    }
    json_t *vars = json_obj_get(in, "provider_env_vars");
    const char *text = jstr(r);
    bool found = false;
    if (text) {
        size_t n = 0;
        char **lines = splitlines(text, &n);
        for (size_t i = 0; i < n && !found; i++) {
            char *s = py_strip(lines[i]);
            if (s[0] == '#' || !strchr(s, '=')) { free(s); continue; }
            char *body = s;
            if (strncmp(body, "export ", 7) == 0) body += 7;
            const char *eq = strchr(body, '=');
            char *key = dupn(body, (size_t)(eq - body));
            char *keys = py_strip(key);
            free(key);
            char *vals = py_strip(eq + 1);
            char *valq = py_strip_chars(vals, "'\"");
            free(vals);
            bool in_set = false;
            if (j_is_arr(vars)) {
                for (size_t k = 0; k < json_len(vars); k++) {
                    const char *kv = jstr(json_get(vars, k));
                    if (kv && strcmp(kv, keys) == 0) { in_set = true; break; }
                }
            }
            if (in_set && valq[0] != '\0') found = true;
            free(keys);
            free(valq);
            free(s);
        }
        free_lines(lines, n);
    }
    json_free(r);
    return json_bool(found);
}
/* ── _latest_session_id ────────────────────────────────────────────────── */
/* PoP: mcl_latest_session_id @ hermes_cli/main.py:_latest_session_id */
json_t *mcl_latest_session_id(mcl_io_t *io, json_t *in) {
    bool use_tui = jtruth(json_obj_get(in, "use_tui"));
    json_t *a = field_str("source", use_tui ? "tui" : "cli");
    json_t *last = mcl_call(io, "_resolve_last_session", a);
    json_free(a);
    if (mcl_raised(last)) return propagate(last);
    if (!jtruth(last) && use_tui) {
        json_free(last);
        a = field_str("source", "cli");
        last = mcl_call(io, "_resolve_last_session", a);
        json_free(a);
        if (mcl_raised(last)) return propagate(last);
    }
    return last;
}

/* ── _first_run_setup_guard ────────────────────────────────────────────── */
/* PoP: mcl_first_run_setup_guard @ hermes_cli/main.py:_first_run_setup_guard */
json_t *mcl_first_run_setup_guard(mcl_io_t *io, json_t *in) {
    /* The oracle models Python print() as text including its trailing newline
     * with end="\n" retained; mirror that exactly. */
    json_t *pr = do_print(io, "\n", "\n", "stdout");
    if (mcl_raised(pr)) return propagate(pr);
    json_free(pr);
    pr = do_print(io, "It looks like Hermes isn't configured yet -- no API keys or providers found.\n",
                  "\n", "stdout");
    if (mcl_raised(pr)) return propagate(pr);
    json_free(pr);
    pr = do_print(io, "\n", "\n", "stdout");
    if (mcl_raised(pr)) return propagate(pr);
    json_free(pr);
    pr = do_print(io, "  Run:  hermes setup\n", "\n", "stdout");
    if (mcl_raised(pr)) return propagate(pr);
    json_free(pr);
    pr = do_print(io, "\n", "\n", "stdout");
    if (mcl_raised(pr)) return propagate(pr);
    json_free(pr);

    json_t *it = mcl_call(io, "is_interactive_stdin", NULL);
    if (mcl_raised(it)) return propagate(it);
    bool interactive = jtruth(it);
    json_free(it);
    if (!interactive) {
        json_t *a = field_str("message",
            "No interactive TTY detected for the first-run setup prompt.");
        json_t *g = mcl_call(io, "print_noninteractive_setup_guidance", a);
        json_free(a);
        if (mcl_raised(g)) return propagate(g);
        json_free(g);
        return exit_node(1);
    }

    /* reply = input("Run setup now? [Y/n] ").strip().lower();
     * except (EOFError, KeyboardInterrupt): reply = "n" */
    json_t *ia = field_str("prompt", "Run setup now? [Y/n] ");
    json_t *ir = mcl_call(io, "input", ia);
    json_free(ia);
    char *reply = NULL;
    if (mcl_raised(ir)) {
        const char *t = exc_type(ir);
        if (strcmp(t, "EOFError") == 0 || strcmp(t, "KeyboardInterrupt") == 0) {
            json_free(ir);
            reply = dupn("n", 1);
        } else {
            return propagate(ir);
        }
    } else {
        char *s = py_strip(jstr(ir));
        reply = ascii_lower(s);
        free(s);
        json_free(ir);
    }
    bool yes = strcmp(reply, "") == 0 || strcmp(reply, "y") == 0 || strcmp(reply, "yes") == 0;
    free(reply);
    if (yes) {
        json_t *a = args_snap(in);
        json_t *r = mcl_call(io, "cmd_setup", a);
        json_free(a);
        if (mcl_raised(r)) return propagate(r);
        json_free(r);
        return json_null();
    }
    pr = do_print(io, "\n", "\n", "stdout");
    if (mcl_raised(pr)) return propagate(pr);
    json_free(pr);
    pr = do_print(io, "You can run 'hermes setup' at any time to configure.\n", "\n", "stdout");
    if (mcl_raised(pr)) return propagate(pr);
    json_free(pr);
    return exit_node(1);
}

/* ── _finalize_update_receipt ──────────────────────────────────────────── */
/* PoP: mcl_finalize_update_receipt @ hermes_cli/main.py:_finalize_update_receipt */
json_t *mcl_finalize_update_receipt(mcl_io_t *io, json_t *in) {
    json_t *code = json_obj_get(in, "code");
    json_t *a = json_object();
    json_set(a, "code", code ? json_copy(code) : json_number(0));
    json_set_hdr(a, "reason", json_get_str(in, "reason", ""));
    json_t *r = mcl_call(io, "finalize_pending_update_receipt", a);
    json_free(a);
    if (mcl_raised(r)) {
        if (exc_is_exception(r)) { json_free(r); return json_null(); }
        return propagate(r);   /* SystemExit/KeyboardInterrupt escape except Exception */
    }
    json_free(r);
    return json_null();
}

/* ── _update_preflight_handled ─────────────────────────────────────────── */
/* PoP: mcl_update_preflight_handled @ hermes_cli/main.py:_update_preflight_handled */
json_t *mcl_update_preflight_handled(mcl_io_t *io, json_t *in) {
    json_t *args = json_obj_get(in, "args");
    const char *root = json_get_str(in, "project_root", "");

    json_t *a = args_snap(in);
    json_set_hdr(a, "project_root", root);
    json_t *r = mcl_call(io, "handle_metadata_args", a);
    json_free(a);
    if (mcl_raised(r)) return propagate(r);
    if (jtruth(r)) { json_free(r); return exit_node(0); }
    json_free(r);

    r = mcl_call(io, "is_managed", NULL);
    if (mcl_raised(r)) return propagate(r);
    bool managed = jtruth(r);
    json_free(r);
    if (managed) {
        a = field_str("action", "update Hermes Agent");
        r = mcl_call(io, "managed_error", a);
        json_free(a);
        if (mcl_raised(r)) return propagate(r);
        json_free(r);
        bool any_flag = jtruth(json_obj_get(args, "plan"))
                     || jtruth(json_obj_get(args, "check"))
                     || jtruth(json_obj_get(args, "list_venv_holders"));
        if (!any_flag) {
            a = json_object();
            json_set_hdr(a, "reason", "managed_install");
            json_set_hdr(a, "without_receipt", "refused");
            r = mcl_call(io, "_record_stop", a);
            json_free(a);
            if (mcl_raised(r)) return propagate(r);
            json_free(r);
        }
        return json_bool(true);
    }

    if (jtruth(json_obj_get(args, "plan"))) {
        json_t *inv = mcl_call(io, "collect_runtime_inventory", NULL);
        if (mcl_raised(inv)) return propagate(inv);
        a = json_object();
        json_set(a, "inventory", json_copy(inv));
        r = mcl_call(io, "print_update_plan", a);
        json_free(a);
        json_free(inv);
        if (mcl_raised(r)) return propagate(r);
        json_free(r);
        return json_bool(true);
    }

    if (jtruth(json_obj_get(args, "list_venv_holders"))) {
        json_t *holders = mcl_call(io, "list_venv_holders", NULL);
        if (mcl_raised(holders)) return propagate(holders);
        a = json_object();
        json_set(a, "value", json_copy(holders));
        json_set(a, "indent", json_number(2));
        json_t *d = mcl_call(io, "json.dumps", a);
        json_free(a);
        if (mcl_raised(d)) { json_free(holders); return propagate(d); }
        json_t *pr = do_print(io, jstr(d), "\n", "stdout");
        json_free(d);
        if (mcl_raised(pr)) { json_free(holders); return propagate(pr); }
        json_free(pr);
        if (jtruth(holders)) {
            json_free(holders);
            return exit_node(3);   /* VENV_HOLDERS_EXIT (update_cmd_windows) */
        }
        json_free(holders);
        return json_bool(true);
    }

    a = field_str("project_root", root);
    json_t *refusal = mcl_call(io, "evaluate_update_admission", a);
    json_free(a);
    if (mcl_raised(refusal)) return propagate(refusal);
    if (refusal && refusal->type != JSON_NULL) {   /* `refusal is not None` */
        json_t *pr = do_print(io, json_get_str(refusal, "message", ""), "\n", "stdout");
        if (mcl_raised(pr)) { json_free(refusal); return propagate(pr); }
        json_free(pr);
        a = json_object();
        json_set(a, "refusal", json_copy(refusal));
        r = mcl_call(io, "record_refusal_receipt", a);
        json_free(a);
        json_free(refusal);
        if (mcl_raised(r)) return propagate(r);
        json_free(r);
        return exit_node(2);
    }
    json_free(refusal);

    if (jtruth(json_obj_get(args, "check"))) {
        a = args_snap(in);
        json_t *branch = mcl_call(io, "_resolve_update_branch", a);
        json_free(a);
        if (mcl_raised(branch)) return propagate(branch);
        a = json_object();
        json_set(a, "branch", json_copy(branch));
        json_set(a, "branch_explicit", json_bool(jtruth(json_obj_get(args, "branch"))));
        json_t *ch = json_obj_get(args, "channel");
        if (jtruth(ch)) json_set(a, "channel", json_copy(ch));
        r = mcl_call(io, "_cmd_update_check", a);
        json_free(a);
        json_free(branch);
        if (mcl_raised(r)) return propagate(r);
        json_free(r);
        return json_bool(true);
    }
    return json_bool(false);
}

/* ── _dashboard_lifecycle_flags ────────────────────────────────────────── */
/* PoP: mcl_dashboard_lifecycle_flags @ hermes_cli/main.py:_dashboard_lifecycle_flags */
json_t *mcl_dashboard_lifecycle_flags(mcl_io_t *io, json_t *in) {
    json_t *args = json_obj_get(in, "args");
    json_t *token = json_obj_get(in, "token_file");
    bool status = jtruth(json_obj_get(args, "status"));
    bool stop = jtruth(json_obj_get(args, "stop"));
    if (jtruth(token) && (status || stop))
        return exit_str_node("--ssh-session-token-file cannot be used with --status or --stop");
    if (status) {
        json_t *r = mcl_call(io, "_report_dashboard_status", NULL);
        if (mcl_raised(r)) return propagate(r);
        json_free(r);
        return exit_node(0);   /* status is informational, always 0 */
    }
    if (stop) {
        json_t *h = mcl_call(io, "get_hermes_home", NULL);
        if (mcl_raised(h)) return propagate(h);
        const char *own_home = jstr(h);   /* str(get_hermes_home()) — posix */
        json_t *a = field_str("scope_home", own_home ? own_home : "");
        json_t *pids = mcl_call(io, "_find_stale_dashboard_pids", a);
        json_free(a);
        if (mcl_raised(pids)) { json_free(h); return propagate(pids); }
        if (!jtruth(pids)) {
            json_free(pids);
            json_free(h);
            json_t *pr = do_print(io, "No hermes dashboard processes running for this profile.",
                                  "\n", "stdout");
            if (mcl_raised(pr)) return propagate(pr);
            json_free(pr);
            return exit_node(0);
        }
        json_free(pids);
        a = json_object();
        json_set_hdr(a, "reason", "requested via --stop");
        json_set_hdr(a, "scope_home", own_home ? own_home : "");
        json_t *result = mcl_call(io, "_kill_stale_dashboard_processes", a);
        json_free(a);
        json_free(h);
        if (mcl_raised(result)) return propagate(result);
        /* exit 1 iff result["failed"] is truthy (KeyError/TypeError if absent) */
        if (!j_is_obj(result)) {
            json_free(result);
            return raise_desc("TypeError", "'NoneType' object is not subscriptable");
        }
        json_t *failed = json_obj_get(result, "failed");
        if (!failed) {
            json_free(result);
            return raise_desc("KeyError", "'failed'");
        }
        bool any_failed = jtruth(failed);
        json_free(result);
        return exit_node(any_failed ? 1 : 0);
    }
    return json_null();
}

/* ── _dashboard_sanitize_desktop_env ───────────────────────────────────── */
/* PoP: mcl_dashboard_sanitize_desktop_env @ hermes_cli/main.py:_dashboard_sanitize_desktop_env */
json_t *mcl_dashboard_sanitize_desktop_env(mcl_io_t *io, json_t *in) {
    bool headless = jtruth(json_obj_get(in, "headless_backend"));
    json_t *r = mcl_call(io, "_is_desktop_owned_backend", NULL);
    if (mcl_raised(r)) return propagate(r);
    bool desktop = jtruth(r);
    json_free(r);
    if (!headless && !desktop) {
        json_t *v = env_get(io, "HERMES_WEB_DIST", "");
        if (mcl_raised(v)) return propagate(v);
        json_t *a = field_str("value", jstr(v) ? jstr(v) : "");
        json_t *pk = mcl_call(io, "_is_electron_packaged_web_dist", a);
        json_free(a);
        json_free(v);
        if (mcl_raised(pk)) return propagate(pk);
        bool packaged = jtruth(pk);
        json_free(pk);
        if (packaged) {
            json_t *p = env_pop(io, "HERMES_WEB_DIST");
            if (mcl_raised(p)) return propagate(p);
            json_free(p);
        }
    }
    if (!headless) {
        json_t *p = env_pop(io, "HERMES_SERVE_HEADLESS");
        if (mcl_raised(p)) return propagate(p);
        json_free(p);
    }
    return json_null();
}

/* ── _require_dashboard_web_deps ───────────────────────────────────────── */
/* The shared ImportError path: smart-app-control message OR repair guidance,
 * then "Details: {e}", then sys.exit(1). Consumes the raise descriptor. */
static json_t *dep_import_error(mcl_io_t *io, json_t *r) {
    char *es = exc_str(r);
    json_free(r);
    json_t *a = field_str("error", es);
    json_t *sm = mcl_call(io, "smart_app_control_block_message", a);
    json_free(a);
    if (mcl_raised(sm)) { free(es); return propagate(sm); }
    json_t *mm = NULL;
    const char *text;
    if (jtruth(sm)) {
        text = jstr(sm);
    } else {
        a = json_object();
        json_set_hdr(a, "surface", "dashboard");
        json_set_hdr(a, "what", "its web-server packages (fastapi, uvicorn)");
        json_set_hdr(a, "extra", "all");
        mm = mcl_call(io, "missing_optional_deps_message", a);
        json_free(a);
        if (mcl_raised(mm)) { json_free(sm); free(es); return propagate(mm); }
        text = jstr(mm);
    }
    json_t *pr = do_print(io, text ? text : "", "\n", "stdout");
    if (mcl_raised(pr)) { json_free(sm); json_free(mm); free(es); return propagate(pr); }
    json_free(pr);
    size_t cap = strlen(es) + 16;
    char *det = malloc(cap);
    snprintf(det, cap, "Details: %s", es);
    pr = do_print(io, det, "\n", "stdout");
    free(det);
    json_free(sm);
    json_free(mm);
    free(es);
    if (mcl_raised(pr)) return propagate(pr);
    json_free(pr);
    return exit_node(1);
}

/* PoP: mcl_require_dashboard_web_deps @ hermes_cli/main.py:_require_dashboard_web_deps */
json_t *mcl_require_dashboard_web_deps(mcl_io_t *io, json_t *in) {
    (void)in;
    json_t *r = mcl_call(io, "import.fastapi", NULL);
    if (mcl_raised(r)) {
        if (exc_is_importerror(r)) return dep_import_error(io, r);
        return propagate(r);
    }
    json_free(r);
    r = mcl_call(io, "import.uvicorn", NULL);
    if (mcl_raised(r)) {
        if (exc_is_importerror(r)) return dep_import_error(io, r);
        return propagate(r);
    }
    json_free(r);
    return json_null();
}
/* ── _guard_noninteractive_user_config ─────────────────────────────────── */
/* PoP: mcl_guard_noninteractive_user_config @ hermes_cli/main.py:_guard_noninteractive_user_config */
json_t *mcl_guard_noninteractive_user_config(mcl_io_t *io, json_t *in) {
    json_t *args = json_obj_get(in, "args");
    if (jtruth(json_obj_get(args, "_noninteractive_config_validated")))
        return json_null();
    bool is_noninteractive = jtruth(json_obj_get(args, "oneshot"))
                          || jtruth(json_obj_get(args, "query"));
    if (!is_noninteractive) return json_null();
    bool ignore = jtruth(json_obj_get(args, "ignore_user_config"))
               || jtruth(json_obj_get(args, "safe_mode"));
    json_t *a = json_object();
    json_set(a, "ignore_user_config", json_bool(ignore));
    json_t *r = mcl_call(io, "require_parseable_user_config", a);
    json_free(a);
    if (mcl_raised(r)) {
        if (strcmp(exc_type(r), "InvalidUserConfigError") == 0) {
            char *es = exc_str(r);
            json_free(r);
            size_t cap = strlen(es) + 16;
            char *msg = malloc(cap);
            snprintf(msg, cap, "Error: %s", es);
            free(es);
            json_t *pr = do_print(io, msg, "\n", "stderr");
            free(msg);
            if (mcl_raised(pr)) return propagate(pr);
            json_free(pr);
            return exit_node(2);   /* raise SystemExit(2) from exc */
        }
        return propagate(r);
    }
    json_free(r);
    if (args && args->type == JSON_OBJECT)
        json_set(args, "_noninteractive_config_validated", json_bool(true));
    return json_null();
}

/* ── _is_tui_chat_launch ───────────────────────────────────────────────── */
/* PoP: mcl_is_tui_chat_launch @ hermes_cli/main.py:_is_tui_chat_launch */
json_t *mcl_is_tui_chat_launch(mcl_io_t *io, json_t *in) {
    json_t *a = args_snap(in);
    json_t *r = mcl_call(io, "_bypass_chat_launch", a);
    json_free(a);
    if (mcl_raised(r)) return propagate(r);
    if (jtruth(r)) { json_free(r); return json_bool(false); }
    json_free(r);
    json_t *args = json_obj_get(in, "args");
    if (jtruth(json_obj_get(args, "tui"))) return json_bool(true);
    json_t *v = env_get(io, "HERMES_TUI", NULL);
    if (mcl_raised(v)) return propagate(v);
    const char *vs = jstr(v);
    bool env_tui = vs && strcmp(vs, "1") == 0;
    json_free(v);
    if (env_tui) return json_bool(true);
    /* getattr(args, "command", None) not in {None, "chat"} */
    json_t *cmd = json_obj_get(args, "command");
    bool chatish = !cmd || cmd->type == JSON_NULL
                || (j_is_str(cmd) && strcmp(cmd->str_val, "chat") == 0);
    if (!chatish) return json_bool(false);
    a = args_snap(in);
    r = mcl_call(io, "_resolve_use_tui", a);
    json_free(a);
    if (mcl_raised(r)) return propagate(r);
    return r;   /* live returns _resolve_use_tui(args) verbatim */
}

/* ── _default_to_chat ──────────────────────────────────────────────────── */
/* PoP: mcl_default_to_chat @ hermes_cli/main.py:_default_to_chat */
json_t *mcl_default_to_chat(mcl_io_t *io, json_t *in) {
    json_t *a = args_snap(in);
    json_t *r = mcl_call(io, "_promote_top_level_resume", a);
    json_free(a);
    if (mcl_raised(r)) return propagate(r);
    json_free(r);
    a = args_snap(in);
    r = mcl_call(io, "_set_chat_arg_defaults", a);
    json_free(a);
    if (mcl_raised(r)) return propagate(r);
    json_free(r);
    a = args_snap(in);
    r = mcl_call(io, "cmd_chat", a);
    json_free(a);
    if (mcl_raised(r)) return propagate(r);
    json_free(r);
    return json_null();
}

/* ── _parse_cli_args ───────────────────────────────────────────────────── */
/* The bpo-9338 subparser-routing workaround. parser.parse_args seam args
 * carry the CURRENT subparsers.required and a speculative flag (true only
 * for the call inside the redirect region); the harness routes the script's
 * "stdout"/"stderr" effects accordingly (speculative stderr is discarded).
 * stringio.getvalue is an echo seam returning the speculative buffer. */
/* Python's recorder does list(argv): a string argv becomes a list of chars. */
static json_t *mcl_argv_value(const json_t *v) {
    if (v && v->type == JSON_STRING) {
        json_t *arr = json_array();
        const char *p = v->str_val ? v->str_val : "";
        for (const unsigned char *c = (const unsigned char *)p; *c; c++) {
            char one[2] = {(char)*c, 0};
            json_append(arr, json_string(one));
        }
        return arr;
    }
    return v ? json_copy(v) : json_array();
}

/* PoP: mcl_parse_cli_args @ hermes_cli/main.py:_parse_cli_args */
json_t *mcl_parse_cli_args(mcl_io_t *io, json_t *in) {
    json_t *argv = json_obj_get(in, "argv");
    json_t *a = json_object();
    json_set(a, "argv", argv ? json_copy(argv) : json_array());
    json_t *processed = mcl_call(io, "_coalesce_session_name_args", a);
    json_free(a);
    if (mcl_raised(processed)) return propagate(processed);

    json_t *sp = json_obj_get(in, "subparsers");
    json_t *known = NULL;
    if (jtruth(json_obj_get(sp, "has_choices")))
        known = json_obj_get(sp, "choices");
    bool has_cmd_token = false;
    if (j_is_arr(processed)) {
        for (size_t i = 0; i < json_len(processed) && !has_cmd_token; i++) {
            const char *t = jstr(json_get(processed, i));
            if (!t || t[0] == '-') continue;
            if (j_is_arr(known)) {
                for (size_t k = 0; k < json_len(known); k++) {
                    const char *c = jstr(json_get(known, k));
                    if (c && strcmp(c, t) == 0) { has_cmd_token = true; break; }
                }
            }
        }
    }

    if (!has_cmd_token) {
        if (j_is_obj(sp)) json_set(sp, "required", json_bool(false));
        a = json_object();
        json_set(a, "argv", mcl_argv_value(processed));
        json_set(a, "required", json_bool(false));
        json_set(a, "speculative", json_bool(false));
        json_t *r = mcl_call(io, "parser.parse_args", a);
        json_free(a);
        json_free(processed);
        if (mcl_raised(r)) return propagate(r);
        return r;
    }

    if (j_is_obj(sp)) json_set(sp, "required", json_bool(true));
    a = json_object();
    json_set(a, "argv", mcl_argv_value(processed));
    json_set(a, "required", json_bool(true));
    json_set(a, "speculative", json_bool(true));
    json_t *r = mcl_call(io, "parser.parse_args", a);
    json_free(a);
    if (mcl_raised(r)) {
        if (exc_is_systemexit(r) && exit_code_is_zero(r)) {
            /* help text: echo the speculative stdout, then re-raise */
            json_free(processed);
            json_t *gv = mcl_call(io, "stringio.getvalue", NULL);
            if (mcl_raised(gv)) { json_free(r); return propagate(gv); }
            json_t *pr = do_print(io, jstr(gv), "", "stdout");
            json_free(gv);
            if (mcl_raised(pr)) { json_free(r); return propagate(pr); }
            json_free(pr);
            return propagate(r);
        }
        /* discard speculative diagnostics; fall back to default behaviour */
        if (j_is_obj(sp)) json_set(sp, "required", json_bool(false));
        json_free(r);
        a = json_object();
        json_set(a, "argv", processed ? json_copy(processed) : json_array());
        json_set(a, "required", json_bool(false));
        json_set(a, "speculative", json_bool(false));
        json_free(processed);
        json_t *r2 = mcl_call(io, "parser.parse_args", a);
        json_free(a);
        if (mcl_raised(r2)) return propagate(r2);
        return r2;
    }
    json_free(processed);
    json_t *gv = mcl_call(io, "stringio.getvalue", NULL);
    if (mcl_raised(gv)) { json_free(r); return propagate(gv); }
    json_t *pr = do_print(io, jstr(gv), "", "stdout");
    json_free(gv);
    if (mcl_raised(pr)) { json_free(r); return propagate(pr); }
    json_free(pr);
    return r;
}
