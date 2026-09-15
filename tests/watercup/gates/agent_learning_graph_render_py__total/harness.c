/* gate: agent_learning_graph_render_py__total — traced copy of learning_graph_render_bucket_total */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int learning_graph_render_bucket_total(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("learning_graph_render_bucket_total");
    int result = learning_graph_render_bucket_total(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("learning_graph_render_bucket_total");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s learning_graph_render_bucket_total <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
