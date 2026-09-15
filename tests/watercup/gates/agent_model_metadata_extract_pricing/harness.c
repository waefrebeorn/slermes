/* gate: agent_model_metadata_extract_pricing — traced copy of
 * src/agent/provider_metadata.c::provider_extract_pricing.
 * LIVE twin: agent/model_metadata.py::_extract_pricing.
 *
 * Shared branch ladder (twin.py mirrors EXACTLY):
 *   B0: payload not an object -> NULL/{}
 *   B1: novita keys present (numeric)
 *       B1a input_token_price_per_m -> "prompt"
 *       B1b output_token_price_per_m -> "completion"
 *   LOOP dicts = payload + first-level child objects (insertion order):
 *     B2 dict has any pricing alias key?
 *       LOOP 5 target groups in fixed order:
 *         B3 group matched (alias present, value not null/"")
 *       B4 pricing non-empty -> return
 *   B5 exhausted -> {}/NULL
 *   V-event: sorted comma-joined result keys ("prompt", ...).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"

/* ---- tiny JSON: object with flat or one-level-nested entries ---- */
typedef struct JV JV;
struct JV {
    int type;            /* 0 num, 1 str, 2 obj, 3 null */
    double num;
    char *str;
    /* object members */
    char **keys;
    JV **vals;
    int n;
};

static void skipws(const char **p) { while (**p==' '||**p=='\t'||**p=='\n'||**p=='\r') (*p)++; }

static JV *parse_obj(const char **p, int depth);

static char *parse_str(const char **p) {
    skipws(p);
    if (**p != '"') return NULL;
    (*p)++;
    const char *s = *p;
    while (**p && **p != '"') (*p)++;
    size_t n = (size_t)(*p - s);
    if (**p) (*p)++;
    char *out = malloc(n + 1);
    memcpy(out, s, n); out[n] = '\0';
    return out;
}

static JV *jnum(double d) { JV *v = calloc(1, sizeof *v); v->type = 0; v->num = d; return v; }
static JV *jstr(char *s)  { JV *v = calloc(1, sizeof *v); v->type = 1; v->str = s; return v; }
static JV *jnull(void)    { JV *v = calloc(1, sizeof *v); v->type = 3; return v; }

static JV *parse_val(const char **p, int depth);

static JV *parse_any(const char **p, int depth) {
    skipws(p);
    if (**p == '{') return parse_obj(p, depth);
    if (**p == '"') return jstr(parse_str(p));
    if (**p == 'n') { (*p) += 4; return jnull(); }
    char *end; double d = strtod(*p, &end); *p = end;
    return jnum(d);
}

static JV *parse_obj(const char **p, int depth) {
    JV *o = calloc(1, sizeof *o);
    o->type = 2;
    o->keys = malloc(sizeof(char *) * 32);
    o->vals = malloc(sizeof(JV *) * 32);
    skipws(p); (*p)++;  /* { */
    while (1) {
        skipws(p);
        if (**p == '}') { (*p)++; break; }
        char *k = parse_str(p);
        skipws(p); (*p)++;  /* : */
        JV *v = parse_val(p, depth + 1);
        o->keys[o->n] = k; o->vals[o->n] = v; o->n++;
        skipws(p);
        if (**p == ',') { (*p)++; continue; }
    }
    return o;
}
static JV *parse_val(const char **p, int depth) {
    skipws(p);
    if (**p == '{') return parse_obj(p, depth);
    if (**p == '"') return jstr(parse_str(p));
    if (**p == 'n') { (*p) += 4; return jnull(); }
    char *end; double d = strtod(*p, &end); *p = end;
    return jnum(d);
}

static JV *obj_get(JV *o, const char *k) {
    if (!o || o->type != 2) return NULL;
    for (int i = 0; i < o->n; i++)
        if (strcmp(o->keys[i], k) == 0) return o->vals[i];
    return NULL;
}

static void lowercpy(char *d, const char *s) {
    size_t i = 0;
    for (; s[i]; i++) d[i] = (char)tolower((unsigned char)s[i]);
    d[i] = '\0';
}

static const char *const GROUPS[5][6] = {
    {"prompt", "input", "input_cost_per_token", "prompt_token_cost", NULL},
    {"completion", "output", "output_cost_per_token", "completion_token_cost", NULL},
    {"request", "request_cost", NULL},
    {"cache_read", "cached_prompt", "input_cache_read", "cache_read_cost_per_token", NULL},
    {"cache_write", "cache_creation", "input_cache_write", "cache_write_cost_per_token", NULL},
};
static const char *const TARGETS[5] =
    {"prompt", "completion", "request", "cache_read", "cache_write"};

static char *res_keys[8];
static int res_n;

static void record(const char *k) { res_keys[res_n++] = strdup(k); }

static void wc_extract_pricing(JV *payload) {
    WCUP_BEGIN("extract_pricing");
    res_n = 0;
    if (!payload || payload->type != 2) { WCUP_BRANCH(1); goto out; }  /* B0 */
    WCUP_BRANCH(0);
    {
        JV *ni = obj_get(payload, "input_token_price_per_m");
        JV *no = obj_get(payload, "output_token_price_per_m");
        if ((ni && ni->type == 0) || (no && no->type == 0)) {          /* B1 */
            WCUP_BRANCH(1);
            if (ni && ni->type == 0) { WCUP_BRANCH(1); record("prompt"); }     /* B1a */
            else WCUP_BRANCH(0);
            if (no && no->type == 0) { WCUP_BRANCH(1); record("completion"); } /* B1b */
            else WCUP_BRANCH(0);
            goto out;
        }
        WCUP_BRANCH(0);
    }
    {
        JV *dicts[16]; int nd = 0;
        dicts[nd++] = payload;
        for (int i = 0; i < payload->n && nd < 16; i++)
            if (payload->vals[i]->type == 2) dicts[nd++] = payload->vals[i];
        for (int d = 0; d < nd; d++) {                                  /* LOOP */
            WCUP_LOOP_TICK();
            JV *m = dicts[d];
            int has_key = 0;
            for (int g = 0; g < 5 && !has_key; g++)
                for (int a = 0; GROUPS[g][a] && !has_key; a++)
                    for (int i = 0; i < m->n; i++) {
                        char kl[256]; lowercpy(kl, m->keys[i]);
                        if (strcmp(kl, GROUPS[g][a]) == 0) { has_key = 1; break; }
                    }
            if (!has_key) { WCUP_BRANCH(0); continue; }                 /* B2 */
            WCUP_BRANCH(1);
            int found = 0;
            for (int g = 0; g < 5; g++) {                               /* LOOP */
                WCUP_LOOP_TICK();
                int got = 0;
                for (int a = 0; GROUPS[g][a] && !got; a++)
                    for (int i = 0; i < m->n; i++) {
                        char kl[256]; lowercpy(kl, m->keys[i]);
                        if (strcmp(kl, GROUPS[g][a]) != 0) continue;
                        JV *val = m->vals[i];
                        if (val->type == 3) continue;                    /* null */
                        if (val->type == 1 && (!val->str || !*val->str)) continue;
                        got = 1; break;
                    }
                if (got) { WCUP_BRANCH(1); record(TARGETS[g]); found++; } /* B3 */
                else WCUP_BRANCH(0);
            }
            if (found > 0) { WCUP_BRANCH(1); goto out; }                /* B4 */
            WCUP_BRANCH(0);
        }
        WCUP_BRANCH(0);                                                 /* B5 */
    }
out:;
    char joined[512]; joined[0] = '\0';
    char *sorted[8]; int sn = res_n;
    for (int i = 0; i < res_n; i++) sorted[i] = res_keys[i];
    for (int i = 0; i < sn; i++) for (int j = i + 1; j < sn; j++)
        if (strcmp(sorted[j], sorted[i]) < 0) { char *t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t; }
    for (int i = 0; i < sn; i++) {
        if (i) strcat(joined, ",");
        strcat(joined, sorted[i]);
    }
    for (int i = 0; i < sn; i++) free(res_keys[i]);
    WCUP_VALSTR(joined);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("extract_pricing");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s extract_pricing <json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        const char *p = argv[i];
        JV *v = parse_val(&p, 0);
        wc_extract_pricing(v);
    }
    return 0;
}
