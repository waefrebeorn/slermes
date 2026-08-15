/* Slermes C port — agent/display.py (pure tool-preview / shell-label helpers) */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "hermes_json.h"

/* Forward decl: src/cli/display_core.c (build_tool_preview port). */
char *cli_agent_display__build_tool_preview(const char *tool_name, const char *args_json, int max_len);

/* PoP: agent_display_oneline @ agent/display.py:_oneline */
void agent_display_oneline(const char *text, char *out, size_t outsz)
{
    if (outsz == 0) return;
    size_t o = 0;
    int prev_ws = 1; /* start as if we just saw whitespace so leading space is dropped */
    for (const char *p = text; *p; p++) {
        unsigned char c = *p;
        int ws = (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
        if (ws) {
            prev_ws = 1;
        } else {
            if (prev_ws && o > 0 && o + 1 < outsz) out[o++] = ' '; /* space between words only */
            if (o + 1 < outsz) out[o++] = (char)c;
            prev_ws = 0;
        }
    }
    /* o never has trailing space because we only emit space BEFORE a word */
    out[o] = '\0';
}

/* PoP: agent_display_truncate_preview @ agent/display.py:_truncate_preview */
void agent_display_truncate_preview(const char *text, int max_len, char *out, size_t outsz)
{
    if (outsz == 0) return;
    size_t L = strlen(text);
    if (max_len > 0 && (size_t)L > (size_t)max_len) {
        if (max_len <= 3) {
            size_t n = (size_t)max_len < outsz ? (size_t)max_len : outsz - 1;
            for (size_t i = 0; i < n; i++) out[i] = '.';
            out[n] = '\0';
            return;
        }
        size_t keep = (size_t)max_len - 3;
        if (keep >= outsz) keep = outsz - 1;
        size_t i = 0;
        for (; i < keep && i < L; i++) out[i] = text[i];
        out[i++] = '.'; out[i++] = '.'; out[i++] = '.';
        out[i] = '\0';
        return;
    }
    size_t i = 0;
    for (; text[i] && i + 1 < outsz; i++) out[i] = text[i];
    out[i] = '\0';
}

/* PoP: agent_display_shell_basename @ agent/display.py:_shell_basename */
void agent_display_shell_basename(const char *head, char *out, size_t outsz)
{
    if (outsz == 0) return;
    if (!head || !*head) { out[0] = '\0'; return; }
    const char *slash = strrchr(head, '/');
    const char *base = slash ? slash + 1 : head;
    size_t i = 0;
    for (; base[i] && i + 1 < outsz; i++) out[i] = base[i];
    out[i] = '\0';
}

/* PoP: http @ agent/display.py:prepare_tool_preview */

/* Port of Python prepare_tool_preview: build one canonical compact preview
 * before platform formatting. Rebuilds the uncapped preview from tool args
 * (so an upstream display cap cannot discard its link target), truncates,
 * and extracts a http(s) URL when truncated.
 *
 * Returns malloc'd JSON:
 *   {"text": ..., "truncated": bool, "url": ...|null}
 * Caller frees. */
char *agent_display_prepare_tool_preview(const char *tool_name,
                                         const char *args_json,
                                         const char *fallback,
                                         int max_len)
{
    char *full_text = NULL;

    /* Python: build_tool_preview(tool_name, args, max_len=0) or fallback. */
    if (tool_name && tool_name[0]) {
        full_text = cli_agent_display__build_tool_preview(tool_name, args_json, 0);
    }
    if (!full_text || !full_text[0]) {
        free(full_text);
        full_text = strdup(fallback ? fallback : "");
    }

    /* Python: text = _truncate_preview(full_text, max_len). */
    char text[4096];
    agent_display_truncate_preview(full_text, max_len, text, sizeof(text));
    bool truncated = strcmp(text, full_text) != 0;

    json_t *obj = json_object();
    json_set(obj, "text", json_string(text));
    json_set(obj, "truncated", json_bool(truncated));

    /* Python: url only when truncated and the full text contains a
     * http(s) URL with a netloc. */
    const char *url = NULL;
    char url_buf[2048];
    url_buf[0] = '\0';
    if (truncated) {
        /* _display_url(full_text): dict url/href or first http(s):// URL. */
        const char *scheme = strstr(full_text, "http://");
        const char *scheme2 = strstr(full_text, "https://");
        const char *cand = NULL;
        if (scheme && scheme2) cand = (scheme < scheme2) ? scheme : scheme2;
        else cand = scheme ? scheme : scheme2;
        if (cand) {
            /* validate scheme http/https and netloc non-empty */
            const char *after = strstr(cand, "://") + 3;
            const char *netloc_end = after ? strchr(after, '/') : NULL;
            size_t nl = netloc_end ? (size_t)(netloc_end - after) : strlen(after);
            if (nl > 0) {
                size_t n = strlen(cand);
                if (n >= sizeof(url_buf)) n = sizeof(url_buf) - 1;
                memcpy(url_buf, cand, n);
                url_buf[n] = '\0';
                url = url_buf;
            }
        }
    }
    if (url) json_set(obj, "url", json_string(url));
    else json_set(obj, "url", json_null());

    free(full_text);
    char *out = json_serialize(obj);
    json_free(obj);
    return out;
}
