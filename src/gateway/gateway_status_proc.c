/*
 * gateway_status_proc.c - gateway process inspection helpers.
 *
 * Split from status.c (soul.md: self-contained modules). Reads a process
 * cmdline, tokenises it and decides whether it belongs to this gateway.
 * No file-scope state.
 */
#include "gateway_status_proc.h"

bool command_line_belongs_to_profile(const char *command,
                                            const char *profile_home);
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>



/* ── Process control ─────────────────────────────────────────────────── */

/* Port of Python: terminate_pid */
int gwstatus_terminate_pid(pid_t pid, bool force) {
    int sig = force ? SIGKILL : SIGTERM;
    if (kill(pid, sig) != 0) return -1;
    return 0;
}


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

/* Port of Python: _read_process_cmdline. Linux: /proc/<pid>/cmdline with
 * NUL separators mapped to spaces; else `ps -p <pid> -o command=`.
 * Returns a malloc'd string (caller frees) or NULL. */ char *read_process_cmdline(pid_t pid) {
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
 * token is a malloc'd string; caller frees via free_tokens(). */ int split_command(const char *command, char **argv, int max) {
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
 void free_tokens(char **argv, int argc) {
    for (int i = 0; i < argc; i++) free(argv[i]);
}


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


/* Port of Python: looks_like_gateway_command_line */
bool gwstatus_looks_like_gateway_command_line(const char *command) {
    char *sub = gateway_command_subcommand(command);
    bool ok = (sub && strcmp(sub, "run") == 0);
    free(sub);
    return ok;
}


/* Port of Python: looks_like_gateway_runtime_command_line */
bool gwstatus_looks_like_gateway_runtime_command_line(const char *command) {
    char *sub = gateway_command_subcommand(command);
    bool ok = (sub && (strcmp(sub, "run") == 0 || strcmp(sub, "restart") == 0));
    free(sub);
    return ok;
}


/* Port of Python: _looks_like_gateway_process */ bool looks_like_gateway_process(pid_t pid) {
    char *cmd = read_process_cmdline(pid);
    if (!cmd) return false;
    bool ok = gwstatus_looks_like_gateway_command_line(cmd);
    free(cmd);
    return ok;
}


/* Port of Python: _record_looks_like_gateway — validate identity from
 * PID-file metadata (kind + argv joined) when cmdline is unavailable. */ bool record_looks_like_gateway(json_t *record) {
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


/* Port of Python: _record_matches_live_gateway_pid. expected_home may be NULL
 * (active-profile case: any live gateway command line is acceptable); when set,
 * the readable live cmdline must additionally belong to that profile. */ bool record_matches_live_gateway_pid(json_t *record, pid_t pid,
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
bool command_line_belongs_to_profile(const char *command,
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
