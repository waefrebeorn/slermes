/* gate: gateway_kanban_watchers_py___acquire_singleton_lock — traced copy of gateway_kanban_watchers_u_acquire_singleton_lock */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_kanban_watchers_u_acquire_singleton_lock(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_kanban_watchers_u_acquire_singleton_lock");
    int result = gateway_kanban_watchers_u_acquire_singleton_lock(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_kanban_watchers_u_acquire_singleton_lock");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_kanban_watchers_u_acquire_singleton_lock <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
