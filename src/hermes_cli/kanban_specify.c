/*
 * kanban_specify.c — canonical C11 port of hermes_cli/kanban_specify.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_kanban_specify_list_triage_ids @ hermes_cli/kanban_specify.py:list_triage_ids */
int hermes_cli_kanban_specify_list_triage_ids(const char *arg) {
    /* Python: triage task ids (tenant-filtered). Arg = "tenant\tids" (tab
     * sep, empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}
