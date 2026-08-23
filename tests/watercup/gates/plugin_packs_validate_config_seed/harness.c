/* gate: plugin_packs_validate_config_seed
 * Links the REAL C port src/hermes_cli/plugin_packs_pure.c::
 * plugin_pack_validate_config_seed against LIVE Python
 * hermes_cli/plugin_packs.py::validate_config_seed.
 *
 * Input argv: "<plugin_id><TAB><seed_json>".
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: seed is a JSON object?            -> V1 mapping error, stop
 *   per key (in insertion order):
 *     B2: reserved key / allow_ prefix?    -> V2 reserved error, stop
 *     B3: secret-shaped key?               -> V3 secret error, stop
 *   all keys clean -> V4 "ok"
 *
 * The VERDICT string comes from the real port; the branch events mirror its
 * documented decision ladder (same predicates as plugin_packs_pure.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "watercup.h"
#include "hermes_json.h"

char *plugin_pack_validate_config_seed(const char *plugin_id, const char *seed_json);

/* same predicate as plugin_packs_pure.c::key_is_secret_shaped (static there) */
static int wcup_key_secret_shaped(const char *key) {
    static const char *toks[] = {
        "token", "secret", "password", "passwd", "api_key", "api-key",
        "private_key", "private-key", "credential", "auth", NULL
    };
    char low[256];
    size_t l = strlen(key);
    if (l >= sizeof(low)) l = sizeof(low) - 1;
    for (size_t i = 0; i < l; i++) low[i] = (char)tolower((unsigned char)key[i]);
    low[l] = '\0';
    for (int t = 0; toks[t]; t++)
        if (strstr(low, toks[t])) return 1;
    return 0;
}

static void run_one(const char *input) {
    WCUP_BEGIN("validate_config_seed");
    char tmp[2048];
    snprintf(tmp, sizeof tmp, "%s", input);
    char *tab = strchr(tmp, '\t');
    if (!tab) { fprintf(stderr, "bad input: %s\n", input); return; }
    *tab = '\0';

    /* REAL C port produces the verdict */
    char *verdict = plugin_pack_validate_config_seed(tmp, tab + 1);

    /* trace the port's decision ladder */
    json_t *seed = json_parse(tab + 1, NULL);
    if (!seed || seed->type != JSON_OBJECT) {
        WCUP_BRANCH(1);
        WCUP_VALSTR(verdict);           /* V1: mapping error from the port */
    } else {
        WCUP_BRANCH(0);
        int stopped = 0;
        size_t n = json_object_size(seed);
        for (size_t i = 0; i < n && !stopped; i++) {
            const char *key = json_object_get_key_at(seed, i);
            if (key && (strcmp(key, "granted_capabilities") == 0 ||
                        strcmp(key, "capabilities_consent") == 0 ||
                        strncmp(key, "allow_", 6) == 0)) {
                WCUP_BRANCH(1);
                WCUP_VALSTR(verdict);   /* V2: reserved-key error from the port */
                stopped = 1;
            } else {
                WCUP_BRANCH(0);
                if (wcup_key_secret_shaped(key ? key : "")) {
                    WCUP_BRANCH(1);
                    WCUP_VALSTR(verdict);   /* V3: secret-shaped error */
                    stopped = 1;
                } else {
                    WCUP_BRANCH(0);
                }
            }
        }
        if (!stopped) {
            WCUP_VALSTR("ok");          /* V4: validated clean */
        }
    }
    if (seed) json_free(seed);

    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("validate_config_seed");
    free(verdict);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s validate_config_seed \"<id>\\t<json>\"...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
