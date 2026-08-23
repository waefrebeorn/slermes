/* gate: agent_chat_completion_helpers_bedrock_reasoning_stale_floor —
 * traced copy of the LIVE agent/chat_completion_helpers.py::
 * _bedrock_reasoning_stale_floor candidate ladder, with the floor lookup
 * itself exercised through the REAL C port
 * src/agent/reasoning_timeouts.c::agent_reasoning_timeouts_get_floor
 * (linked via extra_src).
 * LIVE twin: agent/chat_completion_helpers.py::_bedrock_reasoning_stale_floor
 * (ladder transcribed verbatim; floor via LIVE agent/reasoning_timeouts.py).
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: empty/non-str model_id -> None
 *   LOOP region prefixes (B2 per-prefix startswith)
 *   B3: "." in name -> extend base_candidates (rsplit segment, dash rewrite)
 *   LOOP base_candidates x3 separator forms, dedupe (B4 seen-check)
 *   LOOP candidates: floor lookup (B5 hit) -> first non-None wins; else None
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

extern double agent_reasoning_timeouts_get_floor(const char *model);

static const char *const PREFIXES[] = {
    "global.", "us.", "eu.", "apac.", "ap.", "au.", "jp.",
    "ca.", "sa.", "me.", "af.", NULL
};

static int starts_with(const char *s, const char *p) {
    return strncmp(s, p, strlen(p)) == 0;
}

static int contains(const char *s, char c) {
    return strchr(s, c) != NULL;
}

/* replace first occurrence of `from` with `to` (single char); out<=cap */
static void replace_first(const char *s, char from, char to,
                          char *out, size_t cap) {
    size_t n = strlen(s);
    if (n >= cap) n = cap - 1;
    memcpy(out, s, n);
    out[n] = '\0';
    char *hit = strchr(out, from);
    if (hit) *hit = to;
}

/* regex-equivalent: rewrite digit<sep>digit separators (both directions) */
static void rewrite_version_seps(const char *s, char sep_from, char sep_to,
                                 char *out, size_t cap) {
    size_t o = 0;
    size_t n = strlen(s);
    for (size_t i = 0; i < n && o + 1 < cap; i++) {
        char c = s[i];
        if (c == sep_from && i > 0 && i + 1 < n &&
            isdigit((unsigned char)s[i - 1]) && isdigit((unsigned char)s[i + 1])) {
            out[o++] = sep_to;
        } else {
            out[o++] = c;
        }
    }
    out[o] = '\0';
}

static int seen_before(char (*cands)[256], int ncand, const char *c) {
    for (int i = 0; i < ncand; i++)
        if (strcmp(cands[i], c) == 0) return 1;
    return 0;
}

static double wc_bedrock_stale_floor(const char *model_id, int *found) {
    WCUP_BEGIN("bedrock_reasoning_stale_floor");
    double result = -1.0;
    *found = 0;
    if (!model_id || !*model_id) {          /* B1 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);
    {
        char name[1024];
        size_t n = 0;
        for (const char *p = model_id; *p && n + 1 < sizeof(name); p++)
            name[n++] = (char)tolower((unsigned char)*p);
        name[n] = '\0';
        for (int pi = 0; PREFIXES[pi]; pi++) {       /* LOOP prefixes */
            WCUP_LOOP_TICK();
            if (starts_with(name, PREFIXES[pi])) {   /* B2 */
                WCUP_BRANCH(1);
                memmove(name, name + strlen(PREFIXES[pi]),
                        strlen(name + strlen(PREFIXES[pi])) + 1);
                break;
            }
            WCUP_BRANCH(0);
        }
        char bases[8][256];
        int nbases = 0;
        snprintf(bases[nbases++], 256, "%s", name);
        if (contains(name, '.')) {                   /* B3 */
            WCUP_BRANCH(1);
            const char *dot = strrchr(name, '.');
            snprintf(bases[nbases++], 256, "%s", dot ? dot + 1 : "");
            replace_first(name, '.', '-', bases[nbases++], 256);
        } else {
            WCUP_BRANCH(0);
        }
        char cands[24][256];
        int ncand = 0;
        for (int b = 0; b < nbases; b++) {           /* LOOP base candidates */
            WCUP_LOOP_TICK();
            const char *forms[3] = { bases[b], "", "" };
            char d2d[256], dd2[256];
            rewrite_version_seps(bases[b], '-', '.', d2d, sizeof d2d);
            rewrite_version_seps(bases[b], '.', '-', dd2, sizeof dd2);
            forms[1] = d2d;
            forms[2] = dd2;
            for (int f = 0; f < 3; f++) {            /* LOOP forms */
                WCUP_LOOP_TICK();
                if (!seen_before(cands, ncand, forms[f])) {  /* B4 */
                    WCUP_BRANCH(1);
                    snprintf(cands[ncand++], 256, "%s", forms[f]);
                } else {
                    WCUP_BRANCH(0);
                }
            }
        }
        for (int ci = 0; ci < ncand; ci++) {         /* LOOP candidates */
            WCUP_LOOP_TICK();
            WCUP_VALSTR(cands[ci]);
            double fl = agent_reasoning_timeouts_get_floor(cands[ci]);
            if (fl >= 0.0) {                          /* B5 */
                WCUP_BRANCH(1);
                result = fl;
                *found = 1;
                goto out;
            }
            WCUP_BRANCH(0);
        }
    }
out:
    {
        char vbuf[64];
        snprintf(vbuf, sizeof vbuf, *found ? "%.17g" : "None", result);
        WCUP_VALSTR(vbuf);   /* outcome value rides inside the cup */
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("bedrock_reasoning_stale_floor");
        return result;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s bedrock_reasoning_stale_floor <model_id>...\n",
                argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        int found = 0;
        double v = wc_bedrock_stale_floor(argv[i], &found);
        (void)v; (void)found; /* outcome rides inside the cup via V events */
    }
    return 0;
}
