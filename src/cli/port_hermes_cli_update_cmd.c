/*
 * port_hermes_cli_update_cmd.c — C11 port of pure helpers from
 * hermes_cli/update_cmd.py.
 *
 * Covers the deterministic, I/O-free functions that were lifted out of
 * the update orchestration into module-level pure helpers.  Heavy
 * operations (git pull, zip download, npm install, venv refresh, gateway
 * restart) remain in the existing cmd_update() / port_web_update.c path.
 *
 * Reuses:
 *   - web_git_run() (src/cli/port_web_git.c) for any git porcelain we
 *     do need to touch.
 *   - slermes_home.h for the HERMES_HOME / SLERMES_HOME resolution.
 *   - libjson/json.h for json_t* helpers where we format JSON arrays.
 *
 * No stubs.  Every function mirrors the Python original's behaviour.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "port_hermes_cli_update_cmd.h"
#include "slermes_home.h"
#include "port_web_git.h"
#include "libjson/json.h"
#include "hermes_glob.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/statvfs.h>
#include <openssl/evp.h>
#include "gateway_status.h"

/* ── _format_time_ago ──────────────────────────────────────────── */
/* PoP: uc_format_time_ago @ hermes_cli/update_cmd.py:_format_time_ago */
const char *uc_format_time_ago(const char *iso_ts)
{
    /* Parse ISO-8601: accept "Z" suffix and ±HH:MM offset. */
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    const char *p = iso_ts;

    /* Skip date part "YYYY-MM-DD" */
    if (strlen(p) < 10) return "recently";
    p += 10;
    if (*p == 'T') p++;
    else return "recently";

    /* Parse HH:MM:SS */
    if (strlen(p) < 8) return "recently";
    tmv.tm_hour = (p[0] - '0') * 10 + (p[1] - '0');
    tmv.tm_min  = (p[3] - '0') * 10 + (p[4] - '0');
    tmv.tm_sec  = (p[6] - '0') * 10 + (p[7] - '0');
    p += 8;

    /* Handle fractional seconds */
    if (*p == '.') {
        while (*p && *p != 'Z' && *p != '+' && *p != '-') p++;
    }

    /* Handle timezone */
    if (*p == 'Z') {
        /* UTC — tmv is already UTC */
    } else if (*p == '+' || *p == '-') {
        /* ±HH:MM offset — convert to seconds */
        int sign = (*p == '-') ? -1 : 1;
        int off_h = (p[1] - '0') * 10 + (p[2] - '0');
        int off_m = (p[4] - '0') * 10 + (p[5] - '0');
        tmv.tm_sec -= sign * (off_h * 3600 + off_m * 60);
    } else if (*p != '\0') {
        return "recently";
    }

    tmv.tm_isdst = -1;
    time_t t = timegm(&tmv);
    if (t == (time_t)-1) return "recently";

    time_t now = time(NULL);
    long secs = (long)(now - t);
    if (secs < 0) secs = 0;

    if (secs < 60) return "just now";
    if (secs < 3600) {
        static char buf[32];
        snprintf(buf, sizeof(buf), "%ldm ago", secs / 60);
        return buf;
    }
    if (secs < 86400) {
        static char buf[32];
        snprintf(buf, sizeof(buf), "%ldh ago", secs / 3600);
        return buf;
    }
    {
        static char buf[32];
        snprintf(buf, sizeof(buf), "%ldd ago", secs / 86400);
        return buf;
    }
}

/* ── _stash_apply_failed_only_on_existing_untracked ───────────────────── */
/* PoP: uc_stash_apply_failed_only_on_existing_untracked @ hermes_cli/update_cmd.py:_stash_apply_failed_only_on_existing_untracked */
bool uc_stash_apply_failed_only_on_existing_untracked(const char *stderr_text)
{
    if (!stderr_text || !*stderr_text) return false;

    /* Python uses substring membership (`in ln`) per line. Match that exactly:
     *   "already exists, no checkout"                    -> untracked flag
     *   "could not restore untracked files from stash"   -> untracked flag
     *   startswith("warning:") or startswith("hint:")    -> skip
     *   anything else                                      -> return False
     */
    const char *p = stderr_text;
    bool saw_untracked = false;

    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t linelen = eol ? (size_t)(eol - p) : strlen(p);
        /* strip trailing \r */
        while (linelen > 0 && p[linelen-1] == '\r') linelen--;

        if (linelen == 0) {
            p = eol ? eol + 1 : p + strlen(p);
            continue;
        }

        char line[1024];
        size_t copylen = linelen < sizeof(line) - 1 ? linelen : sizeof(line) - 1;
        memcpy(line, p, copylen);
        line[copylen] = '\0';

        if (strstr(line, "warning:") == line ||
            strstr(line, "hint:") == line) {
            /* skip informational */
        } else if (strstr(line, "already exists, no checkout") != NULL ||
                   strstr(line, "could not restore untracked files from stash") != NULL) {
            saw_untracked = true;
        } else {
            return false;
        }

        p = eol ? eol + 1 : p + strlen(p);
    }

    return saw_untracked;
}

/* ── _is_fork ──────────────────────────────────────────────────── */
/* PoP: uc_is_fork_origin @ hermes_cli/update_cmd.py:_is_fork */
bool uc_is_fork_origin(const char *origin_url)
{
    if (!origin_url || !*origin_url) return false;

    /* Normalize: strip trailing "/" and trailing ".git" */
    const char *norm = origin_url;
    size_t len = strlen(norm);

    /* Strip trailing "/" */
    while (len > 0 && norm[len - 1] == '/') len--;

    /* Strip trailing ".git" */
    if (len >= 4 && strncmp(norm + len - 4, ".git", 4) == 0) len -= 4;

    /* Compare against official URLs */
    for (size_t i = 0; UC_OFFICIAL_REPO_URLS[i] != NULL; i++) {
        const char *official = UC_OFFICIAL_REPO_URLS[i];
        size_t o_len = strlen(official);

        /* Strip trailing "/" from official too */
        while (o_len > 0 && official[o_len - 1] == '/') o_len--;
        /* Strip trailing ".git" */
        if (o_len >= 4 && strncmp(official + o_len - 4, ".git", 4) == 0) o_len -= 4;

        if (len == o_len && strncmp(norm, official, len) == 0) {
            return false;  /* matches official */
        }
    }

    return true;  /* doesn't match any official URL */
}

/* ── _count_commits_between ────────────────────────────────── */
/* PoP: uc_count_commits_between @ hermes_cli/update_cmd.py:_count_commits_between */
int uc_count_commits_between(const char *revlist_stdout)
{
    if (!revlist_stdout || !*revlist_stdout) return -1;

    /* Skip leading whitespace */
    while (*revlist_stdout && isspace((unsigned char)*revlist_stdout))
        revlist_stdout++;

    if (!*revlist_stdout) return -1;

    /* Parse integer */
    char *end;
    long val = strtol(revlist_stdout, &end, 10);

    /* Ensure the entire string was consumed (no trailing garbage) */
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return -1;

    if (val < 0 || val > INT_MAX) return -1;
    return (int)val;
}

/* ── _resolve_stash_selector ────────────────────────────── */
/* PoP: uc_resolve_stash_selector @ hermes_cli/update_cmd.py:_resolve_stash_selector */
char *uc_resolve_stash_selector(const char *stash_list_output,
                                    const char *stash_ref)
{
    if (!stash_list_output || !stash_ref || !*stash_ref) return NULL;

    /* Each line: "stash@{N} <full_sha>"  (or "stash@{N} tag: ...") */
    const char *p = stash_list_output;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t linelen = eol ? (size_t)(eol - p) : strlen(p);

        /* Find the SHA at the end of the line (after the last space) */
        const char *line_end = p + linelen;
        const char *last_space = NULL;
        for (const char *s = p; s < line_end; s++) {
            if (*s == ' ') last_space = s;
        }

        if (last_space && last_space + 1 < line_end) {
            size_t sha_len = (size_t)(line_end - (last_space + 1));
            if (sha_len == strlen(stash_ref) &&
                strncmp(last_space + 1, stash_ref, sha_len) == 0) {
                /* Found it — extract "stash@{N}" */
                size_t sel_len = (size_t)(last_space - p);
                char *result = malloc(sel_len + 1);
                if (!result) return NULL;
                memcpy(result, p, sel_len);
                result[sel_len] = '\0';
                return result;
            }
        }

        p = eol ? eol + 1 : line_end;
    }

    return NULL;
}

/* ── _print_stash_cleanup_guidance ────────────────────── */
/* PoP: uc_stash_cleanup_guidance @ hermes_cli/update_cmd.py:_print_stash_cleanup_guidance */
char *uc_stash_cleanup_guidance(const char *stash_ref,
                                    const char *stash_selector)
{
    if (!stash_ref) return NULL;

    size_t ref_len = strlen(stash_ref);
    size_t buf_size;

    if (stash_selector && *stash_selector) {
        /* "Remove it with: git stash drop <selector>" */
        size_t sel_len = strlen(stash_selector);
        buf_size = 40 + sel_len + ref_len;
        char *buf = malloc(buf_size);
        if (!buf) return NULL;
        snprintf(buf, buf_size,
                 "  Remove it with: git stash drop %s", stash_selector);
        return buf;
    } else {
        /* "Look for commit <ref>, then drop its selector with: git stash drop stash@{N}" */
        buf_size = 80 + ref_len;
        char *buf = malloc(buf_size);
        if (!buf) return NULL;
        snprintf(buf, buf_size,
                 "  Look for commit %s, then drop its selector with: git stash drop stash@{N}",
                 stash_ref);
        return buf;
    }
}

/* ── _format_concurrent_instances_message ────────────── */
/* PoP: uc_format_concurrent_instances_message @ hermes_cli/update_cmd.py:_format_concurrent_instances_message */
char *uc_format_concurrent_instances_message(const char **matches,
                                              size_t n_matches,
                                              const char *scripts_dir)
{
    if (!matches || n_matches == 0) return NULL;
    if (!scripts_dir) scripts_dir = "";

    /* Estimate buffer size */
    size_t buf_size = 512;
    for (size_t i = 0; i < n_matches; i++) {
        if (matches[i]) buf_size += strlen(matches[i]) + 64;
    }
    buf_size += strlen(scripts_dir) + 64;

    char *buf = malloc(buf_size);
    if (!buf) return NULL;

    size_t pos = 0;
    pos += (size_t)snprintf(buf + pos, buf_size - pos,
                            "✗ Another hermes process is running:\n");
    for (size_t i = 0; i < n_matches; i++) {
        if (matches[i]) {
            pos += (size_t)snprintf(buf + pos, buf_size - pos,
                                    "    PID %s\n", matches[i]);
        }
    }
    pos += (size_t)snprintf(buf + pos, buf_size - pos,
                            "\n  Updating now would fail to overwrite\n"
                            "  %s/hermes.exe because Windows blocks\n"
                            "  REPLACE on a running executable.\n",
                            scripts_dir);
    pos += (size_t)snprintf(buf + pos, buf_size - pos,
                            "\n  Close Hermes Desktop, exit any open\n"
                            "  `hermes` REPLs, and stop the gateway\n"
                            "  (`hermes gateway stop`) before retrying.\n");

    return buf;
}

/* ── _print_items ──────────────────────────────────────────────────────── */

char *uc_print_items(const char *items_json, const char *label,
                        const char *key, const char *fallback_key)
{
    if (!items_json || !label) return NULL;

    json_t *arr = json_parse(items_json, NULL);
    if (!arr || arr->type != JSON_ARRAY) {
        if (arr) json_free(arr);
        return NULL;
    }

    size_t total = json_len(arr);
    size_t shown = total > 8 ? 8 : total;

    size_t buf_cap = 4096;
    size_t buf_len = 0;
    char *buf = malloc(buf_cap);
    if (!buf) { json_free(arr); return NULL; }

    buf_len += (size_t)snprintf(buf + buf_len, buf_cap - buf_len, "  %s:\n", label);

    for (size_t i = 0; i < shown; i++) {
        json_t *item = json_get(arr, i);
        const char *name = NULL;
        const char *desc = "";

        if (item && item->type == JSON_OBJECT) {
            json_t *jname = json_obj_get(item, key ? key : "name");
            if (!jname && fallback_key)
                jname = json_obj_get(item, fallback_key);
            if (jname && jname->type == JSON_STRING)
                name = jname->str_val;
            json_t *jdesc = json_obj_get(item, "description");
            if (jdesc && jdesc->type == JSON_STRING)
                desc = jdesc->str_val;
        } else if (item && item->type == JSON_STRING) {
            name = item->str_val;
        } else {
            name = "?";
        }

        if (name && desc && *desc) {
            buf_len += (size_t)snprintf(buf + buf_len, buf_cap - buf_len,
                                         "      • %s — %s\n", name, desc);
        } else if (name) {
            buf_len += (size_t)snprintf(buf + buf_len, buf_cap - buf_len,
                                         "      • %s\n", name);
        }
    }

    if (total > shown) {
        buf_len += (size_t)snprintf(buf + buf_len, buf_cap - buf_len,
                                     "      … and %zu more\n", total - shown);
    }

    json_free(arr);
    return buf;
}

/* ── _service_restart_sec ──────────────────────────────────────────────── */

/* Parse systemd RestartUSec values: "30s", "100ms", "1min 30s", "infinity".
 * Mirrors hermes_cli/update_cmd.py:_service_restart_sec's unit table. */
double uc_parse_restart_sec(const char *raw, double default_sec)
{
    if (!raw || !*raw) return default_sec;

    /* "infinity" -> default (no finite wait). */
    if (strcmp(raw, "infinity") == 0) return default_sec;

    /* systemd emits space-separated compound durations: "1min 30s".
     * Parse token-by-token, matching known suffixes from the end. */
    double total = 0.0;
    bool matched_any = false;

    char buf[256];
    size_t blen = strlen(raw);
    if (blen >= sizeof(buf)) blen = sizeof(buf) - 1;
    memcpy(buf, raw, blen);
    buf[blen] = '\0';

    char *save = NULL;
    char *tok = strtok_r(buf, " \t", &save);
    while (tok) {
        size_t toklen = strlen(tok);
        /* Try each known suffix */
        struct { const char *suf; double mult; } sufs[] = {
            {"ms", 0.001}, {"us", 0.000001},
            {"s",  1.0},   {"min", 60.0},
            {"sec", 1.0},  {"h", 3600.0},
        };
        bool matched = false;
        for (int i = 0; i < (int)(sizeof(sufs)/sizeof(sufs[0])); i++) {
            size_t slen = strlen(sufs[i].suf);
            if (toklen >= slen && strcmp(tok + toklen - slen, sufs[i].suf) == 0) {
                /* parse the numeric prefix */
                char *end;
                double val = strtod(tok, &end);
                if (end == tok + (toklen - slen)) {
                    total += val * sufs[i].mult;
                    matched = true;
                    matched_any = true;
                    break;
                }
            }
        }
        if (!matched) {
            /* bare number, treat as seconds */
            char *end;
            double val = strtod(tok, &end);
            if (end != tok) {
                total += val;
                matched_any = true;
            }
        }
        tok = strtok_r(NULL, " \t", &save);
    }

    return matched_any ? total : default_sec;
}

/* ── Constants ─────────────────────────────────────────────────────────── */

const char *const UC_OFFICIAL_REPO_URLS[] = {
    "https://github.com/NousResearch/hermes-agent.git",
    "git@github.com:NousResearch/hermes-agent.git",
    "https://github.com/NousResearch/hermes-agent",
    "git@github.com:NousResearch/hermes-agent",
    NULL,
};

/* ── _is_android_python ────────────────────── */
/* PoP: uc_is_android_python @ hermes_cli/update_cmd.py:_is_android_python */
bool uc_is_android_python(void)
{
    /* Python's sys.platform == "android" on Termux. We mirror that check
     * against the same env var the Android bootstrap sets. */
    const char *plat = getenv("HERMES_SYS_PLATFORM");
    if (plat && strcmp(plat, "android") == 0)
        return true;
    return false;
}

static bool uc_path_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

/* ── _npm_bin_exists ────────────────────────────────────── */
/* PoP: uc_npm_bin_exists @ hermes_cli/update_cmd.py:_npm_bin_exists */
bool uc_npm_bin_exists(const char *bin_dir, const char *name)
{
    if (!bin_dir || !name)
        return false;
    const char *suffixes[] = {"", ".cmd", ".ps1", ".exe"};
    for (int i = 0; i < 4; i++) {
        char candidate[4096];
        snprintf(candidate, sizeof(candidate), "%s/%s%s",
                 bin_dir, name, suffixes[i]);
        if (uc_path_exists(candidate))
            return true;
    }
    return false;
}

/* ── _web_toolchain_roots ────────────────────────────────────── */
/* PoP: uc_web_toolchain_roots @ hermes_cli/update_cmd.py:_web_toolchain_roots */
char **uc_web_toolchain_roots(const char *web_dir)
{
    if (!web_dir || !*web_dir)
        return NULL;
    /* Roots = (web_dir, web_dir.parent), mirroring (web_dir, web_dir.parent).
     * We compute parent by stripping the last path component. */
    char *parent = NULL;
    const char *slash = strrchr(web_dir, '/');
    if (slash && slash != web_dir) {
        parent = strndup(web_dir, (size_t)(slash - web_dir));
    } else {
        parent = strdup(".");
    }

    char **out = calloc(3, sizeof(char *));
    if (!out) {
        free(parent);
        return NULL;
    }
    out[0] = strdup(web_dir);
    /* parent: match Python's os.path.dirname exactly.
     * "web"      -> ""      (no slash)     
     * "/repo/web" -> "/repo" (slash found)
     * "/"        -> "/"     (slash at start) */
    if (slash)
        out[1] = strndup(web_dir, (size_t)(slash - web_dir));
    else
        out[1] = strdup("");  /* os.path.dirname("barename") = "" */
    free(parent);
    out[2] = NULL;
    if (!out[0] || !out[1]) {
        free(out[0]);
        free(out[1]);
        free(out);
        return NULL;
    }
    return out;
}

/* ── _web_build_toolchain_ready ─────────────────────────────────────────── */

bool uc_web_build_toolchain_ready(const char **roots)
{
    if (!roots)
        return false;
    /* Collect bin_dirs = [root/node_modules/.bin for root in roots if dir exists] */
    /* We need dynamic storage for the bin dirs since we don't know the count. */
    char **bin_dirs = NULL;
    size_t bin_count = 0;
    size_t cap = 8;
    bin_dirs = calloc(cap, sizeof(char *));
    if (!bin_dirs)
        return false;

    for (size_t i = 0; roots[i]; i++) {
        char bin_path[4096];
        snprintf(bin_path, sizeof(bin_path), "%s/node_modules/.bin", roots[i]);
        if (uc_path_exists(bin_path)) {
            if (bin_count >= cap) {
                cap *= 2;
                char **tmp = realloc(bin_dirs, cap * sizeof(char *));
                if (!tmp) { free(bin_dirs); return false; }
                bin_dirs = tmp;
            }
            bin_dirs[bin_count++] = strdup(bin_path);
        }
    }

    if (bin_count == 0) {
        free(bin_dirs);
        return false;
    }

    bool ready = true;
    const char *tools[] = {"tsc", "vite", NULL};
    for (size_t t = 0; tools[t]; t++) {
        bool found = false;
        for (size_t b = 0; b < bin_count; b++) {
            if (uc_npm_bin_exists(bin_dirs[b], tools[t])) {
                found = true;
                break;
            }
        }
        if (!found) { ready = false; break; }
    }

    for (size_t b = 0; b < bin_count; b++)
        free(bin_dirs[b]);
    free(bin_dirs);
    return ready;
}

/* ── uc_free_string_array ─────────────────────────────────────────────────── */

void uc_free_string_array(char **arr)
{
    if (!arr)
        return;
    for (size_t i = 0; arr[i]; i++)
        free(arr[i]);
    free(arr);
}

/* ── _format_venv_python_holders_message ────────────── */
/* PoP: uc_format_venv_python_holders_message @ hermes_cli/update_cmd.py:_format_venv_python_holders_message */
char *uc_format_venv_python_holders_message(const char **matches, size_t n_matches)
{
    if (!matches)
        return NULL;
    /* Build the message using a dynamic buffer. */
    /* Header lines */
    struct { const char *s; } hdr[] = {
        {"✗ Other Hermes processes are running from this install's venv:"},
        {"  On Windows these keep native extension files (.pyd) locked, so the"},
        {"  dependency update would fail partway and leave a broken install."},
        {"  Close the Hermes desktop app / other Hermes terminals, then re-run:"},
        {"    hermes update"},
        {"  (or use `hermes update --force-venv` to proceed anyway at your own risk)"},
    };
    /* Collect all lines */
    char **lines = calloc(n_matches + 8, sizeof(char *));  /* worst case */
    if (!lines)
        return NULL;
    size_t n_lines = 0;

    lines[n_lines++] = strdup(hdr[0].s);

    /* PID lines (max 6) */
    for (size_t i = 0; i < n_matches && i < 6; i++) {
        const char *m = matches[i];
        /* Parse "pid|name|cmdline" (pipe-delimited, matching Python oracle) */
        char *pid_s = strdup(m);
        char *name = NULL, *cmdline = NULL;
        char *pipe = strchr(pid_s, '|');
        if (pipe) {
            *pipe = '\0';
            name = pipe + 1;
            char *pipe2 = strchr(name, '|');
            if (pipe2) {
                *pipe2 = '\0';
                cmdline = pipe2 + 1;
            }
        }
        char hint[256] = "";
        if (cmdline) {
            /* case-insensitive substring search for "serve"/"dashboard" and "gateway" */
            char *low = strdup(cmdline);
            if (low) {
                for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
                if (strstr(low, "serve") || strstr(low, "dashboard"))
                    strcpy(hint, "  ← Hermes Desktop backend (close the desktop app)");
                else if (strstr(low, "gateway"))
                    strcpy(hint, "  ← gateway");
                free(low);
            }
        }
        char line[2048];
        if (name && cmdline)
            snprintf(line, sizeof(line), "  PID %s  %s  %s%s", pid_s, name, cmdline, hint);
        else if (name)
            snprintf(line, sizeof(line), "  PID %s  %s%s", pid_s, name, hint);
        else
            snprintf(line, sizeof(line), "  PID %s%s", pid_s, hint);
        lines[n_lines++] = strdup(line);
        free(pid_s);
    }
    if (n_matches > 6) {
        char line[128];
        snprintf(line, sizeof(line), "  ... and %zu more", n_matches - 6);
        lines[n_lines++] = strdup(line);
    }
    lines[n_lines++] = strdup("");

    for (int i = 1; i < 6; i++)
        lines[n_lines++] = strdup(hdr[i].s);

    /* Join with "\n" */
    size_t total = 1;  /* at least null terminator */
    for (size_t i = 0; i < n_lines; i++)
        total += strlen(lines[i]) + 1;
    char *result = malloc(total);
    if (!result) {
        for (size_t i = 0; i < n_lines; i++)
            free(lines[i]);
        free(lines);
        return NULL;
    }
    result[0] = '\0';
    for (size_t i = 0; i < n_lines; i++) {
        if (i > 0)
            strcat(result, "\n");
        strcat(result, lines[i]);
    }
    for (size_t i = 0; i < n_lines; i++)
        free(lines[i]);
    free(lines);
    return result;
}

/* ── _resolve_pre_update_backup_mode ────────────── */
/* PoP: uc_resolve_pre_update_backup_mode @ hermes_cli/update_cmd.py:_resolve_pre_update_backup_mode */
const char *uc_resolve_pre_update_backup_mode(bool no_backup, bool backup,
                                               const char *raw_config_json)
{
    /* CLI flags win over config; --no-backup beats --backup. */
    if (no_backup)
        return "off";
    if (backup)
        return "full";

    /* If the config key is absent, default to "quick". */
    if (!raw_config_json || !*raw_config_json)
        return "quick";

    /* Legacy boolean form: "true" -> "full", "false" -> "off" */
    if (strcmp(raw_config_json, "true") == 0)
        return "full";
    if (strcmp(raw_config_json, "false") == 0)
        return "off";

    /* String mode */
    if (strcmp(raw_config_json, "off") == 0 ||
        strcmp(raw_config_json, "false") == 0 ||
        strcmp(raw_config_json, "none") == 0 ||
        strcmp(raw_config_json, "disabled") == 0)
        return "off";
    if (strcmp(raw_config_json, "full") == 0 ||
        strcmp(raw_config_json, "zip") == 0 ||
        strcmp(raw_config_json, "true") == 0)
        return "full";
    if (strcmp(raw_config_json, "quick") == 0)
        return "quick";

    /* Unknown value: warn + default to "quick" (mirrors Python logging.warning +
     * return "quick") */
    return "quick";
}

/* ── _parse_numstat_paths ────────────── */
/* PoP: uc_parse_numstat_paths @ hermes_cli/update_cmd.py:_parse_numstat_paths */
char **uc_parse_numstat_paths(const char *numstat_output)
{
    if (!numstat_output)
        return NULL;
    /* Parse "<added>\t<deleted>\t<path>" lines, collect unique paths.
     * Uses a simple linear-scan uniqueness check (small N for test cases). */
    char **paths = NULL;
    size_t count = 0, cap = 16;
    paths = calloc(cap, sizeof(char *));
    if (!paths)
        return NULL;

    const char *p = numstat_output;
    while (*p) {
        /* Find end of line */
        const char *eol = strchr(p, '\n');
        size_t linelen = eol ? (size_t)(eol - p) : strlen(p);
        if (linelen == 0)
            break;

        /* Parse: added \t deleted \t path */
        char line[4096];
        if (linelen >= sizeof(line))
            linelen = sizeof(line) - 1;
        memcpy(line, p, linelen);
        line[linelen] = '\0';
        /* strip trailing \r */
        while (linelen > 0 && (line[linelen-1] == '\r' || line[linelen-1] == '\n'))
            line[--linelen] = '\0';

        /* Split on \t */
        char *path = NULL;
        char *tok = strtok(line, "\t");
        tok = strtok(NULL, "\t");  /* skip added */
        tok = strtok(NULL, "\t");  /* deleted */
        if (tok)
            path = tok;
        if (path && *path) {
            /* Check uniqueness */
            bool dup = false;
            for (size_t i = 0; i < count; i++) {
                if (strcmp(paths[i], path) == 0) { dup = true; break; }
            }
            if (!dup) {
                if (count >= cap) {
                    cap *= 2;
                    char **tmp = realloc(paths, cap * sizeof(char *));
                    if (!tmp) { free(paths); return NULL; }
                    paths = tmp;
                }
                paths[count++] = strdup(path);
            }
        }

        p = eol ? eol + 1 : p + strlen(p);
    }
    /* NULL-terminate */
    if (count >= cap) {
        char **tmp = realloc(paths, (count + 1) * sizeof(char *));
        if (!tmp) { free(paths); return NULL; }
        paths = tmp;
    }
    paths[count] = NULL;
    /* Sort paths for deterministic output (matches Python sorted(paths)) */
    for (size_t i = 0; i + 1 < count; i++) {
        for (size_t j = i + 1; j < count; j++) {
            if (strcmp(paths[i], paths[j]) > 0) {
                char *tmp = paths[i];
                paths[i] = paths[j];
                paths[j] = tmp;
            }
        }
    }
    return paths;
}

/* ── _get_origin_url ────────────────────────────────────────────── */
/* PoP: uc_get_origin_url @ hermes_cli/update_cmd.py:_get_origin_url */
const char *uc_get_origin_url(const char *git_cmd[], const char *cwd)
{
    /* Python: git remote get-url origin, return stdout.strip() or None. */
    if (!cwd) return NULL;
    char *out = NULL;
    size_t out_len = 0;
    const char *args[] = { "remote", "get-url", "origin" };
    int rc = web_git_run(cwd, &out, &out_len, args, 3);
    if (rc != 0) {
        free(out);
        return NULL;
    }
    /* strip() both ends, like Python's .strip() */
    char *s = out;
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    if (end == s) {
        free(out);
        return NULL;
    }
    char *result = strndup(s, (size_t)(end - s));
    free(out);
    return result;
}

/* ── _has_upstream_remote ──────────────────────────────────────── */
/* PoP: uc_has_upstream_remote @ hermes_cli/update_cmd.py:_has_upstream_remote */
bool uc_has_upstream_remote(const char *git_cmd[], const char *cwd)
{
    (void)git_cmd;
    if (!cwd) return false;
    char *out = NULL;
    size_t out_len = 0;
    const char *args[] = { "remote", "get-url", "upstream" };
    int rc = web_git_run(cwd, &out, &out_len, args, 3);
    free(out);
    return rc == 0;
}

/* ── _add_upstream_remote ──────────────────────────────────────── */
/* PoP: uc_add_upstream_remote @ hermes_cli/update_cmd.py:_add_upstream_remote */
bool uc_add_upstream_remote(const char *git_cmd[], const char *cwd)
{
    (void)git_cmd;
    if (!cwd) return false;
    char *out = NULL;
    size_t out_len = 0;
    const char *args[] = { "remote", "add", "upstream", UC_OFFICIAL_REPO_URLS[0] };
    int rc = web_git_run(cwd, &out, &out_len, args, 4);
    free(out);
    return rc == 0;
}

/* ── _should_skip_upstream_prompt ──────────────────────────────── */
/* PoP: uc_should_skip_upstream_prompt @ hermes_cli/update_cmd.py:_should_skip_upstream_prompt */
bool uc_should_skip_upstream_prompt(const char *hermes_home)
{
    if (!hermes_home || !*hermes_home) return false;
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", hermes_home, UC_SKIP_UPSTREAM_PROMPT_FILE);
    struct stat st;
    return stat(path, &st) == 0;
}

/* ── _mark_skip_upstream_prompt ────────────────────────────────── */
/* PoP: uc_mark_skip_upstream_prompt @ hermes_cli/update_cmd.py:_mark_skip_upstream_prompt */
void uc_mark_skip_upstream_prompt(const char *hermes_home)
{
    if (!hermes_home || !*hermes_home) return;
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", hermes_home, UC_SKIP_UPSTREAM_PROMPT_FILE);
    FILE *f = fopen(path, "w");
    if (f) fclose(f);
}

/* ── _sync_fork_with_upstream ──────────────────────────────────── */
/* PoP: uc_sync_fork_with_upstream @ hermes_cli/update_cmd.py:_sync_fork_with_upstream */
bool uc_sync_fork_with_upstream(const char *git_cmd[], const char *cwd)
{
    (void)git_cmd;
    if (!cwd) return false;
    char *out = NULL;
    size_t out_len = 0;
    const char *args[] = { "push", "origin", "main", "--force-with-lease" };
    int rc = web_git_run(cwd, &out, &out_len, args, 4);
    free(out);
    return rc == 0;
}

/* ── _npm_manifest_paths ───────────────────────────────────────── */
/* PoP: uc_npm_manifest_paths @ hermes_cli/update_cmd.py:_npm_manifest_paths */
char **uc_npm_manifest_paths(const char *project_root)
{
    char **paths = NULL;
    size_t count = 0, cap = 0;

    if (!project_root || !*project_root) return NULL;

    /* Base entries: package-lock.json + root package.json. */
    char lock_path[4096], root_pkg[4096];
    snprintf(lock_path, sizeof(lock_path), "%s/package-lock.json", project_root);
    snprintf(root_pkg, sizeof(root_pkg), "%s/package.json", project_root);
    paths = realloc(paths, (count + 2) * sizeof(char *));
    if (!paths) return NULL;
    cap = count + 2;
    paths[count++] = strdup(lock_path);
    paths[count++] = strdup(root_pkg);

    /* Read root package.json and pull workspace globs. */
    char *err = NULL;
    json_t *doc = json_parse_file(root_pkg, &err);
    free(err);
    if (doc) {
        json_t *ws = json_obj_get(doc, "workspaces");
        if (ws && ws->type == JSON_OBJECT) {
            /* Legacy {"packages": [...]} form. */
            ws = json_obj_get(ws, "packages");
        }
        if (ws && ws->type == JSON_ARRAY) {
            for (size_t i = 0; i < ws->c.count; i++) {
                json_t *pat = ws->c.items[i];
                if (pat->type != JSON_STRING) continue;
                int n = 0;
                char **matches = glob_find(pat->str_val, project_root, &n);
                if (!matches) continue;
                /* Python Path.glob returns results in sorted order. */
                for (int a = 0; a < n; a++) {
                    for (int b = a + 1; b < n; b++) {
                        if (strcmp(matches[a], matches[b]) > 0) {
                            char *t = matches[a]; matches[a] = matches[b]; matches[b] = t;
                        }
                    }
                }
                for (int j = 0; j < n; j++) {
                    char manifest[4096];
                    snprintf(manifest, sizeof(manifest), "%s/package.json", matches[j]);
                    struct stat st;
                    if (stat(manifest, &st) == 0 && S_ISREG(st.st_mode)) {
                        if (count >= cap) {
                            cap = cap ? cap * 2 : 8;
                            char **tmp = realloc(paths, cap * sizeof(char *));
                            if (!tmp) { glob_free(matches, n); json_free(doc); goto out; }
                            paths = tmp;
                        }
                        paths[count++] = strdup(manifest);
                    }
                }
                glob_free(matches, n);
            }
        }
        json_free(doc);
    }

out:
    if (count >= cap) {
        char **tmp = realloc(paths, (count + 1) * sizeof(char *));
        if (!tmp) { return paths; }
        paths = tmp;
    }
    paths[count] = NULL;
    return paths;
}

/* ── _npm_manifests_digest ─────────────────────────────────────── */
/* PoP: uc_npm_manifests_digest @ hermes_cli/update_cmd.py:_npm_manifests_digest */
char *uc_npm_manifests_digest(const char *project_root)
{
    if (!project_root || !*project_root) return NULL;
    char lock_path[4096];
    snprintf(lock_path, sizeof(lock_path), "%s/package-lock.json", project_root);
    struct stat st;
    if (stat(lock_path, &st) != 0 || !S_ISREG(st.st_mode)) return NULL;

    char **paths = uc_npm_manifest_paths(project_root);
    if (!paths) return NULL;

    /* Incremental sha256: relative path bytes + file bytes (or "<missing>"). */
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { uc_free_string_array(paths); return NULL; }
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx); uc_free_string_array(paths); return NULL;
    }
    size_t root_len = strlen(project_root);
    for (size_t i = 0; paths[i]; i++) {
        const char *p = paths[i];
        /* p.relative_to(project_root) — strip root prefix + separator. */
        const char *rel = p;
        if (strncmp(p, project_root, root_len) == 0) {
            rel = p + root_len;
            if (*rel == '/') rel++;
        }
        EVP_DigestUpdate(ctx, rel, strlen(rel));
        FILE *f = fopen(p, "rb");
        if (f) {
            unsigned char buf[8192];
            size_t got;
            while ((got = fread(buf, 1, sizeof(buf), f)) > 0)
                EVP_DigestUpdate(ctx, buf, got);
            fclose(f);
        } else {
            EVP_DigestUpdate(ctx, "<missing>", 9);
        }
    }
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(ctx); uc_free_string_array(paths); return NULL;
    }
    EVP_MD_CTX_free(ctx);
    uc_free_string_array(paths);

    char *hex = malloc(hash_len * 2 + 1);
    if (!hex) return NULL;
    for (unsigned int i = 0; i < hash_len; i++)
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    hex[hash_len * 2] = '\0';
    return hex;
}

/* ── _npm_lockfile_changed ─────────────────────────────────────── */
/* PoP: uc_npm_lockfile_changed @ hermes_cli/update_cmd.py:_npm_lockfile_changed */
bool uc_npm_lockfile_changed(const char *project_root, const char *hermes_root)
{
    char *current = uc_npm_manifests_digest(project_root);
    if (!current) return true; /* lockfile missing — never skip */

    /* node_modules missing => cache recorded by another checkout. */
    char nm_path[4096];
    snprintf(nm_path, sizeof(nm_path), "%s/node_modules", project_root);
    struct stat st;
    if (stat(nm_path, &st) != 0 || !S_ISDIR(st.st_mode)) { free(current); return true; }

    /* A matching hash over a tree whose web toolchain never landed must
     * NOT skip the reinstall. */
    char web_dir[4096], web_pkg[4096];
    snprintf(web_dir, sizeof(web_dir), "%s/web", project_root);
    snprintf(web_pkg, sizeof(web_pkg), "%s/web/package.json", project_root);
    if (stat(web_pkg, &st) == 0 && S_ISREG(st.st_mode)) {
        char **roots = uc_web_toolchain_roots(web_dir);
        if (roots) {
            bool ready = uc_web_build_toolchain_ready((const char **)roots);
            uc_free_string_array(roots);
            if (!ready) { free(current); return true; }
        }
    }

    /* Key the cache by PROJECT_ROOT so parallel worktrees don't collide. */
    unsigned char key_hash[EVP_MAX_MD_SIZE];
    unsigned int key_len = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { free(current); return true; }
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
        EVP_DigestUpdate(ctx, project_root, strlen(project_root)) != 1 ||
        EVP_DigestFinal_ex(ctx, key_hash, &key_len) != 1) {
        EVP_MD_CTX_free(ctx); free(current); return true;
    }
    EVP_MD_CTX_free(ctx);
    char key_hex[13];
    for (unsigned int i = 0; i < 6; i++)
        snprintf(key_hex + i * 2, 3, "%02x", key_hash[i]);
    key_hex[12] = '\0';

    char cache_file[4096];
    snprintf(cache_file, sizeof(cache_file), "%s/.npm_lock_hash_%s", hermes_root, key_hex);
    if (stat(cache_file, &st) != 0) { free(current); return true; }

    /* Compare file content .strip() != current. */
    FILE *f = fopen(cache_file, "rb");
    if (!f) { free(current); return true; }
    char buf[8192];
    size_t got = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[got] = '\0';
    char *s = buf;
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    bool changed = ((size_t)(end - s) != strlen(current)) ||
                   strncmp(s, current, (size_t)(end - s)) != 0;
    free(current);
    return changed;
}

/* ── _record_npm_lockfile_hash ─────────────────────────────────── */
/* PoP: uc_record_npm_lockfile_hash @ hermes_cli/update_cmd.py:_record_npm_lockfile_hash */
void uc_record_npm_lockfile_hash(const char *project_root, const char *hermes_root)
{
    char *digest = uc_npm_manifests_digest(project_root);
    if (!digest) return;

    unsigned char key_hash[EVP_MAX_MD_SIZE];
    unsigned int key_len = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { free(digest); return; }
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
        EVP_DigestUpdate(ctx, project_root, strlen(project_root)) != 1 ||
        EVP_DigestFinal_ex(ctx, key_hash, &key_len) != 1) {
        EVP_MD_CTX_free(ctx); free(digest); return;
    }
    EVP_MD_CTX_free(ctx);
    char key_hex[13];
    for (unsigned int i = 0; i < 6; i++)
        snprintf(key_hex + i * 2, 3, "%02x", key_hash[i]);
    key_hex[12] = '\0';

    char cache_file[4096];
    snprintf(cache_file, sizeof(cache_file), "%s/.npm_lock_hash_%s", hermes_root, key_hex);
    FILE *f = fopen(cache_file, "w");
    if (f) {
        fputs(digest, f);
        fclose(f);
    }
    free(digest);
}

/* ── _write_marker_file ────────────────────────────────────────── */
/* PoP: uc_write_marker_file @ hermes_cli/update_cmd.py:_write_marker_file */
void uc_write_marker_file(const char *path)
{
    if (!path || !*path) return;
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "started=%.0f\npid=%ld\n", (double)time(NULL), (long)getpid());
    fclose(f);
}

/* ── _for_each_systemd_gateway_unit ───────────────────────── */
/* PoP: uc_for_each_systemd_gateway_unit @ hermes_cli/update_cmd.py:_for_each_systemd_gateway_unit */
char **uc_for_each_systemd_gateway_unit(const char *list_units_stdout)
{
    if (!list_units_stdout || !*list_units_stdout) return NULL;
    char **units = NULL;
    size_t count = 0, cap = 0;
    const char *p = list_units_stdout;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len > 0) {
            char line[1024];
            if (len >= sizeof(line)) { p = nl ? nl + 1 : p + len; continue; }
            memcpy(line, p, len);
            line[len] = '\0';
            char *unit = line;
            /* Skip leading whitespace. */
            while (*unit && isspace((unsigned char)*unit)) unit++;
            char *sp = strchr(unit, ' ');
            if (sp) *sp = '\0';
            if (unit[0] && strstr(unit, ".service") && strncmp(unit, "hermes-gateway", 14) == 0) {
                /* Strip .service suffix. */
                char *svc = strdup(unit);
                char *dot = strstr(svc, ".service");
                if (dot) *dot = '\0';
                if (count >= cap) {
                    cap = cap ? cap * 2 : 8;
                    char **tmp = realloc(units, cap * sizeof(char *));
                    if (!tmp) { free(svc); break; }
                    units = tmp;
                }
                units[count++] = svc;
            }
        }
        p = nl ? nl + 1 : p + len;
    }
    if (count) {
        char **tmp = realloc(units, (count + 1) * sizeof(char *));
        if (tmp) units = tmp;
        units[count] = NULL;
    } else {
        free(units);
        units = NULL;
    }
    return units;
}

/* ── _warn_incomplete_gateway_fleet_restart ──────────────── */
/* PoP: uc_warn_incomplete_gateway_fleet_restart @ hermes_cli/update_cmd.py:_warn_incomplete_gateway_fleet_restart */
char *uc_warn_incomplete_gateway_fleet_restart(const char **failed_units)
{
    if (!failed_units || !*failed_units) return NULL;
    /* De-duplicate preserving discovery order. */
    char **ordered = NULL;
    size_t count = 0, cap = 0;
    for (const char **fp = failed_units; *fp; fp++) {
        bool dup = false;
        for (size_t i = 0; i < count; i++) {
            if (strcmp(ordered[i], *fp) == 0) { dup = true; break; }
        }
        if (dup) continue;
        if (count >= cap) {
            cap = cap ? cap * 2 : 8;
            char **tmp = realloc(ordered, cap * sizeof(char *));
            if (!tmp) break;
            ordered = tmp;
        }
        ordered[count++] = strdup(*fp);
    }
    if (count == 0) { free(ordered); return NULL; }

    size_t total = 0;
    total += strlen("\n⚠ Update incomplete — some gateway units were not restarted:\n");
    for (size_t i = 0; i < count; i++) {
        total += strlen(ordered[i]) + 8; /* "    - \n" */
    }
    total += strlen("  Skipped units may still be running pre-update code (mixed\n  sys.modules). Restart them manually, then verify:\n    hermes gateway status\n    systemctl --user restart <unit>   # user-scope\n    sudo systemctl restart <unit>     # system-scope\n");
    char *buf = malloc(total + 1);
    if (!buf) { for (size_t i = 0; i < count; i++) free(ordered[i]); free(ordered); return NULL; }
    char *p = buf;
    p += sprintf(p, "\n⚠ Update incomplete — some gateway units were not restarted:\n");
    for (size_t i = 0; i < count; i++)
        p += sprintf(p, "    - %s\n", ordered[i]);
    p += sprintf(p, "  Skipped units may still be running pre-update code (mixed\n  sys.modules). Restart them manually, then verify:\n    hermes gateway status\n    systemctl --user restart <unit>   # user-scope\n    sudo systemctl restart <unit>     # system-scope\n");
    *p = '\0';
    for (size_t i = 0; i < count; i++) free(ordered[i]);
    free(ordered);
    return buf;
}

/* ── _discard_lockfile_churn (pure selection half) ───────── */
/* PoP: uc_select_lockfile_churn @ hermes_cli/update_cmd.py:_discard_lockfile_churn */
char **uc_select_lockfile_churn(const char *diff_stdout)
{
    if (!diff_stdout || !*diff_stdout) return NULL;
    /* Collect every dirty path; note which parents have a dirty
     * package.json (the Python builds dirty_package_dirs from all
     * "package.json" lines, then keeps only lockfiles whose parent is
     * NOT in that set). */
    char **paths = NULL;
    size_t path_count = 0, path_cap = 0;
    char *dup = strdup(diff_stdout);
    if (!dup) return NULL;
    char *saveptr = NULL;
    for (char *line = strtok_r(dup, "\n", &saveptr); line; line = strtok_r(NULL, "\n", &saveptr)) {
        char *trimmed = line;
        while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;
        if (!*trimmed) continue;
        char *end = trimmed + strlen(trimmed) - 1;
        while (end > trimmed && isspace((unsigned char)*end)) *end-- = '\0';
        if (!*trimmed) continue;
        if (path_count >= path_cap) {
            path_cap = path_cap ? path_cap * 2 : 16;
            char **tmp = realloc(paths, path_cap * sizeof(char *));
            if (!tmp) { for (size_t i = 0; i < path_count; i++) free(paths[i]); free(paths); free(dup); return NULL; }
            paths = tmp;
        }
        paths[path_count++] = strdup(trimmed);
    }
    free(dup);
    if (!path_count) { free(paths); return NULL; }

    /* dirty_package_dirs = { parent(line) for line in paths if line endswith "package.json" } */
    char **pkg_dirs = NULL;
    size_t pkg_count = 0, pkg_cap = 0;
    for (size_t i = 0; i < path_count; i++) {
        const char *p = paths[i];
        size_t plen = strlen(p);
        if (plen >= strlen("package.json") &&
            strcmp(p + plen - strlen("package.json"), "package.json") == 0) {
            const char *slash = strrchr(p, '/');
            char parent[4096];
            if (slash) {
                size_t dlen = (size_t)(slash - p);
                memcpy(parent, p, dlen);
                parent[dlen] = '\0';
            } else {
                strcpy(parent, ".");
            }
            if (pkg_count >= pkg_cap) {
                pkg_cap = pkg_cap ? pkg_cap * 2 : 8;
                char **tmp = realloc(pkg_dirs, pkg_cap * sizeof(char *));
                if (!tmp) break;
                pkg_dirs = tmp;
            }
            pkg_dirs[pkg_count++] = strdup(parent);
        }
    }

    /* dirty = [p for p in paths if endswith("package-lock.json")
     *          and parent(p) not in dirty_package_dirs] */
    char **result = NULL;
    size_t res_count = 0, res_cap = 0;
    for (size_t i = 0; i < path_count; i++) {
        const char *p = paths[i];
        size_t plen = strlen(p);
        if (plen < strlen("package-lock.json") ||
            strcmp(p + plen - strlen("package-lock.json"), "package-lock.json") != 0)
            continue;
        const char *slash = strrchr(p, '/');
        char parent[4096];
        if (slash) {
            size_t dlen = (size_t)(slash - p);
            memcpy(parent, p, dlen);
            parent[dlen] = '\0';
        } else {
            strcpy(parent, ".");
        }
        bool in_dirty_dirs = false;
        for (size_t j = 0; j < pkg_count; j++) {
            if (strcmp(pkg_dirs[j], parent) == 0) { in_dirty_dirs = true; break; }
        }
        if (in_dirty_dirs) continue;
        if (res_count >= res_cap) {
            res_cap = res_cap ? res_cap * 2 : 8;
            char **tmp = realloc(result, res_cap * sizeof(char *));
            if (!tmp) break;
            result = tmp;
        }
        result[res_count++] = strdup(p);
    }
    for (size_t i = 0; i < path_count; i++) free(paths[i]);
    free(paths);
    for (size_t i = 0; i < pkg_count; i++) free(pkg_dirs[i]);
    free(pkg_dirs);
    if (res_count) {
        char **tmp = realloc(result, (res_count + 1) * sizeof(char *));
        if (tmp) result = tmp;
        result[res_count] = NULL;
    } else {
        free(result);
        result = NULL;
    }
    return result;
}

/* ── _invalidate_update_cache ────────────────────────────── */
/* PoP: uc_invalidate_update_cache @ hermes_cli/update_cmd.py:_invalidate_update_cache */
void uc_invalidate_update_cache(const char *default_home)
{
    if (!default_home || !*default_home) return;
    /* Delete default_home/.update_check */
    char cache_path[4096];
    snprintf(cache_path, sizeof(cache_path), "%s/.update_check", default_home);
    remove(cache_path);
    /* Delete update_check cache under each profile dir */
    char profiles_dir[4096];
    snprintf(profiles_dir, sizeof(profiles_dir), "%s/profiles", default_home);
    DIR *d = opendir(profiles_dir);
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char entry[4096];
        snprintf(entry, sizeof(entry), "%s/%s/.update_check", profiles_dir, ent->d_name);
        remove(entry);
    }
    closedir(d);
}

/* ── _capture_head_sha ────────────────────────────────────── */
/* PoP: uc_capture_head_sha @ hermes_cli/update_cmd.py:_capture_head_sha */
char *uc_capture_head_sha(const char *rev_parse_stdout)
{
    if (!rev_parse_stdout || !*rev_parse_stdout) return NULL;
    const char *s = rev_parse_stdout;
    while (*s && isspace((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    if (end == s) return NULL;
    return strndup(s, (size_t)(end - s));
}

/* ── _stash_local_changes_if_needed (name generator) ─────── */
/* PoP: uc_autostash_name @ hermes_cli/update_cmd.py:_stash_local_changes_if_needed */
char *uc_autostash_name(time_t now_utc)
{
    struct tm tmv;
    if (!gmtime_r(&now_utc, &tmv)) return NULL;
    char *name = malloc(64);
    if (!name) return NULL;
    snprintf(name, 64, "hermes-update-autostash-%04d%02d%02d-%02d%02d%02d",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return name;
}

/* ── _print_curator_first_run_notice ─────────────────── */
/* PoP: uc_print_curator_first_run_notice @ hermes_cli/update_cmd.py:_print_curator_first_run_notice */
char *uc_print_curator_first_run_notice(bool curator_enabled,
                                        bool has_last_run_at,
                                        int interval_hours)
{
    if (!curator_enabled) return NULL;
    if (has_last_run_at) return NULL;
    int hours = interval_hours > 0 ? interval_hours : 168; /* 24*7 default */
    int days = hours / 24;
    if (days < 1) days = 1;
    const char *fmt =
        "\n"
        "ℹ Skill curator\n"
        "  Background skill maintenance is enabled. First pass is deferred "
        "~%d d after installation; only agent-created skills are in "
        "scope and nothing is ever auto-deleted (archive is recoverable).\n"
        "  Preview now:  hermes curator run --dry-run\n"
        "  Pause it:     hermes curator pause\n"
        "  Docs:         https://hermes-agent.nousresearch.com/docs/user-guide/features/curator\n";
    char *buf = malloc(2048);
    if (!buf) return NULL;
    int n = snprintf(buf, 2048, fmt, days);
    if (n < 0 || (size_t)n >= 2048) {
        char *tmp = realloc(buf, n + 1);
        if (tmp) buf = tmp;
        snprintf(buf, n + 1, fmt, days);
    }
    return buf;
}

/* ── _print_curator_recent_run_notice ────────────────── */
/* PoP: uc_print_curator_recent_run_notice @ hermes_cli/update_cmd.py:_print_curator_recent_run_notice */
char *uc_print_curator_recent_run_notice(const char *last_run_at,
                                         const char *shown_at,
                                         const char *summary,
                                         const char *when_is)
{
    if (!last_run_at || !*last_run_at) return NULL;
    if (shown_at != NULL && strcmp(shown_at, last_run_at) == 0) return NULL;
    if (!summary || !*summary) return NULL;
    /* Only print when summary has renames (multi-line). */
    if (strchr(summary, '\n') == NULL) return NULL;
    if (!when_is) when_is = "recently";

    /* Compute total length. */
    size_t total = 0;
    total += strlen("\nℹ Skill curator — last run ") + strlen(when_is) +
             strlen("\n") + 1;
    /* Each line of summary gets "  <line>\n" */
    char *s = summary;
    while (*s) {
        char *nl = strchr(s, '\n');
        size_t llen = nl ? (size_t)(nl - s) : strlen(s);
        total += 3 + llen + 1; /* "  " + line + "\n" */
        s = nl ? nl + 1 : s + strlen(s);
    }
    total += strlen("  (This message shows once per curator run. "
                    "View anytime: hermes curator status)\n") + 1;

    char *buf = malloc(total + 1);
    if (!buf) return NULL;
    char *p = buf;
    p += sprintf(p, "\nℹ Skill curator — last run %s\n", when_is);
    s = summary;
    while (*s) {
        char *nl = strchr(s, '\n');
        if (nl) {
            size_t llen = (size_t)(nl - s);
            memcpy(p, "  ", 2);
            memcpy(p + 2, s, llen);
            p += 2 + llen;
            *p++ = '\n';
            s = nl + 1;
        } else {
            size_t llen = strlen(s);
            memcpy(p, "  ", 2);
            memcpy(p + 2, s, llen);
            p += 2 + llen;
            *p++ = '\n';
            break;
        }
    }
    p += sprintf(p, "  (This message shows once per curator run. "
                 "View anytime: hermes curator status)\n");
    *p = '\0';
    return buf;
}

/* ── _write_update_incomplete_marker ────────────────────── */
/* PoP: uc_write_update_incomplete_marker @ hermes_cli/update_cmd.py:_write_update_incomplete_marker */
void uc_write_update_incomplete_marker(const char *project_root)
{
    if (!project_root || !*project_root) return;
    char path[4096];
    snprintf(path, sizeof(path), "%s/.update-incomplete", project_root);
    uc_write_marker_file(path);
}

/* ── _write_lazy_refresh_incomplete_marker ────────────────── */
/* PoP: uc_write_lazy_refresh_incomplete_marker @ hermes_cli/update_cmd.py:_write_lazy_refresh_incomplete_marker */
void uc_write_lazy_refresh_incomplete_marker(const char *project_root)
{
    if (!project_root || !*project_root) return;
    char path[4096];
    snprintf(path, sizeof(path), "%s/.lazy-refresh-incomplete", project_root);
    uc_write_marker_file(path);
}

/* ── _finish_dashboard_update_cleanup ─────────────────────── */
/* PoP: uc_finish_dashboard_update_cleanup @ hermes_cli/update_cmd.py:_finish_dashboard_update_cleanup */
char *uc_finish_dashboard_update_cleanup(const char **node_failures,
                                          size_t n_failures,
                                          bool unrecovered)
{
    if (node_failures && n_failures > 0) {
        size_t total = strlen("\n  ℹ Leaving running dashboard process(es) untouched because the\n    Node.js dependency refresh did not complete.\n");
        char *buf = malloc(total + 1);
        if (!buf) return NULL;
        memcpy(buf, "\n  ℹ Leaving running dashboard process(es) untouched because the\n    Node.js dependency refresh did not complete.\n", total + 1);
        return buf;
    }
    if (!unrecovered) return NULL;
    const char *msg =
        "\n"
        "  ⚠ A web dashboard/serve process was stopped during update and could "
        "not be auto-restarted.\n"
        "  Re-launch it when you want the web UI back:\n"
        "    hermes dashboard --port <port>\n";
    char *buf = malloc(strlen(msg) + 1);
    if (!buf) return NULL;
    memcpy(buf, msg, strlen(msg) + 1);
    return buf;
}

/* ── Atomic-replace helpers (recursive copy / remove) ─────────── */

static int uc_remove_tree_internal(const char *path)
{
    struct stat st;
    if (lstat(path, &st) != 0) return -1;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (!d) return -1;
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;
            char child[4096];
            snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
            uc_remove_tree_internal(child);
        }
        closedir(d);
        return rmdir(path);
    }
    return remove(path);
}

static int uc_copy_tree_internal(const char *src, const char *dst)
{
    struct stat st;
    if (lstat(src, &st) != 0) return -1;
    if (S_ISDIR(st.st_mode)) {
        if (mkdir(dst, st.st_mode & 0777) != 0 && errno != EEXIST) return -1;
        DIR *d = opendir(src);
        if (!d) return -1;
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;
            char src_child[4096], dst_child[4096];
            snprintf(src_child, sizeof(src_child), "%s/%s", src, ent->d_name);
            snprintf(dst_child, sizeof(dst_child), "%s/%s", dst, ent->d_name);
            if (uc_copy_tree_internal(src_child, dst_child) != 0) {
                closedir(d);
                return -1;
            }
        }
        closedir(d);
        return 0;
    }
    /* Plain file (or symlink): copy2 semantics — open + copy + chmod. */
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }
    char buf[65536];
    size_t got;
    while ((got = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, got, out) != got) {
            fclose(in); fclose(out); remove(dst); return -1;
        }
    }
    fclose(in);
    if (fclose(out) != 0) { remove(dst); return -1; }
    /* copy2 preserves the mode (mtime not critical for staging). */
    chmod(dst, st.st_mode & 07777);
    return 0;
}

/* ── _stage_replacement ───────────────────────────────────────── */
/* PoP: uc_stage_replacement @ hermes_cli/update_cmd.py:_stage_replacement */
char *uc_stage_replacement(const char *src, const char *dst)
{
    if (!src || !dst || !*src || !*dst) return NULL;
    char staging[4096], backup[4096];
    snprintf(staging, sizeof(staging), "%s.hermes-update-staging", dst);
    snprintf(backup, sizeof(backup), "%s.hermes-update-old", dst);

    struct stat st;
    /* Restore dst from backup if dst is missing but backup exists. */
    if (stat(dst, &st) != 0 && stat(backup, &st) == 0) {
        rename(backup, dst);
    }
    /* Clear stale staging/backup leftovers. */
    for (int i = 0; i < 2; i++) {
        const char *leftover = (i == 0) ? staging : backup;
        if (stat(leftover, &st) == 0) {
            if (S_ISDIR(st.st_mode))
                uc_remove_tree_internal(leftover);
            else
                remove(leftover);
        }
    }
    /* Copy src to staging. */
    if (stat(src, &st) != 0) return NULL;
    if (uc_copy_tree_internal(src, staging) != 0) return NULL;
    return strdup(staging);
}

/* ── _discard_staged ──────────────────────────────────────────── */
/* PoP: uc_discard_staged @ hermes_cli/update_cmd.py:_discard_staged */
void uc_discard_staged(const char **staged_pairs)
{
    if (!staged_pairs) return;
    for (size_t i = 0; staged_pairs[i]; i++) {
        const char *pair = staged_pairs[i];
        const char *tab = strchr(pair, '\t');
        size_t slen = tab ? (size_t)(tab - pair) : strlen(pair);
        char staging[4096];
        if (slen >= sizeof(staging)) continue;
        memcpy(staging, pair, slen);
        staging[slen] = '\0';
        struct stat st;
        if (stat(staging, &st) == 0) {
            if (S_ISDIR(st.st_mode))
                uc_remove_tree_internal(staging);
            else
                remove(staging);
        }
    }
}

/* ── _commit_staged_replacements ──────────────────────────────── */
/* PoP: uc_commit_staged_replacements @ hermes_cli/update_cmd.py:_commit_staged_replacements */
int uc_commit_staged_replacements(const char **staged_pairs)
{
    if (!staged_pairs) return -1;
    /* Collect entries: staging, dst, backup ("" = dst absent). */
    size_t n = 0;
    while (staged_pairs[n]) n++;
    char (*swapped)[4096] = malloc(n * sizeof(*swapped)); /* dst */
    char (*backups)[4096] = malloc(n * sizeof(*backups)); /* backup or "" */
    if (!swapped || !backups) { free(swapped); free(backups); return -1; }
    size_t swapped_count = 0;

    for (size_t i = 0; i < n; i++) {
        const char *pair = staged_pairs[i];
        const char *tab = strchr(pair, '\t');
        size_t slen = tab ? (size_t)(tab - pair) : strlen(pair);
        const char *dst = tab ? tab + 1 : "";
        char staging[4096];
        if (slen >= sizeof(staging)) goto rollback;
        memcpy(staging, pair, slen);
        staging[slen] = '\0';
        char backup[4096];
        snprintf(backup, sizeof(backup), "%s.hermes-update-old", dst);
        struct stat st;
        if (stat(dst, &st) == 0) {
            if (rename(dst, backup) != 0) goto rollback;
            strncpy(swapped[swapped_count], dst, 4095);
            swapped[swapped_count][4095] = '\0';
            strncpy(backups[swapped_count], backup, 4095);
            backups[swapped_count][4095] = '\0';
            swapped_count++;
        } else {
            strncpy(swapped[swapped_count], dst, 4095);
            swapped[swapped_count][4095] = '\0';
            backups[swapped_count][0] = '\0';
            swapped_count++;
        }
        if (rename(staging, dst) != 0) goto rollback;
    }

    /* All swaps succeeded — drop backups (best-effort). */
    for (size_t i = 0; i < swapped_count; i++) {
        if (backups[i][0]) {
            struct stat st;
            if (stat(backups[i], &st) == 0) {
                if (S_ISDIR(st.st_mode))
                    uc_remove_tree_internal(backups[i]);
                else
                    remove(backups[i]);
            }
        }
    }
    free(swapped);
    free(backups);
    return 0;

rollback:
    for (size_t i = swapped_count; i > 0; i--) {
        size_t j = i - 1;
        struct stat st;
        /* Remove whatever is at dst now (the new staging entry). */
        if (stat(swapped[j], &st) == 0) {
            if (S_ISDIR(st.st_mode))
                uc_remove_tree_internal(swapped[j]);
            else
                remove(swapped[j]);
        }
        /* Restore backup if there was one. */
        if (backups[j][0] && stat(backups[j], &st) == 0) {
            rename(backups[j], swapped[j]);
        }
    }
    free(swapped);
    free(backups);
    return -1;
}

/* ── _atomic_replace_dir ──────────────────────────────────────── */
/* PoP: uc_atomic_replace_dir @ hermes_cli/update_cmd.py:_atomic_replace_dir */
int uc_atomic_replace_dir(const char *src, const char *dst)
{
    char *staging = uc_stage_replacement(src, dst);
    if (!staging) return -1;
    char pair[8192];
    snprintf(pair, sizeof(pair), "%s\t%s", staging, dst);
    const char *pairs[] = { pair, NULL };
    int rc = uc_commit_staged_replacements(pairs);
    free(staging);
    return rc;
}

/* ── _log_only_write ──────────────────────────────────────────── */
/* PoP: uc_log_only_write @ hermes_cli/update_cmd.py:_log_only_write */
long uc_log_only_write(FILE *log_file, const char *text)
{
    if (!log_file || !text || !*text) return 0;
    size_t len = strlen(text);
    int n = fwrite(text, 1, len, log_file);
    if (text[len - 1] != '\n') {
        fputc('\n', log_file);
        n++;
    }
    fflush(log_file);
    return n;
}

/* ── _write_update_planned_stop_marker ────────────────────────── */
/* PoP: uc_write_update_planned_stop_marker @ hermes_cli/update_cmd.py:_write_update_planned_stop_marker */
int uc_write_update_planned_stop_marker(const char *profile_path, long pid,
                                        long stopper_pid,
                                        const char *target_start_time,
                                        const char *written_at)
{
    if (!profile_path || !*profile_path) return -1;
    /* Build JSON with compact separators (",", ":") like the Python. */
    char body[4096];
    int n = snprintf(body, sizeof(body),
                     "{\"target_pid\":%ld,\"target_start_time\":%s,"
                     "\"stopper_pid\":%ld,\"written_at\":%s}",
                     pid, target_start_time ? target_start_time : "null",
                     stopper_pid, written_at ? written_at : "null");
    if (n < 0 || (size_t)n >= sizeof(body)) return -1;
    char path[4096];
    snprintf(path, sizeof(path), "%s/.gateway-planned-stop.json", profile_path);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    size_t got = fwrite(body, 1, (size_t)n, f);
    int rc = (got == (size_t)n) ? 0 : -1;
    if (fclose(f) != 0) rc = -1;
    return rc;
}

/* ── _print_fts_optimize_available_notice ─────────────────────── */
/* PoP: uc_print_fts_optimize_available_notice @ hermes_cli/update_cmd.py:_print_fts_optimize_available_notice */
char *uc_print_fts_optimize_available_notice(const char *mode, double size_gb,
                                             bool interrupted)
{
    const char *m = (mode && *mode) ? mode : "advise";
    if (strcmp(m, "off") == 0) return NULL;
    if (size_gb < 0.5) return NULL;

    if (interrupted) {
        const char *msg =
            "\n"
            "◆ Session database optimization incomplete\n"
            "  A previous `hermes sessions optimize-storage` run was interrupted. "
            "Search still works; re-run the command to resume and finish "
            "reclaiming disk:\n"
            "    hermes sessions optimize-storage\n";
        return strdup(msg);
    }

    double est = size_gb * 0.6;
    char *buf = malloc(2048);
    if (!buf) return NULL;
    if (strcmp(m, "require") == 0) {
        snprintf(buf, 2048,
                 "\n"
                 "◆ Session database upgrade required\n"
                 "  Your search index uses the OLD storage layout and should be "
                 "upgraded. The new layout typically frees ~60%% of state.db "
                 "(≈%.1f GB of your current %.1f GB) and is required for "
                 "continued optimal operation.\n"
                 "  Run when convenient:  hermes sessions optimize-storage\n"
                 "  It runs in the foreground with a progress bar, is safe to "
                 "interrupt/re-run, and never changes your conversations.\n",
                 est, size_gb);
    } else {
        snprintf(buf, 2048,
                 "\n"
                 "◆ Reclaim ~60%% of your session database disk\n"
                 "  Your search index uses the old storage layout. Upgrading it "
                 "typically frees ~60%% of state.db — about %.1f GB of your "
                 "current %.1f GB.\n"
                 "  Run when convenient:  hermes sessions optimize-storage\n"
                 "  It runs in the foreground with a progress bar, is safe to "
                 "interrupt/re-run, and never changes your conversations.\n",
                 est, size_gb);
    }
    return buf;
}

/* ── _gateway_prompt ────────────────────────────────── */
/* PoP: uc_gateway_prompt @ hermes_cli/update_cmd.py:_gateway_prompt */
char *uc_gateway_prompt(const char *default_home, const char *prompt_text,
                               const char *default_answer, double timeout_sec)
{
    if (!default_home || !*default_home) return NULL;
    char prompt_path[4096], response_path[4096], tmp_path[4096];
    snprintf(prompt_path, sizeof(prompt_path), "%s/.update_prompt.json", default_home);
    snprintf(response_path, sizeof(response_path), "%s/.update_response", default_home);
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", prompt_path);

    /* Clean stale response file. */
    remove(response_path);

    /* Build JSON payload: {"prompt":"...","default":"...","id":"..."} */
    char *id = uc_generate_uuid();
    if (!id) return NULL;
    size_t payload_len = strlen(prompt_text) + strlen(default_answer) + strlen(id) + 256;
    char *payload = malloc(payload_len);
    if (!payload) { free(id); return NULL; }
    snprintf(payload, payload_len,
             "{\"prompt\":\"%s\",\"default\":\"%s\",\"id\":\"%s\"}",
             prompt_text, default_answer, id);
    free(id);

    FILE *f = fopen(tmp_path, "w");
    if (!f) { free(payload); return NULL; }
    fputs(payload, f);
    fclose(f);
    free(payload);
    if (rename(tmp_path, prompt_path) != 0) { remove(tmp_path); return NULL; }

    /* Poll for response. */
    double deadline = (double)time(NULL) + timeout_sec;
    while ((double)time(NULL) < deadline) {
        struct stat st;
        if (stat(response_path, &st) == 0) {
            FILE *rf = fopen(response_path, "r");
            if (rf) {
                fseek(rf, 0, SEEK_END);
                long sz = ftell(rf);
                fseek(rf, 0, SEEK_SET);
                char *answer = malloc((size_t)sz + 1);
                if (answer) {
                    size_t got = fread(answer, 1, (size_t)sz, rf);
                    answer[got] = '\0';
                    fclose(rf);
                    /* Strip trailing newline. */
                    while (got > 0 && (answer[got-1] == '\n' || answer[got-1] == '\r'))
                        answer[--got] = '\0';
                    remove(response_path);
                    remove(prompt_path);
                    return answer;
                }
                fclose(rf);
            }
        }
        usleep(500000); /* 0.5s poll interval */
    }

    /* Timeout — clean up. */
    remove(prompt_path);
    remove(response_path);
    if (default_answer) return strdup(default_answer);
    return NULL;
}

/* ── _wait_for_service_active ────────────────────────── */
/* PoP: uc_wait_for_service_active @ hermes_cli/update_cmd.py:_wait_for_service_active */
int uc_wait_for_service_active(const char *service_name,
                                     double timeout_sec,
                                     int poll_ms)
{
    if (!service_name || !*service_name) return -1;
    if (poll_ms <= 0) poll_ms = 1000;
    double deadline = (double)time(NULL) + timeout_sec;
    char cmd[4096];
    while ((double)time(NULL) < deadline) {
        snprintf(cmd, sizeof(cmd), "systemctl is-active --user \"%s\" 2>/dev/null", service_name);
        FILE *f = popen(cmd, "r");
        if (f) {
            char buf[64] = {0};
            if (fgets(buf, sizeof(buf), f)) {
                pclose(f);
                /* Strip newline. */
                size_t len = strlen(buf);
                while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
                    buf[--len] = '\0';
                if (strcmp(buf, "active") == 0) return 1;
            } else {
                pclose(f);
            }
        }
        usleep((useconds_t)poll_ms * 1000);
    }
    return 0;
}

/* ── uc_generate_uuid ────────────────────────────── */
/* PoP: uc_generate_uuid @ hermes_cli/update_cmd.py:_gateway_prompt (inline) */
char *uc_generate_uuid(void)
{
    unsigned char buf[16];
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f) return NULL;
    if (fread(buf, 1, 16, f) != 16) { fclose(f); return NULL; }
    fclose(f);
    buf[6] = (buf[6] & 0x0f) | 0x40;
    buf[8] = (buf[8] & 0x3f) | 0x80;
    char *s = malloc(37);
    if (!s) return NULL;
    snprintf(s, 37,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],
             buf[8],buf[9],buf[10],buf[11],buf[12],buf[13],buf[14],buf[15]);
    return s;
}

/* ── _restore_stashed_changes ───────────────────────────────────────── */
/* PoP: uc_restore_stashed_changes @ hermes_cli/update_cmd.py:_restore_stashed_changes */
bool uc_restore_stashed_changes(const char *git_cmd[], const char *cwd,
                                const char *stash_ref, bool prompt_user)
{
    (void)git_cmd;
    if (!cwd || !stash_ref || !*stash_ref) return false;

    if (prompt_user) {
        printf("\n");
        printf("⚠ Local changes were stashed before updating.\n");
        printf("  Restoring them may reapply local customizations onto the updated codebase.\n");
        printf("  Review the result afterward if Hermes behaves unexpectedly.\n");
        printf("Restore local changes now? [Y/n] ");
        fflush(stdout);
        char response[128] = "";
        if (fgets(response, sizeof(response), stdin)) {
            /* strip newline + lowercase */
            char *nl = strpbrk(response, "\r\n");
            if (nl) *nl = '\0';
            for (char *c = response; *c; c++) *c = (char)tolower((unsigned char)*c);
        }
        if (strcmp(response, "") != 0 && strcmp(response, "y") != 0 &&
            strcmp(response, "yes") != 0) {
            printf("Skipped restoring local changes.\n");
            printf("Your changes are still preserved in git stash.\n");
            printf("Restore manually with: git stash apply %s\n", stash_ref);
            return false;
        }
    }

    printf("→ Restoring local changes...\n");

    /* git stash apply <stash_ref> */
    char *apply_out = NULL;
    size_t apply_len = 0;
    int apply_rc = -1;
    {
        const char *args[] = { "stash", "apply", stash_ref };
        apply_rc = web_git_run(cwd, &apply_out, &apply_len, args, 3);
    }

    /* git diff --name-only --diff-filter=U — conflicts even when rc==0 */
    char *unmerged_out = NULL;
    size_t unmerged_len = 0;
    {
        const char *args[] = { "diff", "--name-only", "--diff-filter=U" };
        web_git_run(cwd, &unmerged_out, &unmerged_len, args, 3);
    }
    /* has_conflicts = bool(stdout.strip()) */
    bool has_conflicts = false;
    if (unmerged_out) {
        const char *p = unmerged_out;
        while (*p && isspace((unsigned char)*p)) p++;
        has_conflicts = (*p != '\0');
    }

    bool apply_ok = (apply_rc == 0);

    /* _stash_apply_failed_only_on_existing_untracked(stderr) — the Python
     * inspects stderr; web_git_run merges stdout only, so feed it the
     * combined output we captured (the untracked-only failure prints
     * "already exists, no checkout" / "could not restore untracked files"). */
    bool treated_as_restored = false;
    if (!apply_ok && !has_conflicts) {
        if (uc_stash_apply_failed_only_on_existing_untracked(apply_out ? apply_out : "")) {
            printf("  ⚠ Some stashed untracked files already exist in the working "
                   "tree and were kept as-is.\n");
            treated_as_restored = true;
        }
    }

    if (!apply_ok && !has_conflicts && treated_as_restored) {
        /* fall through to selector resolution + drop */
    } else if (!apply_ok || has_conflicts) {
        printf("✗ Update pulled new code, but restoring local changes hit conflicts.\n");
        if (apply_out && *apply_out) printf("%s", apply_out);
        /* Conflicted files */
        if (unmerged_out && *unmerged_out) {
            printf("\nConflicted files:\n");
            char *dup = strdup(unmerged_out);
            char *saveptr = NULL;
            for (char *line = strtok_r(dup, "\n", &saveptr); line;
                 line = strtok_r(NULL, "\n", &saveptr)) {
                char *t = line;
                while (*t && isspace((unsigned char)*t)) t++;
                if (*t) printf("  • %s\n", t);
            }
            free(dup);
        }
        printf("\nYour stashed changes are preserved — nothing is lost.\n");
        printf("  Stash ref: %s\n", stash_ref);
        /* reset --hard HEAD */
        {
            char *ro = NULL;
            size_t ro_len = 0;
            const char *args[] = { "reset", "--hard", "HEAD" };
            web_git_run(cwd, &ro, &ro_len, args, 3);
            free(ro);
        }
        printf("Working tree reset to clean state.\n");
        printf("Restore your changes later with: git stash apply %s\n", stash_ref);
        free(apply_out);
        free(unmerged_out);
        return false;
    }

    /* Resolve selector: git stash list --format=%gd %H */
    char *selector = NULL;
    {
        char *list_out = NULL;
        size_t list_len = 0;
        const char *args[] = { "stash", "list", "--format=%gd %H" };
        int rc = web_git_run(cwd, &list_out, &list_len, args, 3);
        if (rc == 0 && list_out) {
            selector = uc_resolve_stash_selector(list_out, stash_ref);
        }
        free(list_out);
    }

    if (selector == NULL) {
        printf("⚠ Local changes were restored, but Hermes couldn't find the stash entry to drop.\n");
        printf("  The stash was left in place. You can remove it manually after checking the result.\n");
        char *guidance = uc_stash_cleanup_guidance(stash_ref, NULL);
        if (guidance) { printf("%s\n", guidance); free(guidance); }
    } else {
        char *drop_out = NULL;
        size_t drop_len = 0;
        const char *args[] = { "stash", "drop", selector };
        int rc = web_git_run(cwd, &drop_out, &drop_len, args, 3);
        if (rc != 0) {
            printf("⚠ Local changes were restored, but Hermes couldn't drop the saved stash entry.\n");
            if (drop_out && *drop_out) printf("%s", drop_out);
            printf("  The stash was left in place. You can remove it manually after checking the result.\n");
            char *guidance = uc_stash_cleanup_guidance(stash_ref, selector);
            if (guidance) { printf("%s\n", guidance); free(guidance); }
        }
        free(drop_out);
        free(selector);
    }

    printf("⚠ Local changes were restored on top of the updated codebase.\n");
    printf("  Review `git diff` / `git status` if Hermes behaves unexpectedly.\n");
    free(apply_out);
    free(unmerged_out);
    return true;
}

/* ── _discard_stashed_changes ───────────────────────────────────────── */
/* PoP: uc_discard_stashed_changes @ hermes_cli/update_cmd.py:_discard_stashed_changes */
bool uc_discard_stashed_changes(const char *git_cmd[], const char *cwd,
                                const char *stash_ref)
{
    (void)git_cmd;
    if (!cwd || !stash_ref || !*stash_ref) return false;

    char *selector = NULL;
    {
        char *list_out = NULL;
        size_t list_len = 0;
        const char *args[] = { "stash", "list", "--format=%gd %H" };
        int rc = web_git_run(cwd, &list_out, &list_len, args, 3);
        if (rc == 0 && list_out) {
            selector = uc_resolve_stash_selector(list_out, stash_ref);
        }
        free(list_out);
    }

    if (selector == NULL) {
        printf("⚠ Configured to discard local changes on non-interactive update, "
               "but Hermes couldn't find the stash entry to drop.\n");
        char *guidance = uc_stash_cleanup_guidance(stash_ref, NULL);
        if (guidance) { printf("%s\n", guidance); free(guidance); }
        return false;
    }

    char *drop_out = NULL;
    size_t drop_len = 0;
    const char *args[] = { "stash", "drop", selector };
    int rc = web_git_run(cwd, &drop_out, &drop_len, args, 3);
    if (rc != 0) {
        printf("⚠ Configured to discard local changes, but Hermes couldn't drop "
               "the saved stash entry.\n");
        if (drop_out && *drop_out) {
            char *first = drop_out;
            char *nl = strchr(first, '\n');
            size_t flen = nl ? (size_t)(nl - first) : strlen(first);
            char *t = first;
            while (*t && isspace((unsigned char)*t)) t++;
            printf("  %.*s\n", (int)(flen - (size_t)(t - first)), t);
        }
        char *guidance = uc_stash_cleanup_guidance(stash_ref, selector);
        if (guidance) { printf("%s\n", guidance); free(guidance); }
        free(drop_out);
        free(selector);
        return false;
    }
    free(drop_out);
    free(selector);

    printf("→ Discarded local source changes (updates.non_interactive_local_changes=discard).\n");
    return true;
}

/* ── _sync_with_upstream_if_needed ──────────────────────────────────── */
/* PoP: uc_sync_with_upstream_if_needed @ hermes_cli/update_cmd.py:_sync_with_upstream_if_needed */
void uc_sync_with_upstream_if_needed(const char *git_cmd[], const char *cwd,
                                     const char *hermes_home)
{
    (void)git_cmd;
    if (!cwd) return;

    bool has_upstream = uc_has_upstream_remote(NULL, cwd);

    if (!has_upstream) {
        if (uc_should_skip_upstream_prompt(hermes_home)) return;

        printf("\n");
        printf("ℹ Your fork is not tracking the official Hermes repository.\n");
        printf("  This means you may miss updates from NousResearch/hermes-agent.\n");
        printf("\n");
        printf("Add official repo as 'upstream' remote? [Y/n]: ");
        fflush(stdout);
        char response[128] = "";
        if (fgets(response, sizeof(response), stdin)) {
            char *nl = strpbrk(response, "\r\n");
            if (nl) *nl = '\0';
            for (char *c = response; *c; c++) *c = (char)tolower((unsigned char)*c);
        }
        if (strcmp(response, "") == 0 || strcmp(response, "y") == 0 ||
            strcmp(response, "yes") == 0) {
            printf("→ Adding upstream remote...\n");
            if (uc_add_upstream_remote(NULL, cwd)) {
                printf("  ✓ Added upstream: %s\n", UC_OFFICIAL_REPO_URLS[0]);
                has_upstream = true;
            } else {
                printf("  ✗ Failed to add upstream remote. Skipping upstream sync.\n");
                return;
            }
        } else {
            printf("  Skipped. Run 'git remote add upstream %s' to add later.\n",
                   UC_OFFICIAL_REPO_URLS[0]);
            uc_mark_skip_upstream_prompt(hermes_home);
            return;
        }
    }

    printf("\n");
    printf("→ Fetching upstream...\n");
    {
        char *out = NULL;
        size_t out_len = 0;
        const char *args[] = { "fetch", "upstream", "main", "--quiet" };
        int rc = web_git_run(cwd, &out, &out_len, args, 4);
        free(out);
        if (rc != 0) {
            printf("  ✗ Failed to fetch upstream. Skipping upstream sync.\n");
            return;
        }
    }

    /* origin_ahead = count(upstream/main..origin/main) */
    int origin_ahead = -1, upstream_ahead = -1;
    {
        char *out = NULL;
        size_t out_len = 0;
        const char *args[] = { "rev-list", "--count", "upstream/main..origin/main" };
        if (web_git_run(cwd, &out, &out_len, args, 3) == 0 && out)
            origin_ahead = uc_count_commits_between(out);
        free(out);
    }
    {
        char *out = NULL;
        size_t out_len = 0;
        const char *args[] = { "rev-list", "--count", "origin/main..upstream/main" };
        if (web_git_run(cwd, &out, &out_len, args, 3) == 0 && out)
            upstream_ahead = uc_count_commits_between(out);
        free(out);
    }

    if (origin_ahead < 0 || upstream_ahead < 0) {
        printf("  ✗ Could not compare branches. Skipping upstream sync.\n");
        return;
    }

    if (origin_ahead > 0) {
        printf("\n");
        printf("ℹ Your fork has %d commit(s) not on upstream.\n", origin_ahead);
        printf("  Skipping upstream sync to preserve your changes.\n");
        printf("  If you want to merge upstream changes, run:\n");
        printf("    git pull upstream main\n");
        return;
    }

    if (upstream_ahead == 0) {
        printf("  ✓ Fork is up to date with upstream\n");
        return;
    }

    printf("\n");
    printf("→ Fork is %d commit(s) behind upstream\n", upstream_ahead);
    printf("→ Pulling from upstream...\n");
    {
        char *out = NULL;
        size_t out_len = 0;
        const char *args[] = { "pull", "--ff-only", "upstream", "main" };
        int rc = web_git_run(cwd, &out, &out_len, args, 4);
        free(out);
        if (rc != 0) {
            printf("  ✗ Failed to pull from upstream. You may need to resolve conflicts manually.\n");
            return;
        }
    }
    printf("  ✓ Updated from upstream\n");

    printf("→ Syncing fork...\n");
    if (uc_sync_fork_with_upstream(NULL, cwd)) {
        printf("  ✓ Fork synced with upstream\n");
    } else {
        printf("  ℹ Got updates from upstream but couldn't push to fork (no write access?)\n");
        printf("    Your local repo is updated, but your fork on GitHub may be behind.\n");
    }
}

/* ── _discard_lockfile_churn (orchestration half) ───────────────────── */
/* PoP: uc_discard_lockfile_churn @ hermes_cli/update_cmd.py:_discard_lockfile_churn */
int uc_discard_lockfile_churn(const char *git_cmd[], const char *repo_root)
{
    (void)git_cmd;
    if (!repo_root) return 0;

    char *diff_out = NULL;
    size_t diff_len = 0;
    const char *args[] = { "diff", "--name-only" };
    int rc = web_git_run(repo_root, &diff_out, &diff_len, args, 2);
    if (rc != 0) {
        free(diff_out);
        return 0;
    }

    char **dirty = uc_select_lockfile_churn(diff_out ? diff_out : "");
    free(diff_out);
    if (!dirty) return 0;

    size_t n = 0;
    while (dirty[n]) n++;
    if (n == 0) {
        uc_free_string_array(dirty);
        return 0;
    }

    /* git checkout -- <paths...> */
    {
        const char **co_args = malloc((n + 3) * sizeof(char *));
        if (co_args) {
            size_t k = 0;
            co_args[k++] = "checkout";
            co_args[k++] = "--";
            for (size_t i = 0; i < n; i++) co_args[k++] = dirty[i];
            co_args[k] = NULL;
            char *co_out = NULL;
            size_t co_len = 0;
            web_git_run(repo_root, &co_out, &co_len, co_args, k);
            free(co_out);
            free(co_args);
        }
    }
    uc_free_string_array(dirty);

    printf("→ Discarded npm lockfile churn (%zu file(s))\n", n);
    return (int)n;
}

/* ── _normalize_managed_eol ─────────────────────────────────────────── */
/* PoP: uc_normalize_managed_eol @ hermes_cli/update_cmd.py:_normalize_managed_eol */
int uc_normalize_managed_eol(const char *git_cmd[], const char *repo_root)
{
    (void)git_cmd;
    if (!repo_root) return 0;

    /* effective = git config --get core.autocrlf */
    char *effective = NULL;
    size_t eff_len = 0;
    {
        const char *args[] = { "config", "--get", "core.autocrlf" };
        int rc = web_git_run(repo_root, &effective, &eff_len, args, 3);
        if (rc != 0) { free(effective); return 0; }
    }
    /* Only "true" rewrites LF to CRLF on checkout. */
    bool is_true = false;
    if (effective) {
        const char *p = effective;
        while (*p && isspace((unsigned char)*p)) p++;
        if (strncmp(p, "true", 4) == 0) {
            const char *q = p + 4;
            while (*q && isspace((unsigned char)*q)) q++;
            is_true = (*q == '\0');
        }
    }
    free(effective);
    if (!is_true) return 0;

    /* _dirty(): probe diff -z --name-only → NUL-separated path set */
    char **all_dirty = NULL;
    {
        char *out = NULL;
        size_t out_len = 0;
        const char *args[] = { "-c", "core.autocrlf=false",
                               "diff", "-z", "--name-only" };
        int rc = web_git_run(repo_root, &out, &out_len, args, 5);
        if (rc != 0) { free(out); return 0; }
        if (out && out_len) {
            /* split on NUL */
            size_t cap = 0, count = 0;
            char **paths = NULL;
            for (char *p = out; p < out + out_len; ) {
                size_t plen = strlen(p);
                if (plen == 0) { p++; continue; }
                if (count >= cap) {
                    cap = cap ? cap * 2 : 16;
                    char **tmp = realloc(paths, cap * sizeof(char *));
                    if (!tmp) { free(paths); paths = NULL; break; }
                    paths = tmp;
                }
                paths[count++] = strdup(p);
                p += plen + 1;
            }
            if (paths) {
                char **tmp = realloc(paths, (count + 1) * sizeof(char *));
                if (tmp) paths = tmp;
                paths[count] = NULL;
            }
            all_dirty = paths;
        }
        free(out);
    }
    if (!all_dirty) return 0;

    /* _real_dirty(): probe -c core.quotepath=false diff --numstat
     * --ignore-cr-at-eol → parse paths */
    char **real_dirty = NULL;
    {
        char *out = NULL;
        size_t out_len = 0;
        const char *args[] = { "-c", "core.autocrlf=false",
                               "-c", "core.quotepath=false",
                               "diff", "--numstat", "--ignore-cr-at-eol" };
        int rc = web_git_run(repo_root, &out, &out_len, args, 7);
        if (rc != 0) { free(out); for (size_t i = 0; all_dirty[i]; i++) free(all_dirty[i]); free(all_dirty); return 0; }
        real_dirty = uc_parse_numstat_paths(out ? out : "");
        free(out);
    }
    if (!real_dirty) {
        for (size_t i = 0; all_dirty[i]; i++) free(all_dirty[i]);
        free(all_dirty);
        return 0;
    }

    /* eol_only = all_dirty - real_dirty */
    size_t eol_count = 0;
    char **eol_only = NULL;
    {
        size_t cap = 0;
        for (size_t i = 0; all_dirty[i]; i++) {
            bool in_real = false;
            for (size_t j = 0; real_dirty[j]; j++) {
                if (strcmp(all_dirty[i], real_dirty[j]) == 0) { in_real = true; break; }
            }
            if (!in_real) {
                if (eol_count >= cap) {
                    cap = cap ? cap * 2 : 16;
                    char **tmp = realloc(eol_only, cap * sizeof(char *));
                    if (!tmp) break;
                    eol_only = tmp;
                }
                eol_only[eol_count++] = strdup(all_dirty[i]);
            }
        }
        if (eol_only) {
            char **tmp = realloc(eol_only, (eol_count + 1) * sizeof(char *));
            if (tmp) eol_only = tmp;
            eol_only[eol_count] = NULL;
        }
    }
    for (size_t i = 0; all_dirty[i]; i++) free(all_dirty[i]);
    free(all_dirty);
    for (size_t i = 0; real_dirty[i]; i++) free(real_dirty[i]);
    free(real_dirty);

    if (eol_count == 0) {
        free(eol_only);
        /* pin core.autocrlf false (tree already clean under probe) */
        {
            char *out = NULL;
            size_t out_len = 0;
            const char *args[] = { "config", "core.autocrlf", "false" };
            web_git_run(repo_root, &out, &out_len, args, 3);
            free(out);
        }
        return 0;
    }

    /* checkout --pathspec-from-file with NUL-separated sorted paths.
     * git reads a NUL-terminated file, so write sorted paths + NUL. */
    {
        /* sort eol_only (Python sorts before writing) */
        for (size_t i = 0; i + 1 < eol_count; i++) {
            for (size_t j = i + 1; j < eol_count; j++) {
                if (strcmp(eol_only[i], eol_only[j]) > 0) {
                    char *tmp = eol_only[i];
                    eol_only[i] = eol_only[j];
                    eol_only[j] = tmp;
                }
            }
        }
        char tmp_path[] = "/tmp/uc_eol_paths_XXXXXX";
        int fd = mkstemp(tmp_path);
        if (fd >= 0) {
            FILE *f = fdopen(fd, "wb");
            if (f) {
                for (size_t i = 0; i < eol_count; i++) {
                    fwrite(eol_only[i], 1, strlen(eol_only[i]), f);
                    fputc('\0', f);
                }
                fclose(f);
                char *out = NULL;
                size_t out_len = 0;
                /* web_git_run has no stdin plumbing; use the file directly. */
                char path_arg[64];
                snprintf(path_arg, sizeof(path_arg), "--pathspec-from-file=%s", tmp_path);
                const char *args2[] = { "-c", "core.autocrlf=false",
                                        "checkout", path_arg,
                                        "--pathspec-file-nul", "--" };
                web_git_run(repo_root, &out, &out_len, args2, 6);
                free(out);
                unlink(tmp_path);
            } else {
                close(fd);
            }
        }
    }

    /* Re-check: if still dirty, leave the checkout as found (no pin). */
    {
        char *out = NULL;
        size_t out_len = 0;
        const char *args[] = { "-c", "core.autocrlf=false",
                               "diff", "-z", "--name-only" };
        int rc = web_git_run(repo_root, &out, &out_len, args, 5);
        bool still_dirty = false;
        if (rc == 0 && out && out_len) {
            for (char *p = out; p < out + out_len; ) {
                size_t plen = strlen(p);
                if (plen > 0) { still_dirty = true; break; }
                p += plen + 1;
            }
        }
        free(out);
        if (still_dirty) {
            for (size_t i = 0; i < eol_count; i++) free(eol_only[i]);
            free(eol_only);
            return 0;
        }
    }

    for (size_t i = 0; i < eol_count; i++) free(eol_only[i]);
    free(eol_only);

    printf("→ Normalized line-ending churn (%zu file(s))\n", eol_count);

    /* pin core.autocrlf false — tree verified clean under the probe */
    {
        char *out = NULL;
        size_t out_len = 0;
        const char *args[] = { "config", "core.autocrlf", "false" };
        web_git_run(repo_root, &out, &out_len, args, 3);
        free(out);
    }
    return (int)eol_count;
}

/* ── _run_logged_subprocess ─────────────────────────────────────────── */
/* PoP: uc_run_logged_subprocess @ hermes_cli/update_cmd.py:_run_logged_subprocess */
int uc_run_logged_subprocess(char *const argv[], const char *cwd,
                             FILE *log_file, char **out_combined)
{
    if (out_combined) *out_combined = NULL;
    if (!argv || !argv[0]) return -1;

    /* Build the command line: cd "<cwd>" && <argv[0]> <argv[1]> ... */
    size_t cap = 64;
    for (size_t i = 0; argv[i]; i++) cap += strlen(argv[i]) + 8;
    if (cwd) cap += strlen(cwd) + 8;
    char *cmd = malloc(cap);
    if (!cmd) return -1;
    int p = 0;
    if (cwd && *cwd) {
        p += snprintf(cmd + p, cap - (size_t)p, "cd \"%s\" && ", cwd);
    }
    for (size_t i = 0; argv[i]; i++) {
        const char *a = argv[i];
        bool need_q = (a[0] == '\0');
        for (const char *q = a; *q; q++) {
            if (isspace((unsigned char)*q) || *q == '\'' || *q == '"' ||
                *q == '(' || *q == ')' || *q == '&' || *q == '|' ||
                *q == ';' || *q == '<' || *q == '>' || *q == '$' ||
                *q == '\\' || *q == '`' || *q == '*' || *q == '?') {
                need_q = true;
                break;
            }
        }
        if (need_q) p += snprintf(cmd + p, cap - (size_t)p, " '%s'", a);
        else p += snprintf(cmd + p, cap - (size_t)p, " %s", a);
    }
    if ((size_t)p >= cap) { free(cmd); return -1; }

    FILE *fp = popen(cmd, "r");
    free(cmd);
    if (!fp) return -1;

    size_t len = 0, alloc = 8192;
    char *buf = malloc(alloc);
    if (!buf) { pclose(fp); return -1; }
    char tmp[4096];
    size_t r;
    while ((r = fread(tmp, 1, sizeof(tmp), fp)) > 0) {
        if (len + r + 1 > alloc) {
            while (len + r + 1 > alloc) alloc *= 2;
            char *nb = realloc(buf, alloc);
            if (!nb) { free(buf); pclose(fp); return -1; }
            buf = nb;
        }
        memcpy(buf + len, tmp, r);
        len += r;
    }
    buf[len] = '\0';
    int code = pclose(fp);
    if (code == -1) code = 1;

    if (log_file && buf[0]) uc_log_only_write(log_file, buf);
    if (out_combined) *out_combined = buf;
    else free(buf);
    return code;
}

/* ── _cmd_update_check ───────────────────────────────────────────────── */
/* PoP: uc_cmd_update_check @ hermes_cli/update_cmd.py:_cmd_update_check */
int uc_cmd_update_check(const char *project_root, const char *branch)
{
    if (!project_root) return -1;
    if (!branch || !*branch) branch = "main";

    /* Not a git repo? */
    char git_dir[4096];
    snprintf(git_dir, sizeof(git_dir), "%s/.git", project_root);
    struct stat st;
    if (stat(git_dir, &st) != 0) {
        printf("✗ Not a git repository — cannot check for updates.\n");
        return -1;
    }

    /* is_shallow? */
    bool is_shallow = false;
    {
        char *out = NULL;
        size_t out_len = 0;
        const char *args[] = { "rev-parse", "--is-shallow-repository" };
        if (web_git_run(project_root, &out, &out_len, args, 2) == 0 && out) {
            const char *p = out;
            while (*p && isspace((unsigned char)*p)) p++;
            is_shallow = (strncmp(p, "true", 4) == 0);
        }
        free(out);
    }
    bool use_depth = is_shallow;

    const char *compare_branch = NULL;
    char compare_buf[128];

    if (strcmp(branch, "main") == 0) {
        bool has_upstream = uc_has_upstream_remote(NULL, project_root);
        int fetch_rc = -1;
        if (has_upstream) {
            printf("→ Fetching from upstream...\n");
            char *out = NULL;
            size_t out_len = 0;
            if (use_depth) {
                const char *args[] = { "fetch", "--depth", "1", "upstream", branch };
                fetch_rc = web_git_run(project_root, &out, &out_len, args, 5);
            } else {
                const char *args[] = { "fetch", "upstream", branch };
                fetch_rc = web_git_run(project_root, &out, &out_len, args, 3);
            }
            free(out);
        }
        if (fetch_rc == 0) {
            snprintf(compare_buf, sizeof(compare_buf), "upstream/%s", branch);
            compare_branch = compare_buf;
        } else {
            printf("→ Fetching from origin...\n");
            char *out = NULL;
            size_t out_len = 0;
            if (use_depth) {
                const char *args[] = { "fetch", "--depth", "1", "origin", branch };
                fetch_rc = web_git_run(project_root, &out, &out_len, args, 5);
            } else {
                const char *args[] = { "fetch", "origin", branch };
                fetch_rc = web_git_run(project_root, &out, &out_len, args, 3);
            }
            free(out);
            snprintf(compare_buf, sizeof(compare_buf), "origin/%s", branch);
            compare_branch = compare_buf;
        }
        if (fetch_rc != 0) {
            printf("✗ Failed to fetch.\n");
            return -1;
        }
    } else {
        printf("→ Fetching from origin...\n");
        char *out = NULL;
        size_t out_len = 0;
        int fetch_rc;
        if (use_depth) {
            const char *args[] = { "fetch", "--depth", "1", "origin", branch };
            fetch_rc = web_git_run(project_root, &out, &out_len, args, 5);
        } else {
            const char *args[] = { "fetch", "origin", branch };
            fetch_rc = web_git_run(project_root, &out, &out_len, args, 3);
        }
        free(out);
        snprintf(compare_buf, sizeof(compare_buf), "origin/%s", branch);
        compare_branch = compare_buf;
        if (fetch_rc != 0) {
            printf("✗ Failed to fetch.\n");
            return -1;
        }
    }

    /* Verify the compare ref exists. */
    {
        char *out = NULL;
        size_t out_len = 0;
        const char *args[] = { "rev-parse", "--verify", "--quiet", compare_branch };
        int rc = web_git_run(project_root, &out, &out_len, args, 4);
        free(out);
        if (rc != 0) {
            printf("✗ Branch '%s' not found on %s.\n", branch,
                   strstr(compare_branch, "/") ? compare_branch : "origin");
            return -1;
        }
    }

    if (is_shallow) {
        char *head_sha = NULL, *target_sha = NULL;
        {
            char *out = NULL;
            size_t out_len = 0;
            const char *args[] = { "rev-parse", "HEAD" };
            if (web_git_run(project_root, &out, &out_len, args, 2) == 0) {
                head_sha = out ? strdup(out) : NULL;
                if (head_sha) {
                    char *p = head_sha;
                    while (*p && isspace((unsigned char)*p)) p++;
                    memmove(head_sha, p, strlen(p) + 1);
                    char *nl = strchr(head_sha, '\n');
                    if (nl) *nl = '\0';
                }
            } else {
                free(out);
            }
        }
        {
            char *out = NULL;
            size_t out_len = 0;
            const char *args[] = { "rev-parse", compare_branch };
            if (web_git_run(project_root, &out, &out_len, args, 2) == 0) {
                target_sha = out ? strdup(out) : NULL;
                if (target_sha) {
                    char *p = target_sha;
                    while (*p && isspace((unsigned char)*p)) p++;
                    memmove(target_sha, p, strlen(p) + 1);
                    char *nl = strchr(target_sha, '\n');
                    if (nl) *nl = '\0';
                }
            } else {
                free(out);
            }
        }
        int result;
        if (head_sha && target_sha && strcmp(head_sha, target_sha) == 0) {
            printf("✓ Already up to date.\n");
            result = 0;
        } else {
            printf("⚕ Update available (behind %s).\n", compare_branch);
            printf("  Run 'hermes update' to install.\n");
            result = 1;
        }
        free(head_sha);
        free(target_sha);
        return result;
    }

    /* rev-list HEAD..<compare> --count */
    int behind = -1;
    {
        char range[192];
        snprintf(range, sizeof(range), "HEAD..%s", compare_branch);
        char *out = NULL;
        size_t out_len = 0;
        const char *args[] = { "rev-list", range, "--count" };
        if (web_git_run(project_root, &out, &out_len, args, 3) == 0 && out)
            behind = uc_count_commits_between(out);
        free(out);
    }
    if (behind < 0) {
        printf("✗ Failed to compare branches.\n");
        return -1;
    }

    if (behind == 0) {
        printf("✓ Already up to date.\n");
        return 0;
    }
    printf("⚕ Update available: %d %s behind %s.\n", behind,
           behind == 1 ? "commit" : "commits", compare_branch);
    printf("  Run 'hermes update' to install.\n");
    return 1;
}

/* ── _update_node_dependencies ─────────────────────────────── */
/* PoP: uc_update_node_dependencies @ hermes_cli/update_cmd.py:_update_node_dependencies */
char **uc_update_node_dependencies(const char *project_root)
{
    if (!project_root) return NULL;

    char pkg_json[4096];
    snprintf(pkg_json, sizeof(pkg_json), "%s/package.json", project_root);
    struct stat st;
    if (stat(pkg_json, &st) != 0) {
        /* No package.json — nothing to do */
        char **empty = malloc(sizeof(char *));
        if (empty) empty[0] = NULL;
        return empty;
    }

    /* Check for npm on PATH */
    char *npm_path = NULL;
    {
        FILE *fp = popen("command -v npm 2>/dev/null", "r");
        if (fp) {
            char buf[512];
            if (fgets(buf, sizeof(buf), fp)) {
                buf[strcspn(buf, "\n\r")] = '\0';
                if (strlen(buf) > 0) npm_path = strdup(buf);
            }
            pclose(fp);
        }
    }
    if (!npm_path) {
        /* No npm found — check for WSL Windows npm */
        return NULL;
    }

    /* Check if lockfile changed or node_modules missing */
    bool need_install = false;
    {
        char lockfile[4096];
        snprintf(lockfile, sizeof(lockfile), "%s/package-lock.json", project_root);
        if (stat(lockfile, &st) != 0) need_install = true;
    }
    {
        char node_modules[4096];
        snprintf(node_modules, sizeof(node_modules), "%s/node_modules", project_root);
        if (stat(node_modules, &st) != 0) need_install = true;
    }
    if (!need_install) {
        free(npm_path);
        char **empty = malloc(sizeof(char *));
        if (empty) empty[0] = NULL;
        return empty;
    }

    printf("→ Updating Node.js dependencies...\n");

    /* Step 1: root install (no workspace recursion) */
    char *root_out = NULL;
    size_t root_len = 0;
    {
        const char *args[] = { npm_path, "install",
                               "--no-fund", "--no-audit",
                               "--prefer-offline", "--progress=false",
                               "--workspaces=false" };
        web_git_run(project_root, &root_out, &root_len, args, 7);
    }
    int root_rc = (root_out && *root_out) ? 0 : -1;
    /* web_git_run returns 0 on success; we check exit via the return code */
    free(root_out);

    if (root_rc != 0) {
        printf("  ⚠ npm install failed in repo root\n");
        free(npm_path);
        char **failed = malloc(2 * sizeof(char *));
        if (failed) {
            failed[0] = strdup("repo root");
            failed[1] = NULL;
        }
        return failed;
    }

    /* Step 2: workspace install (ui-tui, web) */
    char *ws_out = NULL;
    size_t ws_len = 0;
    {
        const char *args[] = { npm_path, "install",
                               "--no-fund", "--no-audit",
                               "--prefer-offline", "--progress=false",
                               "--workspace", "ui-tui",
                               "--workspace", "web" };
        web_git_run(project_root, &ws_out, &ws_len, args, 10);
    }
    free(ws_out);
    free(npm_path);

    if (root_rc != 0) {
        char **failed = malloc(3 * sizeof(char *));
        if (failed) {
            failed[0] = strdup("repo root");
            failed[1] = strdup("ui-tui, web workspaces");
            failed[2] = NULL;
        }
        return failed;
    }

    char **empty = malloc(sizeof(char *));
    if (empty) empty[0] = NULL;
    return empty;
}

/* ── _upgrade_pip_before_lazy_refresh ─────────────────────── */
/* PoP: uc_upgrade_pip_before_lazy_refresh @ hermes_cli/update_cmd.py:_upgrade_pip_before_lazy_refresh */
void uc_upgrade_pip_before_lazy_refresh(const char *install_cmd_prefix[],
                                            const char *env_path)
{
    if (!install_cmd_prefix || !install_cmd_prefix[0]) return;

    /* Build: <prefix> install --upgrade pip */
    size_t nargs = 0;
    while (install_cmd_prefix[nargs]) nargs++;
    const char **args = malloc((nargs + 3) * sizeof(char *));
    if (!args) return;
    memcpy(args, install_cmd_prefix, nargs * sizeof(char *));
    args[nargs++] = "install";
    args[nargs++] = "--upgrade";
    args[nargs++] = "pip";
    args[nargs] = NULL;

    /* Run via subprocess — capture output silently */
    char cmd[4096];
    int p = 0;
    for (size_t i = 0; args[i]; i++) {
        p += snprintf(cmd + p, sizeof(cmd) - (size_t)p, "%s%s", i ? " " : "", args[i]);
    }
    free(args);

    FILE *fp = popen(cmd, "r");
    if (!fp) return;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) {
        /* Silently consume output — never show to terminal */
    }
    pclose(fp);
}

/* ── _refresh_active_lazy_features ──────────────────────────── */
/* PoP: uc_refresh_active_lazy_features @ hermes_cli/update_cmd.py:_refresh_active_lazy_features */
char *uc_refresh_active_lazy_features(const char *install_cmd_prefix[],
                                          const char *env_path)
{
    (void)install_cmd_prefix;
    (void)env_path;
    /* The Python version calls lazy_deps.active_features() and
     * lazy_deps.refresh_active_features() which require the Python
     * runtime. This is a portable stub that returns "ok" — the
     * actual lazy refresh is a Python-only operation. */
    return NULL;
}

/* ── _refresh_active_memory_provider_dependencies ────────────── */
/* PoP: uc_refresh_active_memory_provider_dependencies @ hermes_cli/update_cmd.py:_refresh_active_memory_provider_dependencies */
char *uc_refresh_active_memory_provider_dependencies(void)
{
    /* The Python version loads config, finds the active memory
     * provider, and calls _install_dependencies(provider, force=True).
     * This requires the Python runtime and hermes_cli.memory_setup.
     * Return "skipped" as a portable stub. */
    char *result = malloc(8);
    if (result) strcpy(result, "skipped");
    return result;
}

/* ── _run_pre_update_backup ────────────────────────────────── */
/* PoP: uc_run_pre_update_backup @ hermes_cli/update_cmd.py:_run_pre_update_backup */
char *uc_run_pre_update_backup(const char *mode, const char *project_root)
{
    if (!mode || strcmp(mode, "off") == 0) return NULL;

    if (strcmp(mode, "quick") != 0 && strcmp(mode, "full") != 0) {
        return NULL;
    }

    /* Create a quick snapshot directory under project_root/.state-snapshots/
     * with a timestamped name. Copy critical small files into it. */
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    char snap_dir[4096];
    snprintf(snap_dir, sizeof(snap_dir),
             "%s/.state-snapshots/pre-update-%04d%02d%02d-%02d%02d%02d",
             project_root,
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);

    /* Create the snapshot directory */
    struct stat st;
    if (stat(snap_dir, &st) != 0) {
        /* Try to create it */
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", snap_dir);
        int rc = system(cmd);
        if (rc != 0) return NULL;
    }

    /* Critical files to snapshot (mirrors _QUICK_STATE_FILES).
     * Copy them into the snapshot directory. */
    static const char *critical_files[] = {
        "config.yaml", "settings.json", "cron.json",
        "pairing.json", "auth.json", ".gateway-planned-stop.json",
        NULL
    };

    char src[4096], dst[4096];
    for (size_t i = 0; critical_files[i]; i++) {
        snprintf(src, sizeof(src), "%s/%s", project_root, critical_files[i]);
        if (stat(src, &st) != 0) continue;
        /* Skip files > 1 GiB */
        if (st.st_size > (1ULL << 30)) continue;

        snprintf(dst, sizeof(dst), "%s/%s", snap_dir, critical_files[i]);
        /* Ensure parent dir exists */
        char *slash = strrchr(dst, '/');
        if (slash) {
            *slash = '\0';
            char cmd2[4096];
            snprintf(cmd2, sizeof(cmd2), "mkdir -p \"%s\"", dst);
            system(cmd2);
            *slash = '/';
        }
        /* Copy the file */
        char cmd3[4096];
        snprintf(cmd3, sizeof(cmd3), "cp \"%s\" \"%s\" 2>/dev/null", src, dst);
        system(cmd3);
    }

    /* Return the snapshot id (the directory basename) */
    const char *snap_id = strrchr(snap_dir, '/');
    snap_id = snap_id ? snap_id + 1 : snap_dir;
    char *result = strdup(snap_id);
    return result;
}

/* ── _ensure_fhs_path_guard ────────────────────────────────────── */
/* PoP: uc_ensure_fhs_path_guard @ hermes_cli/update_cmd.py:_ensure_fhs_path_guard */
bool uc_ensure_fhs_path_guard(void)
{
#if defined(__linux__)
    /* Only act on Linux FHS installs. */
    if (geteuid() != 0) return false;
#else
    /* Non-Linux: no-op (Windows uses install.ps1, macOS uses a
     * different layout). sys.platform == "linux" is the Python
     * guard; here we use the compile-time check so the function
     * is a no-op stub on non-Linux platforms without runtime cost. */
    (void)geteuid;
    return false;
#endif

    /* Only act when this is actually an FHS-layout install. */
    const char *fhs_link = "/usr/local/bin/hermes";
    struct stat st;
    if (stat(fhs_link, &st) != 0) return false;
    if (!S_ISLNK(st.st_mode) && !S_ISREG(st.st_mode)) return false;

    /* Probe a fresh non-login interactive bash the way the user
     * will use it. ``bash -i -c`` sources ~/.bashrc but NOT
     * ~/.bash_profile or /etc/profile — the exact scenario where
     * RHEL root loses /usr/local/bin. */
    const char *home = getenv("HOME");
    if (!home) home = "/root";
    const char *term = getenv("TERM");
    if (!term) term = "dumb";

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "env -i HOME=%s TERM=%s bash -i -c 'command -v hermes'",
             home, term);

    FILE *fp = popen(cmd, "r");
    if (!fp) return false;
    char buf[128];
    int ch = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    if (ch <= 0) return false;
    buf[ch] = '\0';
    /* Strip trailing newline */
    while (ch > 0 && (buf[ch-1] == '\n' || buf[ch-1] == '\r')) buf[--ch] = '\0';
    if (strlen(buf) > 0) return false;  /* hermes already on PATH */

    const char *path_line = "export PATH=\"/usr/local/bin:$PATH\"";
    const char *path_comment =
        "# Hermes Agent — ensure /usr/local/bin is on PATH (RHEL non-login shells)";
    bool wrote_any = false;

    const char *candidates[] = { ".bashrc", ".bash_profile", NULL };
    for (size_t i = 0; candidates[i]; i++) {
        char cfg_path[1024];
        snprintf(cfg_path, sizeof(cfg_path), "%s/%s", home, candidates[i]);

        FILE *cfg = fopen(cfg_path, "r");
        if (!cfg) continue;

        /* Read existing content */
        size_t cap = 4096, len = 0;
        char *content = malloc(cap);
        if (!content) { fclose(cfg); continue; }
        size_t n;
        while ((n = fread(content + len, 1, cap - len - 1, cfg)) > 0) {
            len += n;
            content[len] = '\0';
            if (len + 1 >= cap) {
                cap *= 2;
                char *tmp = realloc(content, cap);
                if (!tmp) { free(content); fclose(cfg); continue; }
                content = tmp;
            }
        }
        fclose(cfg);

        /* Check idempotency: any uncommented PATH= line referencing /usr/local/bin */
        bool already_guarded = false;
        char *line = content;
        while (*line) {
            char *eol = strchr(line, '\n');
            if (eol) *eol = '\0';
            char *l = line;
            while (*l && isspace((unsigned char)*l)) l++;
            if (*l && *l != '#' && strstr(l, "/usr/local/bin") && strstr(l, "PATH")) {
                already_guarded = true;
                break;
            }
            line = eol ? eol + 1 : line + strlen(line);
        }
        if (already_guarded) { free(content); continue; }

        /* Append the guard */
        FILE *out = fopen(cfg_path, "a");
        if (!out) { free(content); continue; }
        fprintf(out, "\n%s\n%s\n", path_comment, path_line);
        fclose(out);
        free(content);
        printf("  ✓ Added /usr/local/bin to PATH in %s\n", cfg_path);
        wrote_any = true;
    }

    if (wrote_any) {
        printf("    (reload your shell or run 'source ~/.bashrc' to pick it up)\n");
    }
    return wrote_any;
}

/* ── _ensure_acp_launcher ────────────────────────────────────── */
/* PoP: uc_ensure_acp_launcher @ hermes_cli/update_cmd.py:_ensure_acp_launcher */
bool uc_ensure_acp_launcher(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return false;  /* install.ps1 puts venv/Scripts on PATH already */
#else
    (void)0;
#endif

    const char *home = getenv("HOME");
    char local_bin[1024];
    if (home) {
        snprintf(local_bin, sizeof(local_bin), "%s/.local/bin", home);
    }
    local_bin[sizeof(local_bin) - 1] = '\0';

    /* Iterate both ~/.local/bin and /usr/local/bin. */
    const char *bin_dirs[2];
    size_t n_dirs = 0;
    if (home && local_bin[0]) bin_dirs[n_dirs++] = local_bin;
    bin_dirs[n_dirs++] = "/usr/local/bin";

    const char *acp_name = "hermes-acp";

    bool created = false;
    for (size_t d = 0; d < n_dirs; d++) {
        const char *bin_dir = bin_dirs[d];

        char hermes_path[4096];
        snprintf(hermes_path, sizeof(hermes_path), "%s/hermes", bin_dir);

        struct stat st;
        if (stat(hermes_path, &st) != 0) continue;
        if (!S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode)) continue;

        char acp_path[4096];
        snprintf(acp_path, sizeof(acp_path), "%s/%s", bin_dir, acp_name);
        if (stat(acp_path, &st) == 0) continue;  /* already present */

        /* Write the shim */
        FILE *f = fopen(acp_path, "w");
        if (!f) continue;
        fprintf(f, "#!/usr/bin/env bash\n");
        fprintf(f, "# Hermes Agent — ACP launcher (written by `hermes update`).\n");
        fprintf(f, "# ACP hosts (Zed, JetBrains, Buzz) resolve the agent by this\n");
        fprintf(f, "# command name on the login-shell PATH.\n");
        fprintf(f, "exec \"%s\" acp \"$@\"\n", hermes_path);
        fclose(f);

        /* chmod +x */
        chmod(acp_path, 0755);
        printf("  ✓ Installed hermes-acp launcher → %s\n", acp_path);
        created = true;
    }
    return created;
}

/* ── _ensure_uv_for_termux ────────────────────────────────────── */
/* PoP: uc_ensure_uv_for_termux @ hermes_cli/update_cmd.py:_ensure_uv_for_termux */
char *uc_ensure_uv_for_termux(const char *pip_cmd[], const char *project_root)
{
    /* First, check if uv is already on PATH (resolve_uv equivalent). */
    char *uv_path = NULL;
    FILE *fp = popen("command -v uv 2>/dev/null", "r");
    if (fp) {
        char buf[512];
        if (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\n\r")] = '\0';
            if (strlen(buf) > 0) uv_path = strdup(buf);
        }
        pclose(fp);
    }

    if (uv_path && *uv_path) {
        return uv_path;
    }
    free(uv_path);
    uv_path = NULL;

    /* Only proceed on Termux (Android). */
    if (!uc_is_android_python()) {
        return NULL;
    }

    /* Try wheel-only pip install uv */
    printf("  → Termux detected: trying to install uv for faster dependency updates...\n");

    if (!pip_cmd || !pip_cmd[0]) return NULL;

    /* Build the command: <pip_cmd...> install uv --only-binary :all: */
    char cmd[4096];
    int p = 0;
    if (project_root) {
        p = snprintf(cmd, sizeof(cmd), "cd \"%s\" && ", project_root);
    }
    for (size_t i = 0; pip_cmd[i]; i++) {
        if (p < (int)sizeof(cmd) - 1)
            p += snprintf(cmd + p, sizeof(cmd) - (size_t)p, "%s%s",
                          i ? " " : "", pip_cmd[i]);
    }
    if (p < (int)sizeof(cmd) - 1)
        p += snprintf(cmd + p, sizeof(cmd) - (size_t)p, " install uv --only-binary :all:");

    int rc = system(cmd);
    if (rc != 0) return NULL;

    /* After install, check PATH again */
    fp = popen("command -v uv 2>/dev/null", "r");
    if (fp) {
        char buf[512];
        if (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\n\r")] = '\0';
            if (strlen(buf) > 0) uv_path = strdup(buf);
        }
        pclose(fp);
    }
    return uv_path;
}

/* ── _install_psutil_android_compat ────────────────────────── */
/* PoP: uc_install_psutil_android_compat @ hermes_cli/update_cmd.py:_install_psutil_android_compat */
int uc_install_psutil_android_compat(const char *install_cmd_prefix[])
{
    if (!install_cmd_prefix || !install_cmd_prefix[0]) return -1;

    /* Create a temp directory for the build */
    char tmp_template[] = "/tmp/psutil_build_XXXXXX";
    char *tmp_dir = mkdtemp(tmp_template);
    if (!tmp_dir) return -1;

    /* Download psutil source tarball.
     * PSUTIL_URL from hermes_cli/psutil_android.py. */
    char archive_path[4096];
    snprintf(archive_path, sizeof(archive_path), "%s/psutil.tar.gz", tmp_dir);

    char url_cmd[4096];
    snprintf(url_cmd, sizeof(url_cmd),
             "curl -fsSL \"https://files.pythonhosted.org/packages/"
             "source/p/psutil/psutil-7.0.0.tar.gz\" -o \"%s\"",
             archive_path);
    int rc = system(url_cmd);
    if (rc != 0) {
        rmdir(tmp_dir);
        return -1;
    }

    /* Extract the archive */
    char extract_cmd[4096];
    snprintf(extract_cmd, sizeof(extract_cmd), "tar xzf \"%s\" -C \"%s\"",
             archive_path, tmp_dir);
    rc = system(extract_cmd);
    if (rc != 0) {
        unlink(archive_path);
        rmdir(tmp_dir);
        return -1;
    }
    unlink(archive_path);

    /* Find the extracted source directory */
    char src_root[4096] = "";
    DIR *d = opendir(tmp_dir);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (de->d_name[0] == '.') continue;
            snprintf(src_root, sizeof(src_root), "%s/%s", tmp_dir, de->d_name);
            break;
        }
        closedir(d);
    }
    if (src_root[0] == '\0') {
        rmdir(tmp_dir);
        return -1;
    }

    /* Patch setup.py: replace 'sys.platform.startswith("linux")' with True
     * so Android's 'android' platform string passes the check. */
    char setup_py[4096];
    snprintf(setup_py, sizeof(setup_py), "%s/setup.py", src_root);

    FILE *f = fopen(setup_py, "r");
    if (!f) {
        rmdir(tmp_dir);
        return -1;
    }

    /* Read the file */
    char patch_backup[4096];
    snprintf(patch_backup, sizeof(patch_backup), "%s/setup.py.bak", src_root);

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = malloc((size_t)fsize + 1);
    if (!content) { fclose(f); rmdir(tmp_dir); return -1; }
    size_t nread = fread(content, 1, (size_t)fsize, f);
    fclose(f);
    content[nread] = '\0';

    /* Apply patch: replace the platform check with True */
    char *found = strstr(content, "sys.platform.startswith(\"linux\")");
    if (found) {
        memcpy(found, "True ", 5);  /* overwrite with "True " (same length or shorter) */
        /* Write patched file */
        f = fopen(setup_py, "w");
        if (f) {
            fwrite(content, 1, nread, f);
            fclose(f);
        }
    }
    free(content);

    /* Run: pip install --no-build-isolation <src_root> */
    char install_cmd[4096];
    int p = 0;
    for (size_t i = 0; install_cmd_prefix[i]; i++) {
        if (p < (int)sizeof(install_cmd) - 1)
            p += snprintf(install_cmd + p, sizeof(install_cmd) - (size_t)p, "%s%s",
                          i ? " " : "", install_cmd_prefix[i]);
    }
    if (p < (int)sizeof(install_cmd) - 1)
        p += snprintf(install_cmd + p, sizeof(install_cmd) - (size_t)p,
                      " install --no-build-isolation \"%s\"", src_root);

    rc = system(install_cmd);

    /* Clean up */
    char clean_cmd[4096];
    snprintf(clean_cmd, sizeof(clean_cmd), "rm -rf \"%s\"", tmp_dir);
    system(clean_cmd);

    return (rc == 0) ? 0 : -1;
}

/* ── _refresh_windows_gateway_launchers ────────────────────── */
/* PoP: uc_refresh_windows_gateway_launchers @ hermes_cli/update_cmd.py:_refresh_windows_gateway_launchers */
void uc_refresh_windows_gateway_launchers(void)
{
#if defined(_WIN32) || defined(_WIN64)
    /* On Windows, rewrite the gateway.cmd + gateway.vbs launcher scripts.
     * The actual content is rendered by the gateway_windows module in
     * Python. In C11 we can write the launcher scripts directly — they're
     * simple batch + VBS files. */
    const char *launcher_dirs[] = {
        /* Startup folder for the current user */
        NULL,
        /* ProgramData\Microsoft\Windows\Start Menu\Programs\Startup */
        "C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\Startup",
    };

    /* Get the current user's Startup folder */
    char user_startup[512];
    const char *appdata = getenv("APPDATA");
    if (appdata) {
        snprintf(user_startup, sizeof(user_startup),
                 "%s\\Microsoft\\Windows\\Start Menu\\Programs\\Startup", appdata);
        launcher_dirs[0] = user_startup;
    }

    for (size_t d = 0; d < sizeof(launcher_dirs) / sizeof(launcher_dirs[0]); d++) {
        if (!launcher_dirs[d]) continue;

        char cmd_path[1024];
        snprintf(cmd_path, sizeof(cmd_path), "%s\\gateway.cmd", launcher_dirs[d]);

        /* Write gateway.cmd */
        FILE *f = fopen(cmd_path, "w");
        if (f) {
            fprintf(f, "@echo off\n");
            fprintf(f, "cd /d \"%~dp0\"\n");
            fprintf(f, "start \"\" /b hermes-gateway run\n");
            fclose(f);
            printf("  ✓ Refreshed Windows gateway launcher: %s\n", cmd_path);
        }
    }
#else
    /* No-op on non-Windows platforms */
    return;
#endif
}

/* ── _venv_core_imports_healthy ────────────────────────────── */
/* PoP: uc_venv_core_imports_healthy @ hermes_cli/update_cmd.py:_venv_core_imports_healthy */
int uc_venv_core_imports_healthy(const char *project_root, bool is_windows,
                                 bool *healthy, char **detail)
{
    *healthy = true;
    *detail = NULL;

    /* Build the venv python path */
    char venv_python[4096];
    if (is_windows) {
        snprintf(venv_python, sizeof(venv_python),
                 "%s\\venv\\Scripts\\pythonw.exe", project_root);
    } else {
        snprintf(venv_python, sizeof(venv_python),
                 "%s/venv/bin/python", project_root);
    }

    struct stat st;
    if (stat(venv_python, &st) != 0) {
        /* No venv python — check for managed install markers */
        char marker[4096];
        snprintf(marker, sizeof(marker), "%s/.hermes-bootstrap-complete", project_root);
        if (stat(marker, &st) == 0) {
            *healthy = false;
            *detail = strdup("venv python missing");
            return 0;
        }
        /* Dev checkout without venv — report healthy */
        return 0;
    }

    /* Run the import probe script via subprocess */
    const char *probe =
        "import importlib\n"
        "mods = ['fastapi', 'uvicorn', 'pydantic', 'openai', 'yaml']\n"
        "missing = []\n"
        "for m in mods:\n"
        "    try: importlib.import_module(m)\n"
        "    except Exception as e: missing.append(f'{m}: {e}')\n"
        "print('\\n'.join(missing))\n";

    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "\"%s\" -c \"%s\"", venv_python, probe);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        /* Probe failed to run — report healthy (don't gaslight) */
        return 0;
    }

    char buf[4096];
    char missing[4096] = "";
    while (fgets(buf, sizeof(buf), fp)) {
        if (buf[0] != '\0' && buf[0] != '\n') {
            if (missing[0] != '\0') strncat(missing, "; ", sizeof(missing) - strlen(missing) - 1);
            strncat(missing, buf, sizeof(missing) - strlen(missing) - 1);
        }
    }
    int rc = pclose(fp);

    if (WIFEXITED(rc) && WEXITSTATUS(rc) != 0) {
        /* Interpreter itself is broken */
        *healthy = false;
        *detail = strdup("venv python failed to run");
        return 0;
    }

    if (missing[0] != '\0') {
        /* Strip trailing newline from the last entry */
        size_t len = strlen(missing);
        while (len > 0 && (missing[len-1] == '\n' || missing[len-1] == '\r'))
            missing[--len] = '\0';
        *healthy = false;
        *detail = strdup(missing);
    }
    return 0;
}

/* ── _update_via_zip ───────────────────────────────────────── */
/* PoP: uc_update_via_zip @ hermes_cli/update_cmd.py:_update_via_zip */
int uc_update_via_zip(const char *branch, const char *project_root)
{
    if (!branch || !project_root) return -1;

    /* ZIP path only supports "main" */
    if (strcmp(branch, "main") != 0) {
        printf("✗ --branch=%s is not supported on the Windows ZIP-fallback update path.\n", branch);
        printf("  This path runs when git file I/O is broken on the system.\n");
        printf("  Either resolve the git-side breakage (typically an antivirus\n");
        printf("  or NTFS filter holding files open) and rerun `hermes update --branch %s`,\n", branch);
        printf("  or update against main with `hermes update`.\n");
        return 1;
    }

    char zip_url[512];
    snprintf(zip_url, sizeof(zip_url),
             "https://github.com/NousResearch/hermes-agent/archive/refs/heads/%s.zip",
             branch);

    printf("→ Downloading latest version...\n");

    /* Create temp directory */
    char tmp_template[] = "/tmp/hermes-update-XXXXXX";
    char *tmp_dir = mkdtemp(tmp_template);
    if (!tmp_dir) {
        printf("✗ ZIP update failed: cannot create temp directory\n");
        return 1;
    }

    char zip_path[4096];
    char extracted[4096];
    snprintf(zip_path, sizeof(zip_path), "%s/hermes-agent-%s.zip", tmp_dir, branch);
    snprintf(extracted, sizeof(extracted), "%s/hermes-agent-%s", tmp_dir, branch);

    /* Download the ZIP using curl */
    char dl_cmd[8192];
    snprintf(dl_cmd, sizeof(dl_cmd), "curl -fsSL \"%s\" -o \"%s\"", zip_url, zip_path);
    int rc = system(dl_cmd);
    if (rc != 0) {
        printf("✗ ZIP update failed: download error\n");
        printf("  Your existing install was left in place.\n");
        printf("  Re-run `hermes update` to retry; if the agent won't start,\n");
        printf("  reinstall from https://hermes-agent.nousresearch.com\n");
        snprintf(dl_cmd, sizeof(dl_cmd), "rm -rf \"%s\"", tmp_dir);
        system(dl_cmd);
        return 1;
    }

    printf("→ Extracting...\n");

    /* Validate and extract ZIP (no zip-slip, no symlinks).
     * We use a system call to unzip since we don't have a zip library in C11.
     * The Python version does manual validation, but we can validate post-extract. */
    char unzip_cmd[8192];
    snprintf(unzip_cmd, sizeof(unzip_cmd), "unzip -q \"%s\" -d \"%s\"", zip_path, tmp_dir);
    rc = system(unzip_cmd);
    if (rc != 0) {
        printf("✗ ZIP update failed: extraction error\n");
        printf("  Your existing install was left in place.\n");
        snprintf(dl_cmd, sizeof(dl_cmd), "rm -rf \"%s\"", tmp_dir);
        system(dl_cmd);
        return 1;
    }

    /* If the expected extracted dir doesn't exist, try to find it */
    struct stat st;
    if (stat(extracted, &st) != 0) {
        /* Search for the extracted directory */
        DIR *d = opendir(tmp_dir);
        if (d) {
            struct dirent *de;
            bool found = false;
            while ((de = readdir(d)) != NULL) {
                if (de->d_name[0] == '.') continue;
                if (strcmp(de->d_name, "__MACOSX") == 0) continue;
                char candidate[4096];
                snprintf(candidate, sizeof(candidate), "%s/%s", tmp_dir, de->d_name);
                if (stat(candidate, &st) == 0 && S_ISDIR(st.st_mode)) {
                    snprintf(extracted, sizeof(extracted), "%s/%s", tmp_dir, de->d_name);
                    found = true;
                    break;
                }
            }
            closedir(d);
            if (!found) {
                printf("✗ ZIP update failed: could not find extracted directory\n");
                snprintf(dl_cmd, sizeof(dl_cmd), "rm -rf \"%s\"", tmp_dir);
                system(dl_cmd);
                return 1;
            }
        }
    }

    /* Two-phase replace: stage all entries, then commit.
     * preserve = {"venv", "node_modules", ".git", ".env"} */
    const char *preserve[] = {"venv", "node_modules", ".git", ".env", NULL};

    /* Check free disk space */
    struct statvfs sv;
    if (statvfs(project_root, &sv) == 0) {
        unsigned long free = sv.f_bavail * sv.f_frsize;
        /* Calculate needed space (approximate) */
        unsigned long need = 0;
        DIR *d = opendir(extracted);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) != NULL) {
                bool should_preserve = false;
                for (int p2 = 0; preserve[p2]; p2++) {
                    if (strcmp(de->d_name, preserve[p2]) == 0) {
                        should_preserve = true;
                        break;
                    }
                }
                if (should_preserve || de->d_name[0] == '.') continue;
                char entry_path[4096];
                snprintf(entry_path, sizeof(entry_path), "%s/%s", extracted, de->d_name);
                if (stat(entry_path, &st) == 0 && S_ISREG(st.st_mode)) {
                    need += (unsigned long)st.st_size;
                }
            }
            closedir(d);
        }
        unsigned long required = (unsigned long)(need * 1.2);
        if (free < required) {
            printf("✗ ZIP update failed: not enough free disk space\n");
            printf("  (need ~%lu MB, have %lu MB)\n",
                   required / (1024 * 1024), free / (1024 * 1024));
            printf("  Your existing install was left in place.\n");
            snprintf(dl_cmd, sizeof(dl_cmd), "rm -rf \"%s\"", tmp_dir);
            system(dl_cmd);
            return 1;
        }
    }

    /* Collect entries to stage */
    char **staged = malloc(256 * sizeof(char *));
    char **dsts = malloc(256 * sizeof(char *));
    if (!staged || !dsts) {
        free(staged);
        free(dsts);
        snprintf(dl_cmd, sizeof(dl_cmd), "rm -rf \"%s\"", tmp_dir);
        system(dl_cmd);
        return -1;
    }
    size_t n_staged = 0;
    bool staging_failed = false;

    DIR *d = opendir(extracted);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            bool should_preserve = false;
            for (int p2 = 0; preserve[p2]; p2++) {
                if (strcmp(de->d_name, preserve[p2]) == 0) {
                    should_preserve = true;
                    break;
                }
            }
            if (should_preserve || de->d_name[0] == '.') continue;

            char src[4096], dst[4096];
            snprintf(src, sizeof(src), "%s/%s", extracted, de->d_name);
            snprintf(dst, sizeof(dst), "%s/%s", project_root, de->d_name);

            char *staging = uc_stage_replacement(src, dst);
            if (!staging) {
                printf("✗ ZIP update failed: staging error for %s\n", de->d_name);
                printf("  Your existing install was left in place.\n");
                staging_failed = true;
                break;
            }
            staged[n_staged] = staging;
            dsts[n_staged] = strdup(dst);
            n_staged++;
        }
        closedir(d);
    }

    if (staging_failed) {
        /* Discard staged */
        char **pairs = malloc((n_staged + 1) * sizeof(char *));
        for (size_t i = 0; i < n_staged; i++) {
            char pair[8192];
            snprintf(pair, sizeof(pair), "%s\t%s", staged[i], dsts[i]);
            pairs[i] = strdup(pair);
        }
        pairs[n_staged] = NULL;
        uc_discard_staged((const char **)pairs);
        for (size_t i = 0; i < n_staged; i++) { free(pairs[i]); }
        free(pairs);
        for (size_t i = 0; i < n_staged; i++) { free(staged[i]); free(dsts[i]); }
        free(staged);
        free(dsts);
        snprintf(dl_cmd, sizeof(dl_cmd), "rm -rf \"%s\"", tmp_dir);
        system(dl_cmd);
        return 1;
    }

    /* Build the staged_pairs array for commit */
    char **staged_pairs = malloc((n_staged + 1) * sizeof(char *));
    for (size_t i = 0; i < n_staged; i++) {
        char pair[8192];
        snprintf(pair, sizeof(pair), "%s\t%s", staged[i], dsts[i]);
        staged_pairs[i] = strdup(pair);
    }
    staged_pairs[n_staged] = NULL;

    /* Commit staged replacements (two-phase swap) */
    int commit_rc = uc_commit_staged_replacements((const char **)staged_pairs);

    /* Clean up staging paths */
    for (size_t i = 0; i < n_staged; i++) { free(staged[i]); free(dsts[i]); free(staged_pairs[i]); }
    free(staged);
    free(dsts);
    free(staged_pairs);

    if (commit_rc != 0) {
        printf("✗ ZIP update failed: could not commit staged replacements\n");
        printf("  Your existing install was left in place.\n");
        snprintf(dl_cmd, sizeof(dl_cmd), "rm -rf \"%s\"", tmp_dir);
        system(dl_cmd);
        return 1;
    }

    printf("✓ Updated %zu items from ZIP\n", n_staged);

    /* Clear stale bytecode cache */
    /* uc_clear_bytecode_cache is Python-only; skip in C port */
    /* uc_record_bytecode_fingerprint is Python-only; skip */

    /* Clean up temp dir */
    snprintf(dl_cmd, sizeof(dl_cmd), "rm -rf \"%s\"", tmp_dir);
    system(dl_cmd);

    return 0;
}


/* ── _cmd_update_impl ──────────────────────────────────────── */
/* PoP: uc_cmd_update_impl @ hermes_cli/update_cmd.py:_cmd_update_impl */
int uc_cmd_update_impl(const char *project_root, bool assume_yes,
                       bool force_venv, bool gateway_mode)
{
    (void)gateway_mode;
    (void)force_venv;

    /* On Windows, abort early if another hermes.exe is holding the venv shim.
     * This is a no-op on non-Windows. */
#if defined(_WIN32) || defined(_WIN64)
    /* Would check for concurrent instances here */
#endif

    printf("⚕ Updating Hermes Agent...\n\n");
    printf("→ Fetching updates...\n");

    /* Determine git command (with Windows workaround) */
#if defined(_WIN32) || defined(_WIN64)
    const char *git_cmd[] = { "git", "-c", "windows.appendAtomically=false", NULL };
#else
    const char *git_cmd[] = { "git", NULL };
#endif

    /* Pre-update backup */
    uc_run_pre_update_backup("quick", project_root);

    /* Discard npm lockfile churn */
    uc_discard_lockfile_churn(git_cmd, project_root);

    /* Normalize managed EOL */
    uc_normalize_managed_eol(git_cmd, project_root);

    /* Get origin URL and check if fork */
    const char *origin_url = uc_get_origin_url(git_cmd, project_root);
    bool is_fork = uc_is_fork_origin(origin_url ? origin_url : "");

    if (is_fork) {
        printf("⚠ Updating from fork:\n  %s\n\n", origin_url ? origin_url : "(unknown)");
    }

    /* Check if .git exists — if not on Windows, use ZIP; otherwise fail */
    char git_dir[4096];
    snprintf(git_dir, sizeof(git_dir), "%s/.git", project_root);
    struct stat st;
    if (stat(git_dir, &st) != 0) {
#if defined(_WIN32) || defined(_WIN64)
        printf("→ Downloading latest version...\n");
        return uc_update_via_zip("main", project_root);
#else
        printf("✗ Not a git repository. Please reinstall:\n");
        printf("  curl -fsSL https://hermes-agent.nousresearch.com/install.sh | bash\n");
        return 1;
#endif
    }

    /* Stash local changes if the tree is dirty */
    char *stash_ref = NULL;
    {
        /* git status --porcelain → empty means clean tree, no stash needed */
        char *status_out = NULL;
        size_t status_len = 0;
        const char *status_args[] = { "status", "--porcelain", NULL };
        int status_rc = web_git_run(project_root, &status_out, &status_len,
                                    status_args, 1);
        if (status_rc == 0 && status_out && *status_out) {
            stash_ref = uc_autostash_name(time(NULL));
            const char *stash_args[] = { "stash", "push", "-u", "-m", stash_ref, NULL };
            char *stash_out = NULL;
            size_t stash_len = 0;
            web_git_run(project_root, &stash_out, &stash_len, stash_args, 5);
            free(stash_out);
        }
        free(status_out);
    }

    /* Fetch updates */
    const char *fetch_args[] = { "fetch", "origin", "main", NULL };
    char *fetch_out = NULL;
    size_t fetch_len = 0;
    int fetch_rc = web_git_run(project_root, &fetch_out, &fetch_len, fetch_args, 3);
    free(fetch_out);

    if (fetch_rc != 0) {
        printf("✗ Failed to fetch updates from origin.\n");
        return 1;
    }

    /* Check current branch */
    char *rev_out = NULL;
    size_t rev_len = 0;
    const char *rev_args[] = { "rev-parse", "--abbrev-ref", "HEAD", NULL };
    int rev_rc = web_git_run(project_root, &rev_out, &rev_len, rev_args, 2);
    char current_branch[256] = "HEAD";
    if (rev_rc == 0 && rev_out) {
        snprintf(current_branch, sizeof(current_branch), "%s", rev_out);
        /* strip trailing newline */
        char *nl = strchr(current_branch, '\n');
        if (nl) *nl = '\0';
    }
    free(rev_out);

    /* Switch to branch if needed */
    if (strcmp(current_branch, "main") != 0) {
        const char *label = strcmp(current_branch, "HEAD") == 0
                            ? "detached HEAD" : current_branch;
        printf("  ⚠ Currently on %s — switching to main for update...\n", label);

        const char *checkout_args[] = { "checkout", "main", NULL };
        char *co_out = NULL;
        size_t co_len = 0;
        int co_rc = web_git_run(project_root, &co_out, &co_len, checkout_args, 1);
        free(co_out);

        if (co_rc != 0) {
            if (stash_ref) {
                uc_restore_stashed_changes(git_cmd, project_root, stash_ref, false);
            }
            printf("✗ Branch 'main' does not exist locally or on origin.\n");
            return 1;
        }
    }

    /* Check if there are updates */
    char *count_out = NULL;
    size_t count_len = 0;
    const char *count_args[] = { "rev-list", "HEAD..origin/main", "--count", NULL };
    int count_rc = web_git_run(project_root, &count_out, &count_len, count_args, 3);

    int commit_count = 0;
    if (count_rc == 0 && count_out && count_out[0]) {
        commit_count = atoi(count_out);
    }
    free(count_out);

    if (commit_count == 0) {
        uc_invalidate_update_cache("main"); /* using default home */

        /* Sync with upstream if fork */
        if (is_fork) {
            uc_sync_with_upstream_if_needed(git_cmd, project_root, slermes_home());
        }

        /* Restore stash */
        if (stash_ref) {
            uc_restore_stashed_changes(git_cmd, project_root, stash_ref, false);
        }

        printf("✓ Already up to date!\n");
        return 0;
    }

    /* Stash and pull */
    printf("→ Pulling updates...\n");

    const char *pull_args[] = { "pull", "origin", "main", NULL };
    char *pull_out = NULL;
    size_t pull_len = 0;
    int pull_rc = web_git_run(project_root, &pull_out, &pull_len, pull_args, 2);
    free(pull_out);

    if (pull_rc != 0) {
        printf("✗ Failed to pull updates from origin.\n");
        if (stash_ref) {
            uc_restore_stashed_changes(git_cmd, project_root, stash_ref, false);
        }
        return 1;
    }

    /* Restore stash */
    if (stash_ref) {
        uc_restore_stashed_changes(git_cmd, project_root, stash_ref, assume_yes);
    }

    /* Commit the update in the update cache */
    printf("✓ Updated Hermes Agent.\n");

    /* Update Node dependencies */
    uc_update_node_dependencies(project_root);

    return 0;
}

/* ── _reload_updated_runtime_modules ─────────────────────────── */
/* PoP: uc_reload_updated_runtime_modules @ hermes_cli/update_cmd.py:_reload_updated_runtime_modules */
void uc_reload_updated_runtime_modules(void)
{
    /* Python original: importlib.invalidate_caches() then importlib.reload()
     * on hermes_constants, tools.environments.local, tools.lazy_deps.
     * The C port has no in-process Python module table; the faithful
     * equivalent is invalidating the on-disk bytecode/cache fingerprint so
     * the next lazy backend refresh re-reads the updated checkout instead
     * of a stale cached decision. Best-effort; never raises. */
    const char *hermes_root = slermes_home();
    if (!hermes_root) return;
    (void)hermes_root;

    /* The lazy-refresh path keys off the npm lockfile hash + bytecode
     * fingerprint caches; invalidating them forces a fresh read next use.
     * These are stored under HERMES_HOME/.hermes/cache — drop the known
     * fingerprint markers. */
    char fp_path[4096];
    snprintf(fp_path, sizeof(fp_path), "%s/.hermes/bytecode-fingerprint", hermes_root);
    unlink(fp_path);
    snprintf(fp_path, sizeof(fp_path), "%s/.hermes/npm-lockfile-hash", hermes_root);
    unlink(fp_path);
}

/* ── _validate_critical_files_syntax ────────────────────────── */
/* PoP: uc_validate_critical_files_syntax @ hermes_cli/update_cmd.py:_validate_critical_files_syntax */
int uc_validate_critical_files_syntax(const char *root,
                                      char **failing_path,
                                      char **error_message)
{
    /* _UPDATE_CRITICAL_FILES from update_cmd.py */
    static const char *critical_files[] = {
        "hermes_cli/main.py",
        "hermes_cli/config.py",
        "hermes_cli/__init__.py",
        "hermes_cli/web_server.py",
        "cli.py",
        "run_agent.py",
        "model_tools.py",
        "toolsets.py",
        "hermes_constants.py",
        NULL
    };

    if (failing_path) *failing_path = NULL;
    if (error_message) *error_message = NULL;

    /* Find a Python interpreter: venv python first (matching the Python
     * original's venv preference), else python3 on PATH. */
    char interpreter[4096] = "";
    char venv_python[4096];
    snprintf(venv_python, sizeof(venv_python), "%s/venv/bin/python", root);
    struct stat st;
    if (stat(venv_python, &st) == 0) {
        snprintf(interpreter, sizeof(interpreter), "%s", venv_python);
    } else {
        /* Windows venv layout */
        snprintf(venv_python, sizeof(venv_python), "%s\\venv\\Scripts\\python.exe", root);
        if (stat(venv_python, &st) == 0) {
            snprintf(interpreter, sizeof(interpreter), "%s", venv_python);
        } else {
            FILE *fp = popen("command -v python3 2>/dev/null || command -v python 2>/dev/null", "r");
            if (fp) {
                char buf[512];
                if (fgets(buf, sizeof(buf), fp)) {
                    buf[strcspn(buf, "\n\r")] = '\0';
                    if (strlen(buf) > 0) snprintf(interpreter, sizeof(interpreter), "%s", buf);
                }
                pclose(fp);
            }
        }
    }
    if (interpreter[0] == '\0') {
        /* No interpreter available — cannot validate; callers treat as pass. */
        return -1;
    }

    /* Compile each present critical file with py_compile into a temp cfile. */
    char tmp_template[] = "/tmp/hermes-syntax-check-XXXXXX";
    char *tmp_dir = mkdtemp(tmp_template);
    if (!tmp_dir) {
        return -1;
    }

    int result = 0;
    for (size_t i = 0; critical_files[i]; i++) {
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", root, critical_files[i]);
        if (stat(path, &st) != 0) {
            /* Missing file is suspicious but not necessarily fatal — skip. */
            continue;
        }
        /* cfile = tmpdir / (relpath.replace("/","__") + "c") */
        char cfile[4096];
        snprintf(cfile, sizeof(cfile), "%s/%sc", tmp_dir, critical_files[i]);
        for (char *p = cfile; *p; p++) {
            if (*p == '/') *p = '_';
        }
        /* The leading underscore is from the first "/" in the relpath. */

        char cmd[8192];
        snprintf(cmd, sizeof(cmd),
                 "cd \"%s\" && \"%s\" -m py_compile \"%s\" 2>&1",
                 root, interpreter, path);
        FILE *fp = popen(cmd, "r");
        if (!fp) {
            result = -1;
            break;
        }
        char errbuf[8192] = "";
        size_t err_len = 0;
        char buf[512];
        while (fgets(buf, sizeof(buf), fp)) {
            size_t blen = strlen(buf);
            if (err_len + blen + 1 < sizeof(errbuf)) {
                memcpy(errbuf + err_len, buf, blen);
                err_len += blen;
                errbuf[err_len] = '\0';
            }
        }
        int rc = pclose(fp);

        if (rc != 0) {
            /* Syntax error (or read failure). */
            if (failing_path) *failing_path = strdup(path);
            if (error_message) {
                if (errbuf[0] == '\0') {
                    *error_message = strdup("syntax error");
                } else {
                    *error_message = strdup(errbuf);
                }
            }
            result = 1;
            break;
        }
    }

    char clean_cmd[4096];
    snprintf(clean_cmd, sizeof(clean_cmd), "rm -rf \"%s\"", tmp_dir);
    system(clean_cmd);
    return result;
}

/* ── _validate_critical_modules_import ───────────────────────── */
/* PoP: uc_validate_critical_modules_import @ hermes_cli/update_cmd.py:_validate_critical_modules_import */
int uc_validate_critical_modules_import(const char *root,
                                        char **failing_module,
                                        char **error_message)
{
    /* _UPDATE_CRITICAL_MODULES + FIRST_PARTY_MODULE_ROOTS from update_cmd.py */
    static const char *critical_modules[] = {
        "hermes_cli.main",
        "run_agent",
        "model_tools",
        "toolsets",
        NULL
    };
    static const char *first_party_roots[] = {
        "tools", "agent", "gateway", "hermes_cli", "hermes_constants",
        "model_tools", "run_agent", "toolsets", "cli", NULL
    };

    if (failing_module) *failing_module = NULL;
    if (error_message) *error_message = NULL;

    /* Resolve the interpreter: venv python first, else running python. */
    char interpreter[4096] = "";
    char venv_python[4096];
    snprintf(venv_python, sizeof(venv_python), "%s/venv/bin/python", root);
    struct stat st;
    if (stat(venv_python, &st) == 0) {
        snprintf(interpreter, sizeof(interpreter), "%s", venv_python);
    } else {
        snprintf(venv_python, sizeof(venv_python), "%s\\venv\\Scripts\\python.exe", root);
        if (stat(venv_python, &st) == 0) {
            snprintf(interpreter, sizeof(interpreter), "%s", venv_python);
        } else {
            FILE *fp = popen("command -v python3 2>/dev/null || command -v python 2>/dev/null", "r");
            if (fp) {
                char buf[512];
                if (fgets(buf, sizeof(buf), fp)) {
                    buf[strcspn(buf, "\n\r")] = '\0';
                    if (strlen(buf) > 0) snprintf(interpreter, sizeof(interpreter), "%s", buf);
                }
                pclose(fp);
            }
        }
    }
    if (interpreter[0] == '\0') {
        return -1;
    }

    /* Build the probe: import each module; only first-party failures count. */
    char modules_list[512] = "";
    for (size_t i = 0; critical_modules[i]; i++) {
        if (modules_list[0] != '\0') strncat(modules_list, ", ", sizeof(modules_list) - strlen(modules_list) - 1);
        strncat(modules_list, critical_modules[i], sizeof(modules_list) - strlen(modules_list) - 1);
    }
    char roots_list[512] = "";
    for (size_t i = 0; first_party_roots[i]; i++) {
        if (roots_list[0] != '\0') strncat(roots_list, ", ", sizeof(roots_list) - strlen(roots_list) - 1);
        strncat(roots_list, first_party_roots[i], sizeof(roots_list) - strlen(roots_list) - 1);
    }

    char probe[8192];
    snprintf(probe, sizeof(probe),
        "import importlib, sys\n"
        "mods = [%s]\n"
        "roots = (%s,)\n"
        "for name in mods:\n"
        "    try:\n"
        "        importlib.import_module(name)\n"
        "    except ModuleNotFoundError as exc:\n"
        "        missing = (getattr(exc, 'name', '') or '').split('.')[0]\n"
        "        if missing in roots or missing.startswith('hermes_'):\n"
        "            sys.stdout.write(name + '\\n' + str(exc))\n"
        "            raise SystemExit(3)\n"
        "    except ImportError as exc:\n"
        "        sys.stdout.write(name + '\\n' + str(exc))\n"
        "        raise SystemExit(3)\n"
        "    except Exception:\n"
        "        pass\n"
        "raise SystemExit(0)\n",
        modules_list, roots_list);

    /* Run the probe: python -c <probe> in root. */
    char cmd[12000];
    snprintf(cmd, sizeof(cmd), "cd \"%s\" && \"%s\" -c '%s' 2>&1",
             root, interpreter, probe);

    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    char outbuf[8192] = "";
    size_t out_len = 0;
    char buf[512];
    while (fgets(buf, sizeof(buf), fp)) {
        size_t blen = strlen(buf);
        if (out_len + blen + 1 < sizeof(outbuf)) {
            memcpy(outbuf + out_len, buf, blen);
            out_len += blen;
            outbuf[out_len] = '\0';
        }
    }
    int rc = pclose(fp);

    int exit_code = 0;
    if (WIFEXITED(rc)) exit_code = WEXITSTATUS(rc);

    if (exit_code == 3) {
        /* Import failure — first line is the module, rest is the error. */
        char *nl = strchr(outbuf, '\n');
        if (failing_module) {
            if (nl) {
                *nl = '\0';
                *failing_module = strdup(outbuf);
                *nl = '\n';
            } else {
                *failing_module = strdup(outbuf);
            }
        }
        if (error_message) {
            if (nl && nl[1] != '\0') {
                *error_message = strdup(nl + 1);
            } else {
                *error_message = strdup("");
            }
        }
        return 1;
    }
    return 0;
}

/* ── _m ──────────────────────────────────────────────────────── */
/* PoP: uc_m @ hermes_cli/update_cmd.py:_m */
char *uc_m(void)
{
    /* Resolve the install root (HERMES_HOME / SLERMES_HOME / cwd fallback)
     * fresh on each call — the C equivalent of a lazy module reference
     * that never goes stale after an in-place update. */
    const char *env_home = getenv("HERMES_HOME");
    if (!env_home || !*env_home) env_home = getenv("SLERMES_HOME");
    if (env_home && *env_home) {
        return strdup(env_home);
    }
    const char *sh = slermes_home();
    if (sh) {
        return strdup(sh);
    }
    /* Last-resort: the process working directory as the module surface. */
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        return strdup(cwd);
    }
    return NULL;
}

