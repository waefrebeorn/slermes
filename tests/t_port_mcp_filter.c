/* t_port_mcp_filter.c — harness for mcp_startup_gaps.c set_mcp_server_filter.
 * Reads {"toolsets":<str|array|null>}, emits input + out (array or null).
 * Gate: run_one_oracle.sh mcp_filter oracle_mcp_filter.py ""
 *       "src/hermes_cli/mcp_startup_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

json_t *set_mcp_server_filter(const json_t *toolsets);

int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *out = set_mcp_server_filter(json_obj_get(in, "toolsets"));
        printf("{\"toolsets\":");
        json_t *v = json_obj_get(in, "toolsets");
        if (v) { char *s = json_dumps(v, 0); if (s) { fputs(s, stdout); free(s); } }
        else fputs("null", stdout);
        printf(",\"out\":");
        if (out) { char *s = json_dumps(out, 0); if (s) { fputs(s, stdout); free(s); } }
        else fputs("null", stdout);
        printf("}\n");
        json_free(out); json_free(in);
    }
    return 0;
}
