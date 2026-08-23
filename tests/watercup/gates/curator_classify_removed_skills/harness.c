/* gate: curator_classify_removed_skills — drives the REAL
 * src/agent/curator.c::classify_removed_skills (linked via extra_src).
 * LIVE twin: agent/curator.py::_classify_removed_skills.
 *
 * Input: scenario JSON {removed[], added[], after[], calls[]}; calls[] are
 * skill_manage arguments JSON strings.
 * Rung order (must match twin.py EXACTLY), per removed skill in order:
 *   B   consolidated found
 *   V   "name->into" (into empty when pruned)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"
#include "hermes_json.h"
#include "hermes_curator.h"

int classify_removed_skills(const char *removed[], int n_removed,
                            const char *added[], int n_added,
                            const char *after_names[], int n_after,
                            const char *tool_call_args[], int n_calls,
                            curator_consolidation_t consolidated[], int max_cons,
                            char *pruned[], int max_pruned);

/* Link shims: curator.c's usage/cron surface is off the classification path. */
void *cron_list_jobs(void) { return NULL; }
void *skill_usage_load(void) { return NULL; }
long skill_usage_activity_count(void *s, const char *k) { (void)s; (void)k; return 0; }
void skill_usage_archive(void *s, const char *k) { (void)s; (void)k; }
double skill_usage_latest_activity(void *s, const char *k) { (void)s; (void)k; return 0; }
void skill_usage_set_state(void *s, const char *k, int v) { (void)s; (void)k; (void)v; }

#define MAXN 16

static const char *arr_str(const json_t *a, size_t i) {
    json_t *it = json_get((json_t *)a, i);
    return json_string_value(it);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s classify <scenario-json>...\n", argv[0]); return 2; }
    for (int a = 2; a < argc; a++) {
        WCUP_BEGIN("classify_removed_skills");
        json_t *sc = json_parse(argv[a], NULL);
        if (!sc) { fprintf(stderr, "bad scenario\n"); return 2; }

        const json_t *j_removed = json_obj_get(sc, "removed");
        const json_t *j_added = json_obj_get(sc, "added");
        const json_t *j_after = json_obj_get(sc, "after");
        const json_t *j_calls = json_obj_get(sc, "calls");

        const char *removed[MAXN] = {0}, *added[MAXN] = {0};
        const char *after[MAXN] = {0}, *calls[MAXN] = {0};
        int n_removed = 0, n_added = 0, n_after = 0, n_calls = 0;
        for (size_t i = 0; j_removed && i < json_len(j_removed) && n_removed < MAXN; i++)
            removed[n_removed++] = arr_str(j_removed, i);
        for (size_t i = 0; j_added && i < json_len(j_added) && n_added < MAXN; i++)
            added[n_added++] = arr_str(j_added, i);
        for (size_t i = 0; j_after && i < json_len(j_after) && n_after < MAXN; i++)
            after[n_after++] = arr_str(j_after, i);
        for (size_t i = 0; j_calls && i < json_len(j_calls) && n_calls < MAXN; i++)
            calls[n_calls++] = arr_str(j_calls, i);

        curator_consolidation_t cons[MAXN];
        char *pruned[MAXN] = {0};
        classify_removed_skills(removed, n_removed, added, n_added,
                                after, n_after, calls, n_calls,
                                cons, MAXN, pruned, MAXN);

        /* Emit one B/V rung pair per removed skill, in input order. */
        for (int r = 0; r < n_removed; r++) {
            const char *name = removed[r] ? removed[r] : "";
            const char *into = NULL;
            for (int c = 0; c < MAXN; c++) {
                if (cons[c].name[0] && strcmp(cons[c].name, name) == 0) {
                    into = cons[c].into;
                    break;
                }
            }
            WCUP_BRANCH(into != NULL);
            char val[512];
            snprintf(val, sizeof val, "%s->%s", name, into ? into : "");
            WCUP_VALSTR(val);
        }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("classify_removed_skills");
        json_free(sc);
    }
    return 0;
}
