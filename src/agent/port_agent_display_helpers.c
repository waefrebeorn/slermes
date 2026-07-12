/* Slermes C port — agent/display.py (pure tool-preview / shell-label helpers) */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* PoP: _oneline @ agent/display.py:_oneline */
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

/* PoP: _truncate_preview @ agent/display.py:_truncate_preview */
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

/* PoP: _shell_basename @ agent/display.py:_shell_basename */
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
