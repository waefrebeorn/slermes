/*
 * runtime_cwd.c — Port of Python agent/runtime_cwd.py
 *
 * Runtime working-directory resolution. The Python module exposes
 * get_runtime_cwd() / set_runtime_cwd() / format_cwd_in_prompt(); in the C
 * port these are real entry points delegating to the canonical implementation
 * in hermes_system_prompt.h (resolve_agent_cwd / set_session_cwd /
 * build_environment_hints). No "N/A".
 */
#include "hermes_system_prompt.h"   /* resolve_agent_cwd, set_session_cwd, build_environment_hints */
#include <string.h>

/* Port of Python: runtime_cwd.get_runtime_cwd() */
const char *get_runtime_cwd(void) {
    return resolve_agent_cwd();
}

/* Port of Python: runtime_cwd.set_runtime_cwd() */
void set_runtime_cwd(const char *cwd) {
    set_session_cwd(cwd);
}

/* Port of Python: runtime_cwd.format_cwd_in_prompt() */
char *format_cwd_in_prompt(void) {
    return build_environment_hints();
}


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_runtime_cwd_u_is_install_tree @ agent/runtime_cwd.py:_is_install_tree */
int agent_runtime_cwd_u_is_install_tree(const char *arg) {
    /* Python: p == root or under root. Arg = "path\troot". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 0; }
    char p[1024], r[1024];
    size_t plen = (size_t)(tab - arg);
    if (plen >= sizeof(p)) plen = sizeof(p) - 1;
    memcpy(p, arg, plen); p[plen] = '\0';
    snprintf(r, sizeof(r), "%s", tab + 1);
    char rp[1100], rr[1100];
    if (!realpath(p, rp) || !realpath(r, rr)) { printf("0\n"); return 0; }
    if (strcmp(rp, rr) == 0) { printf("1\n"); return 0; }
    size_t rlen = strlen(rr);
    if (strncmp(rp, rr, rlen) == 0 && rp[rlen] == '/') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}
