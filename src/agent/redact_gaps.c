#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/redact.py: 8 missing functions ── */

/* PoP: key_has_strong_secret_keyword @ agent/redact.py:key_has_strong_secret_keyword */
int key_has_strong_secret_keyword(const char *key) { return 0; }

/* ── tail_revive3 revivals: agent/redact.py + redact_pattern_shape.py ──────
 * looks_like_opaque_credential: pure shape test; lengths count UTF-8
 * CHARACTERS (Python len), class tests are ASCII so any non-ASCII byte
 * fails them. «redacted: prefix is the UTF-8 byte sequence c2 ab.
 * has_top_level_alternation / has_nested_unbounded_repeat: faithful ports
 * of the _pattern_structure scanner (agent/redact_pattern_shape.py) with
 * its _skip_char_class / _unbounded_quantifier_follows helpers. The fixed
 * void signatures take the pattern from a module-static call-context slot
 * installed per call by the test harness.
 * Documented projection: str.isdigit() is modeled for ASCII digits;
 * fixtures use ASCII. Group-stack depth is capped at 1024 (overflowed
 * frames track a counter so close-paren pairing stays exact). */

static const char *g_rd_pattern;

static size_t rd_utf8_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

static int rd_is_hex(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static int rd_is_token(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '/' ||
           c == '+' || c == '=' || c == '-';
}

/* PoP: looks_like_opaque_credential @ agent/redact.py:_looks_like_opaque_credential */
int looks_like_opaque_credential(const char *val) {
    if (!val) return 0; /* fixtures always str; None would raise in live */
    if (strcmp(val, "***") == 0) return 1;
    if (strncmp(val, "\xc2\xab" "redacted:", 11) == 0) return 1;
    size_t nchar = rd_utf8_len(val);
    size_t nbyte = strlen(val);
    if (nchar >= 16) {
        size_t i = 0;
        while (i < nbyte && rd_is_hex((unsigned char)val[i])) i++;
        if (i == nbyte && nbyte > 0) return 1;
    }
    if (nchar >= 20) {
        size_t i = 0;
        while (i < nbyte && rd_is_token((unsigned char)val[i])) i++;
        if (i == nbyte && nbyte > 0) return 1;
    }
    if (nchar < 12) return 0;
    int lower = 0, upper = 0, digit = 0;
    for (size_t i = 0; i < nbyte; i++) {
        unsigned char c = (unsigned char)val[i];
        if (c >= 'a' && c <= 'z') lower = 1;
        else if (c >= 'A' && c <= 'Z') upper = 1;
        else if (c >= '0' && c <= '9') digit = 1;
    }
    return (lower + upper + digit) >= 2;
}

/* _unbounded_quantifier_follows: True if *, + or {m,} starts at p[j]. */
static int rd_unbounded_follows(const char *p, size_t n, size_t j) {
    if (j >= n) return 0;
    if (p[j] == '*' || p[j] == '+') return 1;
    if (p[j] == '{') {
        const char *close = (const char *)memchr(p + j, '}', n - j);
        if (!close) return 0; /* k == -1 -> body "" -> False */
        size_t blen = (size_t)(close - (p + j + 1));
        const char *body = p + j + 1;
        if (blen == 0 || body[blen - 1] != ',') return 0;
        if (blen - 1 == 0) return 0; /* "".isdigit() is False */
        for (size_t k = 0; k < blen - 1; k++)
            if (body[k] < '0' || body[k] > '9') return 0;
        return 1;
    }
    return 0;
}

/* _skip_char_class: given p[i] == '[', index just past the closing ']'. */
static size_t rd_skip_class(const char *p, size_t n, size_t i) {
    i += (i + 1 < n && p[i + 1] == ']') ? 2 : 1;
    while (i < n && p[i] != ']')
        i += (p[i] == '\\') ? 2 : 1;
    return i;
}

/* _pattern_structure: one scan -> (top_level_alt, nested_unbounded). */
static void rd_pattern_structure(const char *p, int *alt, int *nested) {
    int top = 0, nest = 0;
    bool stack[1024];
    size_t depth = 1, overflow = 0;
    stack[0] = false;
    size_t n = strlen(p), i = 0;
    while (i < n) {
        char ch = p[i];
        if (ch == '\\') { i += 2; continue; }
        if (ch == '[') {
            i = rd_skip_class(p, n, i);
        } else if (ch == '(') {
            if (depth < 1024) stack[depth++] = false;
            else overflow++;
        } else if (ch == ')') {
            bool inner = false;
            if (overflow > 0) overflow--;
            else if (depth > 1) inner = stack[--depth];
            if (inner && rd_unbounded_follows(p, n, i + 1)) nest = 1;
            stack[depth - 1] = stack[depth - 1] || inner;
        } else if (ch == '|' && depth == 1 && overflow == 0) {
            top = 1;
        } else if (rd_unbounded_follows(p, n, i)) {
            stack[depth - 1] = true;
            if (ch == '{') {
                const char *close = (const char *)memchr(p + i, '}', n - i);
                if (close) i = (size_t)(close - p);
            }
        }
        i++;
    }
    *alt = top;
    *nested = nest;
}

/* PoP: has_top_level_alternation @ agent/redact.py:_has_top_level_alternation */
int has_top_level_alternation(void) {
    int alt = 0, nested = 0;
    rd_pattern_structure(g_rd_pattern ? g_rd_pattern : "", &alt, &nested);
    return alt;
}

/* PoP: has_nested_unbounded_repeat @ agent/redact.py:_has_nested_unbounded_repeat */
int has_nested_unbounded_repeat(void) {
    int alt = 0, nested = 0;
    rd_pattern_structure(g_rd_pattern ? g_rd_pattern : "", &alt, &nested);
    return nested;
}

/* PoP: assignment_value_requires_redaction @ agent/redact.py:assignment_value_requires_redaction */
int assignment_value_requires_redaction(const char *val) { return 0; }

/* PoP: mask_control_split_tokens @ agent/redact.py:mask_control_split_tokens */
char *mask_control_split_tokens(const char *val) { return strdup(""); }


