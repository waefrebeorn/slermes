/*
 * port_tools_skill_provenance.c — C port of tools/skill_provenance.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Thread-local write origin context */
static char _write_origin[256] = "foreground";

/* PoP: cli_tools_skill_provenance_set_current_write_origin @ tools/skill_provenance.py:set_current_write_origin */

/* Port of Python tools/skill_provenance.py:set_current_write_origin */
/* Bind the active write origin to the current context. */
void cli_tools_skill_provenance_set_current_write_origin(const char *origin)
{
    if (!origin || !*origin) {
        strncpy(_write_origin, "foreground", sizeof(_write_origin) - 1);
    } else {
        strncpy(_write_origin, origin, sizeof(_write_origin) - 1);
    }
    _write_origin[sizeof(_write_origin) - 1] = '\0';
    hermes_log(LOG_DEBUG, "skill_provenance", "write_origin set to: %s", _write_origin);
}

/* PoP: cli_tools_skill_provenance_reset_current_write_origin @ tools/skill_provenance.py:reset_current_write_origin */

/* Port of Python tools/skill_provenance.py:reset_current_write_origin */
/* Restore the prior write origin context. */
/* Note: In Python this uses contextvars.Token. In C, we reset to "foreground". */
void cli_tools_skill_provenance_reset_current_write_origin(void)
{
    strncpy(_write_origin, "foreground", sizeof(_write_origin) - 1);
    _write_origin[sizeof(_write_origin) - 1] = '\0';
    hermes_log(LOG_DEBUG, "skill_provenance", "write_origin reset to foreground");
}

/* PoP: cli_tools_skill_provenance_get_current_write_origin @ tools/skill_provenance.py:get_current_write_origin */

/* Port of Python tools/skill_provenance.py:get_current_write_origin */
/* Return the active write origin. */
const char *cli_tools_skill_provenance_get_current_write_origin(void)
{
    return _write_origin;
}
