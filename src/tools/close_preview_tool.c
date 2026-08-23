/*
 * close_preview_tool.c — Faithful C11 port of tools/close_preview_tool.py.
 *
 * Close the Hermes desktop GUI's preview pane, or one of its tabs. Emits
 * ``preview.close`` through the shared desktop_ui bridge; the renderer drops
 * the matching tab — or the whole pane when no url is given.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include "hermes_json.h"
#include "preview_desktop_ui_tools.h"

/* Platform emit seam (see header). */
static desktop_ui_emit_cb g_emit;
static void *g_emit_ctx;
void desktop_ui_set_emitter(desktop_ui_emit_cb cb, void *ctx) { g_emit = cb; g_emit_ctx = ctx; }

/* PoP: tool_error @ tools/registry.py:tool_error */
static char *cp_tool_error(const char *message)
{
    json_t *o = json_object();
    if (!o) return strdup("{\"error\":\"\"}");
    json_set(o, "error", json_string(message ? message : ""));
    char *out = json_serialize(o);
    json_free(o);
    return out;
}

/* PoP: _normalize_target @ tools/open_preview_tool.py:_normalize_target
 *
 * Coax a bare host/domain into a fetchable URL; leave paths + schemes alone.
 * ``www.cnn.com`` -> ``https://www.cnn.com``; ``localhost:3000`` ->
 * ``http://localhost:3000``. File paths and explicit schemes pass through. */
char *preview_normalize_target(const char *raw)
{
    const char *v = raw ? raw : "";
    while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r' || *v == '`') v++;
    size_t n = strlen(v);
    while (n > 0 && (v[n-1]==' '||v[n-1]=='\t'||v[n-1]=='\n'||v[n-1]=='\r'||v[n-1]=='`')) n--;
    char *s = strndup(v, n);
    if (!s) return NULL;

    if (!*s || strstr(s, "://") ||
        s[0] == '/' || strncmp(s, "./", 2) == 0 || strncmp(s, "../", 3) == 0 ||
        s[0] == '~' || strncmp(s, "file:", 5) == 0)
        return s; /* paths and explicit schemes pass through */

    regex_t re;
    /* ^(localhost|127\.0\.0\.1|0\.0\.0\.0|\[::1\])(:\d+)?(/|$) */
    if (regcomp(&re, "^(localhost|127\\.0\\.0\\.1|0\\.0\\.0\\.0|\\[::1\\])(:[0-9]+)?(/|$)",
                REG_EXTENDED | REG_ICASE) == 0) {
        if (regexec(&re, s, 0, NULL, 0) == 0) {
            regfree(&re);
            char *out = malloc(strlen(s) + 8);
            if (!out) { free(s); return NULL; }
            sprintf(out, "http://%s", s);
            free(s);
            return out;
        }
        regfree(&re);
    }
    /* ^[\w.-]+\.[a-z]{2,}(:\d+)?(/.*)?$ */
    if (regcomp(&re, "^[A-Za-z0-9_.-]+\\.[a-z]{2,}(:[0-9]+)?(/.*)?$",
                REG_EXTENDED | REG_ICASE | REG_NOSUB) == 0) {
        if (regexec(&re, s, 0, NULL, 0) == 0) {
            regfree(&re);
            char *out = malloc(strlen(s) + 9);
            if (!out) { free(s); return NULL; }
            sprintf(out, "https://%s", s);
            free(s);
            return out;
        }
        regfree(&re);
    }
    return s;
}

/* PoP: close_preview_tool @ tools/close_preview_tool.py:close_preview_tool */
char *close_preview_tool(const char *url)
{
    char *target = preview_normalize_target(url ? url : "");
    if (!target)
        return cp_tool_error("Failed to close the preview pane.");

    json_t *data = json_object();
    if (!data) { free(target); return cp_tool_error("Failed to close the preview pane."); }
    json_set(data, "url", json_string(target));

    char *err = NULL;
    int ok = g_emit ? g_emit(g_emit_ctx, "preview.close", data, &err) : 0;
    json_free(data);

    if (ok < 0) {
        char buf[512];
        snprintf(buf, sizeof buf, "Failed to close the preview pane: %s",
                 err ? err : "unknown error");
        free(err);
        free(target);
        return cp_tool_error(buf);
    }
    if (!ok) {
        free(target);
        return cp_tool_error("The preview pane is only available in the Hermes desktop app.");
    }

    json_t *res = json_object();
    if (!res) { free(target); return cp_tool_error("alloc failed"); }
    json_set(res, "success", json_bool(true));
    json_set(res, "url", json_string(target));
    free(target);
    char *out = json_serialize(res);
    json_free(res);
    return out;
}
