/* gate: hermes_cli_gateway_platform_status
 * Traced copy of src/hermes_cli/gateway_wrappers.c::cgw_u_platform_status.
 * LIVE twin: hermes_cli/gateway.py::_platform_status.
 *
 * Input argv token encoding (one per input): "<registry>\t<state>\t<result>"
 *   registry: '1' when the platform dict carries a _registry_entry
 *   state:    '1' when the platform has any configuration source at all
 *   result:   '1' when the live status check reports "configured"
 *
 * Branch ladder (twin.py mirrors EXACTLY, same rung order):
 *   B1: arg empty -> "not configured"
 *   B2: state flag (any config source) -> false: "not configured"
 *   B3: registry flag (is_connected/check_fn path vs token_var path)
 *   VSTR("1"/"0") configured verdict
 */
#include <stdio.h>
#include <string.h>
#include "watercup.h"

static void run_one(const char *arg) {
    WCUP_BEGIN("_platform_status");

    WCUP_BRANCH(arg && *arg);                                   /* B1 */
    if (!arg || !*arg) {
        WCUP_VALSTR("0");
        goto out;
    }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int registry = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    WCUP_BRANCH(state);                                         /* B2 */
    if (!state) {
        WCUP_VALSTR("0");
        goto out;
    }
    WCUP_BRANCH(registry);                                      /* B3 */
    int verdict = t2 && t2[1] == '1';
    WCUP_VALSTR(verdict ? "1" : "0");

out:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_platform_status");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_u_platform_status <r\\ts\\tr...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
