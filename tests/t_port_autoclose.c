/* t_port_autoclose.c — harness for browser_connect_gaps.c real_profile_autoclose.
 * Reads {"setting":<json>}, emits {"setting":<json>,"out":0|1}. Oracle replays
 * LIVE _real_profile_autoclose with _browser_setting stubbed from the input.
 * Gate: run_one_oracle.sh autoclose oracle_autoclose.py ""
 *       "src/hermes_cli/browser_connect_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int real_profile_autoclose(const json_t *setting);

int main(void) {
    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *val = json_obj_get(in, "setting");
        int out = real_profile_autoclose(val);
        printf("{\"setting\":");
        if (val) { char *s=json_dumps(val,0); if(s){fputs(s,stdout);free(s);} else fputs("null",stdout); }
        else fputs("null",stdout);
        printf(",\"out\":%d}\n", out);
        json_free(in);
    }
    return 0;
}
