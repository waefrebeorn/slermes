/* gate: _enabled_mcp_servers(config) -> sorted enabled MCP server names.
 * Fixture is the config JSON; the array is rendered comma-joined so order is under test. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
#include "hermes_json.h"
json_t *coding_context_mcp_names_json(const json_t *config);
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_enabled_mcp_servers");
        json_t *cfg = json_parse(argv[i], NULL);
        json_t *arr = coding_context_mcp_names_json(cfg);
        char joined[2048] = ""; size_t used = 0;
        size_t n = arr ? json_array_count(arr) : 0;
        for (size_t k = 0; k < n; k++) {
            const json_t *e = json_array_get(arr, k);
            const char *s = (e && e->type == JSON_STRING) ? e->str_val : "";
            used += (size_t)snprintf(joined + used, sizeof joined - used, "%s%s", k ? "," : "", s ? s : "");
        }
        WCUP_VALSTR(joined);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("_enabled_mcp_servers");
        if (arr) json_free(arr);
        if (cfg) json_free(cfg);
    }
    return 0;
}
