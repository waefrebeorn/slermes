/*
 * status.c — Slermes C11 port of gateway/status.py.
 *
 * Gateway runtime status: PID-file liveness detection, cross-process runtime
 * and scope locks, process fingerprinting (start-time + cmdline identity),
 * runtime health JSON, and the --replace takeover / planned-stop marker
 * protocol. Faithful port; POSIX-only (the slermes binary is native Linux),
 * so the Windows msvcrt/taskkill/psutil branches of the original map to their
 * /proc + kill(2) + flock(2) equivalents.
 *
 * See include/gateway_status.h for the public surface.
 */

#include "gateway_status.h"
#include "hermes_gateway_core.h"
#include "slermes_home.h"
#include "json.h"
#include "hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/file.h>   /* flock */
#include <sys/types.h>

/* ── Constants (mirror gateway/status.py) ────────────────────────────── */

#define GATEWAY_KIND               "hermes-gateway"
#define RUNTIME_STATUS_FILE        "gateway_state.json"
#define LOCKS_DIRNAME              "gateway-locks"
#define GATEWAY_LOCK_FILENAME      "gateway.lock"
#define PID_FILENAME               "gateway.pid"
#define TAKEOVER_MARKER_FILENAME   ".gateway-takeover.json"
#define PLANNED_STOP_MARKER_FILENAME ".gateway-planned-stop.json"
#define TAKEOVER_MARKER_TTL_S      60
#define PLANNED_STOP_MARKER_TTL_S  60

/* Process-global runtime lock fd (mirrors _gateway_lock_handle). -1 = none. */
static int g_gateway_lock_fd = -1;

/* Forward declarations (defined later; referenced before their definition). */
static bool command_line_belongs_to_profile(const char *command,
                                            const char *profile_home);

/* ── Path builders ───────────────────────────────────────────────────── */

/* Write "<home>/<name>" into buf. Returns buf. */
static char *home_path(char *buf, size_t n, const char *name) {
    snprintf(buf, n, "%s/%s", slermes_home(), name);
    return buf;
}

/* PoP: get_pid_path @ gateway/status.py:_get_pid_path */
static char *get_pid_path(char *buf, size_t n) {
    return home_path(buf, n, PID_FILENAME);
}

/* PoP: get_gateway_lock_path @ gateway/status.py:_get_gateway_lock_path */
static char *get_gateway_lock_path(const char *pid_path, char *buf, size_t n) {
    if (pid_path && pid_path[0]) {
        /* replace the basename of pid_path with the lock filename */
        const char *slash = strrchr(pid_path, '/');
        if (slash) {
            size_t dirlen = (size_t)(slash - pid_path);
            snprintf(buf, n, "%.*s/%s", (int)dirlen, pid_path, GATEWAY_LOCK_FILENAME);
        } else {
            snprintf(buf, n, "%s", GATEWAY_LOCK_FILENAME);
        }
        return buf;
    }
    return home_path(buf, n, GATEWAY_LOCK_FILENAME);
}

/* PoP: get_runtime_status_path @ gateway/status.py:_get_runtime_status_path */
static char *get_runtime_status_path(char *buf, size_t n) {
    return home_path(buf, n, RUNTIME_STATUS_FILE);
}

/* PoP: get_takeover_marker_path @ gateway/status.py:_get_takeover_marker_path */
static char *get_takeover_marker_path(char *buf, size_t n) {
    return home_path(buf, n, TAKEOVER_MARKER_FILENAME);
}

/* PoP: get_planned_stop_marker_path @ gateway/status.py:_get_planned_stop_marker_path */
static char *get_planned_stop_marker_path(char *buf, size_t n) {
    return home_path(buf, n, PLANNED_STOP_MARKER_FILENAME);
}

/* Machine-local directory for token-scoped gateway locks. Mirrors
 * _get_lock_dir(): HERMES_GATEWAY_LOCK_DIR override, else
 * $XDG_STATE_HOME/hermes/gateway-locks, else ~/.local/state/hermes/... */
static char *get_lock_dir(char *buf, size_t n) {
    const char *override = getenv("HERMES_GATEWAY_LOCK_DIR");
    if (override && override[0]) {
        snprintf(buf, n, "%s", override);
        return buf;
    }
    const char *xdg = getenv("XDG_STATE_HOME");
    if (xdg && xdg[0]) {
        snprintf(buf, n, "%s/hermes/%s", xdg, LOCKS_DIRNAME);
    } else {
        const char *home = getenv("HOME");
        if (!home || !home[0]) home = "/tmp";
        snprintf(buf, n, "%s/.local/state/hermes/%s", home, LOCKS_DIRNAME);
    }
    return buf;
}

/* mkdir -p on the parent directory of a file path. Best-effort. */
static void ensure_parent_dir(const char *file_path) {
    char tmp[1200];
    snprintf(tmp, sizeof(tmp), "%s", file_path);
    char *slash = strrchr(tmp, '/');
    if (!slash) return;
    *slash = '\0';
    /* create intermediate dirs */
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0700); *p = '/'; }
    }
    mkdir(tmp, 0700);
}

/* UTC now as ISO 8601 with +00:00 offset (mirrors datetime.now(utc).isoformat()). */
static void utc_now_iso(char *buf, size_t n) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tmv;
    gmtime_r(&ts.tv_sec, &tmv);
    int micro = (int)(ts.tv_nsec / 1000);
    snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02d.%06d+00:00",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec, micro);
}

/* ── Process control ─────────────────────────────────────────────────── */

/* PoP: terminate_pid @ gateway/status.py:terminate_pid */
/* Port of Python: terminate_pid */
int gwstatus_terminate_pid(pid_t pid, bool force) {
    int sig = force ? SIGKILL : SIGTERM;
    if (kill(pid, sig) != 0) return -1;
    return 0;
}

/* PoP: _scope_hash @ gateway/status.py:_scope_hash */
/* Port of Python: _scope_hash — first 16 hex chars of sha256(identity). */
static void scope_hash(const char *identity, char out[17]) {
    char *hex = hash_sha256_hex((const unsigned char *)identity, strlen(identity));
    if (!hex) { out[0] = '\0'; return; }
    memcpy(out, hex, 16);
    out[16] = '\0';
    free(hex);
}

/* PoP: gwstatus_get_process_start_time @ gateway/status.py:_get_process_start_time */
/* PoP: gwstatus_get_process_start_time @ gateway/status.py:get_process_start_time */
/* Python's public get_process_start_time is a thin wrapper over the private
 * _get_process_start_time; both collapse to this one C entry point.
 * Linux: field 22 of /proc/<pid>/stat (clock ticks since boot). The comm
 * field (2) can contain spaces/parens, so scan from the closing ')'. */
long gwstatus_get_process_start_time(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char buf[4096];
    size_t got = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (got == 0) return -1;
    buf[got] = '\0';
    /* comm is enclosed in parens and may contain spaces/parens; the last ')'
     * ends field 2. Fields after it are space-separated; start_time is
     * field 22, i.e. the 20th token after the closing paren. */
    char *rparen = strrchr(buf, ')');
    if (!rparen) return -1;
    char *p = rparen + 1;
    /* Token indices after the paren: field 3 = state, ... field 22 = start.
     * That's the 20th whitespace-delimited token starting at field 3. */
    int field = 2; /* we are positioned right after field 2 (comm) */
    char *tok = strtok(p, " \t\n");
    while (tok) {
        field++;
        if (field == 22) {
            errno = 0;
            long v = strtol(tok, NULL, 10);
            if (errno) return -1;
            return v;
        }
        tok = strtok(NULL, " \t\n");
    }
    return -1;
}

/* PoP: _pid_exists @ gateway/status.py:_pid_exists */
/* Port of Python: _pid_exists — alive check that never signals a kill.
 * Reports zombies as dead (issue #42126). */
bool gwstatus_pid_exists(pid_t pid) {
    /* Zombie check via /proc/<pid>/stat field 3 (state). */
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
    FILE *f = fopen(path, "r");
    if (f) {
        char buf[4096];
        size_t got = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        if (got > 0) {
            buf[got] = '\0';
            char *rparen = strrchr(buf, ')');
            if (rparen && rparen[1] == ' ' && rparen[2]) {
                char state = rparen[2];
                if (state == 'Z') return false;  /* zombie: dead */
            }
        }
    }
    /* POSIX liveness: kill(pid, 0). */
    if (kill(pid, 0) == 0) return true;
    if (errno == EPERM) return true;  /* exists, not signalable by us */
    return false;  /* ESRCH or other: gone */
}

/* ── Command-line identity ───────────────────────────────────────────── */

/* PoP: _read_process_cmdline @ gateway/status.py:_read_process_cmdline */
/* Port of Python: _read_process_cmdline. Linux: /proc/<pid>/cmdline with
 * NUL separators mapped to spaces; else `ps -p <pid> -o command=`.
 * Returns a malloc'd string (caller frees) or NULL. */
static char *read_process_cmdline(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);
    FILE *f = fopen(path, "rb");
    if (f) {
        char raw[8192];
        size_t got = fread(raw, 1, sizeof(raw) - 1, f);
        fclose(f);
        if (got > 0) {
            for (size_t i = 0; i < got; i++) if (raw[i] == '\0') raw[i] = ' ';
            raw[got] = '\0';
            /* strip trailing whitespace */
            size_t L = got;
            while (L > 0 && (raw[L-1] == ' ' || raw[L-1] == '\n')) raw[--L] = '\0';
            /* strip leading whitespace */
            char *s = raw;
            while (*s == ' ') s++;
            if (*s) return strdup(s);
        }
    }
    /* Fallback: ps -p <pid> -o command= */
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "ps -p %d -o command=", (int)pid);
    FILE *p = popen(cmd, "r");
    if (!p) return NULL;
    char out[8192];
    size_t got = fread(out, 1, sizeof(out) - 1, p);
    pclose(p);
    if (got == 0) return NULL;
    out[got] = '\0';
    size_t L = got;
    while (L > 0 && (out[L-1] == '\n' || out[L-1] == ' ' || out[L-1] == '\r')) out[--L] = '\0';
    if (L == 0) return NULL;
    return strdup(out);
}

/* Lowercase + backslash→slash a token in place. */
static void norm_token(char *t) {
    for (char *p = t; *p; p++) {
        if (*p == '\\') *p = '/';
        else *p = (char)tolower((unsigned char)*p);
    }
}

/* Return the last '/'-separated component of a token. */
static const char *basename_tok(const char *t) {
    const char *slash = strrchr(t, '/');
    return slash ? slash + 1 : t;
}

/* Simple whitespace tokenizer honoring single/double quotes (approximates
 * shlex.split(posix=False)). Fills argv[] up to max; returns count. Each
 * token is a malloc'd string; caller frees via free_tokens(). */
static int split_command(const char *command, char **argv, int max) {
    int argc = 0;
    const char *p = command;
    char buf[4096];
    while (*p && argc < max) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (!*p) break;
        size_t bi = 0;
        char quote = 0;
        while (*p && bi < sizeof(buf) - 1) {
            char c = *p;
            if (quote) {
                if (c == quote) { quote = 0; p++; continue; }
                buf[bi++] = c; p++;
            } else if (c == '"' || c == '\'') {
                quote = c; p++;
            } else if (c == ' ' || c == '\t' || c == '\n') {
                break;
            } else {
                buf[bi++] = c; p++;
            }
        }
        buf[bi] = '\0';
        argv[argc++] = strdup(buf);
    }
    return argc;
}

static void free_tokens(char **argv, int argc) {
    for (int i = 0; i < argc; i++) free(argv[i]);
}

/* PoP: _gateway_command_subcommand @ gateway/status.py:_gateway_command_subcommand */
/* Port of Python: _gateway_command_subcommand. Returns a malloc'd subcommand
 * string (caller frees) or NULL. */
static char *gateway_command_subcommand(const char *command) {
    if (!command || !command[0]) return NULL;

    char *raw[256];
    int rawc = split_command(command, raw, 256);
    if (rawc == 0) return NULL;

    /* Normalize tokens: strip surrounding quotes already handled; lowercase +
     * slashes. (Quotes were consumed by split_command.) */
    for (int i = 0; i < rawc; i++) norm_token(raw[i]);

    /* Gateway-dedicated entrypoints carry no subcommand. */
    for (int i = 0; i < rawc; i++) {
        const char *t = raw[i];
        size_t tl = strlen(t);
        const char *suffix = "/gateway/run.py";
        size_t sl = strlen(suffix);
        if (strcmp(t, "gateway/run.py") == 0 ||
            (tl >= sl && strcmp(t + tl - sl, suffix) == 0)) {
            free_tokens(raw, rawc);
            return strdup("run");
        }
        const char *bn = basename_tok(t);
        if (strcmp(bn, "hermes-gateway") == 0 || strcmp(bn, "hermes-gateway.exe") == 0) {
            free_tokens(raw, rawc);
            return strdup("run");
        }
    }

    /* has_gateway_entry check on the joined tokens. */
    bool has_entry = false;
    for (int i = 0; i < rawc && !has_entry; i++) {
        if (strstr(raw[i], "hermes_cli.main") || strstr(raw[i], "hermes_cli/main.py"))
            has_entry = true;
    }
    if (!has_entry) {
        for (int i = 0; i < rawc; i++) {
            const char *bn = basename_tok(raw[i]);
            if (strcmp(bn, "hermes") == 0 || strcmp(bn, "hermes.exe") == 0) { has_entry = true; break; }
        }
    }
    if (!has_entry) { free_tokens(raw, rawc); return NULL; }

    /* Drop profile selectors: --profile X / -p X / --profile=X / -p=X. */
    char *filtered[256];
    int fc = 0;
    bool skip_next = false;
    for (int i = 0; i < rawc; i++) {
        const char *t = raw[i];
        if (skip_next) { skip_next = false; continue; }
        if (strcmp(t, "--profile") == 0 || strcmp(t, "-p") == 0) { skip_next = true; continue; }
        if (strncmp(t, "--profile=", 10) == 0 || strncmp(t, "-p=", 3) == 0) continue;
        filtered[fc++] = raw[i];
    }

    char *result = NULL;
    for (int i = 0; i < fc; i++) {
        if (strcmp(filtered[i], "gateway") != 0) continue;
        if (i + 1 >= fc) { result = strdup("run"); break; }  /* bare `gateway` => run */
        result = strdup(filtered[i + 1]);
        break;
    }
    free_tokens(raw, rawc);
    return result;
}

/* PoP: looks_like_gateway_command_line @ gateway/status.py:looks_like_gateway_command_line */
/* Port of Python: looks_like_gateway_command_line */
bool gwstatus_looks_like_gateway_command_line(const char *command) {
    char *sub = gateway_command_subcommand(command);
    bool ok = (sub && strcmp(sub, "run") == 0);
    free(sub);
    return ok;
}

/* PoP: looks_like_gateway_runtime_command_line @ gateway/status.py:looks_like_gateway_runtime_command_line */
/* Port of Python: looks_like_gateway_runtime_command_line */
bool gwstatus_looks_like_gateway_runtime_command_line(const char *command) {
    char *sub = gateway_command_subcommand(command);
    bool ok = (sub && (strcmp(sub, "run") == 0 || strcmp(sub, "restart") == 0));
    free(sub);
    return ok;
}

/* PoP: _looks_like_gateway_process @ gateway/status.py:_looks_like_gateway_process */
/* Port of Python: _looks_like_gateway_process */
static bool looks_like_gateway_process(pid_t pid) {
    char *cmd = read_process_cmdline(pid);
    if (!cmd) return false;
    bool ok = gwstatus_looks_like_gateway_command_line(cmd);
    free(cmd);
    return ok;
}

/* ── JSON record helpers ─────────────────────────────────────────────── */

/* Read + parse a JSON object file. Returns json_t* (caller json_free) or NULL. */
/* PoP: _read_json_file @ gateway/status.py:_read_json_file */
static json_t *read_json_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';
    /* strip */
    char *s = buf;
    while (*s == ' ' || *s == '\n' || *s == '\t' || *s == '\r') s++;
    if (!*s) { free(buf); return NULL; }
    json_t *node = json_parse(s, NULL);
    free(buf);
    if (!node) return NULL;
    if (node->type != JSON_OBJECT) { json_free(node); return NULL; }
    return node;
}

/* PoP: write_json_file @ gateway/status.py:_write_json_file */
static int write_json_file(const char *path, json_t *payload) {
    ensure_parent_dir(path);
    char *ser = json_serialize(payload);
    if (!ser) return -1;
    char tmp[1300];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%d", path, (int)getpid());
    FILE *f = fopen(tmp, "wb");
    if (!f) { free(ser); return -1; }
    size_t len = strlen(ser);
    size_t wrote = fwrite(ser, 1, len, f);
    int fd = fileno(f);
    if (fd >= 0) fsync(fd);
    fclose(f);
    free(ser);
    if (wrote != len) { unlink(tmp); return -1; }
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

/* PoP: build_pid_record @ gateway/status.py:_build_pid_record */
static json_t *build_pid_record(void) {
    json_t *o = json_object();
    json_set(o, "pid", json_number((double)getpid()));
    json_set(o, "kind", json_string(GATEWAY_KIND));
    /* argv: read from /proc/self/cmdline for faithful parity with sys.argv. */
    json_t *argv = json_array();
    char *cmd = read_process_cmdline(getpid());
    if (cmd) {
        char *toks[256];
        int n = split_command(cmd, toks, 256);
        for (int i = 0; i < n; i++) json_append(argv, json_string(toks[i]));
        free_tokens(toks, n);
        free(cmd);
    }
    json_set(o, "argv", argv);
    long st = gwstatus_get_process_start_time(getpid());
    if (st >= 0) json_set(o, "start_time", json_number((double)st));
    else json_set(o, "start_time", json_null());
    return o;
}

/* PoP: pid_from_record @ gateway/status.py:_pid_from_record */
static pid_t pid_from_record(json_t *record) {
    if (!record || record->type != JSON_OBJECT) return -1;
    json_t *p = json_obj_get(record, "pid");
    if (!p) return -1;
    if (p->type == JSON_NUMBER) return (pid_t)p->num_val;
    if (p->type == JSON_STRING) {
        char *end = NULL;
        long v = strtol(p->str_val, &end, 10);
        if (end && end != p->str_val) return (pid_t)v;
    }
    return -1;
}

/* PoP: read_pid_record @ gateway/status.py:_read_pid_record */
static json_t *read_pid_record(const char *pid_path) {
    char defbuf[1200];
    if (!pid_path) pid_path = get_pid_path(defbuf, sizeof(defbuf));
    FILE *f = fopen(pid_path, "rb");
    if (!f) return NULL;
    char raw[8192];
    size_t got = fread(raw, 1, sizeof(raw) - 1, f);
    fclose(f);
    if (got == 0) return NULL;
    raw[got] = '\0';
    char *s = raw;
    while (*s == ' ' || *s == '\n' || *s == '\t' || *s == '\r') s++;
    size_t L = strlen(s);
    while (L > 0 && (s[L-1] == ' ' || s[L-1] == '\n' || s[L-1] == '\t' || s[L-1] == '\r')) s[--L] = '\0';
    if (!*s) return NULL;
    json_t *node = json_parse(s, NULL);
    if (node) {
        if (node->type == JSON_OBJECT) return node;
        if (node->type == JSON_NUMBER) {
            json_t *o = json_object();
            json_set(o, "pid", json_number(node->num_val));
            json_free(node);
            return o;
        }
        json_free(node);
        return NULL;
    }
    /* Not JSON: try bare integer. */
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end && end != s) {
        json_t *o = json_object();
        json_set(o, "pid", json_number((double)v));
        return o;
    }
    return NULL;
}

/* PoP: _record_looks_like_gateway @ gateway/status.py:_record_looks_like_gateway */
/* Port of Python: _record_looks_like_gateway — validate identity from
 * PID-file metadata (kind + argv joined) when cmdline is unavailable. */
static bool record_looks_like_gateway(json_t *record) {
    if (!record) return false;
    const char *kind = json_get_str(record, "kind", "");
    if (strcmp(kind, GATEWAY_KIND) != 0) return false;
    json_t *argv = json_obj_get(record, "argv");
    if (!argv || argv->type != JSON_ARRAY || json_len(argv) == 0) return false;
    /* join argv with spaces */
    size_t cap = 256;
    for (size_t i = 0; i < json_len(argv); i++) {
        json_t *e = json_get(argv, i);
        if (e && e->type == JSON_STRING) cap += strlen(e->str_val) + 1;
    }
    char *joined = malloc(cap);
    if (!joined) return false;
    joined[0] = '\0';
    size_t off = 0;
    for (size_t i = 0; i < json_len(argv); i++) {
        json_t *e = json_get(argv, i);
        const char *part = (e && e->type == JSON_STRING) ? e->str_val : "";
        off += (size_t)snprintf(joined + off, cap - off, "%s%s", i ? " " : "", part);
    }
    bool ok = gwstatus_looks_like_gateway_runtime_command_line(joined);
    free(joined);
    return ok;
}

/* PoP: _record_matches_live_gateway_pid @ gateway/status.py:_record_matches_live_gateway_pid */
/* Port of Python: _record_matches_live_gateway_pid. expected_home may be NULL
 * (active-profile case: any live gateway command line is acceptable); when set,
 * the readable live cmdline must additionally belong to that profile. */
static bool record_matches_live_gateway_pid(json_t *record, pid_t pid,
                                            const char *expected_home) {
    char *live = read_process_cmdline(pid);
    if (live) {
        bool ok = gwstatus_looks_like_gateway_runtime_command_line(live);
        if (ok && expected_home && expected_home[0])
            ok = command_line_belongs_to_profile(live, expected_home);
        free(live);
        return ok;
    }
    return record_looks_like_gateway(record);
}

/* -----------------------------------------------------------------------
 * PoP: profile_name_for_home @ gateway/status.py:_profile_name_for_home
 * A named profile's home is "<root>/profiles/<name>" (immediate parent is
 * "profiles"). The root/default home has no such parent -> NULL (default).
 * Writes the profile name into out[] and returns out, or NULL for default.
 * --------------------------------------------------------------------- */
static const char *profile_name_for_home(const char *profile_home,
                                         char *out, size_t n) {
    if (!profile_home || !profile_home[0]) return NULL;
    /* strip a trailing slash for a stable basename/parent split */
    char tmp[1200];
    snprintf(tmp, sizeof(tmp), "%s", profile_home);
    size_t L = strlen(tmp);
    while (L > 1 && tmp[L - 1] == '/') tmp[--L] = '\0';
    char *slash = strrchr(tmp, '/');
    if (!slash) return NULL;
    const char *base = slash + 1;
    *slash = '\0';
    char *pslash = strrchr(tmp, '/');
    const char *parent = pslash ? pslash + 1 : tmp;
    if (strcmp(parent, "profiles") != 0) return NULL;
    snprintf(out, n, "%s", base);
    return out;
}

/* Lowercase a string into a caller buffer. */
static void str_tolower_buf(const char *src, char *dst, size_t n) {
    size_t i = 0;
    for (; src && src[i] && i + 1 < n; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

/* -----------------------------------------------------------------------
 * PoP: command_line_belongs_to_profile @ gateway/status.py:_command_line_belongs_to_profile
 * Mirrors hermes_cli.gateway._matches_current_profile so a cross-profile
 * liveness fallback scopes a recycled PID to the right profile.
 * --------------------------------------------------------------------- */
static bool command_line_belongs_to_profile(const char *command,
                                            const char *profile_home) {
    if (!command) return false;
    char command_lc[4096];
    str_tolower_buf(command, command_lc, sizeof(command_lc));
    char home_lc[1200];
    str_tolower_buf(profile_home ? profile_home : "", home_lc, sizeof(home_lc));

    char namebuf[512];
    const char *profile_name = profile_name_for_home(profile_home, namebuf,
                                                     sizeof(namebuf));

    if (profile_name && strcmp(profile_name, "default") != 0) {
        char profile_lc[512];
        str_tolower_buf(profile_name, profile_lc, sizeof(profile_lc));
        char needle1[600], needle2[600], needle3[1400];
        snprintf(needle1, sizeof(needle1), "--profile %s", profile_lc);
        snprintf(needle2, sizeof(needle2), "-p %s", profile_lc);
        snprintf(needle3, sizeof(needle3), "hermes_home=%s", home_lc);
        return strstr(command_lc, needle1) || strstr(command_lc, needle2) ||
               strstr(command_lc, needle3);
    }

    /* Default/root profile: bare (no profile flag). Reject if the command
     * advertises some OTHER profile or a conflicting explicit HERMES_HOME. */
    if (strstr(command_lc, "--profile ") || strstr(command_lc, " -p "))
        return false;
    if (strstr(command_lc, "hermes_home=")) {
        char needle3[1400];
        snprintf(needle3, sizeof(needle3), "hermes_home=%s", home_lc);
        if (!strstr(command_lc, needle3)) return false;
    }
    return true;
}

/* -----------------------------------------------------------------------
 * PoP: read_gateway_lock_record @ gateway/status.py:_read_gateway_lock_record
 * The gateway lock file carries the same record shape as the PID file.
 * --------------------------------------------------------------------- */
static json_t *read_gateway_lock_record(const char *lock_path) {
    char buf[1200];
    const char *path = lock_path;
    if (!path) { get_gateway_lock_path(NULL, buf, sizeof(buf)); path = buf; }
    return read_pid_record(path);
}

/* -----------------------------------------------------------------------
 * PoP: write_gateway_lock_record @ gateway/status.py:_write_gateway_lock_record
 * Rewrite an open lock fd from offset 0 with the current PID record + fsync.
 * --------------------------------------------------------------------- */
static void write_gateway_lock_record(int fd) {
    if (fd < 0) return;
    json_t *record = build_pid_record();
    char *ser = json_serialize(record);
    json_free(record);
    if (!ser) return;
    if (lseek(fd, 0, SEEK_SET) == 0 && ftruncate(fd, 0) == 0) {
        size_t len = strlen(ser);
        ssize_t w = write(fd, ser, len);
        (void)w;
        fsync(fd);
    }
    free(ser);
}

/* -----------------------------------------------------------------------
 * PoP: cleanup_invalid_pid_path @ gateway/status.py:_cleanup_invalid_pid_path
 * Force-unlink a stale PID file and its sibling lock metadata (called after
 * the runtime lock is confirmed inactive, so the metadata is known dead).
 * --------------------------------------------------------------------- */
static void cleanup_invalid_pid_path(const char *pid_path, bool cleanup_stale) {
    if (!cleanup_stale || !pid_path) return;
    unlink(pid_path);
    char lock[1200];
    get_gateway_lock_path(pid_path, lock, sizeof(lock));
    unlink(lock);
}

/* PoP: acquire_gateway_runtime_lock @ gateway/status.py:acquire_gateway_runtime_lock */
/* Port of Python: acquire_gateway_runtime_lock */
bool gwstatus_acquire_gateway_runtime_lock(void) {
    if (g_gateway_lock_fd >= 0) return true;  /* already held */
    char path[1200];
    get_gateway_lock_path(NULL, path, sizeof(path));
    ensure_parent_dir(path);
    int fd = open(path, O_CREAT | O_RDWR, 0600);
    if (fd < 0) return false;
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) { close(fd); return false; }
    /* Write the full PID record (kind/argv/start_time) so a fallback reader
     * can validate gateway identity from the lock file. */
    write_gateway_lock_record(fd);
    g_gateway_lock_fd = fd;
    return true;
}

/* PoP: release_gateway_runtime_lock @ gateway/status.py:release_gateway_runtime_lock */
/* Port of Python: release_gateway_runtime_lock */
void gwstatus_release_gateway_runtime_lock(void) {
    if (g_gateway_lock_fd < 0) return;
    flock(g_gateway_lock_fd, LOCK_UN);
    close(g_gateway_lock_fd);
    g_gateway_lock_fd = -1;
}

/* PoP: gwstatus_is_gateway_runtime_lock_active @ gateway/status.py:is_gateway_runtime_lock_active */
/* PoP: gwstatus_is_gateway_runtime_lock_active @ gateway/status.py:_try_acquire_file_lock */
/* PoP: gwstatus_is_gateway_runtime_lock_active @ gateway/status.py:_release_file_lock */
/* flock LOCK_EX|LOCK_NB acquire + LOCK_UN release are inlined here and in
 * acquire/release_gateway_runtime_lock — POSIX has no separate handle type. */
bool gwstatus_is_gateway_runtime_lock_active(const char *lock_path) {
    char buf[1200];
    const char *path = lock_path;
    if (!path) { get_gateway_lock_path(NULL, buf, sizeof(buf)); path = buf; }
    /* If we hold it in-process, it is active. */
    if (g_gateway_lock_fd >= 0) return true;
    int fd = open(path, O_RDWR, 0600);
    if (fd < 0) {
        if (errno == ENOENT) return false;
        /* Cannot open but exists: assume active (conservative). */
        struct stat st;
        return stat(path, &st) == 0;
    }
    /* Try to grab it non-blocking; success => nobody held it => not active. */
    if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
        flock(fd, LOCK_UN);
        close(fd);
        return false;
    }
    close(fd);
    return true;  /* held by someone else */
}

/* ── PID file ────────────────────────────────────────────────────────── */

/* PoP: write_pid_file @ gateway/status.py:write_pid_file */
/* Port of Python: write_pid_file */
int gwstatus_write_pid_file(void) {
    char path[1200];
    get_pid_path(path, sizeof(path));
    ensure_parent_dir(path);
    json_t *record = build_pid_record();
    int rc = write_json_file(path, record);
    json_free(record);
    return rc;
}

/* PoP: remove_pid_file @ gateway/status.py:remove_pid_file */
/* Port of Python: remove_pid_file — only if it belongs to this process. */
void gwstatus_remove_pid_file(void) {
    char path[1200];
    get_pid_path(path, sizeof(path));
    json_t *record = read_pid_record(path);
    if (!record) { unlink(path); return; }
    pid_t rec_pid = pid_from_record(record);
    json_free(record);
    if (rec_pid == getpid() || rec_pid <= 0)
        unlink(path);
}

/* PoP: get_running_pid @ gateway/status.py:get_running_pid */
/* Port of Python: get_running_pid */
pid_t gwstatus_get_running_pid(const char *pid_path, bool cleanup_stale) {
    char defbuf[1200];
    bool default_home = (pid_path == NULL);
    if (!pid_path) pid_path = get_pid_path(defbuf, sizeof(defbuf));

    char lockbuf[1200];
    get_gateway_lock_path(pid_path, lockbuf, sizeof(lockbuf));

    /* If the runtime lock is not held by anyone, the on-disk metadata is
     * known dead: try the runtime-status fallback, then clean up. */
    if (!gwstatus_is_gateway_runtime_lock_active(lockbuf)) {
        if (default_home) {
            pid_t rp = gwstatus_get_runtime_status_running_pid(NULL, NULL);
            if (rp > 0) return rp;
        }
        cleanup_invalid_pid_path(pid_path, cleanup_stale);
        return -1;
    }

    json_t *primary = read_pid_record(pid_path);
    json_t *fallback = read_gateway_lock_record(lockbuf);

    json_t *records[2] = { primary, fallback };
    pid_t found = -1;
    for (int i = 0; i < 2 && found < 0; i++) {
        json_t *record = records[i];
        pid_t pid = pid_from_record(record);
        if (pid <= 0) continue;
        if (!gwstatus_pid_exists(pid)) continue;

        json_t *st = record ? json_obj_get(record, "start_time") : NULL;
        if (st && st->type == JSON_NUMBER) {
            long recorded = (long)st->num_val;
            long live = gwstatus_get_process_start_time(pid);
            if (live >= 0 && recorded >= 0 && live != recorded) continue;
        }
        if (record_matches_live_gateway_pid(record, pid, NULL))
            found = pid;
    }

    if (primary) json_free(primary);
    if (fallback) json_free(fallback);

    if (found > 0) return found;

    cleanup_invalid_pid_path(pid_path, cleanup_stale);
    if (default_home) {
        pid_t rp = gwstatus_get_runtime_status_running_pid(NULL, NULL);
        if (rp > 0) return rp;
    }
    return -1;
}

/* -----------------------------------------------------------------------
 * PoP: gwstatus_get_runtime_status_running_pid @ gateway/status.py:get_runtime_status_running_pid
 * Conservative fallback: validate the runtime status record against the OS.
 * --------------------------------------------------------------------- */
pid_t gwstatus_get_runtime_status_running_pid(const char *runtime_json,
                                              const char *expected_home) {
    char *owned = NULL;
    const char *ser = runtime_json;
    if (!ser) {
        owned = gwstatus_read_runtime_status(NULL);
        ser = owned;
    }
    if (!ser) return -1;

    json_t *payload = json_parse(ser, NULL);
    if (owned) free(owned);
    if (!payload || payload->type != JSON_OBJECT) {
        if (payload) json_free(payload);
        return -1;
    }

    /* gateway_state in {null, "stopped", "startup_failed"} => not running. */
    json_t *gs = json_obj_get(payload, "gateway_state");
    if (!gs || gs->type == JSON_NULL) { json_free(payload); return -1; }
    if (gs->type == JSON_STRING &&
        (strcmp(gs->str_val, "stopped") == 0 ||
         strcmp(gs->str_val, "startup_failed") == 0)) {
        json_free(payload);
        return -1;
    }

    pid_t pid = pid_from_record(payload);
    if (pid <= 0 || !gwstatus_pid_exists(pid)) { json_free(payload); return -1; }

    json_t *st = json_obj_get(payload, "start_time");
    if (st && st->type == JSON_NUMBER) {
        long recorded = (long)st->num_val;
        long live = gwstatus_get_process_start_time(pid);
        if (live >= 0 && recorded >= 0 && live != recorded) {
            json_free(payload);
            return -1;
        }
    }

    pid_t result = record_matches_live_gateway_pid(payload, pid, expected_home)
                   ? pid : -1;
    json_free(payload);
    return result;
}

/* PoP: is_gateway_running @ gateway/status.py:is_gateway_running */
/* Port of Python: is_gateway_running */
bool gwstatus_is_gateway_running(const char *pid_path, bool cleanup_stale) {
    return gwstatus_get_running_pid(pid_path, cleanup_stale) > 0;
}

/* ── Cached running-pid probe (Python get_running_pid_cached) ────────── */

/* get_running_pid() probes the runtime lock by briefly opening and locking
 * gateway.lock. That is the right authoritative check for control paths, but
 * high-frequency read-only HTTP polling can call it hundreds of times per
 * minute. Mirror the Python TTL cache: 1.0s window, invalidated when any of
 * the pid / lock / (unscoped) runtime-status files change signature
 * (exists, mtime_ns, size). */

#define RUNNING_PID_CACHE_TTL_NS (1000000000LL)   /* 1.0 s */

typedef struct {
    char        pid_path[1200];   /* "" = unscoped (default home) */
    bool        cleanup_stale;
    bool        include_runtime_status;
    bool        valid;
    struct timespec cached_at;
    bool        sig_exists[3];
    long        sig_mtime_ns[3];
    long        sig_size[3];
    pid_t       pid;
} running_pid_cache_t;

static running_pid_cache_t g_pid_cache;

/* Mirror Python _file_cache_signature: (exists, mtime_ns, size). */
static void file_cache_signature(const char *path, bool *exists,
                                 long *mtime_ns, long *size) {
    struct stat st;
    if (path && stat(path, &st) == 0) {
        *exists = true;
        *mtime_ns = (long)st.st_mtime * 1000000000L + (long)st.st_mtim.tv_nsec;
        *size = (long)st.st_size;
    } else {
        *exists = false;
        *mtime_ns = 0;
        *size = 0;
    }
}

static pid_t cached_get_running_pid(const char *pid_path, bool cleanup_stale) {
    char resolved[1200];
    if (!pid_path) get_pid_path(resolved, sizeof(resolved));
    else {
        snprintf(resolved, sizeof(resolved), "%s", pid_path);
    }
    bool include_runtime_status = (pid_path == NULL);

    char lock_path[1200];
    get_gateway_lock_path(resolved, lock_path, sizeof(lock_path));
    char state_path[1200];
    get_runtime_status_path(state_path, sizeof(state_path));

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    bool sig_ok = true;
    const char *files[3] = { resolved, lock_path,
                             include_runtime_status ? state_path : NULL };
    bool sig_exists[3];
    long sig_mtime[3], sig_size[3];
    for (int i = 0; i < 3; i++) {
        if (!files[i]) { sig_exists[i] = false; sig_mtime[i] = 0; sig_size[i] = 0; continue; }
        file_cache_signature(files[i], &sig_exists[i], &sig_mtime[i], &sig_size[i]);
    }

    if (g_pid_cache.valid &&
        strcmp(g_pid_cache.pid_path, resolved) == 0 &&
        g_pid_cache.cleanup_stale == cleanup_stale &&
        g_pid_cache.include_runtime_status == include_runtime_status) {
        long age_ns = (now.tv_sec - g_pid_cache.cached_at.tv_sec) * 1000000000LL
                    + (now.tv_nsec - g_pid_cache.cached_at.tv_nsec);
        if (age_ns >= 0 && age_ns < RUNNING_PID_CACHE_TTL_NS) {
            for (int i = 0; i < 3 && sig_ok; i++) {
                if (g_pid_cache.sig_exists[i] != sig_exists[i] ||
                    g_pid_cache.sig_mtime_ns[i] != sig_mtime[i] ||
                    g_pid_cache.sig_size[i] != sig_size[i])
                    sig_ok = false;
            }
            if (sig_ok) return g_pid_cache.pid;
        }
    }

    pid_t pid = gwstatus_get_running_pid(pid_path, cleanup_stale);
    g_pid_cache.valid = true;
    g_pid_cache.cached_at = now;
    snprintf(g_pid_cache.pid_path, sizeof(g_pid_cache.pid_path), "%s", resolved);
    g_pid_cache.cleanup_stale = cleanup_stale;
    g_pid_cache.include_runtime_status = include_runtime_status;
    for (int i = 0; i < 3; i++) {
        g_pid_cache.sig_exists[i] = sig_exists[i];
        g_pid_cache.sig_mtime_ns[i] = sig_mtime[i];
        g_pid_cache.sig_size[i] = sig_size[i];
    }
    g_pid_cache.pid = pid;
    return pid;
}

/* PoP: resolve_gateway_liveness @ gateway/status.py:resolve_gateway_liveness */
/* Single source of truth for "is the gateway up?" across dashboard surfaces.
 * Mirrors Python gateway/status.py:resolve_gateway_liveness():
 *   1. PID file + runtime lock (scoped to profile_dir when non-NULL),
 *      TTL-cached when use_cache is true.
 *   2. HTTP health probe (health_probe may be NULL; when non-NULL it is
 *      called as health_probe(&body) and returns true when the gateway is
 *      alive, storing a malloc'd serialized body for the caller to free).
 *   3. Runtime status PID validated against the live process table with
 *      expected_home=profile_dir.
 * runtime_json may be pre-read state (caller owns it); NULL means "not yet
 * read" and the resolver reads it itself. Returns false only on invalid
 * arguments; the ladder result lands in *out. */
bool gwstatus_resolve_gateway_liveness(
    const char *profile_dir,
    const char *runtime_json,
    bool use_cache,
    bool (*health_probe)(char **out_body),
    gwstatus_liveness_t *out)
{
    if (!out) return false;
    out->running = false;
    out->pid = -1;
    out->source = "none";
    out->health_body = NULL;
    out->probe_error = false;

    /* Rung 1: PID file + runtime lock (scoped to profile_dir). */
    char pid_path_buf[1200];
    char *pid_path = NULL;
    if (profile_dir && profile_dir[0]) {
        snprintf(pid_path_buf, sizeof(pid_path_buf), "%s/%s",
                 profile_dir, PID_FILENAME);
        pid_path = pid_path_buf;
    }

    pid_t pid = use_cache ? cached_get_running_pid(pid_path, true)
                          : gwstatus_get_running_pid(pid_path, true);
    if (pid > 0) {
        out->running = true;
        out->pid = pid;
        out->source = "pid";
        return true;
    }

    /* Rung 2: HTTP health probe (caller-supplied). A non-NULL body from a
     * failed probe is carried through to the final result, mirroring the
     * Python behavior of returning health_body on every ladder exit. */
    if (health_probe) {
        char *body = NULL;
        bool alive = health_probe(&body);
        if (alive) {
            pid_t remote_pid = -1;
            if (body) {
                json_t *parsed = json_parse(body, NULL);
                if (parsed && parsed->type == JSON_OBJECT) {
                    json_t *p = json_obj_get(parsed, "pid");
                    if (p && p->type == JSON_NUMBER)
                        remote_pid = (pid_t)p->num_val;
                }
                if (parsed) json_free(parsed);
            }
            out->running = true;
            out->pid = remote_pid;
            out->source = "health";
            out->health_body = body;   /* caller frees; may be NULL */
            return true;
        }
        out->health_body = body;       /* carry through; caller frees */
    }

    /* Rung 3: runtime status PID validated against the live process table. */
    char *owned_runtime = NULL;
    const char *runtime = runtime_json;
    if (!runtime) {
        char state_path[1200];
        if (profile_dir && profile_dir[0])
            snprintf(state_path, sizeof(state_path), "%s/%s",
                     profile_dir, RUNTIME_STATUS_FILE);
        else
            get_runtime_status_path(state_path, sizeof(state_path));
        owned_runtime = gwstatus_read_runtime_status(state_path);
        runtime = owned_runtime;
        if (!runtime) out->probe_error = true;
    }
    pid_t runtime_pid = gwstatus_get_runtime_status_running_pid(runtime,
                                                                profile_dir);
    if (owned_runtime) free(owned_runtime);
    if (runtime_pid > 0) {
        out->running = true;
        out->pid = runtime_pid;
        out->source = "runtime_status";
        return true;
    }

    /* Rung 4: not running. */
    return true;
}

/* ── Runtime health status JSON ──────────────────────────────────────── */

/* PoP: parse_active_agents @ gateway/status.py:parse_active_agents */
/* Port of Python: parse_active_agents (string form). */
int gwstatus_parse_active_agents_str(const char *raw) {
    if (!raw || !raw[0]) return 0;
    /* Python int(raw): accept leading/trailing whitespace, optional sign. */
    char *end = NULL;
    errno = 0;
    long v = strtol(raw, &end, 10);
    if (errno || end == raw) return 0;
    /* trailing non-space => ValueError in Python. */
    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
    if (*end != '\0') return 0;
    if (v < 0) return 0;
    if (v > 0x7fffffff) return 0x7fffffff;
    return (int)v;
}

/* PoP: derive_gateway_busy @ gateway/status.py:derive_gateway_busy */
/* Port of Python: derive_gateway_busy */
bool gwstatus_derive_gateway_busy(bool gateway_running,
                                  const char *gateway_state,
                                  int active_agents) {
    if (!gateway_running) return false;
    if (!gateway_state || strcmp(gateway_state, "running") != 0) return false;
    return active_agents > 0;
}

/* PoP: derive_gateway_drainable @ gateway/status.py:derive_gateway_drainable */
/* Port of Python: derive_gateway_drainable */
bool gwstatus_derive_gateway_drainable(bool gateway_running,
                                       const char *gateway_state) {
    return gateway_running && gateway_state && strcmp(gateway_state, "running") == 0;
}

/* PoP: _build_runtime_status_record @ gateway/status.py:_build_runtime_status_record */
/* Port of Python: _build_runtime_status_record — seed record when file absent. */
static json_t *build_runtime_status_record(void) {
    json_t *o = build_pid_record();
    json_set(o, "gateway_state", json_string("starting"));
    json_set(o, "exit_reason", json_null());
    json_set(o, "restart_requested", json_bool(false));
    json_set(o, "active_agents", json_number(0));
    json_set(o, "platforms", json_object());
    char iso[64]; utc_now_iso(iso, sizeof(iso));
    json_set(o, "updated_at", json_string(iso));
    return o;
}

/* PoP: write_runtime_status @ gateway/status.py:write_runtime_status */
/* Port of Python: write_runtime_status */
int gwstatus_write_runtime_status(const char *gateway_state,
                                  const char *exit_reason,
                                  int restart_requested,
                                  int active_agents,
                                  const char *platform,
                                  const char *platform_state,
                                  const char *error_code,
                                  const char *error_message) {
    char path[1200];
    get_runtime_status_path(path, sizeof(path));

    json_t *payload = read_json_file(path);
    if (!payload) payload = build_runtime_status_record();

    /* ensure platforms sub-object */
    json_t *plats = json_obj_get(payload, "platforms");
    if (!plats || plats->type != JSON_OBJECT) {
        json_set(payload, "platforms", json_object());
        plats = json_obj_get(payload, "platforms");
    }

    /* refresh identity fields from current process */
    json_t *cur = build_pid_record();
    json_set(payload, "kind", json_string(json_get_str(cur, "kind", GATEWAY_KIND)));
    json_set(payload, "pid", json_number(json_get_num(cur, "pid", (double)getpid())));
    json_t *cargv = json_obj_get(cur, "argv");
    json_set(payload, "argv", cargv ? json_copy(cargv) : json_array());
    json_t *cst = json_obj_get(cur, "start_time");
    json_set(payload, "start_time", cst ? json_copy(cst) : json_null());
    json_free(cur);

    char iso[64]; utc_now_iso(iso, sizeof(iso));
    json_set(payload, "updated_at", json_string(iso));

    if (gateway_state) json_set(payload, "gateway_state", json_string(gateway_state));
    if (exit_reason) {
        if (exit_reason[0]) json_set(payload, "exit_reason", json_string(exit_reason));
        else json_set(payload, "exit_reason", json_null());
    }
    if (restart_requested >= 0)
        json_set(payload, "restart_requested", json_bool(restart_requested != 0));
    if (active_agents >= 0)
        json_set(payload, "active_agents", json_number(active_agents));

    if (platform && platform[0]) {
        json_t *pp = json_obj_get(plats, platform);
        json_t *platform_payload;
        if (pp && pp->type == JSON_OBJECT) {
            platform_payload = json_copy(pp);
        } else {
            platform_payload = json_object();
        }
        if (platform_state) json_set(platform_payload, "state", json_string(platform_state));
        if (error_code) json_set(platform_payload, "error_code", json_string(error_code));
        if (error_message) json_set(platform_payload, "error_message", json_string(error_message));
        char piso[64]; utc_now_iso(piso, sizeof(piso));
        json_set(platform_payload, "updated_at", json_string(piso));
        json_set(plats, platform, platform_payload);
    }

    int rc = write_json_file(path, payload);
    json_free(payload);
    return rc;
}

/* PoP: read_runtime_status @ gateway/status.py:read_runtime_status */
/* Port of Python: read_runtime_status — serialized string or NULL. */
char *gwstatus_read_runtime_status(const char *path) {
    char defbuf[1200];
    if (!path) path = get_runtime_status_path(defbuf, sizeof(defbuf));
    json_t *node = read_json_file(path);
    if (!node) return NULL;
    char *out = json_serialize(node);
    json_free(node);
    return out;
}

/* ── Scope locks ─────────────────────────────────────────────────────── */

/* PoP: _get_scope_lock_path @ gateway/status.py:_get_scope_lock_path */
/* Port of Python: _get_scope_lock_path -> "<lockdir>/<scope>-<hash>.lock". */
static char *get_scope_lock_path(const char *scope, const char *identity,
                                 char *buf, size_t n) {
    char dir[1100];
    get_lock_dir(dir, sizeof(dir));
    char h[17];
    scope_hash(identity, h);
    snprintf(buf, n, "%s/%s-%s.lock", dir, scope, h);
    return buf;
}

/* Is /proc/<pid> in a stopped state (T/t)? */
static bool proc_is_stopped(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f) return false;
    char line[256];
    bool stopped = false;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "State:", 6) == 0) {
            /* State:\t<char> ... */
            char *p = line + 6;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == 'T' || *p == 't') stopped = true;
            break;
        }
    }
    fclose(f);
    return stopped;
}

/* Long value or LONG_MIN if key absent/null. */
static long record_start_time(json_t *rec) {
    json_t *st = json_obj_get(rec, "start_time");
    if (!st || st->type != JSON_NUMBER) return LONG_MIN;
    return (long)st->num_val;
}

/* PoP: acquire_scoped_lock @ gateway/status.py:acquire_scoped_lock */
/* Port of Python: acquire_scoped_lock */
bool gwstatus_acquire_scoped_lock(const char *scope, const char *identity,
                                  const char *metadata_json,
                                  char **out_existing) {
    if (out_existing) *out_existing = NULL;
    char lock_path[1200];
    get_scope_lock_path(scope, identity, lock_path, sizeof(lock_path));
    ensure_parent_dir(lock_path);

    /* Build our record. */
    char h[17]; scope_hash(identity, h);
    json_t *record = build_pid_record();
    json_set(record, "scope", json_string(scope));
    json_set(record, "identity_hash", json_string(h));
    if (metadata_json && metadata_json[0]) {
        json_t *md = json_parse(metadata_json, NULL);
        json_set(record, "metadata", (md && md->type == JSON_OBJECT) ? md : json_object());
        if (md && md->type != JSON_OBJECT) json_free(md);
    } else {
        json_set(record, "metadata", json_object());
    }
    char iso[64]; utc_now_iso(iso, sizeof(iso));
    json_set(record, "updated_at", json_string(iso));

    json_t *existing = read_json_file(lock_path);
    struct stat stbuf;
    if (!existing && stat(lock_path, &stbuf) == 0) {
        /* File present but empty/invalid — treat as stale. */
        unlink(lock_path);
    }
    if (existing) {
        pid_t existing_pid = pid_from_record(existing);
        long our_start = record_start_time(record);
        long existing_start = record_start_time(existing);

        if (existing_pid == getpid() && existing_start == our_start) {
            /* Re-acquire our own lock; refresh. */
            write_json_file(lock_path, record);
            if (out_existing) *out_existing = json_serialize(existing);
            json_free(existing);
            json_free(record);
            return true;
        }

        bool stale = (existing_pid <= 0);
        if (!stale) {
            if (!gwstatus_pid_exists(existing_pid)) {
                stale = true;
            } else {
                long current_start = gwstatus_get_process_start_time(existing_pid);
                if (existing_start != LONG_MIN && current_start >= 0 &&
                    current_start != existing_start) {
                    stale = true;
                }
                /* start_time unknown on both sides: fall back to cmdline. */
                if (!stale && existing_start == LONG_MIN && current_start < 0 &&
                    !looks_like_gateway_process(existing_pid)) {
                    char *live = read_process_cmdline(existing_pid);
                    if (live != NULL || !record_looks_like_gateway(existing)) stale = true;
                    free(live);
                }
                /* PID+start_time collide but live process isn't a gateway. */
                if (!stale && existing_start != LONG_MIN && current_start >= 0 &&
                    !looks_like_gateway_process(existing_pid)) {
                    char *live = read_process_cmdline(existing_pid);
                    if (live != NULL) stale = true;
                    free(live);
                }
                /* Stopped (Ctrl+Z) process: treat as stale. */
                if (!stale && proc_is_stopped(existing_pid)) stale = true;
            }
        }

        if (stale) {
            unlink(lock_path);
        } else {
            if (out_existing) *out_existing = json_serialize(existing);
            json_free(existing);
            json_free(record);
            return false;
        }
        json_free(existing);
    }

    /* Atomic create. */
    int fd = open(lock_path, O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (fd < 0) {
        if (errno == EEXIST) {
            json_t *now = read_json_file(lock_path);
            if (out_existing && now) *out_existing = json_serialize(now);
            if (now) json_free(now);
        }
        json_free(record);
        return false;
    }
    char *ser = json_serialize(record);
    if (ser) {
        ssize_t w = write(fd, ser, strlen(ser));
        (void)w;
        free(ser);
    }
    close(fd);
    json_free(record);
    return true;
}

/* PoP: release_scoped_lock @ gateway/status.py:release_scoped_lock */
/* Port of Python: release_scoped_lock */
void gwstatus_release_scoped_lock(const char *scope, const char *identity) {
    char lock_path[1200];
    get_scope_lock_path(scope, identity, lock_path, sizeof(lock_path));
    json_t *existing = read_json_file(lock_path);
    if (!existing) return;
    pid_t p = pid_from_record(existing);
    long est = record_start_time(existing);
    json_free(existing);
    if (p != getpid()) return;
    if (est != gwstatus_get_process_start_time(getpid())) return;
    unlink(lock_path);
}

/* PoP: release_all_scoped_locks @ gateway/status.py:release_all_scoped_locks */
/* Port of Python: release_all_scoped_locks */
int gwstatus_release_all_scoped_locks(pid_t owner_pid, long owner_start_time) {
    char dir[1100];
    get_lock_dir(dir, sizeof(dir));
    DIR *d = opendir(dir);
    if (!d) return 0;
    int removed = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;
        size_t nl = strlen(name);
        if (nl < 5 || strcmp(name + nl - 5, ".lock") != 0) continue;
        char full[1300];
        snprintf(full, sizeof(full), "%s/%s", dir, name);
        if (owner_pid > 0) {
            json_t *rec = read_json_file(full);
            if (!rec) continue;
            pid_t rp = pid_from_record(rec);
            long rst = record_start_time(rec);
            json_free(rec);
            if (rp != owner_pid) continue;
            if (owner_start_time >= 0 && rst != owner_start_time) continue;
        }
        if (unlink(full) == 0) removed++;
    }
    closedir(d);
    return removed;
}

/* ── --replace takeover / planned-stop markers ───────────────────────── */

/* PoP: _marker_is_stale @ gateway/status.py:_marker_is_stale */
/* Port of Python: _marker_is_stale — true if age > ttl or unparseable. */
static bool marker_is_stale(const char *written_at, int ttl_s) {
    if (!written_at || !written_at[0]) return true;
    /* Parse ISO 8601 (may carry +00:00 and fractional seconds). */
    struct tm tmv; memset(&tmv, 0, sizeof(tmv));
    int y, mo, d, h, mi, s;
    if (sscanf(written_at, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6)
        return true;
    tmv.tm_year = y - 1900; tmv.tm_mon = mo - 1; tmv.tm_mday = d;
    tmv.tm_hour = h; tmv.tm_min = mi; tmv.tm_sec = s;
    time_t written = timegm(&tmv);  /* value was UTC */
    if (written == (time_t)-1) return true;
    double age = difftime(time(NULL), written);
    return age > (double)ttl_s;
}

/* PoP: _consume_pid_marker_for_self @ gateway/status.py:_consume_pid_marker_for_self */
/* Port of Python: _consume_pid_marker_for_self. Always unlinks a matched or
 * stale/malformed marker. Returns true when the marker names this process. */
static bool consume_pid_marker_for_self(const char *path,
                                        const char *pid_field,
                                        const char *start_time_field,
                                        int ttl_s) {
    json_t *record = read_json_file(path);
    if (!record) return false;

    json_t *tp = json_obj_get(record, pid_field);
    if (!tp || (tp->type != JSON_NUMBER && tp->type != JSON_STRING)) {
        json_free(record);
        unlink(path);
        return false;
    }
    pid_t target_pid = (tp->type == JSON_NUMBER)
        ? (pid_t)tp->num_val : (pid_t)strtol(tp->str_val, NULL, 10);
    json_t *tst = json_obj_get(record, start_time_field);
    long target_start = (tst && tst->type == JSON_NUMBER) ? (long)tst->num_val : LONG_MIN;
    const char *written_at = json_get_str(record, "written_at", "");

    if (marker_is_stale(written_at, ttl_s)) {
        json_free(record);
        unlink(path);
        return false;
    }

    /* Cross-home guard: reject markers written under a different home. */
    json_t *rh = json_obj_get(record, "replacer_hermes_home");
    if (rh && rh->type == JSON_STRING) {
        if (strcmp(rh->str_val, slermes_home()) != 0) {
            json_free(record);
            return false;  /* leave marker in place for the correct home */
        }
    }

    pid_t our_pid = getpid();
    long our_start = gwstatus_get_process_start_time(our_pid);
    bool matches;
    if (target_pid != our_pid) {
        matches = false;
    } else if (target_start != LONG_MIN && our_start >= 0) {
        matches = (target_start == our_start);
    } else {
        matches = true;
    }

    json_free(record);
    unlink(path);
    return matches;
}

/* PoP: write_takeover_marker @ gateway/status.py:write_takeover_marker */
/* Port of Python: write_takeover_marker */
bool gwstatus_write_takeover_marker(pid_t target_pid) {
    long target_start = gwstatus_get_process_start_time(target_pid);
    json_t *record = json_object();
    json_set(record, "target_pid", json_number((double)target_pid));
    if (target_start >= 0) json_set(record, "target_start_time", json_number((double)target_start));
    else json_set(record, "target_start_time", json_null());
    json_set(record, "replacer_pid", json_number((double)getpid()));
    json_set(record, "replacer_hermes_home", json_string(slermes_home()));
    char iso[64]; utc_now_iso(iso, sizeof(iso));
    json_set(record, "written_at", json_string(iso));
    char path[1200];
    get_takeover_marker_path(path, sizeof(path));
    int rc = write_json_file(path, record);
    json_free(record);
    return rc == 0;
}

/* PoP: consume_takeover_marker_for_self @ gateway/status.py:consume_takeover_marker_for_self */
/* Port of Python: consume_takeover_marker_for_self */
bool gwstatus_consume_takeover_marker_for_self(void) {
    char path[1200];
    get_takeover_marker_path(path, sizeof(path));
    return consume_pid_marker_for_self(path, "target_pid",
                                       "target_start_time", TAKEOVER_MARKER_TTL_S);
}

/* PoP: clear_takeover_marker @ gateway/status.py:clear_takeover_marker */
/* Port of Python: clear_takeover_marker */
void gwstatus_clear_takeover_marker(void) {
    char path[1200];
    get_takeover_marker_path(path, sizeof(path));
    unlink(path);
}

/* PoP: write_planned_stop_marker @ gateway/status.py:write_planned_stop_marker */
/* Port of Python: write_planned_stop_marker */
bool gwstatus_write_planned_stop_marker(pid_t target_pid) {
    long target_start = gwstatus_get_process_start_time(target_pid);
    json_t *record = json_object();
    json_set(record, "target_pid", json_number((double)target_pid));
    if (target_start >= 0) json_set(record, "target_start_time", json_number((double)target_start));
    else json_set(record, "target_start_time", json_null());
    json_set(record, "stopper_pid", json_number((double)getpid()));
    char iso[64]; utc_now_iso(iso, sizeof(iso));
    json_set(record, "written_at", json_string(iso));
    char path[1200];
    get_planned_stop_marker_path(path, sizeof(path));
    int rc = write_json_file(path, record);
    json_free(record);
    return rc == 0;
}

/* PoP: consume_planned_stop_marker_for_self @ gateway/status.py:consume_planned_stop_marker_for_self */
/* Port of Python: consume_planned_stop_marker_for_self */
bool gwstatus_consume_planned_stop_marker_for_self(void) {
    char path[1200];
    get_planned_stop_marker_path(path, sizeof(path));
    return consume_pid_marker_for_self(path, "target_pid",
                                       "target_start_time", PLANNED_STOP_MARKER_TTL_S);
}

/* PoP: planned_stop_marker_targets_self @ gateway/status.py:planned_stop_marker_targets_self */
/* Port of Python: planned_stop_marker_targets_self — non-destructive probe. */
bool gwstatus_planned_stop_marker_targets_self(void) {
    char path[1200];
    get_planned_stop_marker_path(path, sizeof(path));
    json_t *record = read_json_file(path);
    if (!record) return false;

    json_t *tp = json_obj_get(record, "target_pid");
    if (!tp || (tp->type != JSON_NUMBER && tp->type != JSON_STRING)) {
        json_free(record);
        unlink(path);  /* malformed: drop */
        return false;
    }
    pid_t target_pid = (tp->type == JSON_NUMBER)
        ? (pid_t)tp->num_val : (pid_t)strtol(tp->str_val, NULL, 10);
    json_t *tst = json_obj_get(record, "target_start_time");
    long target_start = (tst && tst->type == JSON_NUMBER) ? (long)tst->num_val : LONG_MIN;
    const char *written_at = json_get_str(record, "written_at", "");

    if (marker_is_stale(written_at, PLANNED_STOP_MARKER_TTL_S)) {
        json_free(record);
        unlink(path);
        return false;
    }

    pid_t our_pid = getpid();
    if (target_pid != our_pid) { json_free(record); return false; }

    long our_start = gwstatus_get_process_start_time(our_pid);
    bool result;
    if (target_start != LONG_MIN && our_start >= 0)
        result = (target_start == our_start);
    else
        result = true;
    json_free(record);
    return result;  /* never unlink a matching marker (non-destructive) */
}

/* PoP: clear_planned_stop_marker @ gateway/status.py:clear_planned_stop_marker */
/* Port of Python: clear_planned_stop_marker */
void gwstatus_clear_planned_stop_marker(void) {
    char path[1200];
    get_planned_stop_marker_path(path, sizeof(path));
    unlink(path);
}
