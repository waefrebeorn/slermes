/* t_port_abort_complete.c — harness for update_abort_recovery_gaps.c
 * abort_recovery_is_complete. Reads {"planned":[...],"covered":[...],
 * "result":{...},"stale":[...]}, emits input + out.
 * Gate: run_one_oracle.sh abort_complete oracle_abort_complete.py ""
 *       "src/hermes_cli/update_abort_recovery_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int abort_recovery_is_complete(const json_t *planned, const json_t *covered,
                               const json_t *result, const json_t *stale);

static void emit(json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    if (v) { char *s = json_dumps(v, 0); if (s) { fputs(s, stdout); free(s); } }
    else fputs("null", stdout);
}
int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        int out = abort_recovery_is_complete(json_obj_get(in, "planned"),
                                             json_obj_get(in, "covered"),
                                             json_obj_get(in, "result"),
                                             json_obj_get(in, "stale"));
        printf("{\"planned\":"); emit(in, "planned");
        printf(",\"covered\":"); emit(in, "covered");
        printf(",\"result\":"); emit(in, "result");
        printf(",\"stale\":"); emit(in, "stale");
        printf(",\"out\":%d}\n", out);
        json_free(in);
    }
    return 0;
}
