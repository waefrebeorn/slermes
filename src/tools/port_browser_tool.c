/**
 * port_browser_tool.c — Facade / lifecycle for browser tool concerns.
 *
 * The actual browser-tool helpers live in focused, self-contained modules
 * (browser_tool_env, _platform, _eval, _install, _path, _cdp), each with its
 * own opaque state. This file owns the aggregate port_browser_tool_state_t,
 * instantiates each sub-module, and exposes the lifecycle API. No god header,
 * no monolith — every concern is split and reusable.
 */

#include "port_browser_tool.h"
#include "browser_tool_env.h"
#include "browser_tool_platform.h"
#include "browser_tool_eval.h"
#include "browser_tool_install.h"
#include "browser_tool_path.h"
#include "browser_tool_cdp.h"
#include <stdlib.h>
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
