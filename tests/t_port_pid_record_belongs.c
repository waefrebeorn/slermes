/* t_port_pid_record_belongs.c — harness for status_gaps.c
 * pid_record_belongs_to_current_profile. Reads {"record":<json>,"process_home":<str>},
 * emits {"out":0|1}. Gate: run_one_oracle.sh pid_record_belongs
 * oracle_pid_record_belongs.py "" "src/gateway/status_gaps.o src/gateway/port_gateway_status.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int pid_record_belongs_to_current_profile(const json_t *record, const char *process_home);

int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *rec = json_obj_get(in, "record");
        json_t *ph = json_obj_get(in, "process_home");
        const char *phs = (ph && ph->type == JSON_STRING) ? ph->str_val : "";
        int out = pid_record_belongs_to_current_profile(rec, phs);
        printf("{\"record\":");
        if (rec) { char *s=json_dumps(rec,0); if(s){fputs(s,stdout);free(s);} else fputs("null",stdout); } else fputs("null",stdout);
        json_t *phj = json_string(phs);
        printf(",\"process_home\":");
        if (phj) { char *s=json_dumps(phj,0); if(s){fputs(s,stdout);free(s);} else fputs("\"\"",stdout); json_free(phj); } else fputs("\"\"",stdout);
        printf(",\"out\":%d}\n", out);
        json_free(in);
    }
    return 0;
}
