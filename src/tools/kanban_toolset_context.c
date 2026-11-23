/*
 * kanban_toolset_context.c — faithful C11 port of tools/kanban_toolset_context.py.
 * Gate: kantool.
 * PoP: kantool_requested @ tools/kanban_toolset_context.py:kanban_toolset_requested
 * PoP: kantool_enter @ tools/kanban_toolset_context.py:scoped_kanban_toolset_selection
 * PoP: kantool_exit @ tools/kanban_toolset_context.py:scoped_kanban_toolset_selection
 * PoP: kantool_explicit @ tools/kanban_toolset_context.py:scoped_kanban_toolset_selection
 *
 * The module keeps a ContextVar[_requested] (default None). Outside schema
 * assembly it reads None; scoped_kanban_toolset_selection sets it to
 * ("kanban" in (toolsets or ())) and restores the prior value in finally.
 * Modelled as a tri-state int: -1 None, 0 False, 1 True.
 */
#include "noc_kantool.h"
#include <string.h>

static int g_requested = -1; /* ContextVar default = None */

int kantool_requested(void) { return g_requested; }

int kantool_enter(int requested) {
    int token = g_requested;
    g_requested = requested;
    return token;
}

void kantool_exit(int token) { g_requested = token; }

void kantool_reset(void) { g_requested = -1; }

int kantool_explicit(const char *const *toolsets, size_t n) {
    /* "kanban" in (toolsets or ()) -- empty/None selection is not opt-in. */
    if (!toolsets) return 0;
    for (size_t i = 0; i < n; i++) {
        if (toolsets[i] && !strcmp(toolsets[i], "kanban")) return 1;
    }
    return 0;
}
