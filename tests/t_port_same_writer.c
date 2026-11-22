/* t_port_same_writer.c — harness for active_sessions_gaps.c is_same_writer.
 * Reads {"entry":...,"metadata":...,"pid":<int>}, emits input + out.
 * Gate: run_one_oracle.sh same_writer oracle_same_writer.py ""
 *       "src/hermes_cli/active_sessions_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int is_same_writer(const json_t *entry, const json_t *metadata, int current_pid);

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
        json_t *pidv = json_obj_get(in, "pid");
        int pid = (pidv && pidv->type == JSON_NUMBER) ? (int)pidv->num_val : 0;
        int out = is_same_writer(json_obj_get(in, "entry"), json_obj_get(in, "metadata"), pid);
        printf("{\"entry\":"); emit(in, "entry");
        printf(",\"metadata\":"); emit(in, "metadata");
        printf(",\"pid\":"); emit(in, "pid");
        printf(",\"out\":%d}\n", out);
        json_free(in);
    }
    return 0;
}
