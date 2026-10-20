/* gate: port__agent_learning_mutations_py___parse_memory_id — traced copy of learning_mutations_parse_memory_id (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int learning_mutations_parse_memory_id(const char *node_id, char *out_source,
                                       int *out_gidx, char *err, size_t errsz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s learning_mutations_parse_memory_id <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("learning_mutations_parse_memory_id");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = learning_mutations_parse_memory_id(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("learning_mutations_parse_memory_id");
    }
    return 0;
}
