/* gate: skills_hub_search_score — traced copy of the FIXED
 * src/tools/skills_hub_ports.c::sh_search_score (full port of
 * SkillsHub._search_score; the previous strstr stub was replaced by this
 * tiered scorer — a real parity win).
 * LIVE twin: tools/skills_hub.py::ClawHubSource._search_score.
 *
 * Input encoding (argv-safe): "query|identifier|name|description".
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: empty query after strip/lower -> return 1
 *   B2..B12 tier ladder: 140,130,125,120,95,90,70,65,40,35,10
 *   LOOP over query terms:
 *     per iteration L tick + B13 (+15 in identifier terms) /
 *     B14 (+12 in name terms) / B15 (+3 substring in description)
 *   V: final score as decimal string
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "watercup.h"

#define MAXT 32

static char *lc(const char *s) {
    char *d = strdup(s ? s : "");
    for (char *p = d; *p; p++) *p = (char)tolower((unsigned char)*p);
    return d;
}
static void lc_inplace(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}
static void strip(char *s) {
    size_t l = strlen(s);
    size_t b = 0;
    while (b < l && (s[b]==' '||s[b]=='\t'||s[b]=='\n'||s[b]=='\r')) b++;
    memmove(s, s + b, l - b + 1);
    l -= b;
    while (l > 0 && (s[l-1]==' '||s[l-1]=='\t'||s[l-1]=='\n'||s[l-1]=='\r')) s[--l] = '\0';
}
/* split on runs of [^a-z0-9]; input must be lowercased */
static size_t qterms(const char *s, char out[MAXT][64]) {
    size_t n = 0;
    for (const char *p = s; *p; ) {
        while (*p && !((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9'))) p++;
        if (!*p) break;
        const char *st = p;
        while (*p && ((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9'))) p++;
        if (n < MAXT) {
            size_t len = (size_t)(p - st);
            if (len > 63) len = 63;
            memcpy(out[n], st, len);
            out[n][len] = '\0';
            n++;
        }
    }
    return n;
}
static void join_terms(char out[256], char t[MAXT][64], size_t n) {
    out[0] = '\0';
    size_t off = 0;
    for (size_t i = 0; i < n; i++) {
        if (i) out[off++] = ' ';
        off += (size_t)snprintf(out + off, 256 - off, "%s", t[i]);
    }
}
static bool prefix_eq(char pre[MAXT][64], size_t pn, char hay[MAXT][64], size_t hn) {
    if (pn > hn) return false;
    for (size_t i = 0; i < pn; i++)
        if (strcmp(pre[i], hay[i]) != 0) return false;
    return true;
}
static bool contains_term(char hay[MAXT][64], size_t hn, const char *needle) {
    for (size_t i = 0; i < hn; i++)
        if (strcmp(hay[i], needle) == 0) return true;
    return false;
}

static int wc_search_score(const char *query, const char *identifier,
                           const char *name, const char *description) {
    WCUP_BEGIN("search_score");
    int score = 0;
    char query_norm[256];
    snprintf(query_norm, sizeof query_norm, "%s", query ? query : "");
    lc_inplace(query_norm);
    strip(query_norm);
    if (!*query_norm) {                       /* B1 */
        WCUP_BRANCH(1);
        score = 1;
        goto out;
    }
    WCUP_BRANCH(0);
    {
        char *id_lc = lc(identifier), *name_lc = lc(name), *desc_lc = lc(description);
        static char ni[MAXT][64], nn[MAXT][64], qt[MAXT][64], it[MAXT][64], nt[MAXT][64];
        size_t ni_n = qterms(id_lc, ni);
        size_t nn_n = qterms(name_lc, nn);
        size_t qt_n = qterms(query_norm, qt);
        size_t it_n = qterms(id_lc, it);
        size_t nt_n = qterms(name_lc, nt);
        char nij[256], nnj[256];
        join_terms(nij, ni, ni_n);
        join_terms(nnj, nn, nn_n);

        score = 0;
        if (strcmp(query_norm, id_lc) == 0) { WCUP_BRANCH(1); score += 140; } else WCUP_BRANCH(0);
        if (strcmp(query_norm, name_lc) == 0) { WCUP_BRANCH(1); score += 130; } else WCUP_BRANCH(0);
        if (strcmp(nij, query_norm) == 0) { WCUP_BRANCH(1); score += 125; } else WCUP_BRANCH(0);
        if (strcmp(nnj, query_norm) == 0) { WCUP_BRANCH(1); score += 120; } else WCUP_BRANCH(0);
        if (strncmp(nij, query_norm, strlen(query_norm)) == 0) { WCUP_BRANCH(1); score += 95; } else WCUP_BRANCH(0);
        if (strncmp(nnj, query_norm, strlen(query_norm)) == 0) { WCUP_BRANCH(1); score += 90; } else WCUP_BRANCH(0);
        if (qt_n > 0 && prefix_eq(qt, qt_n, it, it_n)) { WCUP_BRANCH(1); score += 70; } else WCUP_BRANCH(0);
        if (qt_n > 0 && prefix_eq(qt, qt_n, nt, nt_n)) { WCUP_BRANCH(1); score += 65; } else WCUP_BRANCH(0);
        if (strstr(id_lc, query_norm)) { WCUP_BRANCH(1); score += 40; } else WCUP_BRANCH(0);
        if (strstr(name_lc, query_norm)) { WCUP_BRANCH(1); score += 35; } else WCUP_BRANCH(0);
        if (strstr(desc_lc, query_norm)) { WCUP_BRANCH(1); score += 10; } else WCUP_BRANCH(0);

        for (size_t i = 0; i < qt_n; i++) {
            WCUP_LOOP_TICK();
            if (contains_term(it, it_n, qt[i])) { WCUP_BRANCH(1); score += 15; } else WCUP_BRANCH(0);
            if (contains_term(nt, nt_n, qt[i])) { WCUP_BRANCH(1); score += 12; } else WCUP_BRANCH(0);
            if (strstr(desc_lc, qt[i])) { WCUP_BRANCH(1); score += 3; } else WCUP_BRANCH(0);
        }
        free(id_lc); free(name_lc); free(desc_lc);
    }
out:;
    {
        char cup[32], sval[16];
        snprintf(sval, sizeof sval, "%d", score);
        WCUP_VALSTR(sval);
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("search_score");
        return score;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s search_score <query|id|name|desc>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        /* split on '|' into up to 4 fields */
        char *fields[4] = {NULL, NULL, NULL, NULL};
        char *buf = strdup(argv[i]), *p = buf;
        for (int f = 0; f < 4; f++) {
            fields[f] = p;
            char *bar = strchr(p, '|');
            if (bar) { *bar = '\0'; p = bar + 1; }
        }
        wc_search_score(fields[0], fields[1], fields[2], fields[3]);
        free(buf);
    }
    return 0;
}
