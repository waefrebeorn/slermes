/*
 * blueprint_cmd_test.c — behavioral tests for blueprint_cmd.c
 * Validates the forgiving resolution + KV parsing + seed/catalog formatters.
 */
#include "blueprint_cmd.h"
#include "blueprint_catalog_common.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int failures = 0, checks = 0;
#define CK(cond, msg) do { checks++; if(!(cond)){ printf("FAIL: %s\n", msg); failures++; } } while(0)

static void free_strv(char **v, int n) {
    if (!v) return;
    for (int i = 0; i < n; i++) free(v[i]);
    free(v);
}

static blueprint_catalog_t *load(void) {
    blueprint_catalog_t *c = blueprint_catalog_load_json(blueprint_catalog_raw_json());
    assert(c && blueprint_catalog_count(c) > 0);
    return c;
}

int main(void) {
    blueprint_catalog_t *cat = load();
    CK(blueprint_catalog_count(cat) == 14, "catalog has 14 blueprints (baked)");

    /* ── parse_kv ───────────────────────────────────────────── */
    {
        char **k=NULL, **v=NULL, **left=NULL; int n=0, nl=0;
        blueprint_cmd_parse_kv("morning-brief time=08:00 deliver=telegram", &k,&v,&n,&left,&nl);
        CK(n == 2, "kv: 2 pairs");
        CK(strcmp(k[0],"time")==0 && strcmp(v[0],"08:00")==0, "kv: time=08:00");
        CK(strcmp(k[1],"deliver")==0 && strcmp(v[1],"telegram")==0, "kv: deliver=telegram");
        CK(nl == 1 && strcmp(left[0],"morning-brief")==0, "kv: query token is a leftover");
        free_strv(k,n); free_strv(v,n); free_strv(left,nl);
    }
    /* quoted value keeps spaces (shlex fidelity) */
    {
        char **k=NULL, **v=NULL, **left=NULL; int n=0, nl=0;
        blueprint_cmd_parse_kv("important-mail criteria=\"from my boss\"", &k,&v,&n,&left,&nl);
        CK(n == 1, "kv quoted: 1 pair");
        CK(strcmp(v[0],"from my boss")==0, "kv quoted: value keeps spaces");
        free_strv(k,n); free_strv(v,n); free_strv(left,nl);
    }

    /* ── match: exact key ───────────────────────────────────── */
    {
        char *mk=NULL; char **cand=NULL; int nc=0;
        int r = blueprint_cmd_match(cat, "morning-brief", &mk, &cand, &nc);
        CK(r == 1 && mk && strcmp(mk,"morning-brief")==0, "match: exact key");
        CK(nc == 0, "match: exact => no candidates");
        free(mk); free_strv(cand,nc);
    }
    /* prefix (unique) */
    {
        char *mk=NULL; char **cand=NULL; int nc=0;
        int r = blueprint_cmd_match(cat, "morn", &mk, &cand, &nc);
        CK(r == 1 && mk && strcmp(mk,"morning-brief")==0, "match: unique prefix 'morn'");
        free(mk); free_strv(cand,nc);
    }
    /* ambiguous prefix -> candidates (weekly-review key + meal-plan title word "weekly") */
    {
        char *mk=NULL; char **cand=NULL; int nc=0;
        int r = blueprint_cmd_match(cat, "week", &mk, &cand, &nc);
        CK(r == 0 && nc >= 2, "match: 'week' ambiguous -> >=2 candidates");
        int has_weekly = 0, has_meal = 0;
        for (int i=0;i<nc;i++){ if(strcmp(cand[i],"weekly-review")==0) has_weekly=1; if(strcmp(cand[i],"meal-plan")==0) has_meal=1; }
        CK(has_weekly && has_meal, "match: week candidates include weekly-review + meal-plan");
        free(mk); free_strv(cand,nc);
    }
    /* unique by title-word prefix ("brief" -> morning-brief) */
    {
        char *mk=NULL; char **cand=NULL; int nc=0;
        int r = blueprint_cmd_match(cat, "brief", &mk, &cand, &nc);
        CK(r == 1 && mk && strcmp(mk,"morning-brief")==0, "match: 'brief' -> morning-brief (title word)");
        free(mk); free_strv(cand,nc);
    }
    /* fuzzy typo tolerance */
    {
        char *mk=NULL; char **cand=NULL; int nc=0;
        int r = blueprint_cmd_match(cat, "mornig-brief", &mk, &cand, &nc);
        CK(r == 1 && mk && strcmp(mk,"morning-brief")==0, "match: fuzzy 'mornig-brief' -> morning-brief");
        free(mk); free_strv(cand,nc);
    }
    /* no match */
    {
        char *mk=NULL; char **cand=NULL; int nc=0;
        int r = blueprint_cmd_match(cat, "zzzznotathing", &mk, &cand, &nc);
        CK(r == 0 && mk == NULL, "match: no match -> NULL key");
        free(mk); free_strv(cand,nc);
    }

    /* ── formatters ─────────────────────────────────────────── */
    {
        char *c = blueprint_cmd_format_catalog(cat);
        CK(c && strstr(c, "morning-brief") && strstr(c, "Automation Blueprints"), "format catalog lists blueprints");
        free(c);
    }
    {
        char *c = blueprint_cmd_format_no_match(cat, "morin-brief");
        CK(c && strstr(c, "morning-brief"), "no-match suggests 'morning-brief'");
        free(c);
    }

    /* ── build seed ─────────────────────────────────────────── */
    {
        char *s = blueprint_cmd_build_seed(cat, "morning-brief");
        CK(s != NULL, "seed: built for morning-brief");
        CK(strstr(s, "Morning briefing"), "seed: contains title");
        CK(strstr(s, "cronjob tool"), "seed: instructs cronjob tool");
        CK(strstr(s, "What time?"), "seed: lists the time slot");
        free(s);
        char *nope = blueprint_cmd_build_seed(cat, "does-not-exist");
        CK(nope == NULL, "seed: unknown key -> NULL");
    }

    blueprint_catalog_free(cat);
    printf("blueprint_cmd_test: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
