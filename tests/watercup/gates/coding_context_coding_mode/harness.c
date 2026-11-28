/* gate: _coding_mode(config) -> auto|focus|on|off. Fixture is the config JSON. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
#include "hermes_json.h"
const char *coding_ctx_mode(json_t *config);
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_coding_mode");
        json_t *cfg = json_parse(argv[i], NULL);
        WCUP_VALSTR(coding_ctx_mode(cfg));
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("_coding_mode");
        if (cfg) json_free(cfg);
    }
    return 0;
}
