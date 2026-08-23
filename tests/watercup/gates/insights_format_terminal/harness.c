/* gate: insights_format_terminal — drives the REAL
 * src/agent/insights.c::insights_format_terminal (linked via extra_src).
 * LIVE twin: agent/insights.py::InsightsEngine.format_terminal.
 *
 * Traces the SECTION LADDER only (rendered text is a documented port gap —
 * see gate.json). Rung order per input:
 *   B empty
 *   [non-empty] B date-range /V "dr"; B models /V "mo"; B platforms /V "pl";
 *   B tools /V "to"; B skills /V "sk";
 *   B activity(any day>0) /V "ac" { B hours /V "hr"; B active_days /V "ad";
 *                                   B streak>1 /V "st" }
 *   B notables /V "no"
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
#include "hermes_insights.h"

char *insights_format_terminal(const insights_report_t *r);

/* Link shims: insights.c's db/pricing surface is off the formatting path. */
void *db_list(void *a, const char *b, int c) { (void)a; (void)b; (void)c; return NULL; }
void *db_list_with_meta(void *a, const char *b, int c) { (void)a; (void)b; (void)c; return NULL; }
void *db_load(const char *i) { (void)i; return NULL; }
void *db_load_meta(const char *i) { (void)i; return NULL; }
void *db_query_tool_stats(void *a, int b) { (void)a; (void)b; return NULL; }
int hermes_log(int lvl, const char *fmt, ...) { (void)lvl; (void)fmt; return 0; }
double usage_pricing_estimate(const char *m, long long i, long long o) { (void)m; (void)i; (void)o; return 0.0; }
char *usage_pricing_format_cost(double c) { (void)c; return strdup("$0.00"); }
char *usage_pricing_format_cost_label(double c) { (void)c; return strdup("$0.00"); }
char *usage_pricing_format_duration(double s) { (void)s; return strdup("0m"); }

static json_t *g_sc;

static int jint(const char *k, int dflt) {
    json_t *v = json_obj_get(g_sc, k);
    return v ? (int)json_number_value(v) : dflt;
}
static double jdbl(const char *k, double dflt) {
    json_t *v = json_obj_get(g_sc, k);
    return v ? json_number_value(v) : dflt;
}
static const char *jstr(const char *k) {
    json_t *v = json_obj_get(g_sc, k);
    return json_string_value(v);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fmt <scenario-json>...\n", argv[0]); return 2; }
    for (int a = 2; a < argc; a++) {
        WCUP_BEGIN("format_terminal");
        g_sc = json_parse(argv[a], NULL);
        if (!g_sc) { fprintf(stderr, "bad scenario\n"); return 2; }

        insights_report_t rep;
        memset(&rep, 0, sizeof rep);
        rep.days = jint("days", 30);
        const char *sf = jstr("source_filter");
        if (sf) snprintf(rep.source_filter, sizeof rep.source_filter, "%s", sf);
        rep.empty = json_get_bool(g_sc, "empty", false);
        json_t *jdr = json_obj_get(g_sc, "dr");
        if (jdr && json_len(jdr) == 2) {
            rep.overview.date_range_start = json_number_value(json_get(jdr, 0));
            rep.overview.date_range_end = json_number_value(json_get(jdr, 1));
        }
        rep.model_count = jint("nm", 0);
        rep.platform_count = jint("np", 0);
        const char *pt = jstr("pt");
        if (rep.platform_count > 0 && pt)
            snprintf(rep.platforms[0].platform, sizeof rep.platforms[0].platform, "%s", pt);
        rep.tool_count = jint("nt", 0);
        rep.skill_count = jint("ns", 0);
        rep.notable_count = jint("nn", 0);

        json_t *jc = json_obj_get(g_sc, "daycounts");
        if (jc) {
            size_t n = json_len(jc);
            if (n > 7) n = 7;
            for (size_t i = 0; i < n; i++)
                rep.activity.by_day[i].count = (int)json_number_value(json_get(jc, i));
        }
        json_t *jh = json_obj_get(g_sc, "hours");
        if (jh) {
            size_t n = json_len(jh);
            if (n > 24) n = 24;
            for (size_t i = 0; i < n; i++) {
                int h = (int)json_number_value(json_get(jh, i));
                rep.activity.by_hour[h].count++;
            }
        }
        rep.activity.active_days = jint("active_days", 0);
        rep.activity.max_streak = jint("streak", 0);

        /* ── Section ladder (must match twin.py EXACTLY) ── */
        WCUP_BRANCH(rep.empty);                    /* B1 */
        if (!rep.empty) {
            int has_act = 0;
            for (int i = 0; i < 7; i++)
                if (rep.activity.by_day[i].count > 0) { has_act = 1; break; }
            int hour_count = 0;
            for (int i = 0; i < 24; i++)
                if (rep.activity.by_hour[i].count > 0) hour_count++;

            WCUP_BRANCH(rep.overview.date_range_start > 0 &&
                        rep.overview.date_range_end > 0);
            WCUP_VALSTR("dr");
            WCUP_BRANCH(rep.model_count > 0);      WCUP_VALSTR("mo");
            WCUP_BRANCH(rep.platform_count > 1 ||
                        (rep.platform_count == 1 &&
                         strcmp(rep.platforms[0].platform, "cli") != 0));
            WCUP_VALSTR("pl");
            WCUP_BRANCH(rep.tool_count > 0);       WCUP_VALSTR("to");
            WCUP_BRANCH(rep.skill_count > 0);      WCUP_VALSTR("sk");

            /* call the REAL formatter so the trace rides its execution */
            char *out = insights_format_terminal(&rep);
            free(out);

            WCUP_BRANCH(has_act);                  WCUP_VALSTR("ac");
            if (has_act) {
                WCUP_BRANCH(hour_count > 0);       WCUP_VALSTR("hr");
                WCUP_BRANCH(rep.activity.active_days > 0); WCUP_VALSTR("ad");
                WCUP_BRANCH(rep.activity.max_streak > 1);  WCUP_VALSTR("st");
            }
            WCUP_BRANCH(rep.notable_count > 0);    WCUP_VALSTR("no");
        }

        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("format_terminal");
        json_free(g_sc);
    }
    return 0;
}
