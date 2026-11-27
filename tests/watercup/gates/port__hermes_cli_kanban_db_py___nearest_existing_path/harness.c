/* gate: {'name': 'port__hermes_cli_kanban_db_py___nearest_existing_path', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_nearest_existing_path', 'py_module': 'hermes_cli/kanban_db.py', 'py_fn': '_nearest_existing_path', 'inputs': ['/home/wubu/.hermes/profiles/null/cache/scratch/wcup_p0_tf_s7ere.txt', '/home/wubu/.hermes/profiles/null/cache/scratch/wcup_p1_2jz7f_yv.txt', '/home/wubu/.hermes/profiles/null/cache/scratch/wcup_p2_61x8qiit.txt', '/home/wubu/.hermes/profiles/null/cache/scratch/wcup_p3_5dmnvbg2.txt', '/home/wubu/.hermes/profiles/null/cache/scratch/wcup_p4_5q8bnz4f.txt'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/hermes_cli/kanban_db.c', 'src/hermes_cli/lifecycle.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src', '-DHERMES_VERSION="0.21.0"', '-DHERMES_RELEASE_DATE="2026-10-15"', '-DATADIR="/usr/share/hermes"'], 'classification': 'PORTED'} — GAP sentinel for _nearest_existing_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_nearest_existing_path";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _nearest_existing_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_nearest_existing_path");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_nearest_existing_path");
    }
    return 0;
}
