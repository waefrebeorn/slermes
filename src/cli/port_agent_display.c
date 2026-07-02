/*
 * port_agent_display.c — C port of agent/display.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_display___enter__ @ agent/display.py:__enter__ */

/* Port of Python agent/display.py:__enter__ */
/* Context manager entry for display state. */
int cli_agent_display___enter__(void *display_state)
{
    (void)display_state;
    /* CLI port: display state managed by the CLI itself. */
    return 0;
}

/* PoP: cli_agent_display___exit__ @ agent/display.py:__exit__ */

/* Port of Python agent/display.py:__exit__ */
/* Context manager exit for display state. */
int cli_agent_display___exit__(void *display_state, int exc_type)
{
    (void)display_state;
    (void)exc_type;
    /* CLI port: display state managed by the CLI itself. */
    return 0;
}
