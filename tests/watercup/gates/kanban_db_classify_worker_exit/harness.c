/* gate: kanban_db_classify_worker_exit — traced C copy of
 * src/hermes_cli/kanban_workers.c::kdb_classify_worker_exit (PoP of LIVE
 * hermes_cli/kanban_db.py::_classify_worker_exit).
 * Each input is a wait raw status integer, or "none" for the registry-miss
 * (unknown) path. pid is fixed at 4242 on both sides.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "watercup.h"

#define PID 4242
#define RATE_LIMIT_EXIT_CODE 75

/* Faithful traced copy of kdb_classify_worker_exit with a one-entry registry:
 * in_registry=1 means pid was reaped and carries raw_status. */
static void classify(int in_registry, int raw_status) {
    WCUP_BEGIN("classify_worker_exit");
    char kind[32];
    int code;
    if (!in_registry) {
        WCUP_BRANCH(1);
        snprintf(kind, sizeof kind, "unknown");
        code = -1;
    } else {
        WCUP_BRANCH(0);
        if (WIFEXITED(raw_status)) {
            WCUP_BRANCH(1);
            int c = WEXITSTATUS(raw_status);
            if (c == 0) { WCUP_BRANCH(1);
                snprintf(kind, sizeof kind, "clean_exit"); code = 0;
            } else { WCUP_BRANCH(0);
                if (c == RATE_LIMIT_EXIT_CODE) { WCUP_BRANCH(1);
                    snprintf(kind, sizeof kind, "rate_limited"); code = c;
                } else { WCUP_BRANCH(0);
                    snprintf(kind, sizeof kind, "nonzero_exit"); code = c;
                }
            }
        } else { WCUP_BRANCH(0);
            if (WIFSIGNALED(raw_status)) { WCUP_BRANCH(1);
                snprintf(kind, sizeof kind, "signaled");
                code = WTERMSIG(raw_status);
            } else { WCUP_BRANCH(0);
                snprintf(kind, sizeof kind, "unknown");
                code = -1;
            }
        }
    }
    char val[64];
    snprintf(val, sizeof val, "%s:%d", kind, code);
    WCUP_VALSTR(val);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("classify_worker_exit");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdb_classify_worker_exit <raw|none>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "none") == 0)
            classify(0, 0);
        else
            classify(1, atoi(argv[i]));
    }
    return 0;
}
