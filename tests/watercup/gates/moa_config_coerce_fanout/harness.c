/* gate: moa_config_coerce_fanout — traced C copy of
 * src/hermes_cli/moa_config_pure.c::moa_coerce_fanout (after port fix).
 * Faithful port of LIVE Python hermes_cli/moa_config.py::_coerce_fanout
 * (string form; dict mapping form normalized by callers).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"

static long moa_fanout_coerce_int_traced(wcup_t *wc, const char *s, long defval) {
    /* traced _coerce_int: events fold into the CALLER'S cup. */
    int empty = (!s || !*s);
    wcup_branch(wc, empty);
    if (empty) return defval;
    char *end = NULL;
    long n = strtol(s, &end, 10);
    int ok_int = (end != s && *end == '\0');
    wcup_branch(wc, ok_int);
    if (ok_int) return n;
    double d = strtod(s, &end);
    int ok_flt = (end != s && *end == '\0');
    wcup_branch(wc, ok_flt);
    if (ok_flt) return (long)d;
    return defval;
}

static void moa_coerce_fanout(const char *value, char *out, size_t out_size) {
    WCUP_BEGIN("coerce_fanout");
    WCUP_BRANCH(0); /* Python: isinstance(value, dict) -> False (string form) */
    char buf[64];
    snprintf(buf, sizeof buf, "%s", value ? value : "");
    /* trim + lower */
    char *p = buf; while (*p == ' ' || *p == '\t') p++;
    size_t L = strlen(p);
    while (L > 0 && (p[L-1] == ' ' || p[L-1] == '\t')) p[--L] = '\0';
    for (char *q = p; *q; q++) *q = (char)tolower((unsigned char)*q);
    int canon = (strcmp(p, "per_iteration") == 0 || strcmp(p, "user_turn") == 0);
    WCUP_BRANCH(canon);
    if (canon) {
        snprintf(out, out_size, "%s", p);
        goto done;
    }
    {
        int ev = (strncmp(p, "every_n", 7) == 0);
        WCUP_BRANCH(ev);
        if (ev) {
            const char *sep = strchr(p, ':');
            int has_sep = (sep != NULL);
            WCUP_BRANCH(has_sep);
            long n = 0;
            if (has_sep) {
                /* Python: _coerce_int(rest.strip(), 0) */
                char rest[64];
                snprintf(rest, sizeof rest, "%s", sep + 1);
                char *r = rest; while (*r == ' ' || *r == '\t') r++;
                size_t RL = strlen(r);
                while (RL > 0 && (r[RL-1] == ' ' || r[RL-1] == '\t')) r[--RL] = '\0';
                n = moa_fanout_coerce_int_traced(&_wc, r, 0);
            }
            int ge2 = (n >= 2);
            WCUP_BRANCH(ge2);
            if (ge2) { snprintf(out, out_size, "every_n:%ld", n); goto done; }
            int eq1 = (n == 1);
            WCUP_BRANCH(eq1);
            if (eq1) { snprintf(out, out_size, "per_iteration"); goto done; }
        }
    }
    snprintf(out, out_size, "user_turn");
done:
    WCUP_VALSTR(out);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("coerce_fanout");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s moa_coerce_fanout <value>...\n", argv[0]); return 2; }
    char out[64];
    for (int i = 2; i < argc; i++) {
        moa_coerce_fanout(argv[i], out, sizeof out);
        fprintf(stderr, "%s\n", out);
    }
    return 0;
}
