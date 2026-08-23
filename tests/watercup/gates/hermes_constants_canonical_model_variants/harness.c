/* gate: hermes_constants_canonical_model_variants — traced copy of
 * src/gateway/hermes_constants_reasoning.c::reasoning_canonical_model_variants
 * (+ its static helpers sv_add / sv_add_derivatives / version_sep_swap /
 *   replace_char).
 * LIVE twin: hermes_constants.py::_canonical_model_variants.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: len(parts) >= 2  -> bare model derivatives added
 *   B2: len(parts) >= 3  -> aggregator-stripped derivatives added
 * Then VSTR per final variant (insertion order) + VSTR count.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"

static char *xstrdup2(const char *s) {
    size_t n = strlen(s);
    char *p = malloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

static char *replace_char(const char *s, char from, char to) {
    char *p = xstrdup2(s);
    for (char *q = p; *q; q++) if (*q == from) *q = to;
    return p;
}

/* version-dot recovery: swap separator between adjacent digits only,
 * consuming full matches (non-overlapping, like re.sub) */
static char *version_sep_swap(const char *s, char sep, char rep) {
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    size_t w = 0;
    for (size_t j = 0; j < n; ) {
        if (j + 2 < n && isdigit((unsigned char)s[j]) &&
            s[j + 1] == sep && isdigit((unsigned char)s[j + 2])) {
            o[w++] = s[j];
            o[w++] = rep;
            o[w++] = s[j + 2];
            j += 3;
        } else {
            o[w++] = s[j++];
        }
    }
    o[w] = '\0';
    return o;
}

typedef struct { char **items; int count, cap; } strvec_t;

static void sv_init(strvec_t *v) { v->items = NULL; v->count = 0; v->cap = 0; }
static void sv_add(strvec_t *v, char *owned) {
    if (!owned || owned[0] == '\0') { free(owned); return; }
    for (int i = 0; i < v->count; i++)
        if (strcmp(v->items[i], owned) == 0) { free(owned); return; }
    if (v->count >= v->cap) {
        v->cap = v->cap ? v->cap * 2 : 16;
        v->items = realloc(v->items, (size_t)v->cap * sizeof(char *));
    }
    v->items[v->count++] = owned;
}
static void sv_add_derivatives(strvec_t *v, const char *s) {
    sv_add(v, xstrdup2(s));
    char *all_dashed = replace_char(s, '.', '-');
    sv_add(v, xstrdup2(all_dashed));
    char *all_dotted = replace_char(s, '-', '.');
    sv_add(v, xstrdup2(all_dotted));
    sv_add(v, version_sep_swap(s, '-', '.'));
    sv_add(v, version_sep_swap(s, '.', '-'));
    sv_add(v, version_sep_swap(all_dashed, '-', '.'));
    sv_add(v, version_sep_swap(all_dotted, '.', '-'));
    free(all_dashed);
    free(all_dotted);
}

static void wc_variants(const char *model) {
    WCUP_BEGIN("reasoning_canonical_model_variants");
    strvec_t v;
    sv_init(&v);

    sv_add_derivatives(&v, model);

    int nparts = 1;
    for (const char *p = model; *p; p++) if (*p == '/') nparts++;

    int bare_hit = nparts >= 2;
    WCUP_BRANCH(bare_hit);
    if (bare_hit) {
        const char *bare = strrchr(model, '/');
        bare = bare ? bare + 1 : model;
        sv_add_derivatives(&v, bare);
    }

    int agg_hit = nparts >= 3;
    WCUP_BRANCH(agg_hit);
    if (agg_hit) {
        const char *after_first = strchr(model, '/');
        if (after_first) sv_add_derivatives(&v, after_first + 1);
    }

    static const char *known_providers[] = {
        "anthropic", "openai", "google", "openrouter", "groq", "mistral",
        "xai", "cohere", "perplexity", "together", "fireworks", "deepseek", NULL
    };
    static const char *known_aggregators[] = {
        "openrouter", "opencode", "fireworks", "groq", "together", NULL
    };

    /* snapshot before step-5 prepend (Python's bare_variants at this point) */
    int base_n = v.count;
    char **base_snapshot = malloc((size_t)base_n * sizeof(char *));
    for (int i = 0; i < base_n; i++) base_snapshot[i] = v.items[i];
    for (int i = 0; i < base_n; i++) {
        const char *cur = base_snapshot[i];
        if (strchr(cur, '/') != NULL) continue;
        for (int p = 0; known_providers[p]; p++) {
            size_t L = strlen(known_providers[p]) + 1 + strlen(cur) + 1;
            char *buf = malloc(L);
            snprintf(buf, L, "%s/%s", known_providers[p], cur);
            sv_add(&v, buf);
        }
    }
    free(base_snapshot);

    int mid_n = v.count;
    char **mid_snapshot = malloc((size_t)mid_n * sizeof(char *));
    for (int i = 0; i < mid_n; i++) mid_snapshot[i] = v.items[i];
    for (int i = 0; i < mid_n; i++) {
        const char *cur = mid_snapshot[i];
        int slashes = 0;
        for (const char *p = cur; *p; p++) if (*p == '/') slashes++;
        if (slashes != 1) continue;
        for (int a = 0; known_aggregators[a]; a++) {
            size_t L = strlen(known_aggregators[a]) + 1 + strlen(cur) + 1;
            char *buf = malloc(L);
            snprintf(buf, L, "%s/%s", known_aggregators[a], cur);
            sv_add(&v, buf);
        }
    }
    free(mid_snapshot);

    char cnt[32];
    snprintf(cnt, sizeof cnt, "%d", v.count);
    for (int i = 0; i < v.count; i++) WCUP_VALSTR(v.items[i]);
    WCUP_VALSTR(cnt);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("reasoning_canonical_model_variants");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s reasoning_canonical_model_variants <model>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_variants(argv[i]);
    return 0;
}
