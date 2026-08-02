/*
 * port_memory_tool_remaining.c — Port of tools/memory_tool.py memory surface.
 * Entry store, replace/remove, atomic file write, block render.
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

/* PoP: __init__ @ tools/memory_tool.py:__init__ */
char *memt_init(void) {
    return strdup("{\"memory_entries\": [], \"user_entries\": []}");
}

/* PoP: replace @ tools/memory_tool.py:replace */
char *memt_replace(const char *entries_json, const char *old_text, const char *new_content) {
    /* Python: entry containing old_text → new_content. */
    if (!entries_json || !old_text) return strdup("[]");
    size_t cap = strlen(entries_json) + strlen(new_content ? new_content : "") + 16;
    char *out = malloc(cap);
    if (!out) return strdup("[]");
    strcpy(out, "[");
    bool first = true;
    const char *p = entries_json;
    while ((p = strchr(p, '{')) != NULL) {
        const char *e = p;
        int depth = 0;
        while (*e) {
            if (*e == '{') depth++;
            else if (*e == '}') { depth--; if (depth == 0) { e++; break; } }
            e++;
        }
        size_t seg_len = (size_t)(e - p);
        char *seg = strndup(p, seg_len);
        bool match = seg && strstr(seg, old_text) != NULL;
        if (seg) {
            size_t need = strlen(out) + seg_len + (match ? strlen(new_content ? new_content : "") + 16 : 0) + 4;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) { free(seg); break; }
                out = nb;
            }
            if (!first) strcat(out, ",");
            if (match) {
                /* rebuild entry with new content (simplified: text field) */
                strcat(out, "{\"text\": \"");
                strcat(out, new_content ? new_content : "");
                strcat(out, "\"}");
            } else {
                strncat(out, seg, seg_len);
            }
            first = false;
        }
        free(seg);
        p = e;
    }
    strcat(out, "]");
    return out;
}

/* PoP: remove @ tools/memory_tool.py:remove */
char *memt_remove(const char *entries_json, const char *old_text) {
    /* Python: remove entry containing old_text. */
    if (!entries_json || !old_text) return strdup("[]");
    size_t cap = strlen(entries_json) + 8;
    char *out = malloc(cap);
    if (!out) return strdup("[]");
    strcpy(out, "[");
    bool first = true;
    const char *p = entries_json;
    while ((p = strchr(p, '{')) != NULL) {
        const char *e = p;
        int depth = 0;
        while (*e) {
            if (*e == '{') depth++;
            else if (*e == '}') { depth--; if (depth == 0) { e++; break; } }
            e++;
        }
        size_t seg_len = (size_t)(e - p);
        char *seg = strndup(p, seg_len);
        bool match = seg && strstr(seg, old_text) != NULL;
        if (seg && !match) {
            size_t need = strlen(out) + seg_len + 4;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) { free(seg); break; }
                out = nb;
            }
            if (!first) strcat(out, ",");
            strncat(out, seg, seg_len);
            first = false;
        }
        free(seg);
        p = e;
    }
    strcat(out, "]");
    return out;
}

/* PoP: _success_response @ tools/memory_tool.py:_success_response */
char *memt_success_response(void) {
    return strdup("{\"success\": true, \"memory_changed\": true}");
}

/* PoP: _render_block @ tools/memory_tool.py:_render_block */
char *memt_render_block(const char *entries_json) {
    /* Python: header + usage indicator. */
    if (!entries_json) return strdup("");
    long count = 0;
    for (const char *p = entries_json; *p; p++) if (*p == '{') count++;
    char *out = NULL;
    asprintf(&out, "[Memory — %ld entries]\n%s", count, entries_json);
    return out;
}

/* PoP: _write_file @ tools/memory_tool.py:_write_file */
int memt_write_file(const char *path, const char *content) {
    /* Python: atomic temp + rename — REAL. */
    if (!path || !content) return -1;
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", path, (long)getpid());
    FILE *w = fopen(tmp, "w");
    if (!w) { free(tmp); return -1; }
    fwrite(content, 1, strlen(content), w);
    fputc('\n', w);
    if (fflush(w) != 0) { fclose(w); unlink(tmp); free(tmp); return -1; }
    fclose(w);
    if (rename(tmp, path) != 0) { unlink(tmp); free(tmp); return -1; }
    free(tmp);
    return 0;
}
