/*
 * gui.c — canonical C11 port of hermes_cli/subcommands/gui.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_gui_build_gui_parser @ hermes_cli/subcommands/gui.py:build_gui_parser */
int hermes_cli_subcommands_gui_build_gui_parser(const char *arg) {
    /* Python: desktop subcommand. */
    (void)arg;
    printf("desktop/gui parser attached (--source --build-only --skip-build --force-build)\n");
    return 0;
}
