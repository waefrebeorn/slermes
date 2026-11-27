/* gate: {'name': 'port__hermes_cli_banner_py___is_official_ssh_remote', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': 'banner_is_official_ssh_remote', 'py_module': 'hermes_cli/banner.py', 'py_fn': '_is_official_ssh_remote', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/hermes_cli/banner_pure.c', 'src/hermes_cli/config_ports.c', 'src/hermes_cli/config_py_io.c', 'src/hermes_cli/dashboard_procs.c', 'src/hermes_cli/web_server_paths.c', 'src/tools/skills_tool.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for _is_official_ssh_remote */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_is_official_ssh_remote";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_official_ssh_remote <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_is_official_ssh_remote");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_is_official_ssh_remote");
    }
    return 0;
}
