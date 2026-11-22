/* t_port_cron_jobs_rewrite.c — fixture harness. Emits the exact inputs it used
 * plus the rewritten result and the post-save job list.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

json_t *cj_rewrite_skill_refs(const json_t *jobs, const json_t *consolidated, const json_t *pruned);
static void emit(json_t *v) { char *s = json_dumps(v, 0); fputs(s ? s : "null", stdout); free(s); }

int main(void) {
    char line[32768];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *a = json_obj_get(in, "args");
        if (!a) a = json_object();
        json_t *jobs = json_obj_get(in, "jobs");
        if (!jobs) jobs = json_array();
        json_t *out = cj_rewrite_skill_refs(jobs, json_obj_get(a, "consolidated"), json_obj_get(a, "pruned"));
        fputs("{\"args\":", stdout); emit(a);
        fputs(",\"jobs\":", stdout); emit(jobs);
        fputs(",\"out\":", stdout); emit(out);
        fputs("}\n", stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
