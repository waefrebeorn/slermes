/*
 * surface_switch.c — C11 port of the pure persisted-prompt string parsers in
 * upstream Hermes agent/surface_switch.py.
 *
 * PoP: w7_surfaceswit_4b9e48_split_runtime_boundary @ agent/surface_switch.py:split_runtime_boundary
 * PoP: w7_surfaceswit_4b9e48_runtime_host_value @ agent/surface_switch.py:runtime_host_value
 * PoP: w7_surfaceswit_4b9e48_identity_line_value @ agent/surface_switch.py:identity_line_value
 *
 * platform_hint / flatten_message_text / agent mutation are side-effecting and
 * out of scope; the three pure parsers below are the gate's semantic core.
 */
#include "noc_w7_surfaceswit_4b9e48.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define RT_HEADING "# Hermes runtime environment"
#define RT_END "<!-- End Hermes runtime environment -->"
#define RT_SEP "\n\n" RT_HEADING "\n\n"

static char *xstrndup(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
static char *xstrdup2(const char *s) { return xstrndup(s, s ? strlen(s) : 0); }

/* Python str.strip() (ASCII whitespace covers the prompt fixtures). */
static char *strip(const char *s) {
    if (!s) s = "";
    const char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    const char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) end--;
    return xstrndup(start, (size_t)(end - start));
}

/* Python str.splitlines() for \n / \r\n text, dropping the trailing empty line. */
static json_t *splitlines(const char *s) {
    json_t *arr = json_array();
    if (!s || !*s) return arr;
    const char *p = s;
    while (*p) {
        const char *q = p;
        while (*q && *q != '\n' && *q != '\r') q++;
        char *line = xstrndup(p, (size_t)(q - p));
        json_append(arr, json_string(line));
        free(line);
        if (*q == '\r') { q++; if (*q == '\n') q++; }
        else if (*q == '\n') q++;
        p = q;
    }
    return arr;
}

/* Last occurrence of needle in haystack, or NULL. */
static const char *last_str(const char *hay, const char *needle) {
    const char *found = NULL, *p = hay;
    size_t n = strlen(needle);
    while ((p = strstr(p, needle)) != NULL) { found = p; p += n; }
    return found;
}

static bool ends_with(const char *s, const char *suffix) {
    if (!s) return false;
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && !strcmp(s + a - b, suffix);
}

json_t *w7_surfaceswit_4b9e48_split_runtime_boundary(const char *prompt) {
    if (!prompt) prompt = "";
    json_t *out = json_array();
    const int has_end = ends_with(prompt, RT_END);
    const char *sep = has_end ? last_str(prompt, RT_SEP) : NULL;
    if (!has_end) {
        /* Legacy prompt (does not END in the boundary): whole thing is identity. */
        json_append(out, json_string(prompt));
        json_append(out, json_string(""));
        json_append(out, json_string(""));
    } else if (!sep) {
        /* Python str.rpartition with no separator -> ('', '', whole_string). */
        json_append(out, json_string(""));
        json_append(out, json_string(""));
        json_append(out, json_string(prompt));
    } else {
        char *head = xstrndup(prompt, (size_t)(sep - prompt));
        char *runtime = xstrdup2(sep + strlen(RT_SEP));
        json_append(out, json_string(head));
        json_append(out, json_string(RT_SEP));
        json_append(out, json_string(runtime));
        free(head);
        free(runtime);
    }
    return out;
}

char *w7_surfaceswit_4b9e48_runtime_host_value(const char *prompt, const char *label) {
    if (!prompt) prompt = "";
    json_t *parts = w7_surfaceswit_4b9e48_split_runtime_boundary(prompt);
    const char *runtime = json_string_value(json_get(parts, 2));
    bool has_marker = json_string_value(json_get(parts, 1)) && *json_string_value(json_get(parts, 1));
    const char *source = prompt;
    char *first_block = NULL;
    if (has_marker && runtime) {
        const char *cut = strstr(runtime, "\n\n");
        first_block = xstrndup(runtime, cut ? (size_t)(cut - runtime) : strlen(runtime));
        source = first_block;
    }
    json_t *lines = splitlines(source);
    char *result = NULL;
    size_t count = json_len(lines);
    for (size_t i = 0; i < count; i++) {
        const char *line = json_string_value(json_get(lines, i));
        if (!line || strncmp(line, "User home directory:", 20)) continue;
        for (size_t j = i + 1; j < count && j <= i + 3; j++) {
            const char *cand = json_string_value(json_get(lines, j));
            size_t plen = label ? strlen(label) : 0;
            if (cand && strncmp(cand, label, plen) == 0 && cand[plen] == ':') {
                result = strip(cand + plen + 1);
                break;
            }
        }
        break;
    }
    if (!result) result = xstrdup2("");
    json_free(lines);
    free(first_block);
    json_free(parts);
    return result;
}

char *w7_surfaceswit_4b9e48_identity_line_value(const char *prompt, const char *label) {
    if (!prompt) prompt = "";
    json_t *parts = w7_surfaceswit_4b9e48_split_runtime_boundary(prompt);
    const char *identity = json_string_value(json_get(parts, 0));
    const char *tail = last_str(identity ? identity : "", "\n\n");
    const char *trailer = tail ? tail + 2 : (identity ? identity : "");
    json_t *lines = splitlines(trailer);
    char *result = NULL;
    size_t plen = label ? strlen(label) : 0;
    for (size_t i = 0; i < json_len(lines); i++) {
        const char *line = json_string_value(json_get(lines, i));
        if (line && strncmp(line, label, plen) == 0 && line[plen] == ':') {
            free(result);
            result = strip(line + plen + 1);
        }
    }
    if (!result) result = xstrdup2("");
    json_free(lines);
    json_free(parts);
    return result;
}

