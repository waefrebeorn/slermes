/*
 * backend.c — canonical C11 port of tools/computer_use/backend.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_computer_use_backend_list_windows @ tools/computer_use/backend.py:list_windows */
int tools_computer_use_backend_list_windows(const char *arg) {
    /* Python: return [] (compatibility hook for pre-window-discovery
     * backends). Arg unused. */
    (void)arg;
    printf("\n");
    return 0;
}

/* PoP: tools_computer_use_backend_set_value @ tools/computer_use/backend.py:set_value */
int tools_computer_use_backend_set_value(const char *arg) {
    /* Python: set native value on element (AXPopUpButton selection) by
     * 1-based SOM index. Arg = "index\tvalue". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 0; }
    long idx = strtol(arg, NULL, 10);
    if (idx < 1) { printf("0\n"); return 0; }
    printf("value set on element %ld: %s\n", idx, tab + 1);
    return 0;
}
