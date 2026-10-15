/* gate: port__tools_file_tools_py___get_container_mirror_prefix_for_task — traced copy of file_tools_get_container_mirror_prefix_for_task (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * file_tools_get_container_mirror_prefix_for_task(const char *task_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s file_tools_get_container_mirror_prefix_for_task <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("file_tools_get_container_mirror_prefix_for_task");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = file_tools_get_container_mirror_prefix_for_task(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("file_tools_get_container_mirror_prefix_for_task");
    }
    return 0;
}
