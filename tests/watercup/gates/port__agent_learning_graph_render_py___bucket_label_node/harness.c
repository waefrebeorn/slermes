/* gate: port__agent_learning_graph_render_py___bucket_label_node — traced copy of learning_graph_render_bucket_label_node (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * learning_graph_render_bucket_label_node(const char *bucket_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s learning_graph_render_bucket_label_node <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("learning_graph_render_bucket_label_node");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = learning_graph_render_bucket_label_node(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("learning_graph_render_bucket_label_node");
    }
    return 0;
}
