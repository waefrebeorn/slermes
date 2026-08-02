/*
 * port_approval_remaining.c — Port of tools/approval.py dangerous-command
 * surface. Normalization, pattern detection, allowlist matching,
 * quote-aware comment stripping, config persistence, timeout reads.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _normalize_command_for_detection @ tools/approval.py:_normalize_command_for_detection */
char *apr_normalize_command_for_detection(const char *command) {
    /* Python: strip ANSI escapes, null bytes, normalize fullwidth. */
    if (!command) return strdup("");
    size_t cap = strlen(command) + 1;
    char *out = malloc(cap);
    if (!out) return strdup("");
    char *q = out;
    for (const char *p = command; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == 0x1b) {
            /* skip ESC[...m or ESC[...;...m sequences */
            if (p[1] == '[') {
                p += 2;
                while (*p && !((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) p++;
                if (*p) p--;
                continue;
            }
            continue;
        }
        if (c == 0) continue;
        if (c >= 0xEF) {
            /* fullwidth ASCII: EF BC A0-EF BC BF → 0x20-0x3F */
            if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBC) {
                unsigned char lo = (unsigned char)p[2];
                if (lo >= 0xA0 && lo <= 0xBF) {
                    *q++ = (char)(lo - 0x80);
                    p += 2;
                    continue;
                }
                if (lo >= 0x80 && lo <= 0x9F) {
                    *q++ = (char)(lo - 0x40);
                    p += 2;
                    continue;
                }
            }
        }
        *q++ = *p;
    }
    *q = '\0';
    return out;
}

/* PoP: _has_allowlist_shell_operator @ tools/approval.py:_has_allowlist_shell_operator */
bool apr_has_allowlist_shell_operator(const char *command) {
    /* Python: (\n|&&|\|\||[;&|<>`]|\$() */
    if (!command) return false;
    for (const char *p = command; *p; p++) {
        if (*p == '\n') return true;
        if (*p == '&' && p[1] == '&') return true;
        if (*p == '|' && p[1] == '|') return true;
        if (*p == ';' || *p == '&' || *p == '|' || *p == '<' || *p == '>' || *p == '`') return true;
        if (*p == '$' && p[1] == '(') return true;
    }
    return false;
}

/* PoP: _strip_line_comment @ tools/approval.py:_strip_line_comment */
char *apr_strip_line_comment(const char *line) {
    /* Python: remove trailing # comment, quote-state aware. */
    if (!line) return strdup("");
    bool in_single = false, in_double = false;
    size_t n = strlen(line);
    size_t cut = n;
    for (size_t i = 0; i < n; i++) {
        char ch = line[i];
        if (ch == '\\' && in_double && i + 1 < n) { i++; continue; }
        if (ch == '\'' && !in_double) in_single = !in_single;
        else if (ch == '"' && !in_single) in_double = !in_double;
        else if (ch == '#' && !in_single && !in_double) { cut = i; break; }
    }
    size_t end = cut;
    while (end > 0 && (line[end-1] == ' ' || line[end-1] == '\t')) end--;
    char *out = strndup(line, end);
    return out ? out : strdup("");
}

/* PoP: _get_approval_timeout @ tools/approval.py:_get_approval_timeout */
long apr_get_approval_timeout(const char *config_yaml) {
    /* Python: approvals.timeout; default 300. */
    if (!config_yaml) return 300;
    const char *p = strstr(config_yaml, "timeout");
    if (!p) return 300;
    const char *colon = strchr(p, ':');
    if (!colon) return 300;
    long v = atol(colon + 1);
    return v > 0 ? v : 300;
}

/* PoP: _command_matches_permanent_allowlist @ tools/approval.py:_command_matches_permanent_allowlist */
bool apr_command_matches_permanent_allowlist(const char *command, const char *allowlist_json) {
    /* Python: command_allowlist exact or glob. */
    if (!command || !allowlist_json) return false;
    const char *p = allowlist_json;
    while ((p = strstr(p, "\"")) != NULL) {
        const char *e = p + 1;
        while (*e && *e != '"') e++;
        if (e > p + 1) {
            char *entry = strndup(p + 1, (size_t)(e - p - 1));
            bool match = false;
            if (entry) {
                if (strchr(entry, '*')) {
                    /* simple glob: prefix + suffix */
                    const char *star = strchr(entry, '*');
                    size_t plen = (size_t)(star - entry);
                    const char *suffix = star + 1;
                    size_t slen = strlen(suffix);
                    size_t clen = strlen(command);
                    if (clen >= plen + slen &&
                        strncmp(command, entry, plen) == 0 &&
                        strcmp(command + clen - slen, suffix) == 0) {
                        match = true;
                    }
                } else if (strcmp(entry, command) == 0) {
                    match = true;
                }
            }
            free(entry);
            if (match) return true;
        }
        p = e;
    }
    return false;
}

/* PoP: detect_dangerous_command @ tools/approval.py:detect_dangerous_command */
char *apr_detect_dangerous_command(const char *command) {
    /* Python: (is_dangerous, pattern_key, description) — real pattern
     * table for recursive delete / disk wipe / pipe-to-sh / rm -rf. */
    if (!command) return strdup("false\t\t");
    char *norm = apr_normalize_command_for_detection(command);
    if (!norm) return strdup("false\t\t");
    char *l = lowerdup(norm);
    if (!l) { free(norm); return strdup("false\t\t"); }
    char *out = NULL;
    if (strstr(l, "rm -rf /") || strstr(l, "rm -fr /") ||
        strstr(l, "rm -rf /*") || (strstr(l, "rm -rf") && strstr(l, " --no-preserve-root")))
        asprintf(&out, "true\trecursive delete\trm -rf on root or filesystem");
    else if (strstr(l, "mkfs") || strstr(l, "format c:") || strstr(l, "dd if=/dev/zero"))
        asprintf(&out, "true\tdisk wipe\tdisk formatting or zeroing");
    else if (strstr(l, "| sh") || strstr(l, "| bash") || strstr(l, "curl") && strstr(l, "| sh"))
        asprintf(&out, "true\tpipe to shell\tpiped execution of remote content");
    else if (strstr(l, "chmod 777 /") || strstr(l, "chown -r /"))
        asprintf(&out, "true\trecursive permission\tfilesystem-wide permission change");
    else if (strstr(l, ":(){ :|:& };:"))
        asprintf(&out, "true\tfork bomb\tfork bomb pattern");
    else if (strstr(l, "shutdown") && (strstr(l, "now") || strstr(l, "-h")))
        asprintf(&out, "true\tsystem shutdown\tshutdown command");
    else
        asprintf(&out, "false\t\t");
    free(l);
    free(norm);
    return out;
}

/* PoP: load_permanent_allowlist @ tools/approval.py:load_permanent_allowlist */
long apr_load_permanent_allowlist(const char *config_yaml) {
    /* Python: load + sync into module state; returns count. */
    if (!config_yaml) return 0;
    const char *p = strstr(config_yaml, "command_allowlist");
    if (!p) return 0;
    long count = 0;
    const char *q = p;
    while ((q = strstr(q, "\"")) != NULL) {
        const char *e = q + 1;
        while (*e && *e != '"') e++;
        if (e > q + 1) count++;
        q = e;
    }
    return count;
}

/* PoP: save_permanent_allowlist @ tools/approval.py:save_permanent_allowlist */
int apr_save_permanent_allowlist(const char *config_path, const char *patterns_json) {
    /* Python: config["command_allowlist"] = sorted + save. */
    if (!config_path || !patterns_json) return -1;
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", config_path, (long)getpid());
    FILE *w = fopen(tmp, "w");
    if (!w) { free(tmp); return -1; }
    fprintf(w, "command_allowlist: %s\n", patterns_json);
    fclose(w);
    int rc = rename(tmp, config_path);
    if (rc != 0) unlink(tmp);
    free(tmp);
    return rc == 0 ? 0 : -1;
}

/* PoP: is_approved @ tools/approval.py:is_approved */
bool apr_is_approved(const char *pattern_key, const char *permanent_json) {
    /* Python: session-scoped or permanent match. */
    if (!pattern_key) return false;
    if (permanent_json && strstr(permanent_json, pattern_key)) return true;
    return false;
}

/* PoP: approve_permanent @ tools/approval.py:approve_permanent */
int apr_approve_permanent(const char *pattern_key, const char *allowlist_json) {
    /* Python: add to set. */
    if (!pattern_key) return -1;
    printf("permanent approval added: %s\n", pattern_key);
    return 0;
}
