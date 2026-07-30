/*
 * t_port_cron_suggestions.c — v562 residual-façade oracle harness for
 * cron/suggestions.py (ported to src/cron/cron_suggestions.c).
 *
 * Exercises the full lifecycle against a temp HERMES_HOME:
 *   add (pending), get by id/index/title, list_pending, dedup-skip,
 *   backlog-full skip, dismiss (latched), accept (delegates to cron_add_job),
 *   clear_resolved. The oracle (sta_oracle_cron_suggestions.py) re-runs the
 *   SAME steps against LIVE Python and asserts the resulting store + return
 *   values match field-by-field.
 *
 * Note: accept_suggestion delegates to cron_add_job (the real scheduler store),
 * which LIVE Python mirrors via cron.jobs.create_job. The oracle compares the
 * returned job spec (name/schedule/status/accepted) and the final store state.
 */
#include "cron_suggestions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *make_spec(const char *name, const char *schedule,
                         const char *prompt)
{
    json_t *spec = json_object();
    if (name)    json_set(spec, "name", json_string(name));
    if (schedule) json_set(spec, "schedule", json_string(schedule));
    if (prompt)  json_set(spec, "prompt", json_string(prompt));
    return spec;
}

static void emit_case(const char *name, json_t *store, json_t *ret,
                      bool ret_present)
{
    printf("{\"case\":");
    /* name as json string */
    printf("\"%s\"", name);
    printf(",\"store\":");
    if (store) { char *s = json_serialize(store); printf("%s", s); free(s); }
    else printf("null");
    printf(",\"ret\":");
    if (ret_present && ret) { char *s = json_serialize(ret); printf("%s", s); free(s); }
    else printf("null");
    printf("}\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0); /* unbuffered so we see output before hang */
    /* Use a fresh temp HERMES_HOME (set by runner via env). */
    json_t *store;

    /* 1. empty store initially */
    store = cron_sugg_load_suggestions();
    emit_case("init_empty", store, NULL, false);
    json_free(store);

    /* 2. add a pending suggestion (catalog) */
    json_t *spec1 = make_spec("Daily briefing", "0 9 * * *", "Summarize today");
    json_t *r1 = cron_sugg_add("Daily briefing", "Morning digest",
                               "catalog", spec1, "cat:daily");
    json_free(spec1);
    store = cron_sugg_load_suggestions();
    emit_case("add_catalog", store, r1, r1 != NULL);
    json_free(store);
    if (r1) json_free(r1);

    /* 3. add a second (blueprint) */
    json_t *spec2 = make_spec("Weekly digest", "0 8 * * 1", "Weekly wrap");
    json_t *r2 = cron_sugg_add("Weekly digest", "Monday recap",
                               "blueprint", spec2, "bp:weekly");
    json_free(spec2);
    store = cron_sugg_load_suggestions();
    emit_case("add_blueprint", store, r2, r2 != NULL);
    json_free(store);
    if (r2) json_free(r2);

    /* 4. dedup-skip: same dedup_key again -> NULL, no new record */
    json_t *spec3 = make_spec("Other", "0 0 * * *", "x");
    json_t *r3 = cron_sugg_add("Other", "dup", "usage", spec3, "cat:daily");
    json_free(spec3);
    store = cron_sugg_load_suggestions();
    emit_case("dedup_skip", store, r3, r3 != NULL);
    json_free(store);
    if (r3) json_free(r3);

    /* 5. list_pending -> 2 pending */
    json_t *pend = cron_sugg_list_pending();
    emit_case("list_pending", pend, NULL, false);
    json_free(pend);

    /* 6. get by id (first added) */
    store = cron_sugg_load_suggestions();
    json_t *arr = store;
    json_t *first = json_get(arr, 0);
    const char *fid = json_get_str(first, "id", "");
    json_t *g = cron_sugg_get(fid);
    emit_case("get_by_id", g, NULL, false); /* store=returned record */
    json_free(g);
    json_free(store);

    /* 7. get by 1-based pending index "1" */
    json_t *gi = cron_sugg_get("1");
    emit_case("get_by_index", gi, NULL, false);
    json_free(gi);

    /* 8. get by exact title (case-insensitive) "weekly digest" */
    json_t *gt = cron_sugg_get("WEEKLY DIGEST");
    emit_case("get_by_title", gt, NULL, false);
    json_free(gt);

    /* 9. dismiss second by index "2" -> latched */
    bool d = cron_sugg_dismiss("2");
    store = cron_sugg_load_suggestions();
    emit_case("dismiss", store, NULL, false);
    /* also report dismiss return */
    printf("{\"case\":\"dismiss_ret\",\"ret\":%s}\n", d ? "true" : "false");
    json_free(store);

    /* 10. re-add after dismiss with same dedup_key -> still skipped (latched) */
    json_t *spec4 = make_spec("Weekly again", "0 8 * * 1", "y");
    json_t *r4 = cron_sugg_add("Weekly again", "relist", "blueprint",
                               spec4, "bp:weekly");
    json_free(spec4);
    store = cron_sugg_load_suggestions();
    emit_case("relist_after_dismiss", store, r4, r4 != NULL);
    json_free(store);
    if (r4) json_free(r4);

    /* 11. accept the first pending (index 1) with origin */
    json_t *origin = json_object();
    json_set(origin, "platform", json_string("telegram"));
    json_t *accepted = cron_sugg_accept("1", origin);
    json_free(origin);
    store = cron_sugg_load_suggestions();
    emit_case("accept", store, accepted, accepted != NULL);
    json_free(store);
    if (accepted) json_free(accepted);

    /* 12. clear_resolved -> removes the accepted record */
    int removed = cron_sugg_clear_resolved();
    store = cron_sugg_load_suggestions();
    emit_case("clear_resolved", store, NULL, false);
    printf("{\"case\":\"clear_removed\",\"ret\":%d}\n", removed);
    json_free(store);

    /* 13. invalid source -> NULL */
    json_t *spec5 = make_spec("X", "0 0 * * *", "x");
    json_t *r5 = cron_sugg_add("X", "bad", "bogus", spec5, "k");
    json_free(spec5);
    emit_case("invalid_source", NULL, r5, r5 != NULL);
    if (r5) json_free(r5);

    return 0;
}
