/*
 * port_gw_service_lifecycle.c — Port of Python hermes_cli/gateway.py
 * service-lifecycle FLOW cores + high-value leaves.
 *
 * Decision sequences only: every OS / service / registry / subprocess probe
 * enters through gwsl_seams_t (see the header for the live counterpart of each
 * member). sys.exit is a gwsl_verdict_t; uncaught exceptions are gwsl_exc_t.
 * tests/oracle_gw_service_lifecycle.py drives the LIVE functions under the same
 * scripted seam results and compares the ordered trace + ret + verdict.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "port_gw_service_lifecycle.h"

/* ── trace ── */
void gwsl_trace_add(gwsl_trace_t *t, json_t *ev) {
    if (!t) { json_free(ev); return; }
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
void gwsl_trace_free(gwsl_trace_t *t) {
    if (t && t->events) { json_free(t->events); t->events = NULL; }
}

/* ── exception model ── */
gwsl_exc_kind_t gwsl_exc_kind_from_name(const char *name) {
    if (!name || !*name) return GWSL_EXC_NONE;
    if (strcmp(name, "CalledProcessError") == 0) return GWSL_EXC_CALLED_PROCESS;
    if (strcmp(name, "RuntimeError") == 0) return GWSL_EXC_RUNTIME;
    if (strcmp(name, "OSError") == 0) return GWSL_EXC_OSERROR;
    if (strcmp(name, "TimeoutExpired") == 0) return GWSL_EXC_TIMEOUT;
    if (strcmp(name, "SystemExit") == 0) return GWSL_EXC_SYSTEM_EXIT;
    if (strcmp(name, "KeyError") == 0) return GWSL_EXC_KEY_ERROR;
    return GWSL_EXC_RUNTIME;
}
const char *gwsl_exc_kind_name(gwsl_exc_kind_t k) {
    switch (k) {
        case GWSL_EXC_CALLED_PROCESS: return "CalledProcessError";
        case GWSL_EXC_RUNTIME: return "RuntimeError";
        case GWSL_EXC_OSERROR: return "OSError";
        case GWSL_EXC_TIMEOUT: return "TimeoutExpired";
        case GWSL_EXC_SYSTEM_EXIT: return "SystemExit";
        case GWSL_EXC_KEY_ERROR: return "KeyError";
        default: return NULL;
    }
}

/* ── small helpers ── */
static char *gfmt(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    char *out = NULL;
    if (vasprintf(&out, fmt, ap) < 0) out = NULL;
    va_end(ap);
    if (!out) out = strdup("");
    return out;
}
static void pr(const gwsl_seams_t *s, const char *text) { s->print(s->ctx, text); }
static void prf(const gwsl_seams_t *s, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    char *m = NULL;
    if (vasprintf(&m, fmt, ap) < 0) m = NULL;
    va_end(ap);
    s->print(s->ctx, m ? m : "");
    free(m);
}
static bool starts_with(const char *s, const char *p) {
    return strncmp(s, p, strlen(p)) == 0;
}
static bool ends_with(const char *s, const char *p) {
    size_t ls = strlen(s), lp = strlen(p);
    return ls >= lp && strcmp(s + ls - lp, p) == 0;
}
/* Python str.replace: left-to-right, non-overlapping. malloc'd result. */
static char *str_replace_all(const char *src, const char *from, const char *to) {
    size_t lf = strlen(from), lt = strlen(to);
    if (lf == 0) return strdup(src);
    size_t cap = strlen(src) + 1, len = 0;
    char *out = malloc(cap);
    const char *p = src;
    while (*p) {
        if (strncmp(p, from, lf) == 0) {
            while (len + lt + 1 > cap) { cap *= 2; out = realloc(out, cap); }
            memcpy(out + len, to, lt); len += lt; p += lf;
        } else {
            if (len + 2 > cap) { cap *= 2; out = realloc(out, cap); }
            out[len++] = *p++;
        }
    }
    out[len] = '\0';
    return out;
}

/* _print_lines(*lines): one print per line. */
/* PoP: gwsl_print_lines @ hermes_cli/gateway.py:_print_lines */
static void gwsl_print_lines(const gwsl_seams_t *s, const char *const *lines, size_t n) {
    for (size_t i = 0; i < n; i++) pr(s, lines[i]);
}

/* _service_mgmt_blocked(): is_termux() or _is_apt_termux_install(). */
/* PoP: gwsl_service_mgmt_blocked @ hermes_cli/gateway.py:_service_mgmt_blocked */
static bool gwsl_service_mgmt_blocked(const gwsl_seams_t *s) {
    if (s->is_termux(s->ctx)) return true;
    return s->is_apt_termux(s->ctx);
}

/* _status_host_kind(): termux -> wsl -> windows -> other. */
/* PoP: gwsl_status_host_kind @ hermes_cli/gateway.py:_status_host_kind */
static const char *gwsl_status_host_kind(const gwsl_seams_t *s) {
    if (s->is_termux(s->ctx)) return "termux";
    if (s->is_wsl(s->ctx)) return "wsl";
    return s->is_windows(s->ctx) ? "windows" : "other";
}

/* _running_under_s6(): detect_service_manager() == "s6". */
/* PoP: gwsl_running_under_s6 @ hermes_cli/gateway.py:_running_under_s6 */
static bool gwsl_running_under_s6(const gwsl_seams_t *s) {
    char *m = s->detect_service_mgr(s->ctx);
    bool r = m && strcmp(m, "s6") == 0;
    free(m);
    return r;
}

/* _refuse_from_inside_gateway(verb, reason): print_error + sys.exit(1) when the
 * current process IS the supervised gateway. */
/* PoP: gwsl_refuse_from_inside_gateway @ hermes_cli/gateway.py:_refuse_from_inside_gateway */
static gwsl_verdict_t gwsl_refuse_from_inside_gateway(const char *verb, const char *reason,
                                                      const gwsl_seams_t *s) {
    gwsl_verdict_t v = {false, 0};
    if (s->is_supervised(s->ctx)) {
        char *m = gfmt("Refusing to %s the gateway from inside the gateway process.\n"
                       "This command was blocked to prevent %s.\n"
                       "Use `hermes gateway %s` from a shell outside the running gateway.",
                       verb, reason, verb);
        s->print_error(s->ctx, m);
        free(m);
        v.exited = true; v.exit_code = 1;
    }
    return v;
}

/* Propagate a seam exception: SystemExit becomes a verdict, anything else is
 * left in *exc for the caller to report. Returns true when the flow must stop. */
static bool exc_stop(gwsl_exc_t *exc, gwsl_verdict_t *v) {
    if (exc->kind == GWSL_EXC_NONE) return false;
    if (exc->kind == GWSL_EXC_SYSTEM_EXIT) { v->exited = true; v->exit_code = exc->exit_code; }
    return true;
}

/* ── positive pid ── */
/* int(s) for a decimal string with optional sign/whitespace/underscores;
 * returns -1 on ValueError. Mirrors CPython int(str) base-10 grammar. */
static long parse_int_str(const char *raw, bool *ok) {
    *ok = false;
    const char *p = raw;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *end = raw + strlen(raw);
    while (end > p && isspace((unsigned char)end[-1])) end--;
    bool neg = false;
    if (p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); p++; }
    if (p >= end) return -1;
    long val = 0;
    bool prev_digit = false, any = false;
    for (; p < end; p++) {
        char ch = *p;
        if (ch == '_') {
            if (!prev_digit) return -1;
            prev_digit = false;
            continue;
        }
        if (ch < '0' || ch > '9') return -1;
        val = val * 10 + (ch - '0');
        prev_digit = true; any = true;
    }
    if (!any || !prev_digit) return -1;
    *ok = true;
    return neg ? -val : val;
}

/* PoP: gwsl_positive_pid @ hermes_cli/gateway.py:_positive_pid */
long gwsl_positive_pid(const json_t *value) {
    /* int(value or 0): falsy -> 0 -> None */
    if (!value || value->type == JSON_NULL) return -1;
    if (value->type == JSON_BOOL) return value->bool_val ? 1 : -1;
    if (value->type == JSON_NUMBER) {
        double d = value->num_val;
        if (d == 0.0) return -1;
        long pid = (long)d;   /* int(float) truncates toward zero */
        return pid > 0 ? pid : -1;
    }
    if (value->type == JSON_STRING) {
        const char *s = value->str_val;
        if (!s || !*s) return -1;   /* "" or 0 */
        bool ok = false;
        long pid = parse_int_str(s, &ok);
        if (!ok) return -1;
        return pid > 0 ? pid : -1;
    }
    return -1;
}

/* ── service backend / kind / call / stop ── */

/* PoP: gwsl_service_backend @ hermes_cli/gateway.py:_service_backend */
const char *gwsl_service_backend(bool windows, const gwsl_seams_t *s) {
    if (s->supports_systemd(s->ctx)) return "systemd";
    if (s->is_macos(s->ctx)) return "launchd";
    if (windows && s->is_windows(s->ctx)) return "windows";
    return NULL;
}

/* PoP: gwsl_installed_service_kind_for @ hermes_cli/gateway.py:_installed_service_kind_for */
const char *gwsl_installed_service_kind_for(const gwsl_seams_t *s,
                                            bool (*windows_thunk)(void *thunk_ctx),
                                            void *thunk_ctx) {
    if (s->systemd_unit_installed(s->ctx)) return "systemd";
    if (s->is_macos(s->ctx) && s->launchd_plist_exists(s->ctx)) return "launchd";
    bool w = windows_thunk ? windows_thunk(thunk_ctx) : false;
    return w ? "windows" : NULL;
}

/* thunk for _installed_service_kind: is_windows() and _gw_windows().is_installed() */
static bool kind_windows_thunk(void *vctx) {
    const gwsl_seams_t *s = vctx;
    if (!s->is_windows(s->ctx)) return false;
    gwsl_exc_t e = {GWSL_EXC_NONE, 0};
    json_t *r = s->gw_windows_call(s->ctx, "is_installed", NULL, &e);
    bool b = json_is_true(r);
    json_free(r);
    return b;
}
/* thunk for the bare is_windows function (restart / restart_all_as_host) */
static bool is_windows_thunk(void *vctx) {
    const gwsl_seams_t *s = vctx;
    return s->is_windows(s->ctx);
}

/* PoP: gwsl_installed_service_kind @ hermes_cli/gateway.py:_installed_service_kind */
const char *gwsl_installed_service_kind(const gwsl_seams_t *s) {
    return gwsl_installed_service_kind_for(s, kind_windows_thunk, (void *)s);
}

/* PoP: gwsl_service_call @ hermes_cli/gateway.py:_service_call */
void gwsl_service_call(const char *backend, const char *verb, int system_tristate,
                       const gwsl_seams_t *s, gwsl_exc_t *exc) {
    /* LIVE resolves the callable through globals()[f"systemd_{verb}"] (and the
     * equivalent getattr/launchd lookup). An unknown verb is a KeyError
     * ("systemd_<verb>") raised before any seam is touched. */
    static const char *VALID_VERBS[] = {"start", "stop", "restart", "uninstall"};
    bool valid = false;
    if (verb) {
        for (size_t i = 0; i < sizeof(VALID_VERBS) / sizeof(VALID_VERBS[0]); i++)
            if (strcmp(verb, VALID_VERBS[i]) == 0) { valid = true; break; }
    }
    if (!valid) {
        if (exc) exc->kind = GWSL_EXC_KEY_ERROR;
        return;
    }
    if (strcmp(backend, "windows") == 0) {
        json_t *r = s->gw_windows_call(s->ctx, verb, NULL, exc);
        json_free(r);
        return;
    }
    if (strcmp(backend, "launchd") == 0) {
        s->launchd_call(s->ctx, verb, exc);
        return;
    }
    s->systemd_call(s->ctx, verb, system_tristate, exc);
}

/* PoP: gwsl_stop_installed_service @ hermes_cli/gateway.py:_stop_installed_service */
bool gwsl_stop_installed_service(bool system, const gwsl_seams_t *s, gwsl_exc_t *exc) {
    const char *kind = gwsl_installed_service_kind(s);
    if (!kind) return false;
    gwsl_exc_t e = {GWSL_EXC_NONE, 0};
    gwsl_service_call(kind, "stop", system ? 1 : 0, s, &e);
    if (e.kind == GWSL_EXC_CALLED_PROCESS) return false;
    if (e.kind == GWSL_EXC_RUNTIME && strcmp(kind, "windows") == 0) return false;
    if (e.kind != GWSL_EXC_NONE) { *exc = e; return false; }
    return true;
}

/* ── profile name from home ── */
/* Split a host path into anchor + components. '.' and empty parts dropped,
 * '..' preserved (PurePath.parts semantics). Returns component count; anchor
 * written to *anchor (malloc'd). */
static size_t path_parts(const char *path, char **anchor, char ***parts) {
    size_t n = strlen(path);
    char *norm = malloc(n + 1);
    for (size_t i = 0; i < n; i++) norm[i] = (path[i] == '\\') ? '/' : path[i];
    norm[n] = '\0';
    char *a = NULL;
    char *body = norm;
    if (n >= 2 && isalpha((unsigned char)norm[0]) && norm[1] == ':') {
        a = gfmt("%c:", toupper((unsigned char)norm[0]));
        body = norm + 2;
    } else if (norm[0] == '/') {
        a = strdup("/");
        body = norm + 1;
    } else {
        a = strdup("");
    }
    char **ps = NULL; size_t cnt = 0, cap = 0;
    char *tok = strtok(body, "/");
    while (tok) {
        if (strcmp(tok, ".") != 0) {
            if (cnt == cap) { cap = cap ? cap * 2 : 8; ps = realloc(ps, cap * sizeof(char *)); }
            ps[cnt++] = strdup(tok);
        }
        tok = strtok(NULL, "/");
    }
    free(norm);
    *anchor = a; *parts = ps;
    return cnt;
}
static void free_parts(char *anchor, char **parts, size_t n) {
    free(anchor);
    for (size_t i = 0; i < n; i++) free(parts[i]);
    free(parts);
}
/* collapse '.'/'..' for resolve() of the base path */
static void collapse_parts(char **parts, size_t *n) {
    size_t out = 0;
    for (size_t i = 0; i < *n; i++) {
        if (strcmp(parts[i], "..") == 0) {
            if (out > 0) { free(parts[--out]); }
            free(parts[i]);
        } else {
            parts[out++] = parts[i];
        }
    }
    *n = out;
}
static bool part_eq(const char *a, const char *b) {
#ifdef _WIN32
    return _stricmp(a, b) == 0;
#else
    return strcmp(a, b) == 0;
#endif
}
static bool service_safe_name(const char *s) {
    size_t n = strlen(s);
    if (n < 1 || n > 64) return false;
    if (!((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= '0' && s[0] <= '9'))) return false;
    for (size_t i = 1; i < n; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return true;
}

/* PoP: gwsl_profile_name_from_home @ hermes_cli/gateway.py:_profile_name_from_home */
char *gwsl_profile_name_from_home(const char *home, const char *default_root) {
    char *ba = NULL, **bp = NULL; size_t bn = path_parts(default_root, &ba, &bp);
    /* base = (default / "profiles").resolve(): absolute, collapsed */
    if (bn == 0 && ba[0] == '\0') { free_parts(ba, bp, bn); return NULL; }
    /* append "profiles" */
    bp = realloc(bp, (bn + 1) * sizeof(char *));
    bp[bn++] = strdup("profiles");
    collapse_parts(bp, &bn);
    char *ha = NULL, **hp = NULL; size_t hn = path_parts(home, &ha, &hp);
    char *result = NULL;
    /* relative_to requires matching anchor (absolute base) */
    bool match = (ha[0] != '\0') && part_eq(ha, ba);
    if (match && hn >= bn) {
        for (size_t i = 0; i < bn && match; i++) match = part_eq(hp[i], bp[i]);
        if (match && hn - bn == 1 && service_safe_name(hp[bn])) result = strdup(hp[bn]);
    }
    free_parts(ba, bp, bn);
    free_parts(ha, hp, hn);
    return result;
}

/* ── unit environment value ── */
/* PoP: gwsl_unit_environment_value @ hermes_cli/gateway.py:_unit_environment_value */
char *gwsl_unit_environment_value(const char *unit_path, const char *name, const gwsl_seams_t *s) {
    bool raised = false;
    char *text = s->unit_read_text(s->ctx, unit_path, &raised);
    if (raised || !text) { free(text); return NULL; }
    /* utf-8-sig: strip a leading BOM */
    char *body_start = text;
    if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        body_start = text + 3;
    char *result = NULL;
    char *prefix = gfmt("%s=", name);
    /* splitlines: \n, \r\n, \r */
    char *line = body_start;
    while (*line && !result) {
        char *nl = strpbrk(line, "\r\n");
        char save = 0;
        if (nl) {
            save = *nl; *nl = '\0';
        }
        /* strip() the line */
        char *b = line;
        while (*b && isspace((unsigned char)*b)) b++;
        char *e = b + strlen(b);
        while (e > b && isspace((unsigned char)e[-1])) e--;
        *e = '\0';
        if (starts_with(b, "Environment=")) {
            char *body = strdup(b + strlen("Environment="));
            /* strip */
            char *bs = body; while (*bs && isspace((unsigned char)*bs)) bs++;
            char *be = bs + strlen(bs); while (be > bs && isspace((unsigned char)be[-1])) be--;
            *be = '\0';
            char *unq = NULL;
            if (bs[0] == '"' && ends_with(bs, "\"") && strlen(bs) >= 1) {
                size_t bl = strlen(bs);
                char *inner = malloc(bl + 1);
                memcpy(inner, bs + 1, bl >= 2 ? bl - 2 : 0);
                inner[bl >= 2 ? bl - 2 : 0] = '\0';
                char *t1 = str_replace_all(inner, "\\\"", "\"");
                char *t2 = str_replace_all(t1, "\\\\", "\\");
                unq = str_replace_all(t2, "%%", "%");
                free(inner); free(t1); free(t2);
            } else {
                unq = strdup(bs);
            }
            if (starts_with(unq, prefix)) {
                char *val = unq + strlen(prefix);
                while (*val && isspace((unsigned char)*val)) val++;
                char *ve = val + strlen(val);
                while (ve > val && isspace((unsigned char)ve[-1])) ve--;
                *ve = '\0';
                if (*val) result = strdup(val);   /* .strip() or None */
            }
            free(unq); free(body);
        }
        if (nl) {
            *nl = save;
            line = nl + 1;
            if (save == '\r' && *line == '\n') line++;
        } else {
            break;
        }
    }
    free(prefix); free(text);
    return result;
}

/* ── no-backend message table ── */
typedef struct nb_entry {
    const char *subcommand;
    const char *reason;
    bool has_code;
    int code;
    const char *lines[8];
    size_t nlines;
} nb_entry_t;

static const char *WSL_HINT[] = {
    "",
    "  hermes gateway run                              # direct foreground",
    "  tmux new -s hermes 'hermes gateway run'         # persistent via tmux",
    "  nohup hermes gateway run > ~/.hermes/logs/gateway.log 2>&1 &  # background",
};

static const nb_entry_t NB_TABLE[] = {
    {"install", "termux", true, 1,
     {"Gateway service installation is not supported on Termux.", "Run manually: hermes gateway"}, 2},
    {"install", "wsl", true, 1,
     {"WSL detected but systemd is not running.",
      "Either enable systemd (add systemd=true to /etc/wsl.conf and restart WSL)",
      "or run the gateway in foreground mode:", NULL, NULL, NULL, NULL, NULL}, 3},
    {"install", "s6", false, 0,
     {"Per-profile gateways are auto-registered when you create a profile.", "",
      "  hermes profile create <name>     # creates the s6 service slot",
      "  hermes -p <name> gateway start   # bring it up via s6",
      "  hermes status                    # see currently-supervised gateways"}, 5},
    {"install", "container", true, 0,
     {"Service installation is not needed inside a Docker container.",
      "The container runtime is your service manager \xe2\x80\x94 use Docker restart policies instead:", "",
      "  docker run --restart unless-stopped ...   # auto-restart on crash/reboot",
      "  docker restart <container>                # manual restart", "",
      "To run the gateway: hermes gateway run"}, 7},
    {"install", "unsupported", true, 1,
     {"Service installation not supported on this platform.", "Run manually: hermes gateway run"}, 2},
    {"uninstall", "termux", true, 1,
     {"Gateway service uninstall is not supported on Termux because there is no managed service to remove.",
      "Stop manual runs with: hermes gateway stop"}, 2},
    {"uninstall", "s6", false, 0,
     {"Per-profile gateways are auto-unregistered when you delete the profile.", "",
      "  hermes profile delete <name>     # tears down the s6 service slot",
      "  hermes -p <name> gateway stop    # stop without deleting the profile"}, 4},
    {"uninstall", "container", true, 0,
     {"Service uninstall is not applicable inside a Docker container.",
      "To stop the gateway, stop or remove the container:", "",
      "  docker stop <container>", "  docker rm <container>"}, 5},
    {"uninstall", "unsupported", true, 1,
     {"Running the gateway as a background service is not available on this platform "
      "(no systemd, launchd or Scheduled Tasks), so there is nothing to uninstall.",
      "Stop a manually started gateway with: hermes gateway stop"}, 2},
    {"start", "termux", true, 1,
     {"Gateway service start is not supported on Termux because there is no system service manager.",
      "Run manually: hermes gateway"}, 2},
    {"start", "wsl", true, 1,
     {"WSL detected but systemd is not available.",
      "Run the gateway in foreground mode instead:", NULL, NULL, NULL, NULL, NULL, NULL}, 2},
    {"start", "container", true, 0,
     {"Service start is not applicable inside a Docker container.",
      "The gateway runs as the container's main process.", "",
      "  docker start <container>     # start a stopped container",
      "  docker restart <container>   # restart a running container", "",
      "Or run the gateway directly: hermes gateway run"}, 7},
    {"start", "unsupported", true, 1,
     {"Running the gateway as a background service is not available on this platform "
      "(no systemd, launchd or Scheduled Tasks).",
      "Run it directly with: hermes gateway run"}, 2},
};

/* PoP: gwsl_no_backend_exit @ hermes_cli/gateway.py:_no_backend_exit */
gwsl_verdict_t gwsl_no_backend_exit(const char *subcommand, const char *reason,
                                    const gwsl_seams_t *s, bool *key_error) {
    gwsl_verdict_t v = {false, 0};
    *key_error = false;
    const nb_entry_t *ent = NULL;
    for (size_t i = 0; i < sizeof(NB_TABLE) / sizeof(NB_TABLE[0]); i++) {
        if (strcmp(NB_TABLE[i].subcommand, subcommand) == 0 &&
            strcmp(NB_TABLE[i].reason, reason) == 0) { ent = &NB_TABLE[i]; break; }
    }
    if (!ent) { *key_error = true; return v; }
    /* code, *lines = entry; _print_lines(*lines) */
    const char *lines[16]; size_t n = 0;
    for (size_t i = 0; i < ent->nlines; i++) {
        if (ent->lines[i]) lines[n++] = ent->lines[i];
    }
    /* install/wsl and start/wsl splice _WSL_FOREGROUND_HINT after the head */
    if (strcmp(reason, "wsl") == 0) {
        for (size_t i = 0; i < 4; i++) lines[n++] = WSL_HINT[i];
        if (strcmp(subcommand, "start") == 0) {
            lines[n++] = "";
            lines[n++] = "To enable systemd: add systemd=true to /etc/wsl.conf and run 'wsl --shutdown' from PowerShell.";
        }
    }
    gwsl_print_lines(s, lines, n);
    if (ent->has_code) { v.exited = true; v.exit_code = ent->code; }
    return v;
}

/* PoP: gwsl_handle_no_backend_flow @ hermes_cli/gateway.py:_handle_no_backend */
gwsl_verdict_t gwsl_handle_no_backend_flow(const char *subcommand, bool wsl, bool s6,
                                           const gwsl_seams_t *s, bool *key_error) {
    const char *reason;
    if (wsl && s->is_wsl(s->ctx)) {
        reason = "wsl";
    } else if (s->is_container(s->ctx)) {
        reason = (s6 && gwsl_running_under_s6(s)) ? "s6" : "container";
    } else {
        reason = "unsupported";
    }
    return gwsl_no_backend_exit(subcommand, reason, s, key_error);
}

/* ── graceful systemd restart action ── */
/* PoP: gwsl_systemd_graceful_restart_action_flow @ hermes_cli/gateway.py:_systemd_graceful_restart_action */
const char *gwsl_systemd_graceful_restart_action_flow(bool system, int pid, const gwsl_seams_t *s) {
    const char *label = s->scope_label(s->ctx, system);
    /* .capitalize(): first char upper, rest lower */
    char *cap = strdup(label ? label : "");
    if (cap[0]) cap[0] = (char)toupper((unsigned char)cap[0]);
    for (size_t i = 1; cap[i]; i++) cap[i] = (char)tolower((unsigned char)cap[i]);
    double budget = s->restart_wait_budget(s->ctx);
    prf(s, "\xe2\x8f\xb3 %s service restarting gracefully (PID %d) \xe2\x80\x94 "
           "waiting up to %.0fs for in-flight turns + drain...", cap, pid, budget);
    free(cap);
    bool has_born = false;
    double born = s->process_start_time(s->ctx, pid, &has_born);
    const char *reporter = s->drain_reporter(s->ctx, budget);
    if (!s->graceful_sigusr1(s->ctx, pid, budget, reporter)) {
        char *lbl = gfmt("Gateway PID %d", pid);
        bool pending = s->leave_restart_pending(s->ctx, pid, lbl, born, has_born);
        free(lbl);
        if (pending) return NULL;
        prf(s, "\xe2\x9a\xa0 Graceful restart did not complete within %lds; forcing a service restart...",
            (long)budget);
        return "restart";
    }
    bool replacement_observed = false;
    if (s->wait_systemd_restart(s->ctx, system, pid, &replacement_observed)) return NULL;
    if (replacement_observed || s->systemd_start_limited(s->ctx, system)) return NULL;
    json_t *props = s->read_unit_props(s->ctx, system);
    if (!props || props->type != JSON_OBJECT || props->c.count == 0) { json_free(props); return NULL; }
    /* _systemd_main_pid_from_props: _positive_pid(props.get("MainPID","0") or "0") */
    const char *mp = json_get_str(props, "MainPID", "0");
    if (!mp || !*mp) mp = "0";
    json_t *mpv = json_string(mp);
    long replacement_pid = gwsl_positive_pid(mpv);
    json_free(mpv);
    const char *active = json_get_str(props, "ActiveState", NULL);
    const char *sub = json_get_str(props, "SubState", NULL);
    bool active_set = active && (strcmp(active, "active") == 0 || strcmp(active, "activating") == 0 ||
                                 strcmp(active, "reloading") == 0);
    bool sub_auto = sub && strcmp(sub, "auto-restart") == 0;
    bool pid_diff = (replacement_pid >= 0 && replacement_pid != (long)pid);
    json_free(props);
    if (active_set || sub_auto || pid_diff) return NULL;
    pr(s, "\xe2\x9a\xa0 Systemd did not relaunch the gateway after its graceful exit; starting the inactive service...");
    return "start";
}

/* ── _restart_all_as_host ── */
/* PoP: gwsl_restart_all_as_host_flow @ hermes_cli/gateway.py:_restart_all_as_host */
gwsl_verdict_t gwsl_restart_all_as_host_flow(const json_t *owner, bool system, bool detach,
                                             const gwsl_seams_t *s, gwsl_exc_t *exc) {
    gwsl_verdict_t v = {false, 0};
    bool service_stopped = gwsl_stop_installed_service(system, s, exc);
    if (exc_stop(exc, &v)) return v;
    int total;
    if (owner && owner->type != JSON_NULL) {
        int stopped = s->stop_host_multiplexer(s->ctx, owner);
        total = stopped + (service_stopped ? 1 : 0);
        if (total) {
            int opid = (int)json_get_num(owner, "pid", 0);
            const json_t *profs = json_obj_get(owner, "profiles");
            char *joined = NULL;
            if (profs && profs->type == JSON_ARRAY && profs->c.count > 0) {
                size_t cap = 64, len = 0; joined = malloc(cap); joined[0] = '\0';
                for (size_t i = 0; i < profs->c.count; i++) {
                    const char *p = json_get(profs, i)->str_val;
                    size_t pl = strlen(p);
                    while (len + pl + 3 > cap) { cap *= 2; joined = realloc(joined, cap); }
                    if (i) { memcpy(joined + len, ", ", 2); len += 2; }
                    memcpy(joined + len, p, pl); len += pl; joined[len] = '\0';
                }
            }
            prf(s, "\xe2\x9c\x93 Stopped the host gateway (PID %d; serves %s)",
                opid, (joined && *joined) ? joined : "unknown");
            free(joined);
        }
        s->print_unfolded_note(s->ctx, owner);
    } else {
        int killed = s->kill_processes(s->ctx, true);
        total = killed + (service_stopped ? 1 : 0);
        if (total) prf(s, "\xe2\x9c\x93 Stopped %d gateway process(es) across all profiles", total);
    }
    s->wait_gateway_exit(s->ctx, 10.0, 5.0);
    s->wait_port_free(s->ctx);
    s->discard_host_record(s->ctx);
    s->forget_host_probe(s->ctx);
    s->owe_online_notice(s->ctx);
    pr(s, "Starting gateway...");
    const char *kind = gwsl_installed_service_kind_for(s, is_windows_thunk, (void *)s);
    if (kind) {
        gwsl_service_call(kind, "start", system ? 1 : 0, s, exc);
        if (exc_stop(exc, &v)) return v;
        return v;
    }
    /* detach or named_profile_home(get_routing_process_hermes_home()) is not None */
    bool detached_route = detach;
    if (!detached_route) {
        const char *rh = s->routing_home(s->ctx);
        const char *np = s->named_profile_home(s->ctx, rh);
        detached_route = (np != NULL);
    }
    if (detached_route) {
        if (!s->spawn_detached(s->ctx)) {
            s->print_error(s->ctx, "Failed to start the host gateway as a background process.");
            v.exited = true; v.exit_code = 1;
            return v;
        }
        pr(s, "\xe2\x9c\x93 Started the host gateway as a background process");
        return v;
    }
    s->run_gateway(s->ctx, 0, true);
    return v;
}

/* ── _restart_all ── */
/* PoP: gwsl_restart_all_flow @ hermes_cli/gateway.py:_restart_all */
gwsl_verdict_t gwsl_restart_all_flow(bool system, bool detach, const gwsl_seams_t *s, gwsl_exc_t *exc) {
    gwsl_verdict_t v = {false, 0};
    json_t *owner = s->host_owner(s->ctx);
    if (owner && !s->host_is_ours(s->ctx, owner)) {
        const char *pl = json_get_str(owner, "profile_label", "");
        char *m = gfmt("The host gateway runs under profile '%s'.", pl);
        s->print_error(s->ctx, m); free(m);
        prf(s, "  %s", s->owner_describe(s->ctx, owner));
        pr(s, "  `--all` restarts the one host multiplexer, and this profile is not its owner.");
        pr(s, "");
        prf(s, "    hermes -p %s gateway restart --all", pl);
        json_free(owner);
        v.exited = true; v.exit_code = 78;   /* GATEWAY_FATAL_CONFIG_EXIT_CODE */
        return v;
    }
    s->restart_scope_enter(s->ctx, owner);
    gwsl_verdict_t r = gwsl_restart_all_as_host_flow(owner, system, detach, s, exc);
    s->restart_scope_exit(s->ctx);
    json_free(owner);
    return r;
}

/* ── _cmd_install ── */
/* PoP: gwsl_cmd_install_flow @ hermes_cli/gateway.py:_cmd_install */
gwsl_verdict_t gwsl_cmd_install_flow(const json_t *args, const gwsl_seams_t *s, gwsl_exc_t *exc) {
    gwsl_verdict_t v = {false, 0};
    if (s->is_managed(s->ctx)) {
        s->managed_error(s->ctx, "install gateway service");
        return v;
    }
    if (json_get_bool(args, "if_missing", false) && gwsl_installed_service_kind(s) != NULL) {
        pr(s, "\xe2\x9c\x93 Gateway service already installed");
        return v;
    }
    bool force = json_get_bool(args, "force", false);
    s->guard_named_profile(s->ctx, force, exc);
    if (exc_stop(exc, &v)) return v;
    bool system = json_get_bool(args, "system", false);
    const char *run_as_user = json_get_str(args, "run_as_user", NULL);
    if (gwsl_service_mgmt_blocked(s)) {
        bool ke = false;
        return gwsl_no_backend_exit("install", "termux", s, &ke);
    }
    const char *backend = gwsl_service_backend(true, s);
    if (backend && strcmp(backend, "systemd") == 0) {
        if (s->refuses_container(s->ctx, system)) { v.exited = true; v.exit_code = 1; return v; }
        s->install_systemd_cli(s->ctx, args, force, system, run_as_user, exc);
        if (exc_stop(exc, &v)) return v;
    } else if (backend && strcmp(backend, "launchd") == 0) {
        const json_t *sn = json_obj_get(args, "start_now");
        bool start_now = !(sn && sn->type == JSON_BOOL && !sn->bool_val);
        bool fup = json_get_bool(args, "force_unit_path", false);
        s->launchd_install(s->ctx, force, start_now, fup, exc);
        if (exc_stop(exc, &v)) return v;
    } else if (backend && strcmp(backend, "windows") == 0) {
        json_t *kw = json_object();
        json_set(kw, "force", json_bool(force));
        const json_t *sn = json_obj_get(args, "start_now");
        json_set(kw, "start_now", sn ? json_copy(sn) : json_null());
        const json_t *sol = json_obj_get(args, "start_on_login");
        json_set(kw, "start_on_login", sol ? json_copy(sol) : json_null());
        json_set(kw, "elevated_handoff", json_bool(json_get_bool(args, "elevated_handoff", false)));
        json_t *r = s->gw_windows_call(s->ctx, "install", kw, exc);
        json_free(r); json_free(kw);
        if (exc_stop(exc, &v)) return v;
    } else {
        bool ke = false;
        gwsl_verdict_t nv = gwsl_handle_no_backend_flow("install", true, true, s, &ke);
        if (ke) { exc->kind = GWSL_EXC_KEY_ERROR; return v; }
        if (nv.exited) return nv;
    }
    if (backend && gwsl_installed_service_kind(s) != NULL) {
        s->record_service_choice(s->ctx, "install");
    }
    return v;
}

/* ── _cmd_uninstall ── */
/* PoP: gwsl_cmd_uninstall_flow @ hermes_cli/gateway.py:_cmd_uninstall */
gwsl_verdict_t gwsl_cmd_uninstall_flow(const json_t *args, const gwsl_seams_t *s, gwsl_exc_t *exc) {
    gwsl_verdict_t v = gwsl_refuse_from_inside_gateway("uninstall", "the gateway from terminating itself", s);
    if (v.exited) return v;
    if (s->is_managed(s->ctx)) {
        s->managed_error(s->ctx, "uninstall gateway service");
        return v;
    }
    bool system = json_get_bool(args, "system", false);
    if (gwsl_service_mgmt_blocked(s)) {
        bool ke = false;
        return gwsl_no_backend_exit("uninstall", "termux", s, &ke);
    }
    const char *backend = gwsl_service_backend(true, s);
    if (backend) {
        gwsl_service_call(backend, "uninstall", system ? 1 : 0, s, exc);
        if (exc_stop(exc, &v)) return v;
    } else {
        bool ke = false;
        gwsl_verdict_t nv = gwsl_handle_no_backend_flow("uninstall", false, true, s, &ke);
        if (ke) { exc->kind = GWSL_EXC_KEY_ERROR; return v; }
        return nv;
    }
    return v;
}

/* ── _cmd_start ── */
static gwsl_verdict_t cmd_start_body(const json_t *args, bool start_all, bool system,
                                     const gwsl_seams_t *s, gwsl_exc_t *exc) {
    gwsl_verdict_t v = {false, 0};
    if (start_all) {
        int killed = s->kill_processes(s->ctx, true);
        if (killed) {
            prf(s, "\xe2\x9c\x93 Killed %d stale gateway process(es) across all profiles", killed);
            s->wait_gateway_exit(s->ctx, 10.0, 5.0);
            s->wait_port_free(s->ctx);
        }
    }
    if (gwsl_service_mgmt_blocked(s)) {
        bool ke = false;
        return gwsl_no_backend_exit("start", "termux", s, &ke);
    }
    const char *backend = gwsl_service_backend(true, s);
    if (backend) {
        gwsl_service_call(backend, "start", system ? 1 : 0, s, exc);
        if (exc_stop(exc, &v)) return v;
    } else {
        bool ke = false;
        gwsl_verdict_t nv = gwsl_handle_no_backend_flow("start", true, false, s, &ke);
        if (ke) { exc->kind = GWSL_EXC_KEY_ERROR; return v; }
        if (nv.exited) return nv;
    }
    return v;
}

/* PoP: gwsl_cmd_start_flow @ hermes_cli/gateway.py:_cmd_start */
gwsl_verdict_t gwsl_cmd_start_flow(const json_t *args, const gwsl_seams_t *s, gwsl_exc_t *exc) {
    gwsl_verdict_t v = {false, 0};
    if (s->profile_lifecycle(s->ctx, "start", args)) return v;
    bool system = json_get_bool(args, "system", false);
    bool start_all = json_get_bool(args, "all", false);
    bool force = json_get_bool(args, "force", false);
    json_t *owner = NULL;
    bool have_scope = false;
    if (start_all) {
        if (!force) owner = s->host_owner(s->ctx);
        if (owner) {
            prf(s, "\xe2\x9c\x93 The host gateway is already running \xe2\x80\x94 %s",
                s->owner_describe(s->ctx, owner));
            pr(s, "  One gateway per host serves every profile; nothing to start.");
            s->print_unfolded_note(s->ctx, owner);
            json_free(owner);
            return v;
        }
        have_scope = true;   /* host_scope_for_all_verb(owner) */
    } else {
        s->guard_named_profile(s->ctx, force, exc);
        if (exc_stop(exc, &v)) return v;
        bool d = s->dispatch_s6(s->ctx, "start", exc);
        if (exc_stop(exc, &v)) return v;
        if (d) return v;
        /* start_scope = contextlib.nullcontext() */
    }
    if (have_scope) s->host_scope_enter(s->ctx, owner);
    gwsl_verdict_t r = cmd_start_body(args, start_all, system, s, exc);
    if (have_scope) s->host_scope_exit(s->ctx);
    json_free(owner);
    return r;
}

/* ── _cmd_stop ── */
/* PoP: gwsl_cmd_stop_flow @ hermes_cli/gateway.py:_cmd_stop */
gwsl_verdict_t gwsl_cmd_stop_flow(const json_t *args, const gwsl_seams_t *s, gwsl_exc_t *exc) {
    gwsl_verdict_t v = gwsl_refuse_from_inside_gateway("stop", "restart loops", s);
    if (v.exited) return v;
    if (s->profile_lifecycle(s->ctx, "stop", args)) { v.exited = false; return v; }
    bool stop_all = json_get_bool(args, "all", false);
    bool system = json_get_bool(args, "system", false);
    if (!stop_all) {
        json_t *pids = s->find_pids(s->ctx);
        bool no_pids = (json_len(pids) == 0);
        json_free(pids);
        if (no_pids) {
            json_t *a = s->served_owner(s->ctx);
            bool cond = (a != NULL);
            if (!cond) cond = s->named_served(s->ctx);
            if (cond) {
                json_t *owner = s->served_owner(s->ctx);
                const char *cpn = s->current_profile_name(s->ctx);
                char *m = gfmt("The host gateway serves profile '%s' \xe2\x80\x94 there is no separate "
                               "gateway for this profile to stop.", cpn);
                s->print_error(s->ctx, m); free(m);
                if (owner) prf(s, "  %s", s->owner_describe(s->ctx, owner));
                pr(s, "  Stop or restart the host gateway instead:");
                pr(s, "");
                char *of = owner ? gfmt("-p %s ", json_get_str(owner, "profile_label", "")) : strdup("");
                prf(s, "    hermes %sgateway stop      # takes every served profile offline", of);
                prf(s, "    hermes %sgateway restart", of);
                free(of); json_free(owner); json_free(a);
                v.exited = true; v.exit_code = 78;
                return v;
            }
            json_free(a);
        }
    }
    s->refuse_sessions_live(s->ctx, "stop", args, exc);
    if (exc_stop(exc, &v)) return v;
    if (stop_all) {
        bool d = s->dispatch_all_s6(s->ctx, "stop", exc);
        if (exc_stop(exc, &v)) return v;
        if (d) return v;
    } else {
        bool d = s->dispatch_s6(s->ctx, "stop", exc);
        if (exc_stop(exc, &v)) return v;
        if (d) return v;
    }
    if (stop_all) {
        json_t *owner = s->host_owner(s->ctx);
        s->host_scope_enter(s->ctx, owner);
        bool service_available = gwsl_stop_installed_service(system, s, exc);
        s->host_scope_exit(s->ctx);
        if (exc_stop(exc, &v)) { json_free(owner); return v; }
        int killed = s->kill_processes(s->ctx, true);
        int total = killed + (service_available ? 1 : 0);
        if (total) prf(s, "\xe2\x9c\x93 Stopped %d gateway process(es) across all profiles", total);
        else pr(s, "\xe2\x9c\x97 No gateway processes found");
        json_free(owner);
        return v;
    }
    bool stopped = gwsl_stop_installed_service(system, s, exc);
    if (exc_stop(exc, &v)) return v;
    if (stopped) {
        prf(s, "\xe2\x9c\x93 Stopped %s service", s->service_name(s->ctx));
    } else if (s->stop_profile(s->ctx)) {
        pr(s, "\xe2\x9c\x93 Stopped gateway for this profile");
    } else {
        s->report_stop_failure(s->ctx);
    }
    return v;
}

/* ── _cmd_restart ── */
/* PoP: gwsl_cmd_restart_flow @ hermes_cli/gateway.py:_cmd_restart */
gwsl_verdict_t gwsl_cmd_restart_flow(const json_t *args, const gwsl_seams_t *s, gwsl_exc_t *exc) {
    gwsl_verdict_t v = gwsl_refuse_from_inside_gateway("restart", "restart loops", s);
    if (v.exited) return v;
    if (s->profile_lifecycle(s->ctx, "restart", args)) { v.exited = false; return v; }
    bool system = json_get_bool(args, "system", false);
    bool restart_all = json_get_bool(args, "all", false);
    bool force = json_get_bool(args, "force", false);
    if (!restart_all) {
        s->guard_named_profile(s->ctx, force, exc);
        if (exc_stop(exc, &v)) return v;
    }
    s->refuse_sessions_live(s->ctx, "restart", args, exc);
    if (exc_stop(exc, &v)) return v;
    if (restart_all) {
        bool d = s->dispatch_all_s6(s->ctx, "restart", exc);
        if (exc_stop(exc, &v)) return v;
        if (d) return v;
    } else {
        bool d = s->dispatch_s6(s->ctx, "restart", exc);
        if (exc_stop(exc, &v)) return v;
        if (d) return v;
    }
    if (restart_all) {
        bool detach = json_get_bool(args, "detach", false);
        return gwsl_restart_all_flow(system, detach, s, exc);
    }
    /* best-effort orphan reap */
    gwsl_exc_t re = {GWSL_EXC_NONE, 0};
    const char *re_msg = NULL;
    s->reap_orphans(s->ctx, &re, &re_msg);
    if (re.kind == GWSL_EXC_SYSTEM_EXIT) { *exc = re; return v; }
    if (re.kind != GWSL_EXC_NONE) {
        char *m = gfmt("orphan reap before gateway restart failed: %s", re_msg ? re_msg : "");
        s->logger_debug(s->ctx, m); free(m);
    }
    const char *kind = gwsl_installed_service_kind_for(s, is_windows_thunk, (void *)s);
    bool service_configured = false;
    if (kind) {
        if (strcmp(kind, "windows") != 0) {
            service_configured = true;
        } else {
            gwsl_exc_t ie = {GWSL_EXC_NONE, 0};
            json_t *r = s->gw_windows_call(s->ctx, "is_installed", NULL, &ie);
            service_configured = json_is_true(r);
            json_free(r);
            if (ie.kind != GWSL_EXC_NONE) { *exc = ie; return v; }
        }
    }
    if (kind) {
        gwsl_exc_t se = {GWSL_EXC_NONE, 0};
        gwsl_service_call(kind, "restart", system ? 1 : 0, s, &se);
        if (se.kind == GWSL_EXC_NONE) return v;
        bool swallow = (strcmp(kind, "windows") == 0);
        bool caught = (se.kind == GWSL_EXC_CALLED_PROCESS) ||
                      (swallow && (se.kind == GWSL_EXC_RUNTIME || se.kind == GWSL_EXC_OSERROR));
        if (!caught) { *exc = se; return v; }
    }
    if (kind && strcmp(kind, "systemd") == 0 && s->supports_systemd(s->ctx)) {
        bool linger_ok = false;
        s->get_linger_status(s->ctx, &linger_ok);
        if (!linger_ok) {
            const char *user = s->getuser(s->ctx);
            char *runline = gfmt("  Run:  sudo loginctl enable-linger %s", user);
            const char *lines[] = {
                "", "\xe2\x9a\xa0 Cannot restart gateway as a service \xe2\x80\x94 linger is not enabled.",
                "  The gateway user service requires linger to function on headless servers.", "",
                runline, "", "  Then restart the gateway:", "    hermes gateway restart",
            };
            gwsl_print_lines(s, lines, 8);
            free(runline);
            return v;
        }
    }
    if (service_configured) {
        const char *lines[] = {
            "", "\xe2\x9c\x97 Gateway service restart failed.",
            "  The service definition exists, but the service manager did not recover it.",
            "  Fix the service, then retry: hermes gateway start",
        };
        gwsl_print_lines(s, lines, 4);
        v.exited = true; v.exit_code = 1;
        return v;
    }
    long spid = s->get_running_pid(s->ctx);
    if (spid > 0 && s->declares_external_supervisor(s->ctx, spid)) {
        s->restart_externally_supervised(s->ctx, spid);
        return v;
    }
    s->refuse_restart_service_managed(s->ctx, spid, exc);
    if (exc_stop(exc, &v)) return v;
    if (s->stop_profile(s->ctx)) pr(s, "\xe2\x9c\x93 Stopped gateway for this profile");
    s->wait_gateway_exit(s->ctx, 10.0, 5.0);
    s->wait_port_free(s->ctx);
    s->forget_host_probe(s->ctx);
    pr(s, "Starting gateway...");
    s->start_restart_replacement(s->ctx, args, force, exc);
    if (exc_stop(exc, &v)) return v;
    return v;
}

/* ── _cmd_status ── */
static bool snapshot_running(const json_t *snap) {
    if (json_get_bool(snap, "service_running", false)) return true;
    const json_t *pids = json_obj_get(snap, "gateway_pids");
    return pids && pids->type == JSON_ARRAY && pids->c.count > 0;
}

/* PoP: gwsl_cmd_status_flow @ hermes_cli/gateway.py:_cmd_status */
gwsl_verdict_t gwsl_cmd_status_flow(const json_t *args, const gwsl_seams_t *s, gwsl_exc_t *exc) {
    gwsl_verdict_t v = {false, 0};
    bool deep = json_get_bool(args, "deep", false);
    bool full = json_get_bool(args, "full", false);
    bool system = json_get_bool(args, "system", false);
    json_t *snap = s->get_snapshot(s->ctx, system);
    bool running = snapshot_running(snap);
    if (s->print_parked_status(s->ctx) && !running) { json_free(snap); return v; }
    /* active_standalone = ((get_active_profile_name() or "default") != "default"
     *                      and profile_is_standalone(get_hermes_home())) */
    const char *apn = s->active_profile_name(s->ctx);
    const char *name = (apn && *apn) ? apn : "default";
    bool active_standalone = false;
    if (strcmp(name, "default") != 0) {
        const char *hh = s->get_hermes_home(s->ctx);
        active_standalone = s->profile_is_standalone(s->ctx, hh);
    }
    if (active_standalone) {
        pr(s, "standalone by config (gateway.standalone: true) \xe2\x80\x94 temporary compatibility shim");
        prf(s, "  %s", s->standalone_notice(s->ctx));
    }
    /* _windows_service_installed = is_windows() and _gw_windows().is_installed() */
    bool win_installed = false;
    if (s->is_windows(s->ctx)) {
        gwsl_exc_t ie = {GWSL_EXC_NONE, 0};
        json_t *r = s->gw_windows_call(s->ctx, "is_installed", NULL, &ie);
        win_installed = json_is_true(r);
        json_free(r);
        if (ie.kind != GWSL_EXC_NONE) { *exc = ie; json_free(snap); return v; }
    }
    if (!active_standalone && !running && s->named_served(s->ctx)) {
        pr(s, "\xe2\x9c\x93 Gateway is running via the default-profile multiplexer");
        pr(s, "  Manage it from the default profile: hermes gateway status");
        s->print_served_urls(s->ctx, s->active_profile_name(s->ctx));
        s->print_unserved_urls(s->ctx, s->active_profile_name(s->ctx));
    } else {
        /* kind := _installed_service_kind_for(lambda: _windows_service_installed) */
        const char *kind = gwsl_installed_service_kind_for(s, NULL, NULL);
        /* thunk returns the cached win_installed bool */
        if (!kind && win_installed) kind = "windows";
        if (kind) {
            if (strcmp(kind, "systemd") == 0) {
                s->systemd_status(s->ctx, deep, system, full, exc);
                if (exc_stop(exc, &v)) { json_free(snap); return v; }
            } else if (strcmp(kind, "launchd") == 0) {
                s->launchd_status(s->ctx, deep, exc);
                if (exc_stop(exc, &v)) { json_free(snap); return v; }
            } else {
                json_t *kw = json_object();
                json_set(kw, "deep", json_bool(deep));
                json_t *r = s->gw_windows_call(s->ctx, "status", kw, exc);
                json_free(r); json_free(kw);
                if (exc_stop(exc, &v)) { json_free(snap); return v; }
            }
            s->print_mismatch(s->ctx, snap);
            json_t *empty = json_array();
            s->report_incompat_owner(s->ctx, empty);
            json_free(empty);
            s->print_multiplex_reason(s->ctx);
            s->print_served_urls(s->ctx, NULL);
        } else {
            const json_t *pids = json_obj_get(snap, "gateway_pids");
            size_t np = (pids && pids->type == JSON_ARRAY) ? pids->c.count : 0;
            bool incompat = false;
            if (np > 0) {
                json_t *pa = json_array();
                for (size_t i = 0; i < np; i++) json_append(pa, json_copy(json_get(pids, i)));
                incompat = s->report_incompat_owner(s->ctx, pa);
                json_free(pa);
            }
            if (np > 0 && incompat) {
                /* pass */
            } else if (np > 0) {
                char *joined = NULL; size_t cap = 64, len = 0;
                joined = malloc(cap); joined[0] = '\0';
                for (size_t i = 0; i < np; i++) {
                    char buf[32]; snprintf(buf, sizeof(buf), "%ld", (long)json_get(pids, i)->num_val);
                    size_t bl = strlen(buf);
                    while (len + bl + 3 > cap) { cap *= 2; joined = realloc(joined, cap); }
                    if (i) { memcpy(joined + len, ", ", 2); len += 2; }
                    memcpy(joined + len, buf, bl); len += bl; joined[len] = '\0';
                }
                prf(s, "\xe2\x9c\x93 Gateway is running (PID: %s)", joined);
                free(joined);
                pr(s, "  (Running manually, not as a system service)");
                s->print_runtime_health(s->ctx);
                s->print_multiplex_reason(s->ctx);
                s->print_served_urls(s->ctx, NULL);
                pr(s, "");
                const char *hk = gwsl_status_host_kind(s);
                const char *const *hints; size_t nh;
                if (strcmp(hk, "termux") == 0) {
                    static const char *h[] = {"Termux note:", "  Android may stop background jobs when Termux is suspended"};
                    hints = h; nh = 2;
                } else if (strcmp(hk, "wsl") == 0) {
                    static const char *h[] = {"WSL note:", "  The gateway is running in foreground/manual mode (recommended for WSL).",
                                              "  Use tmux or screen for persistence across terminal closes."};
                    hints = h; nh = 3;
                } else if (strcmp(hk, "windows") == 0) {
                    static const char *h[] = {"To install as a Windows Scheduled Task (auto-start on login):", "  hermes gateway install"};
                    hints = h; nh = 2;
                } else {
                    static const char *h[] = {"To install as a service:", "  hermes gateway install", "  sudo hermes gateway install --system"};
                    hints = h; nh = 3;
                }
                gwsl_print_lines(s, hints, nh);
            } else if (json_get_bool(snap, "service_running", false)) {
                prf(s, "\xe2\x9c\x93 Gateway is running (supervised by %s)", json_get_str(snap, "manager", ""));
                s->print_runtime_health(s->ctx);
                s->print_multiplex_reason(s->ctx);
                s->print_served_urls(s->ctx, NULL);
            } else {
                pr(s, "\xe2\x9c\x97 Gateway is not running");
                s->print_runtime_health(s->ctx);
                pr(s, "");
                pr(s, "To start:");
                pr(s, "  hermes gateway run      # Run in foreground");
                const char *hk = gwsl_status_host_kind(s);
                const char *const *hints; size_t nh;
                if (strcmp(hk, "termux") == 0) {
                    static const char *h[] = {"  nohup hermes gateway run > ~/.hermes/logs/gateway.log 2>&1 &  # Best-effort background start"};
                    hints = h; nh = 1;
                } else if (strcmp(hk, "wsl") == 0) {
                    static const char *h[] = {"  tmux new -s hermes 'hermes gateway run'         # persistent via tmux",
                                              "  nohup hermes gateway run > ~/.hermes/logs/gateway.log 2>&1 &  # background"};
                    hints = h; nh = 2;
                } else if (strcmp(hk, "windows") == 0) {
                    static const char *h[] = {"  hermes gateway install  # Install as Windows Scheduled Task (auto-start on login)"};
                    hints = h; nh = 1;
                } else {
                    static const char *h[] = {"  hermes gateway install  # Install as user service",
                                              "  sudo hermes gateway install --system  # Install as boot-time system service"};
                    hints = h; nh = 2;
                }
                gwsl_print_lines(s, hints, nh);
            }
        }
    }
    s->print_dup_creds(s->ctx);
    s->print_other_profiles(s->ctx);
    s->print_standalone_cfg(s->ctx);
    json_free(snap);
    return v;
}