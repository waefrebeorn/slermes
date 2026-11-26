/* gate: agent_auxiliary_client_py___client_declares — traced copy of _client_declares */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int client_declares(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("_client_declares");
    int result = client_declares(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_client_declares");
}

int main(int argc, char **argv) {
    /* One cup per input, like every other harness: argv[2..] are the inputs. The second argument
     * of this two-parameter fn is the NEXT input in the walk (wrapping), which is exactly what the
     * twin's call_fn feeds -- the generated single-call body printed one cup for five inputs and
     * compared the fn NAME as the first argument. */
    int n = argc - 2;
    if (n < 1) { fprintf(stderr, "usage: %s client_declares <a> <b>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        trace_one(argv[i], argv[2 + ((i - 2 + 1) % n)]);
    return 0;
}
