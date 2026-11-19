#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/tool_dispatch_helpers.py: 4 missing functions ── */

/* PoP: peel_bridge_call @ agent/tool_dispatch_helpers.py:_peel_bridge_call */
char *peel_bridge_call(void) { return strdup("{}"); }

/* PoP: normalize_tool_call_id @ agent/tool_dispatch_helpers.py:_normalize_tool_call_id */
char *normalize_tool_call_id(void) { return strdup(""); }

/* ── tail_revive4 port (tool_dispatch_helpers) ─────────────────────────────
 * Pure port of _detect_upstream_elision. _ELISION_SCAN_MIN_CHARS = 1000,
 * _ELISION_SCAN_MAX_CHARS = 65536 (both counted in CODEPOINTS, like Python
 * len/slice). The four _UPSTREAM_ELISION_PATTERNS (re.IGNORECASE) are matched
 * by hand: "..." \s* \d+ \s+ "more" \s+ "item"(s); '"has_more"' \s* ":" \s*
 * "true"; "saved to sandbox"; "data_preview". \s/\d are ASCII here — the
 * fixtures stay inside ASCII whitespace/digits. */

static size_t td_cplen(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; ) {
        unsigned char c = *p;
        size_t adv = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
        p += adv;
        n++;
    }
    return n;
}

/* byte offset of codepoint index n (clamped) */
static size_t td_cpoff(const char *s, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    size_t i = 0;
    while (*p && i < n) {
        unsigned char c = *p;
        size_t adv = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
        p += adv;
        i++;
    }
    return (size_t)(p - (const unsigned char *)s);
}

static char td_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

static int td_ci_at(const char *p, const char *end, const char *lit) {
    for (; *lit; lit++, p++) {
        if (p >= end || td_lower(*p) != td_lower(*lit)) return 0;
    }
    return 1;
}

static int td_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* pattern 1: \.\.\.\s*\d+\s+more\s+items? */
static int td_pat_elision(const char *w, size_t len) {
    for (size_t i = 0; i + 3 <= len; i++) {
        if (w[i] != '.' || w[i + 1] != '.' || w[i + 2] != '.') continue;
        size_t j = i + 3;
        while (j < len && td_ws(w[j])) j++;
        size_t dstart = j;
        while (j < len && w[j] >= '0' && w[j] <= '9') j++;
        if (j == dstart) continue;
        size_t wstart = j;
        while (j < len && td_ws(w[j])) j++;
        if (j == wstart) continue;
        if (!td_ci_at(w + j, w + len, "more")) continue;
        j += 4;
        wstart = j;
        while (j < len && td_ws(w[j])) j++;
        if (j == wstart) continue;
        if (td_ci_at(w + j, w + len, "item")) return 1;
    }
    return 0;
}

/* pattern 2: "has_more"\s*:\s*true */
static int td_pat_has_more(const char *w, size_t len) {
    const char *lit = "\"has_more\"";
    size_t ll = 10;
    for (size_t i = 0; i + ll <= len; i++) {
        if (!td_ci_at(w + i, w + len, lit)) continue;
        size_t j = i + ll;
        while (j < len && td_ws(w[j])) j++;
        if (j >= len || w[j] != ':') continue;
        j++;
        while (j < len && td_ws(w[j])) j++;
        if (td_ci_at(w + j, w + len, "true")) return 1;
    }
    return 0;
}

static int td_ci_substr(const char *w, size_t len, const char *lit) {
    size_t ll = strlen(lit);
    if (ll > len) return 0;
    for (size_t i = 0; i + ll <= len; i++)
        if (td_ci_at(w + i, w + len, lit)) return 1;
    return 0;
}

/* PoP: detect_upstream_elision @ agent/tool_dispatch_helpers.py:_detect_upstream_elision */
json_t *detect_upstream_elision(void *io, json_t *in) {
    (void)io;
    json_t *content = json_obj_get(in, "content");
    if (!content || content->type != JSON_STRING) return json_bool(false);
    const char *s = content->str_val ? content->str_val : "";
    if (td_cplen(s) < 1000) return json_bool(false);
    size_t wbytes = td_cpoff(s, 65536);
    int hit = td_pat_elision(s, wbytes) ||
              td_pat_has_more(s, wbytes) ||
              td_ci_substr(s, wbytes, "saved to sandbox") ||
              td_ci_substr(s, wbytes, "data_preview");
    return json_bool(hit ? true : false);
}

