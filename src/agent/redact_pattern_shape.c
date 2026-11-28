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

/* PoP: agent_redact_pattern_shape_u_has_top_level_alternation
 * @ agent/redact_pattern_shape.py:_has_top_level_alternation
 * PoP: agent_redact_pattern_shape_u_has_nested_unbounded_repeat
 * @ agent/redact_pattern_shape.py:_has_nested_unbounded_repeat
 * One scan over the pattern yields (has_top_level_alternation,
 * has_nested_unbounded_repeat); each accessor returns one half. Structural only. */
static int _rps_skip_class(const char *p, int n, int i) {
    i += (i+1 < n && p[i+1] == ']') ? 2 : 1;      /* a leading "]" is literal */
    while (i < n && p[i] != ']') i += (p[i] == '\\') ? 2 : 1;
    return i;
}
static int _rps_unbounded_follows(const char *p, int n, int j) {
    if (j >= n) return 0;
    if (p[j] == '*' || p[j] == '+') return 1;
    if (p[j] == '{') {
        int k = -1;
        for (int t = j; t < n; t++) if (p[t] == '}') { k = t; break; }
        if (k < 0) return 0;
        /* body = p[j+1:k]; open-ended iff body[:-1] is all digits and body ends "," */
        int bl = k - (j + 1);
        if (bl < 1) return 0;
        if (p[k-1] != ',') return 0;
        for (int t = j + 1; t < k - 1; t++) if (!(p[t] >= '0' && p[t] <= '9')) return 0;
        return 1;
    }
    return 0;
}
static void _rps_scan(const char *p, int n, int *top, int *nested) {
    int stack[256]; int sp = 0; stack[sp++] = 0;
    *top = 0; *nested = 0;
    int i = 0;
    while (i < n) {
        char ch = p[i];
        if (ch == '\\') { i += 2; continue; }
        if (ch == '[') { i = _rps_skip_class(p, n, i); }
        else if (ch == '(') { if (sp < 256) stack[sp++] = 0; }
        else if (ch == ')') {
            int inner = (sp > 1) ? stack[--sp] : 0;
            if (inner && _rps_unbounded_follows(p, n, i + 1)) *nested = 1;
            if (sp > 0) stack[sp-1] = stack[sp-1] || inner;
        }
        else if (ch == '|' && sp == 1) { *top = 1; }
        else if (_rps_unbounded_follows(p, n, i)) {
            if (sp > 0) stack[sp-1] = 1;
            if (ch == '{') { int k = -1; for (int t = i; t < n; t++) if (p[t] == '}') { k = t; break; } if (k >= 0) i = k; }
        }
        i += 1;
    }
}
int agent_redact_pattern_shape_u_has_top_level_alternation(const char *arg) {
    const char *p = arg ? arg : ""; int top = 0, nested = 0;
    _rps_scan(p, (int)strlen(p), &top, &nested);
    return top;
}
int agent_redact_pattern_shape_u_has_nested_unbounded_repeat(const char *arg) {
    const char *p = arg ? arg : ""; int top = 0, nested = 0;
    _rps_scan(p, (int)strlen(p), &top, &nested);
    return nested;
}
