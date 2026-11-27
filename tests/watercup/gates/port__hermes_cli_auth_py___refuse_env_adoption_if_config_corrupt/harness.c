/* gate: {'name': 'port__hermes_cli_auth_py___refuse_env_adoption_if_config_corrupt', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_refuse_env_adoption_if_config_corrupt', 'py_module': 'hermes_cli/auth.py', 'py_fn': '_refuse_env_adoption_if_config_corrupt', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/hermes_cli/auth_gaps.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for _refuse_env_adoption_if_config_corrupt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_refuse_env_adoption_if_config_corrupt";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _refuse_env_adoption_if_config_corrupt <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_refuse_env_adoption_if_config_corrupt");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_refuse_env_adoption_if_config_corrupt");
    }
    return 0;
}
