/* gate: compact_skill_categories(profile, mode) -> demoted category set.
 * Fixture form: "<profile>|<config_mode>". Sorted join, empty string for the empty set. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
json_t *coding_context_compact_categories(const char *profile_name, const char *config_mode);
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    for (int i = 2; i < argc; i++) {
        char work[512]; snprintf(work, sizeof work, "%s", argv[i]);
        char *bar = strchr(work, '|'); const char *mode = "";
        if (bar) { *bar = 0; mode = bar + 1; }
        WCUP_BEGIN("gate");
        json_t *r = coding_context_compact_categories(work, mode);
        char joined[4096] = ""; size_t used = 0;
        size_t n = r ? json_array_count(r) : 0;
        for (size_t k = 0; k < n; k++) {
            const json_t *e = json_array_get(r, k);
            const char *s = (e && e->type == JSON_STRING) ? e->str_val : "";
            used += (size_t)snprintf(joined + used, sizeof joined - used, "%s%s", k ? "," : "", s ? s : "");
        }
        WCUP_VALSTR(joined);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("gate");
        if (r) json_free(r);
    }
    return 0;
}
