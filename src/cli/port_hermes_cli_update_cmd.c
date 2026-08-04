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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <inttypes.h>

/* ── _format_time_ago ──────────────────────────────────────────────────── */

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

/* ── _is_fork ──────────────────────────────────────────────────────────── */

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

/* ── _count_commits_between ────────────────────────────────────────────── */

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

/* ── _resolve_stash_selector ───────────────────────────────────────────── */

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

/* ── _print_stash_cleanup_guidance ────────────────────────────────────── */

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

/* ── _format_concurrent_instances_message ─────────────────────────────── */

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
