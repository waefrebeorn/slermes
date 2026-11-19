/* gate: port__tools_skills_hub_py___normalize_bundle_path — traced copy of hub_normalize_bundle_path (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool hub_normalize_bundle_path(const char *path_value, const char *field_name,
                               bool allow_nested, char *out, size_t out_len);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hub_normalize_bundle_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("hub_normalize_bundle_path");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = hub_normalize_bundle_path(argv[i], argv[2 + (i - 2 + 1) % 5], (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), argv[2 + (i - 2 + 3) % 5], (long)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("hub_normalize_bundle_path");
    }
    return 0;
}
