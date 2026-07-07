/*
 * port_hermes_cli_kanban_helpers.c — C port of selected CLI-arg/time
 * helpers from hermes_cli/kanban.py.
 *
 * Only dependency-light, faithful re-implementations are ported here.
 * DB-coupled _cmd_* commands are deferred.
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>

/* PoP: _fmt_ts @ hermes_cli/kanban.py:_fmt_ts */
char *fmt_kanban_ts(long ts)
{
    if (!ts) return strdup("");
    time_t t = (time_t)ts;
    struct tm *tm = localtime(&t);
    char b[32];
    strftime(b, sizeof(b), "%Y-%m-%d %H:%M", tm);
    return strdup(b);
}

/* PoP: _parse_duration @ hermes_cli/kanban.py:_parse_duration
 * Parse "30s"/"5m"/"2h"/"1d" or a raw integer -> seconds.
 * Returns seconds, or -1 on malformed input (error copied to *err if non-NULL). */
long parse_kanban_duration(const char *val, char *err, size_t errsz)
{
    if (err) err[0] = '\0';
    if (!val || val[0] == '\0') return -1; /* empty: caller treats as None */
    char buf[64];
    size_t n = 0;
    for (const char *p = val; *p && n + 1 < sizeof(buf); p++) buf[n++] = tolower((unsigned char)*p);
    buf[n] = '\0';
    /* Bare integer -> seconds. */
    char *end = NULL;
    long bare = strtol(buf, &end, 10);
    if (*end == '\0') return bare;
    /* Suffixed form. */
    static const char *units = "smhd";
    long mult[4] = {1, 60, 3600, 86400};
    char suf = buf[n-1];
    int ui = -1;
    for (int i = 0; i < 4; i++) if (units[i] == suf) { ui = i; break; }
    if (ui < 0) {
        if (err) snprintf(err, errsz, "malformed duration %s (expected 30s, 5m, 2h, 1d, or a number)", val);
        return -1;
    }
    buf[n-1] = '\0';
    char *e2 = NULL;
    double num = strtod(buf, &e2);
    if (*e2 != '\0') {
        if (err) snprintf(err, errsz, "malformed duration %s", val);
        return -1;
    }
    return (long)(num * mult[ui]);
}

/* PoP: _parse_workspace_flag @ hermes_cli/kanban.py:_parse_workspace_flag
 * Parse --workspace into (kind, path|NULL).
 * Returns 0 on success (fills *kind_out, *path_out — malloc'd), -1 on error
 * (err filled). Caller frees kind_out/path_out. */
int parse_kanban_workspace_flag(const char *value, char **kind_out, char **path_out, char *err, size_t errsz)
{
    if (kind_out) *kind_out = NULL;
    if (path_out) *path_out = NULL;
    if (err) err[0] = '\0';
    if (!value || !value[0]) { if (kind_out) *kind_out = strdup("scratch"); return 0; }
    char *v = strdup(value);
    /* strip */
    char *p = v;
    while (*p==' '||*p=='\t') p++;
    size_t L = strlen(p);
    while (L>0 && (p[L-1]==' '||p[L-1]=='\t')) p[--L]=0;

    if (strcmp(p, "scratch") == 0 || strcmp(p, "worktree") == 0) {
        if (kind_out) *kind_out = strdup(p);
        free(v); return 0;
    }
    struct { const char *pre; const char *kind; } prefixes[] = {{"dir:", "dir"}, {"worktree:", "worktree"}};
    for (int i = 0; i < 2; i++) {
        size_t pl = strlen(prefixes[i].pre);
        if (strncmp(p, prefixes[i].pre, pl) != 0) continue;
        char *path = p + pl;
        while (*path==' '||*path=='\t') path++;
        if (!*path) {
            if (err) snprintf(err, errsz, "--workspace %s requires a path after the colon", prefixes[i].pre);
            free(v); return -1;
        }
        /* expand ~ */
        char expanded[PATH_MAX];
        if (path[0]=='~') snprintf(expanded, sizeof(expanded), "%s%s", getenv("HOME")?getenv("HOME"):"", path+1);
        else snprintf(expanded, sizeof(expanded), "%s", path);
        if (kind_out) *kind_out = strdup(prefixes[i].kind);
        if (path_out) *path_out = strdup(expanded);
        free(v); return 0;
    }
    if (err) snprintf(err, errsz, "unknown --workspace value %s: use scratch, worktree, worktree:<path>, or dir:<path>", value);
    free(v);
    return -1;
}

/* PoP: _parse_branch_flag @ hermes_cli/kanban.py:_parse_branch_flag
 * Normalize an optional branch name. Returns malloc'd string, or NULL on
 * error (err filled). */
char *parse_kanban_branch_flag(const char *value, char *err, size_t errsz)
{
    if (err) err[0] = '\0';
    if (value == NULL) return NULL;
    char *b = strdup(value);
    char *p = b;
    while (*p==' '||*p=='\t') p++;
    size_t L = strlen(p);
    while (L>0 && (p[L-1]==' '||p[L-1]=='\t')) p[--L]=0;
    if (L == 0) { if (err) snprintf(err, errsz, "--branch requires a non-empty name"); free(b); return NULL; }
    if (p[0]=='-') { if (err) snprintf(err, errsz, "--branch must not start with '-'"); free(b); return NULL; }
    for (char *q=p; *q; q++) if (*q==' '||*q=='\t') { if (err) snprintf(err, errsz, "--branch must not contain whitespace"); free(b); return NULL; }
    char *r = strdup(p);
    free(b);
    return r;
}
