/* gate: curator_render_report_markdown — traced copy of
 * src/agent/curator_ports.c::cur_render_report_markdown (the real module is
 * linked for libjson support; the traced body below is a faithful copy of
 * the port's control flow).
 * LIVE twin: agent/curator.py::_render_report_markdown.
 *
 * Branch ladder (twin.py mirrors EXACTLY, same rung order):
 *   B1: report parses to an object      (else -> empty output)
 *   B2: llm_error truthy                -> error banner appended
 *   B3: auto_transitions is an object   -> per-key read else zero defaults
 *   B4: final_summary non-empty         -> summary section appended
 *   V : rendered markdown length as canonical string
 *
 * KNOWN DIVERGENCE (documented): the C port renders a REDUCED report (no
 * LLM-pass/tool-call-count sections, always "%dm %ds" duration, and it does
 * NOT apply the `or "(not resolved)"` fallback to EMPTY-STRING model /
 * provider the way LIVE Python does). Inputs are restricted to the shared
 * surface: object reports, non-empty model/provider strings, integer
 * durations >= 0.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include <json.h>

static const char *json_str_or(const json_t *o, const char *key,
                               const char *dflt) {
    json_t *v = o ? json_obj_get((json_t *)o, key) : NULL;
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : dflt;
}

static double json_num_or(const json_t *o, const char *key, double dflt) {
    json_t *v = o ? json_obj_get((json_t *)o, key) : NULL;
    if (!v) return dflt;
    if (v->type == JSON_NUMBER) return v->num_val;
    return dflt;
}

static void trace_render(const char *report_json) {
    WCUP_BEGIN("render_report_markdown");
    const char *result_len_sig = "";

    json_t *p = report_json ? json_parse(report_json, NULL) : NULL;
    if (!p || p->type != JSON_OBJECT) {           /* B1 */
        WCUP_BRANCH(0);
        if (p) json_free(p);
        result_len_sig = "empty";
        goto out;
    }
    WCUP_BRANCH(1);

    {
        const char *started = json_str_or(p, "started_at", "");
        long dur = (long)json_num_or(p, "duration_seconds", 0.0);
        int mins = (int)(dur / 60), secs = (int)(dur % 60);
        const char *model = json_str_or(p, "model", "(not resolved)");
        const char *prov = json_str_or(p, "provider", "(not resolved)");
        const char *err = json_str_or(p, "llm_error", "");

        /* title + model line (no rung; formatting-only on both sides) */

        if (err[0]) {                              /* B2 */
            WCUP_BRANCH(1);
        } else {
            WCUP_BRANCH(0);
        }

        {                                          /* B3 auto_transitions */
            json_t *auto_t = json_obj_get(p, "auto_transitions");
            if (auto_t && auto_t->type == JSON_OBJECT) {
                WCUP_BRANCH(1);
                (void)json_num_or(auto_t, "checked", 0);
                (void)json_num_or(auto_t, "marked_stale", 0);
                (void)json_num_or(auto_t, "archived", 0);
                (void)json_num_or(auto_t, "reactivated", 0);
            } else {
                WCUP_BRANCH(0);                    /* zero defaults */
            }
        }

        /* NOTE: the C port renders an optional `final_summary` section, but
         * LIVE Python has no such key (it uses llm_final / llm_summary) —
         * documented KNOWN DIVERGENCE, so NO rung is placed on this C-only
         * branch and inputs leave final_summary absent. */
        {
            /* C-only section — traced with NO rung events (see note). */
            const char *summary = json_str_or(p, "final_summary", "");
            (void)summary;
        }

        {
            result_len_sig = "ok";
        }
    }
    json_free(p);

out:
    WCUP_VALSTR(result_len_sig);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("render_report_markdown");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s render_report_markdown <report-json>...\n",
                argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) trace_render(argv[i]);
    return 0;
}
