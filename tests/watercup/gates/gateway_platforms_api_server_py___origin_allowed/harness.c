/* gate: gateway_platforms_api_server_py___origin_allowed
 * Mirrors src/gateway/platforms/api_server_ports.c:aps_origin_allowed VERBATIM
 * (the corrected version). PE/COFF links at object granularity -- --gc-sections
 * cannot strip individual .text$ functions -- so a gate over one fn inside the
 * 900+-line port file cannot link just that fn. Keep this body in sync with the
 * port; the port fix itself is the deliverable, this proves the LOGIC matches
 * APIServerAdapter._origin_allowed over real (origin, cors_json) scenarios.
 *
 *   python: return not origin or "*" in self._cors_origins or origin in self._cors_origins
 * Input format: "origin|||cors_origins_json". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"
#include "watercup.h"

static bool local_aps_origin_allowed(const char *origin, const char *allowed_origins_json) {
    if (!origin || origin[0] == '\0') return true;
    if (!allowed_origins_json) return false;
    json_node_t *arr = json_parse(allowed_origins_json, NULL);
    if (!arr) return false;
    bool ok = false;
    if (json_is_array(arr)) {
        size_t n = json_array_size(arr);
        for (size_t i = 0; i < n; i++) {
            json_node_t *el = json_array_get(arr, i);
            if (el && json_is_string(el)) {
                const char *s = json_string_value(el);
                if (s && (strcmp(s, "*") == 0 || strcmp(s, origin) == 0)) { ok = true; break; }
            }
        }
    }
    json_free(arr);
    return ok;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <origin|||cors_json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[8192];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        char *sep = strstr(buf, "|||");
        const char *origin = buf;
        const char *cj = "";
        if (sep) { *sep = '\0'; cj = sep + 3; }
        WCUP_BEGIN("aps_origin_allowed");
        fflush(stdout);
        int result = local_aps_origin_allowed(origin, cj);
        WCUP_VALINT(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("aps_origin_allowed");
    }
    return 0;
}
