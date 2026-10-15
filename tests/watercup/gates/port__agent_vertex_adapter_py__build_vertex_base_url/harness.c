/* gate: port__agent_vertex_adapter_py__build_vertex_base_url — traced copy of vertex_adapter_build_base_url (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * vertex_adapter_build_base_url(const char *project_id, const char *region);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s vertex_adapter_build_base_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("vertex_adapter_build_base_url");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = vertex_adapter_build_base_url(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("vertex_adapter_build_base_url");
    }
    return 0;
}
