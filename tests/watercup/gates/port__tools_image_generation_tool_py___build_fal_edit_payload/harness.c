/* gate: port__tools_image_generation_tool_py___build_fal_edit_payload — traced copy of _build_fal_edit_payload (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _build_fal_edit_payload(const char *model_id, const char *prompt,
                              const char *image_urls, double aspect_ratio,
                              const char *seed, const char *overrides);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _build_fal_edit_payload <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_build_fal_edit_payload");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _build_fal_edit_payload((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), strtod(argv[2 + (i - 2 + 3) % 5], NULL), (int)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 5) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_build_fal_edit_payload");
    }
    return 0;
}
