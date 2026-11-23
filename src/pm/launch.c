/*
 * launch.c — C port of upstream Hermes pm/launch.py (gate w5_launch_b4dec9).
 *
 * PoP: w5_launch_b4dec9_path_insert @ pm/launch.py:<module>
 * PoP: w5_launch_b4dec9_main @ pm/launch.py:main
 *
 * The real main() injects truststore, leases the runtime and delegates to
 * pm.cli.main. Those three calls are injected by the gate and recorded as
 * events here, so the control flow and return value are faithful while the
 * port stays free of SSL/subprocess side effects.
 */
#include "noc_w5_launch_b4dec9.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

typedef struct { char b[8192]; size_t n; } sbuf_t;
static void sb_init(sbuf_t *s) { s->b[0] = '\0'; s->n = 0; }
static void sb_add(sbuf_t *s, const char *x) {
    size_t l = strlen(x);
    if (s->n + l >= sizeof(s->b)) l = sizeof(s->b) - 1 - s->n;
    memcpy(s->b + s->n, x, l);
    s->n += l;
    s->b[s->n] = '\0';
}
static void sb_ch(sbuf_t *s, char c) {
    if (s->n + 1 < sizeof(s->b)) { s->b[s->n++] = c; s->b[s->n] = '\0'; }
}

/* Path(__file__).resolve() under Windows path semantics. */
static void win_norm(const char *path, char *out, size_t outsz) {
    char tmp[8192];
    size_t n = 0;
    for (const char *q = path; q && *q && n < sizeof(tmp) - 1; q++)
        tmp[n++] = (*q == '/') ? '\\' : *q;
    tmp[n] = '\0';

    sbuf_t pref;
    sb_init(&pref);
    size_t start = 0;
    if (n >= 2 && tmp[0] == '\\' && tmp[1] == '\\') { sb_add(&pref, "\\\\"); start = 2; }
    else if (n >= 1 && tmp[0] == '\\') { sb_ch(&pref, '\\'); start = 1; }
    else if (n >= 2 && isalpha((unsigned char)tmp[0]) && tmp[1] == ':') {
        sb_ch(&pref, tmp[0]); sb_ch(&pref, ':'); start = 2;
    }

    char *comps[256];
    size_t nc = 0;
    size_t i = start;
    while (i < n) {
        while (i < n && tmp[i] == '\\') i++;
        if (i >= n) break;
        size_t j = i;
        while (j < n && tmp[j] != '\\') j++;
        tmp[j] = '\0';
        size_t len = j - i;
        if (len == 1 && tmp[i] == '.') { /* drop */ }
        else if (len == 2 && tmp[i] == '.' && tmp[i + 1] == '.') { if (nc) nc--; }
        else if (nc < 256) comps[nc++] = tmp + i;
        i = j + 1;
    }

    sbuf_t res;
    sb_init(&res);
    sb_add(&res, pref.b);
    for (size_t k = 0; k < nc; k++) {
        if (res.n && res.b[res.n - 1] != '\\') sb_ch(&res, '\\');
        sb_add(&res, comps[k]);
    }
    snprintf(out, outsz, "%s", res.b);
}

/* PoP: w5_launch_b4dec9_path_insert @ pm/launch.py:<module> */
char *w5_launch_b4dec9_path_insert(const char *file_path) {
    char norm[8192];
    win_norm(file_path ? file_path : "", norm, sizeof(norm));
    size_t n = strlen(norm);
    while (n > 0 && norm[n - 1] == '\\') norm[--n] = '\0';
    char *p1 = strrchr(norm, '\\');
    if (!p1) return NULL;
    *p1 = '\0';
    char *p2 = strrchr(norm, '\\');
    if (p2) *p2 = '\0';
    n = strlen(norm);
    if (n == 2 && norm[1] == ':') { norm[2] = '\\'; norm[3] = '\0'; }
    return dupstr(norm);
}

/* PoP: w5_launch_b4dec9_main @ pm/launch.py:main */
json_t *w5_launch_b4dec9_main(int cli_rc) {
    json_t *events = json_array();
    json_append(events, json_string("inject"));
    json_append(events, json_string("lease"));
    json_append(events, json_string("cli"));
    json_t *out = json_object();
    json_set(out, "events", events);
    json_set(out, "rc", json_number((double)cli_rc));
    return out;
}

void w5_launch_b4dec9_free(char *s) { free(s); }
