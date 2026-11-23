/*
 * cli_unknown_command.c — faithful C11 port of upstream Hermes
 * hermes_cli/cli_unknown_command.py (gate cmdunknown).
 * PoP: cmdunknown_unknown_command_lines @ hermes_cli/cli_unknown_command.py:unknown_command_lines
 *
 * Includes a faithful port of difflib.SequenceMatcher.ratio() (isjunk=None,
 * autojunk=True) and get_close_matches(n=1, cutoff=0.6). Sequences are byte
 * strings: for the ASCII command names exercised by this gate, bytes are the
 * same code points Python compares.
 */
#include "noc_cmdunknown.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

static int cu_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *cu_dup_n(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

char *cmdunknown_command_base(const char *typed) {
    const char *p;
    const char *end;
    if (!typed) return cu_dup_n("", 0);
    p = typed;
    while (*p && cu_is_ws((unsigned char)*p)) p++;
    if (*p == '\0') return cu_dup_n(typed, strlen(typed)); /* split() empty -> original */
    end = p;
    while (*end && !cu_is_ws((unsigned char)*end)) end++;
    return cu_dup_n(p, (size_t)(end - p));
}

/* ---- difflib.SequenceMatcher(None, a, b).ratio() (byte sequences) ---- */
typedef struct {
    const char *a;
    const char *b;
    size_t la, lb;
    unsigned char popular[256];
    size_t start[256], count[256];
    int *positions;
    int *j2len, *newj2len;
} SM;

static void sm_build_b2j(SM *sm) {
    size_t i;
    int cnt[256] = {0};
    size_t ntest;
    size_t acc;
    size_t cursor[256] = {0};
    int c;

    for (i = 0; i < sm->lb; i++) cnt[(unsigned char)sm->b[i]]++;
    ntest = (sm->lb >= 200) ? (sm->lb / 100 + 1) : 0;
    for (c = 0; c < 256; c++)
        sm->popular[c] = (unsigned char)(ntest && cnt[c] > (int)ntest);

    acc = 0;
    for (c = 0; c < 256; c++) {
        sm->start[c] = acc;
        sm->count[c] = sm->popular[c] ? 0 : (size_t)cnt[c];
        acc += sm->count[c];
    }
    for (i = 0; i < sm->lb; i++) {
        int ch = (unsigned char)sm->b[i];
        if (!sm->popular[ch])
            sm->positions[sm->start[ch] + cursor[ch]++] = (int)i;
    }
}

/* find_longest_match over the given sub-ranges; returns its size and bounds. */
static size_t sm_longest(SM *sm, size_t alo, size_t ahi, size_t blo, size_t bhi,
                         size_t *out_i, size_t *out_j) {
    size_t besti = alo, bestj = blo, bestsize = 0;
    size_t i, k;
    memset(sm->j2len, 0, sm->lb * sizeof(int));
    for (i = alo; i < ahi; i++) {
        int ch = (unsigned char)sm->a[i];
        size_t n = sm->count[ch];
        size_t p;
        /* difflib creates a fresh newj2len dict for every i. */
        memset(sm->newj2len, 0, sm->lb * sizeof(int));
        for (p = 0; p < n; p++) {
            size_t j = (size_t)sm->positions[sm->start[ch] + p];
            int prev, newv;
            if (j < blo || j >= bhi) continue;
            prev = (j > 0) ? sm->j2len[j - 1] : 0;
            newv = prev + 1;
            sm->newj2len[j] = newv;
            if ((size_t)newv > bestsize) {
                bestsize = (size_t)newv;
                besti = i - (size_t)newv + 1;
                bestj = j - (size_t)newv + 1;
            }
        }
        { int *tmp = sm->j2len; sm->j2len = sm->newj2len; sm->newj2len = tmp; }
    }
    /* Extend over matching elements (isjunk is None => bjunk is empty). */
    while (besti > alo && bestj > blo && sm->a[besti - 1] == sm->b[bestj - 1]) {
        besti--; bestj--; bestsize++;
    }
    while (besti + bestsize < ahi && bestj + bestsize < bhi &&
           sm->a[besti + bestsize] == sm->b[bestj + bestsize]) {
        bestsize++;
    }
    *out_i = besti;
    *out_j = bestj;
    return bestsize;
}

static long sm_match_sum(SM *sm, size_t alo, size_t ahi, size_t blo, size_t bhi) {
    size_t bi, bj;
    size_t bs = sm_longest(sm, alo, ahi, blo, bhi, &bi, &bj);
    long total;
    if (bs == 0) return 0;
    total = (long)bs;
    if (alo < bi && blo < bj) total += sm_match_sum(sm, alo, bi, blo, bj);
    if (bi + bs < ahi && bj + bs < bhi)
        total += sm_match_sum(sm, bi + bs, ahi, bj + bs, bhi);
    return total;
}

static double sm_ratio(const char *a, const char *b) {
    SM sm;
    double matches;
    size_t denom;
    memset(&sm, 0, sizeof(sm));
    sm.a = a;
    sm.b = b;
    sm.la = strlen(a);
    sm.lb = strlen(b);
    denom = sm.la + sm.lb;
    if (denom == 0) return 1.0; /* difflib: SequenceMatcher('','').ratio() == 1.0 */
    sm.positions = (int *)malloc((sm.lb ? sm.lb : 1) * sizeof(int));
    sm.j2len = (int *)calloc(sm.lb ? sm.lb : 1, sizeof(int));
    sm.newj2len = (int *)calloc(sm.lb ? sm.lb : 1, sizeof(int));
    if (!sm.positions || !sm.j2len || !sm.newj2len) {
        free(sm.positions); free(sm.j2len); free(sm.newj2len);
        return 0.0;
    }
    sm_build_b2j(&sm);
    matches = (double)sm_match_sum(&sm, 0, sm.la, 0, sm.lb);
    free(sm.positions); free(sm.j2len); free(sm.newj2len);
    return 2.0 * matches / (double)denom;
}

char *cmdunknown_close_match(const char *base, const struct json_t *known) {
    const char *best_x = NULL;
    double best_r = -1.0;
    size_t i, n;
    if (!base || !known || known->type != JSON_ARRAY) return NULL;
    n = json_len(known);
    for (i = 0; i < n; i++) {
        json_t *item = json_get(known, i);
        const char *x;
        double r;
        if (!item || item->type != JSON_STRING || !item->str_val) continue;
        x = item->str_val;
        r = sm_ratio(x, base);                 /* a = known, b = base (as in difflib) */
        if (r < 0.6) continue;                  /* cutoff */
        if (best_x == NULL || r > best_r ||
            (r == best_r && strcmp(x, best_x) > 0)) {
            best_r = r;
            best_x = x;
        }
    }
    return best_x ? cu_dup_n(best_x, strlen(best_x)) : NULL;
}

void cmdunknown_unknown_command_lines(const char *typed, const struct json_t *known,
                                      char **lead, char **pointer) {
    char *base = cmdunknown_command_base(typed);
    char *close = cmdunknown_close_match(base, known);
    char *hint;
    size_t hint_len, lead_len;

    if (close) {
        hint_len = strlen(" Did you mean ") + strlen(close) + strlen("?") + 1;
        hint = (char *)malloc(hint_len);
        snprintf(hint, hint_len, " Did you mean %s?", close);
    } else {
        hint_len = 1;
        hint = (char *)malloc(hint_len);
        hint[0] = '\0';
    }
    lead_len = strlen("Unknown command ") + strlen(base) +
               strlen(" — nothing was sent.") + strlen(hint) + 1;
    *lead = (char *)malloc(lead_len);
    snprintf(*lead, lead_len, "Unknown command %s — nothing was sent.%s", base, hint);
    *pointer = (char *)malloc(strlen("Type /help for the full list.") + 1);
    strcpy(*pointer, "Type /help for the full list.");

    free(hint);
    free(close);
    free(base);
}
