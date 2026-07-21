/*
 * gw_format.c — Gateway message-formatting helpers (faithful port of the
 * formatting cluster formerly in gateway/server.c: E30–E43 markdown/HTML
 * conversion, MarkdownV2 escaping, plain-text stripping, word-boundary
 * truncation).
 *
 * Pure string transforms — no gateway globals, no I/O. Self-contained module;
 * public API declared in include/hermes_gateway.h.
 */

#include "hermes_core_types.h"
#include "hermes_gateway_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* E30: Strip markdown for plain-text platforms. Static: only used by
 * gw_strip_all_formatting below. */
static char *gw_strip_markdown(const char *text, bool strip_code, bool strip_bold,
                                bool strip_italic) {
    if (!text) return NULL;
    /* Simple in-place markdown stripping. Allocates for worst case. */
    char *out = (char *)malloc(strlen(text) + 1);
    if (!out) return NULL;
    int j = 0;
    for (int i = 0; text[i]; i++) {
        if (text[i] == '`' && strip_code) continue;
        if (text[i] == '*' && strip_bold) {
            /* Skip ** */
            if (text[i+1] == '*') i++;
            continue;
        }
        if (text[i] == '_' && strip_italic) continue;
        if (text[i] == '~' && text[i+1] == '~') { i++; continue; } /* strikethrough ~~ */
        if (text[i] == '#' && (i == 0 || text[i-1] == '\n')) continue; /* headers */
        if (text[i] == '>') { /* block quotes */
            if (i == 0 || text[i-1] == '\n') continue;
        }
        out[j++] = text[i];
    }
    out[j] = '\0';
    return out;
}

/* E40: Convert markdown to HTML for platforms that support it.
 * Simple conversion: **bold** -> <b>bold</b>, *italic* -> <i>italic</i>,
 * `code` -> <code>code</code> */
char *gw_markdown_to_html(const char *text) {
    if (!text) return NULL;
    char *out = (char *)malloc(strlen(text) * 2 + 1);
    if (!out) return NULL;
    int j = 0;
    for (int i = 0; text[i]; i++) {
        if (text[i] == '*' && text[i+1] == '*') {
            out[j++] = '<'; out[j++] = 'b'; out[j++] = '>';
            i++;
            while (text[i+1] && !(text[i+1] == '*' && text[i+2] == '*')) {
                out[j++] = text[++i];
            }
            out[j++] = '<'; out[j++] = '/'; out[j++] = 'b'; out[j++] = '>';
            i += 2;
        } else if (text[i] == '*' && text[i+1] != '*') {
            out[j++] = '<'; out[j++] = 'i'; out[j++] = '>';
            i++;
            while (text[i] && text[i] != '*') {
                out[j++] = text[i++];
            }
            out[j++] = '<'; out[j++] = '/'; out[j++] = 'i'; out[j++] = '>';
        } else if (text[i] == '`') {
            out[j++] = '<'; out[j++] = 'c'; out[j++] = 'o'; out[j++] = 'd';
            out[j++] = 'e'; out[j++] = '>';
            i++;
            while (text[i] && text[i] != '`') {
                if (text[i] == '\\' && text[i+1] == '`') i++;
                out[j++] = text[i++];
            }
            out[j++] = '<'; out[j++] = '/'; out[j++] = 'c'; out[j++] = 'o';
            out[j++] = 'd'; out[j++] = 'e'; out[j++] = '>';
        } else {
            /* Escape HTML entities */
            if (text[i] == '<') { out[j++] = '&'; out[j++] = 'l'; out[j++] = 't'; out[j++] = ';'; }
            else if (text[i] == '>') { out[j++] = '&'; out[j++] = 'g'; out[j++] = 't'; out[j++] = ';'; }
            else if (text[i] == '&') { out[j++] = '&'; out[j++] = 'a'; out[j++] = 'm'; out[j++] = 'p'; out[j++] = ';'; }
            else out[j++] = text[i];
        }
    }
    out[j] = '\0';
    return out;
}

/* E41: Telegram MarkdownV2 escaping — escape reserved chars */
char *gw_markdown_v2_escape(const char *text) {
    if (!text) return NULL;
    char *out = (char *)malloc(strlen(text) * 2 + 1);
    if (!out) return NULL;
    int j = 0;
    for (int i = 0; text[i]; i++) {
        /* Characters that need escaping in MarkdownV2: _ * [ ] ( ) ~ ` > # + - = | { } . ! */
        if (strchr("_*[]()~`>#+-=|{}.!", text[i])) {
            out[j++] = '\\';
        }
        out[j++] = text[i];
    }
    out[j] = '\0';
    return out;
}

/* E42: Strip all formatting for plain text platforms */
char *gw_strip_all_formatting(const char *text) {
    return gw_strip_markdown(text, true, true, true);
}

/* E43: Smart message truncation with ellipsis.
 * Truncates at word boundary if possible. */
char *gw_truncate_message(const char *text, size_t max_len) {
    if (!text || max_len == 0) return NULL;
    size_t len = strlen(text);
    if (len <= max_len) return strdup(text);

    char *out = (char *)malloc(max_len + 4);
    if (!out) return NULL;
    memcpy(out, text, max_len);

    /* Try to break at word boundary (space) */
    int break_at = (int)max_len;
    while (break_at > 0 && out[break_at - 1] != ' ') break_at--;

    if (break_at > (int)max_len / 2) {
        out[break_at] = '\0';
        strcat(out, "...");
    } else {
        out[max_len] = '\0';
        strcat(out, "...");
    }
    return out;
}
