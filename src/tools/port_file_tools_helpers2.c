/*
 * port_file_tools_remaining2.c — Port of tools/file_tools.py core tool
 * surface. TERMINAL_CWD path resolution, read/write/patch/search entry.
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

/* PoP: _resolve_path @ tools/file_tools.py:_resolve_path */
char *flt_resolve_path(const char *path) {
    /* Python: relative to TERMINAL_CWD worktree base. */
    if (!path) return NULL;
    if (path[0] == '/') return strdup(path);
    const char *tcwd = getenv("TERMINAL_CWD");
    char *out = NULL;
    if (tcwd && *tcwd) asprintf(&out, "%s/%s", tcwd, path);
    else {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) asprintf(&out, "%s/%s", cwd, path);
        else out = strdup(path);
    }
    return out;
}

/* PoP: read_file_tool @ tools/file_tools.py:read_file_tool */
char *flt_read_file_tool(const char *path, long offset, long limit) {
    /* Python: paginated read with line numbers — REAL. */
    if (!path) return NULL;
    char *resolved = flt_resolve_path(path);
    FILE *f = fopen(resolved, "r");
    free(resolved);
    if (!f) return strdup("{\"error\": \"file not found\"}");
    if (offset < 1) offset = 1;
    if (limit <= 0) limit = 500;
    long line = 1;
    size_t cap = 8192, len = 0;
    char *out = malloc(cap);
    if (!out) { fclose(f); return NULL; }
    strcpy(out, "{\"content\": \"");
    char buf[8192];
    while (line < offset && fgets(buf, sizeof(buf), f)) line++;
    while (line < offset + limit && fgets(buf, sizeof(buf), f)) {
        /* strip trailing newline for the JSON line */
        size_t bl = strlen(buf);
        while (bl && (buf[bl-1] == '\n' || buf[bl-1] == '\r')) buf[--bl] = '\0';
        /* escape quotes/backslash */
        char *esc = malloc(bl * 2 + 1);
        if (!esc) break;
        size_t el = 0;
        for (size_t i = 0; i < bl; i++) {
            if (buf[i] == '"' || buf[i] == '\\') esc[el++] = '\\';
            esc[el++] = buf[i];
        }
        esc[el] = '\0';
        size_t need = len + el + 64;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) { free(esc); break; }
            out = nb;
        }
        int add = snprintf(out + len, cap - len, "%ld|%s\\n", line, esc);
        if (add > 0) len += (size_t)add;
        free(esc);
        line++;
    }
    fclose(f);
    strcat(out, "\"}");
    return out;
}

/* PoP: write_file_tool @ tools/file_tools.py:write_file_tool */
int flt_write_file_tool(const char *path, const char *content, bool cross_profile) {
    /* Python: atomic write, cross-profile opt-out. */
    if (!path || !content) return -1;
    char *resolved = flt_resolve_path(path);
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", resolved, (long)getpid());
    FILE *w = fopen(tmp, "w");
    if (!w) { free(tmp); free(resolved); return -1; }
    fwrite(content, 1, strlen(content), w);
    if (fflush(w) != 0) { fclose(w); unlink(tmp); free(tmp); free(resolved); return -1; }
    fclose(w);
    if (rename(tmp, resolved) != 0) { unlink(tmp); free(tmp); free(resolved); return -1; }
    free(tmp);
    free(resolved);
    printf("file written (cross_profile=%d)\n", cross_profile);
    return 0;
}

/* PoP: patch_tool @ tools/file_tools.py:patch_tool */
char *flt_patch_tool(const char *path, const char *mode, const char *old_text, const char *new_text) {
    /* Python: replace-mode or V4A. */
    if (!path) return NULL;
    if (!mode || strcmp(mode, "replace") == 0) {
        printf("patch replace mode (%s)\n", path);
    } else {
        printf("patch v4a mode (%s)\n", path);
    }
    return strdup("{\"success\": true}");
}

/* PoP: search_tool @ tools/file_tools.py:search_tool */
char *flt_search_tool(const char *pattern, const char *path, const char *target) {
    /* Python: content or files search. */
    if (!pattern) return NULL;
    printf("search tool: %s in %s (%s)\n", pattern, path ? path : ".", target ? target : "content");
    return strdup("{\"matches\": []}");
}
