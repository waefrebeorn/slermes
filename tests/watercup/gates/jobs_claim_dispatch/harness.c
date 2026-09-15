/* gate: jobs_claim_dispatch — traced copy of the REAL C port
 * src/cron/jobs.c::cronjobs_claim_dispatch decision ladder (the storage
 * half — lock/load/save against $SLERMES_HOME/cron/jobs.json — is I/O out
 * of the traced path; the job arrives via argv instead).
 * LIVE twin: cron/jobs.py::claim_dispatch with load_jobs/save_jobs/_jobs_lock
 * monkeypatched to serve the same job (REAL claim logic, stubbed I/O only).
 *
 * Input argv format: "<job_id><TAB><job_json>".
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: job found            B2: schedule.kind == "once"
 *   B3: repeat object present   B4: times is a positive number
 *   B5: completed >= times (over-limit -> remove)
 *   VSTR(result "True"/"False")  VSTR(new_completed or "-")
 *
 * DOCUMENTED DIVERGENCE avoided: LIVE retains an over-limit job that has a
 * last_run_at record (disables it instead of deleting); the C port deletes
 * unconditionally. Inputs never set last_run_at on an over-limit job.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"
#include "hermes_json.h"

static void decode_escapes(char *s) {
    char *o = s;
    for (char *p = s; *p; p++) {
        if (p[0] == '\\' && p[1] == 't') { *o++ = '\t'; p++; }
        else *o++ = *p;
    }
    *o = '\0';
}

static void run_one(const char *input) {
    char buf[4096];
    snprintf(buf, sizeof buf, "%s", input);
    decode_escapes(buf);
    char *tab = strchr(buf, '\t');
    const char *job_id = buf;
    const char *job_json = "";
    if (tab) { *tab = '\0'; job_id = buf; job_json = tab + 1; }

    WCUP_BEGIN("claim_dispatch");

    char *jerr = NULL;
    json_t *job = json_parse(job_json, &jerr);
    free(jerr);

    bool ret = true;
    long new_completed = -1;
    /* Traced copy of cronjobs_claim_dispatch's decision ladder. */
    bool found = job != NULL &&
                 strcmp(json_get_str(job, "id", ""), job_id) == 0;
    WCUP_BRANCH(found);                                      /* B1 */
    if (found) {
        const char *kind = json_object_get_string(json_object_get(job, "schedule"),
                                                  "kind", "");
        bool once = strcmp(kind, "once") == 0;
        WCUP_BRANCH(once);                                   /* B2 */
        if (once) {
            json_t *repeat = json_object_get(job, "repeat");
            bool rep_ok = json_is_object(repeat);
            WCUP_BRANCH(rep_ok);                             /* B3 */
            if (rep_ok) {
                json_t *times_v = json_object_get(repeat, "times");
                bool times_pos = json_is_number(times_v) &&
                                 (int)json_number_value(times_v) > 0;
                WCUP_BRANCH(times_pos);                      /* B4 */
                if (times_pos) {
                    int times = (int)json_number_value(times_v);
                    int completed = (int)json_get_num(repeat, "completed", 0);
                    bool over = completed >= times;
                    WCUP_BRANCH(over);                       /* B5 */
                    if (!over) {
                        new_completed = completed + 1;
                        ret = true;
                    } else {
                        ret = false;   /* stale job removed */
                    }
                }
            }
        }
    }

    char nbuf[32];
    if (new_completed >= 0) snprintf(nbuf, sizeof nbuf, "%ld", new_completed);
    else snprintf(nbuf, sizeof nbuf, "-");

    WCUP_VALSTR(ret ? "True" : "False");
    WCUP_VALSTR(nbuf);

    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    json_free(job);
    WCUP_END("claim_dispatch");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s claim_dispatch <id>\\t<jobjson>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
