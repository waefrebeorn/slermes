/* gate: moa_loop_reference_messages
 * Traced copy of src/agent/moa_loop_wrappers.c::moa_u_reference_messages.
 * LIVE twin: agent/moa_loop.py::_reference_messages.
 *
 * Input argv token encoding (one per input): "<scenario>\t<state>\t<result>"
 *   scenario: selects the synthetic message list in the twin (C ignores it
 *             beyond non-emptiness)
 *   state:    '1' when the scenario yields a non-empty message list
 *   result:   '1' when the live advisory view needed a trailing synthetic
 *             user turn appended
 *
 * Branch ladder (twin.py mirrors EXACTLY, same rung order):
 *   B1: arg empty -> empty advisory view "[]"
 *   B2: state flag (non-empty conversation to shape)
 *   VSTR("1"/"0") trailing-user-appended verdict
 */
#include <stdio.h>
#include <string.h>
#include "watercup.h"

static void run_one(const char *arg) {
    WCUP_BEGIN("_reference_messages");

    WCUP_BRANCH(arg && *arg);                                   /* B1 */
    if (!arg || !*arg) {
        WCUP_VALSTR("0");
        goto out;
    }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    WCUP_BRANCH(state);                                         /* B2 */
    if (!state) {
        WCUP_VALSTR("0");
        goto out;
    }
    int appended = t2 && t2[1] == '1';
    WCUP_VALSTR(appended ? "1" : "0");

out:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_reference_messages");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s moa_u_reference_messages <scenario\\tstate\\tresult...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
