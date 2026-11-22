/* t_port_real_profile_pin.c — harness for browser_connect_gaps.c real_profile_pin.
 * Reads {"value":<json>}, emits {"value":...,"out":<string|null>}.
 * Gate: run_one_oracle.sh real_profile_pin oracle_real_profile_pin.py ""
 *       "src/hermes_cli/browser_connect_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *real_profile_pin(const json_t *value);

int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *v = json_obj_get(in, "value");
        const char *out = real_profile_pin(v);
        printf("{\"value\":");
        if (v) { char *s=json_dumps(v,0); if(s){fputs(s,stdout);free(s);} else fputs("null",stdout); } else fputs("null",stdout);
        printf(",\"out\":");
        if (out) { printf("\""); fputs(out, stdout); printf("\""); } else fputs("null", stdout);
        printf("}\n");
        json_free(in);
    }
    return 0;
}
