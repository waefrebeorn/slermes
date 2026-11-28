/* gate: toolset_selection(profile, mode, config) -> list | None.
 * Fixture form: "<profile>|<mode>|<config json>". "None" means keep the platform default. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
json_t *coding_context_toolset_selection(const char *profile_name, const char *config_mode, const json_t *config);
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    for (int i = 2; i < argc; i++) {
        char work[4096]; snprintf(work, sizeof work, "%s", argv[i]);
        char *p1 = strchr(work, '|'); if (!p1) continue; *p1 = 0;
        char *p2 = strchr(p1 + 1, '|'); if (!p2) continue; *p2 = 0;
        const char *prof = work, *mode = p1 + 1, *cfg_json = p2 + 1;
        json_t *cfg = json_parse(cfg_json, NULL);
        WCUP_BEGIN("gate");
        json_t *r = coding_context_toolset_selection(prof, mode, cfg);
        char joined[2048] = ""; size_t used = 0;
        if (!r || r->type == JSON_NULL) {
            snprintf(joined, sizeof joined, "None");
        } else {
            size_t n = json_array_count(r);
            for (size_t k = 0; k < n; k++) {
                const json_t *e = json_array_get(r, k);
                const char *s = (e && e->type == JSON_STRING) ? e->str_val : "";
                used += (size_t)snprintf(joined + used, sizeof joined - used, "%s%s", k ? "," : "", s ? s : "");
            }
        }
        WCUP_VALSTR(joined);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("gate");
        if (r) json_free(r);
        if (cfg) json_free(cfg);
    }
    return 0;
}
