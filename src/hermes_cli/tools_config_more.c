/* tools_config_more.c — additional faithful C11 ports from hermes_cli/tools_config.py.
 *
 * Covers REAL_GAP functions that are simple standalone helpers with minimal
 * Python dependencies. Does NOT duplicate functions already in tools_config_providers.c.
 */

#define _GNU_SOURCE
#include "tools_config_more.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

/* PoP: tcm_tools_command @ hermes_cli/tools_config.py:tools_command */
void tcm_tools_command(void) {
    /* Placeholder — full tools_command is ~625 lines of interactive platform menu
     * with first_install flow. Stub for now so the build symbol exists. */
    printf("tcm_tools_command: not yet implemented (tools_config.py:tools_command)\n");
}
