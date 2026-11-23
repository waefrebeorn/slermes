/*
 * progress.c — C home for upstream Hermes pm/progress.py (gate w5_progress_88711f).
 *
 * PoP: w5_progress_88711f_verbose_output @ pm/progress.py:verbose_output
 * PoP: w5_progress_88711f_ansi_strip      @ pm/progress.py:LiveTail._line
 * PoP: w5_progress_88711f_normalize_lines @ pm/progress.py:LiveTail.write
 *
 * Faithful ports of the deterministic, env/string-only decision and line
 * handling logic. Process/subprocess I/O is out of scope for a differential
 * oracle; every collaborator is an explicit input.
 */
#include "noc_w5_progress_88711f.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define W5P_MAX_LINE 4096

/* ASCII lower + Python str.strip() over the fixture (ASCII) domain. */
static void w5p_strip_lower(const char *in, char *out, size_t outsz) {
    size_t a = 0, b;
    if (!in) in = "";
    while (in[a] && isspace((unsigned char)in[a])) a++;
    b = strlen(in);
    while (b > a && isspace((unsigned char)in[b - 1])) b--;
    size_t n = b - a;
    if (n >= outsz) n = outsz - 1;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)in[a + i]);
    out[n] = '\0';
}

static int w5p_in_set(const char *s, const char *const *set, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (strcmp(s, set[i]) == 0) return 1;
    return 0;
}

int w5_progress_88711f_verbose_output(const json_t *env) {
    static const char *const TRUE_SET[]  = {"1", "true", "yes", "on"};
    static const char *const FALSE_SET[] = {"0", "false", "no", "off"};
    char explicit[256];
    w5p_strip_lower(json_get_str(env, "HERMES_VERBOSE", ""), explicit, sizeof(explicit));
    if (w5p_in_set(explicit, TRUE_SET, 4)) return 1;
    if (w5p_in_set(explicit, FALSE_SET, 4)) return 0;
    static const char *const CI_KEYS[] = {"CI", "GITHUB_ACTIONS"};
    for (size_t i = 0; i < 2; i++) {
        char v[256];
        w5p_strip_lower(json_get_str(env, CI_KEYS[i], ""), v, sizeof(v));
        if (v[0] != '\0' && !w5p_in_set(v, FALSE_SET, 4)) return 1;
    }
    return 0;
}

char *w5_progress_88711f_ansi_strip(const char *line) {
    if (!line) line = "";
    size_t len = strlen(line);
    size_t start = 0;
    if (len > W5P_MAX_LINE) start = len - W5P_MAX_LINE;
    size_t n = len - start;

    /* Remove every matched \x1b\[[0-9;?]*[ -/]*[@-~] (CSI) sequence. */
    char *tmp = (char *)malloc(n + 1);
    if (!tmp) return NULL;
    size_t t = 0;
    for (size_t i = start; i < len;) {
        if (line[i] == '\x1b' && i + 1 < len && line[i + 1] == '[') {
            size_t j = i + 2;
            while (j < len && (isdigit((unsigned char)line[j]) || line[j] == ';' || line[j] == '?')) j++;
            while (j < len && (unsigned char)line[j] >= 0x20 && (unsigned char)line[j] <= 0x2f) j++;
            if (j < len && (unsigned char)line[j] >= 0x40 && (unsigned char)line[j] <= 0x7e) {
                i = j + 1;
                continue;
            }
        }
        tmp[t++] = line[i++];
    }
    tmp[t] = '\0';
    while (t > 0 && isspace((unsigned char)tmp[t - 1])) t--;
    char *out = (char *)malloc(t + 1);
    if (!out) { free(tmp); return NULL; }
    memcpy(out, tmp, t);
    out[t] = '\0';
    free(tmp);
    return out;
}

typedef struct { char *buf; size_t len, cap; } w5p_buf;
static void w5p_push(w5p_buf *b, char c) {
    if (b->len + 1 >= b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        char *nb = (char *)realloc(b->buf, nc);
        if (!nb) return;
        b->buf = nb; b->cap = nc;
    }
    b->buf[b->len++] = c;
}
static json_t *w5p_strn_json(const char *s, size_t n) {
    char *t = (char *)malloc(n + 1);
    if (!t) return json_string("");
    memcpy(t, s, n);
    t[n] = '\0';
    json_t *v = json_string(t);
    free(t);
    return v;
}

json_t *w5_progress_88711f_normalize_lines(const char *partial, const char *text) {
    if (!partial) partial = "";
    if (!text) text = "";
    w5p_buf s = {0};
    for (const char *p = partial; *p; p++) w5p_push(&s, *p);
    for (const char *p = text; *p; p++) w5p_push(&s, *p);
    w5p_push(&s, '\0');
    const char *raw = s.buf ? s.buf : "";

    w5p_buf norm = {0};
    for (size_t i = 0; raw[i];) {
        if (raw[i] == '\r' && raw[i + 1] == '\n') { w5p_push(&norm, '\n'); i += 2; }
        else if (raw[i] == '\r') { w5p_push(&norm, '\n'); i++; }
        else { w5p_push(&norm, raw[i]); i++; }
    }
    w5p_push(&norm, '\0');
    const char *ns = norm.buf ? norm.buf : "";

    json_t *lines = json_array();
    const char *seg = ns;
    const char *last = ns;
    for (const char *p = ns;; p++) {
        if (*p == '\0') { last = seg; break; }
        if (*p == '\n') {
            json_append(lines, w5p_strn_json(seg, (size_t)(p - seg)));
            seg = p + 1;
        }
    }
    size_t lastlen = strlen(last);
    const char *ltail = last;
    if (lastlen > W5P_MAX_LINE) ltail = last + (lastlen - W5P_MAX_LINE);

    json_t *out = json_object();
    json_set(out, "lines", lines);
    json_set(out, "partial", json_string(ltail));
    free(s.buf);
    free(norm.buf);
    return out;
}

