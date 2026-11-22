/* t_port_codex_event_content.c — harness for auxiliary_client_gaps.c codex_event_has_content.
 * Reads a JSON event object, emits {"event":...,"out":0|1}.
 * Gate: run_one_oracle.sh codex_event_content oracle_codex_event_content.py ""
 *       "src/agent/auxiliary_client_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int codex_event_has_content(const json_t *event);

int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        int out = codex_event_has_content(in);
        printf("{\"event\":");
        char *s = json_dumps(in, 0);
        if (s) { fputs(s, stdout); free(s); } else fputs("null", stdout);
        printf(",\"out\":%d}\n", out);
        json_free(in);
    }
    return 0;
}
