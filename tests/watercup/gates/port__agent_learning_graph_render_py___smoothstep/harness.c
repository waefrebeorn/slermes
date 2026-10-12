/* gate: port__agent_learning_graph_render_py___smoothstep — traced copy of learning_graph_render_smoothstep (double_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern double learning_graph_render_smoothstep(double p);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s learning_graph_render_smoothstep <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("learning_graph_render_smoothstep");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    double result = learning_graph_render_smoothstep(strtod(argv[i], NULL));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("learning_graph_render_smoothstep");
    }
    return 0;
}
