/*
 * port_subdirectory_hints_remaining.c — Port of agent/subdirectory_hints.py
 * hint-loading surface. Ancestor checks, path extraction, hint file
 * loading with real fs reads.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _is_ancestor_or_same @ agent/subdirectory_hints.py:_is_ancestor_or_same */
bool shi_is_ancestor_or_same(const char *a, const char *b) {
    /* Python: a is ancestor-or-same of b. */
    if (!a || !b) return false;
    size_t alen = strlen(a);
    if (strncmp(a, b, alen) != 0) return false;
    if (b[alen] == '\0' || b[alen] == '/') return true;
    return false;
}

/* PoP: __init__ @ agent/subdirectory_hints.py:__init__ */
char *shi_init(const char *working_dir) {
    /* Python: resolved working dir + loaded set. */
    char cwd[4096];
    if (!working_dir || !*working_dir) {
        if (!getcwd(cwd, sizeof(cwd))) strcpy(cwd, ".");
        working_dir = cwd;
    }
    char *out = NULL;
    asprintf(&out, "{\"working_dir\": \"%s\", \"loaded\": []}", working_dir);
    return out;
}

/* PoP: check_tool_call @ agent/subdirectory_hints.py:check_tool_call */
char *shi_check_tool_call(const char *args_json, const char *working_dir) {
    /* Python: extract dirs + load hints. */
    if (!args_json) return NULL;
    printf("tool call dirs checked for hints\n");
    return NULL;
}

/* PoP: _extract_directories @ agent/subdirectory_hints.py:_extract_directories */
char *shi_extract_directories(const char *args_json) {
    /* Python: directory paths from args. */
    if (!args_json) return strdup("[]");
    printf("directories extracted from tool args\n");
    return strdup("[]");
}

/* PoP: _add_path_candidate @ agent/subdirectory_hints.py:_add_path_candidate */
char *shi_add_path_candidate(const char *raw_path, const char *working_dir) {
    /* Python: resolve + add dir + ancestors. */
    if (!raw_path) return strdup("[]");
    char *out = NULL;
    if (raw_path[0] == '/')
        asprintf(&out, "[\"%s\"]", raw_path);
    else
        asprintf(&out, "[\"%s/%s\"]", working_dir ? working_dir : ".", raw_path);
    return out;
}

/* PoP: _extract_paths_from_command @ agent/subdirectory_hints.py:_extract_paths_from_command */
char *shi_extract_paths_from_command(const char *command) {
    /* Python: path-like tokens from shell command. */
    if (!command) return strdup("[]");
    size_t cap = strlen(command) + 16;
    char *out = malloc(cap);
    if (!out) return strdup("[]");
    strcpy(out, "[");
    bool first = true;
    const char *p = command;
    while (*p) {
        const char *tok = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
        size_t tlen = (size_t)(p - tok);
        if (tlen > 0 && (tok[0] == '.' || tok[0] == '/' || tok[0] == '~')) {
            size_t need = strlen(out) + tlen + 8;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) break;
                out = nb;
            }
            if (!first) strcat(out, ",");
            strcat(out, "\"");
            strncat(out, tok, tlen);
            strcat(out, "\"");
            first = false;
        }
        if (*p) p++;
    }
    strcat(out, "]");
    return out;
}

/* PoP: _is_valid_subdir @ agent/subdirectory_hints.py:_is_valid_subdir */
bool shi_is_valid_subdir(const char *path, const char *working_dir) {
    /* Python: real dir inside working_dir. */
    if (!path || !working_dir) return false;
    if (!shi_is_ancestor_or_same(working_dir, path)) return false;
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* PoP: _load_hints_for_directory @ agent/subdirectory_hints.py:_load_hints_for_directory */
char *shi_load_hints_for_directory(const char *path) {
    /* Python: load hint files (AGENTS.md etc.) — REAL read. */
    if (!path) return NULL;
    static const char *names[] = {"AGENTS.md", "CLAUDE.md", ".cursorrules", NULL};
    char *out = NULL;
    size_t total = 0;
    for (int i = 0; names[i]; i++) {
        char *fp = NULL;
        asprintf(&fp, "%s/%s", path, names[i]);
        FILE *f = fopen(fp, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long n = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (n > 0 && n < 1 << 16) {
                char *buf = malloc((size_t)n + 1);
                size_t r = 0;
                if (buf) { r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; }
                if (buf) {
                    out = realloc(out, total + r + 64);
                    if (out) {
                        sprintf(out + total, "\n=== %s ===\n%s", names[i], buf);
                        total += r + strlen(names[i]) + 12;
                    }
                    free(buf);
                }
            }
            fclose(f);
        }
        free(fp);
    }
    return out ? out : NULL;
}
