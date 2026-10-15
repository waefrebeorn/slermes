/* gate: port__agent_learning_graph_render_py__rgb_to_hex — traced copy of learning_graph_render_rgb_to_hex (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * learning_graph_render_rgb_to_hex(int r, int g, int b);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s learning_graph_render_rgb_to_hex <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("learning_graph_render_rgb_to_hex");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = learning_graph_render_rgb_to_hex((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("learning_graph_render_rgb_to_hex");
    }
    return 0;
}
