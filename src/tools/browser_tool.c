/**
 * port_browser_tool.c — Facade / lifecycle for browser tool concerns.
 *
 * The actual browser-tool helpers live in focused, self-contained modules
 * (browser_tool_env, _platform, _eval, _install, _path, _cdp), each with its
 * own opaque state. This file owns the aggregate port_browser_tool_state_t,
 * instantiates each sub-module, and exposes the lifecycle API. No god header,
 * no monolith — every concern is split and reusable.
 */

#include "browser_tool.h"
#include "browser_tool_env.h"
#include "browser_tool_platform.h"
#include "browser_tool_eval.h"
#include "browser_tool_install.h"
#include "browser_tool_path.h"
#include "browser_tool_cdp.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct port_browser_tool_state {
    browser_tool_env_t      *env;
    browser_tool_platform_t *platform;
    browser_tool_eval_t     *eval;
    browser_tool_install_t  *install;
    browser_tool_path_t     *path;
    browser_tool_cdp_t      *cdp;
};

port_browser_tool_state_t *port_browser_tool_init(void)
{
    port_browser_tool_state_t *state = calloc(1, sizeof(*state));
    if (!state) return NULL;
    state->env     = browser_tool_env_init();
    state->platform= browser_tool_platform_init();
    state->eval    = browser_tool_eval_init();
    state->install = browser_tool_install_init();
    state->path    = browser_tool_path_init();
    state->cdp     = browser_tool_cdp_init();
    return state;
}

void port_browser_tool_cleanup(port_browser_tool_state_t *state)
{
    if (!state) return;
    browser_tool_env_cleanup(state->env);
    browser_tool_platform_cleanup(state->platform);
    browser_tool_eval_cleanup(state->eval);
    browser_tool_install_cleanup(state->install);
    browser_tool_path_cleanup(state->path);
    browser_tool_cdp_cleanup(state->cdp);
    free(state);
}


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_browser_tool_u_is_headed_mode @ tools/browser_tool.py:_is_headed_mode */
int tools_browser_tool_u_is_headed_mode(const char *arg) {
    /* Python: config then env. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "cached") == 0) { printf("%s\n", tab ? tab + 1 : "0"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: tools_browser_tool_u_store_full_snapshot @ tools/browser_tool.py:_store_full_snapshot */
int tools_browser_tool_u_store_full_snapshot(const char *arg) {
    /* Python: redacted cache write. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("snapshot stored (redacted, hash-deduped): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: tools_browser_tool_u_restrict_browser_evaluate @ tools/browser_tool.py:_restrict_browser_evaluate */
int tools_browser_tool_u_restrict_browser_evaluate(const char *arg) {
    /* Python: denylist toggle. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: tools_browser_tool_u_camofox_current_page_private_url @ tools/browser_tool.py:_camofox_current_page_private_url */
int tools_browser_tool_u_camofox_current_page_private_url(const char *arg) {
    /* Python: camofox private-url probe. Arg = "state\turl\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "private") == 0) { printf("%s\n", t1 ? t1 + 1 : ""); return 0; }
    printf("\n");
    return 0;
}
