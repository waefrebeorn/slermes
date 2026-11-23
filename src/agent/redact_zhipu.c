/*
 * redact_zhipu.c — faithful C11 port of upstream Hermes agent/redact_zhipu.py.
 *
 * PoP: redactzhipu_find @ agent/redact_zhipu.py:_ZHIPU_API_KEY_RE
 *
 * The upstream module defines exactly one compiled regex and no functions:
 *   _ZHIPU_API_KEY_RE = re.compile(
 *       r"(?<![A-Za-z0-9_.-])([0-9a-f]{32}\.[A-Za-z0-9]{16,})(?![A-Za-z0-9_.-])")
 * We reproduce re.finditer's leftmost, non-overlapping scan with the negative
 * lookbehind/lookahead and the greedy trailing alnum run.
 */
#include "noc_w6_redactzhipu_4cd0af.h"
#include <stdlib.h>
#include <string.h>

static int rz_is_alnum(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}
static int rz_is_hex_lower(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}
/* The lookaround exclusion class [A-Za-z0-9_.-]. */
static int rz_is_forbidden(unsigned char c) {
    return rz_is_alnum(c) || c == '_' || c == '.' || c == '-';
}

json_t *redactzhipu_find(const char *text) {
    json_t *out = json_array();
    if (!text) return out;
    size_t n = strlen(text);
    size_t i = 0;
    while (i < n) {
        /* Negative lookbehind: preceding char must not be in the class. */
        if (i > 0 && rz_is_forbidden((unsigned char)text[i - 1])) { i++; continue; }
        /* Need 32 lowercase hex, '.', then >=16 alnum. */
        if (i + 33 <= n && text[i + 32] == '.') {
            int ok = 1;
            for (size_t k = 0; k < 32; k++) {
                if (!rz_is_hex_lower((unsigned char)text[i + k])) { ok = 0; break; }
            }
            if (ok) {
                size_t j = i + 33;                 /* first char of the alnum run */
                while (j < n && rz_is_alnum((unsigned char)text[j])) j++;
                size_t run = j - (i + 33);
                /* Greedy run + negative lookahead: the char after the whole run
                 * must not be in the class (backtracking to a shorter run can
                 * never satisfy the lookahead, so the maximal run is required). */
                if (run >= 16 && (j == n || !rz_is_forbidden((unsigned char)text[j]))) {
                    char *buf = (char *)malloc(j - i + 1);
                    if (buf) {
                        memcpy(buf, text + i, j - i);
                        buf[j - i] = '\0';
                        json_append(out, json_string(buf));
                        free(buf);
                    }
                    i = j;                          /* finditer resumes after the match */
                    continue;
                }
            }
        }
        i++;
    }
    return out;
}
