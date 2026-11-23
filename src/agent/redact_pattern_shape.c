/*
 * redact_pattern_shape.c — C port of upstream Hermes agent/redact_pattern_shape.py.
 *
 * PoP: w13_redactpatt_316808_skip_char_class @ agent/redact_pattern_shape.py:_skip_char_class
 * PoP: w13_redactpatt_316808_unbounded_quantifier_follows @ agent/redact_pattern_shape.py:_unbounded_quantifier_follows
 * PoP: w13_redactpatt_316808_pattern_structure @ agent/redact_pattern_shape.py:_pattern_structure
 *
 * Pure string scans over a regex pattern string; no regex engine needed.
 */
#include "noc_w13_redactpatt_316808.h"
#include "hermes_json.h"
#include <ctype.h>
#include <string.h>

#define STACK_MAX 1024

static int plen(const char *p) { return p ? (int)strlen(p) : 0; }

int w13_redactpatt_316808_skip_char_class(const char *pattern, int i) {
    int n = plen(pattern);
    if (!pattern) return i;
    /* i += 2 if pattern[i+1:i+2] == "]" else 1 */
    i += (i + 1 < n && pattern[i + 1] == ']') ? 2 : 1;
    while (i < n && pattern[i] != ']')
        i += (pattern[i] == '\\') ? 2 : 1;
    return i;
}

int w13_redactpatt_316808_unbounded_quantifier_follows(const char *pattern, int j) {
    int n = plen(pattern);
    if (!pattern || j >= n) return 0;
    char ch = pattern[j];
    if (ch == '*' || ch == '+') return 1;
    if (ch == '{') {
        int k = -1;
        for (int p = j; p < n; p++) if (pattern[p] == '}') { k = p; break; }
        int bl = (k != -1) ? (k - (j + 1)) : 0;  /* body = pattern[j+1:k] */
        if (bl < 2) return 0;                    /* body[:-1] empty -> isdigit False */
        int digits = 1;
        for (int p = 0; p < bl - 1; p++)
            if (!isdigit((unsigned char)pattern[j + 1 + p])) { digits = 0; break; }
        return digits && pattern[k - 1] == ',';  /* body.endswith(',') */
    }
    return 0;
}

struct json_t *w13_redactpatt_316808_pattern_structure(const char *pattern) {
    int top_alt = 0, nested = 0;
    char stack[STACK_MAX];
    int depth = 1;
    stack[0] = 0;  /* root group's contains_unbounded */
    int n = plen(pattern);
    int i = 0;
    while (i < n) {
        char ch = pattern[i];
        if (ch == '\\') { i += 2; continue; }
        if (ch == '[') {
            i = w13_redactpatt_316808_skip_char_class(pattern, i);
        } else if (ch == '(') {
            if (depth < STACK_MAX) stack[depth++] = 0;
        } else if (ch == ')') {
            int inner = (depth > 1) ? stack[--depth] : 0;
            if (inner && w13_redactpatt_316808_unbounded_quantifier_follows(pattern, i + 1))
                nested = 1;
            if (depth > 0) stack[depth - 1] = stack[depth - 1] || inner;
        } else if (ch == '|' && depth == 1) {
            top_alt = 1;
        } else if (w13_redactpatt_316808_unbounded_quantifier_follows(pattern, i)) {
            if (depth > 0) stack[depth - 1] = 1;
            if (ch == '{') {
                int k = -1;
                for (int p = i; p < n; p++) if (pattern[p] == '}') { k = p; break; }
                if (k != -1) i = k;  /* skip the {m,} body */
            }
        }
        i += 1;
    }
    json_t *out = json_array();
    json_append(out, json_bool(top_alt ? true : false));
    json_append(out, json_bool(nested ? true : false));
    return out;
}
