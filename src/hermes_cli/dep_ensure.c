/*
 * dep_ensure.c — canonical C11 port of hermes_cli/dep_ensure.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_dep_ensure_u_has_system_browser @ hermes_cli/dep_ensure.py:_has_system_browser */
int hermes_cli_dep_ensure_u_has_system_browser(const char *arg) {
    /* Python: shutil.which() over known browser names. Arg = "system" or
     * empty (POSIX list). */
    static const char *posix[] = {"google-chrome", "google-chrome-stable",
                                  "chromium", "chromium-browser", "chrome"};
    int is_windows = arg && strncasecmp(arg, "windows", 7) == 0;
    const char **names = posix;
    size_t n = sizeof(posix) / sizeof(posix[0]);
    for (size_t i = 0; i < n; i++) {
        char which[1400];
        snprintf(which, sizeof(which), "command -v %s >/dev/null 2>&1", names[i]);
        if (system(which) == 0) { printf("1\n"); return 0; }
    }
    printf("0\n");
    return 0;
}

/* PoP: hermes_cli_dep_ensure_u_has_hermes_agent_browser @ hermes_cli/dep_ensure.py:_has_hermes_agent_browser */
int hermes_cli_dep_ensure_u_has_hermes_agent_browser(const char *arg) {
    /* Python: node/agent-browser(.cmd) or node_modules/.bin. Arg =
     * "home\twindows\tpaths". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int windows = t1 && t1[1] == '1';
    const char *paths = t2 ? t2 + 1 : "";
    if (paths[0]) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: hermes_cli_dep_ensure_u_find_install_script @ hermes_cli/dep_ensure.py:_find_install_script */
int hermes_cli_dep_ensure_u_find_install_script(const char *arg) {
    /* Python: bundled/repo install script. Arg =
     * "is_windows\tstate\tpath\tshell". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n\n"); return 0; }
    printf("%s\t%s\n", t2 ? t2 + 1 : "", t3 ? t3 + 1 : "");
    return 0;
}
