/* t_port_get_context_timeout.c — harness for agent/prompt_builder_gaps.c
 * get_context_file_read_timeout. Reads {"val": <config value or null>}, emits
 * {"val":..., "out":<double>}. Oracle replays LIVE Python with _config_readonly
 * stubbed to return the emitted value.
 * Gate: run_one_oracle.sh get_context_timeout oracle_get_context_timeout.py ""
 *       "src/agent/prompt_builder_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

double get_context_file_read_timeout(const json_t *cfg);

int main(void) {
    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *val = json_obj_get(in, "val");  /* borrowed, may be NULL */
        double out = get_context_file_read_timeout(val);
        printf("{\"val\":");
        if (val) { char *s = json_dumps(val, 0); if (s) { fputs(s, stdout); free(s); } else fputs("null", stdout); }
        else fputs("null", stdout);
        printf(",\"out\":%.17g}\n", out);
        json_free(in);
    }
    return 0;
}
