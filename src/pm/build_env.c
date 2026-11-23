/*
 * build_env.c — C home for upstream Hermes pm/build_env.py (gate w5_buildenv_1eb2c8).
 *
 * PoP: w5_buildenv_1eb2c8_parse_requirements @ pm/build_env.py:main
 * PoP: w5_buildenv_1eb2c8_record_selection_invalid @ pm/build_env.py:_check_record_selection
 *
 * The two deterministic decisions in the CLI builder: requirements-file
 * filtering and --record-selection validation. Process execution is out of
 * scope for a differential oracle.
 */
#include "noc_w5_buildenv_1eb2c8.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int w5b_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str.strip()/lstrip() over the ASCII whitespace domain. */
static const char *w5b_lstrip(const char *s) {
    while (*s && w5b_is_ws(*s)) s++;
    return s;
}
static void w5b_strip(const char *s, const char **out, size_t *outlen) {
    const char *a = w5b_lstrip(s);
    const char *b = a + strlen(a);
    while (b > a && w5b_is_ws(b[-1])) b--;
    *out = a; *outlen = (size_t)(b - a);
}
static json_t *w5b_str_json(const char *s, size_t n) {
    char *t = (char *)malloc(n + 1);
    if (!t) return json_string("");
    memcpy(t, s, n); t[n] = '\0';
    json_t *v = json_string(t);
    free(t);
    return v;
}

json_t *w5_buildenv_1eb2c8_parse_requirements(const char *text) {
    json_t *out = json_array();
    if (!text) return out;
    const char *p = text;
    while (*p) {
        /* one splitlines() segment: ends at a line boundary, not included */
        const char *seg = p;
        while (*p && *p != '\n' && *p != '\r' && *p != '\v' && *p != '\f') p++;
        size_t seglen = (size_t)(p - seg);
        /* consume the boundary: CRLF is one boundary */
        if (*p == '\r' && p[1] == '\n') p += 2;
        else if (*p) p += 1;

        /* copy segment into a bounded buffer to trim safely */
        char line[8192];
        size_t cap = seglen < sizeof(line) - 1 ? seglen : sizeof(line) - 1;
        memcpy(line, seg, cap); line[cap] = '\0';
        const char *stripped; size_t n;
        w5b_strip(line, &stripped, &n);
        if (n == 0) continue;
        const char *ls = w5b_lstrip(line);
        if (*ls == '#') continue;
        json_append(out, w5b_str_json(stripped, n));
    }
    return out;
}

int w5_buildenv_1eb2c8_record_selection_invalid(int record_selection, int build,
        int requirements_count, int manager_runtime, int all_extras,
        int groups_count, int extras_count) {
    if (!record_selection) return 0;
    return (!build || requirements_count > 0 || manager_runtime || all_extras
            || groups_count > 0 || extras_count == 0) ? 1 : 0;
}

