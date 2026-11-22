/* t_port_session_db_timeout.c — harness for scheduler_gaps.c get_session_db_timeout.
 * Reads {"env":<json>,"config":<json>}, emits normalized input + out.
 * Gate: run_one_oracle.sh session_db_timeout oracle_session_db_timeout.py ""
 *       "src/cron/scheduler_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

double get_session_db_timeout(const json_t *env_value, const json_t *config_value);

int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *env = json_obj_get(in, "env");
        json_t *cfg = json_obj_get(in, "config");
        double out = get_session_db_timeout(env, cfg);
        printf("{\"env\":");
        if (env) { char *s=json_dumps(env,0); if(s){fputs(s,stdout);free(s);} else fputs("null",stdout); } else fputs("null",stdout);
        printf(",\"config\":");
        if (cfg) { char *s=json_dumps(cfg,0); if(s){fputs(s,stdout);free(s);} else fputs("null",stdout); } else fputs("null",stdout);
        printf(",\"out\":%.17g}\n", out);
        json_free(in);
    }
    return 0;
}
