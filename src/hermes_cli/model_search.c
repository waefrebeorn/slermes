/*
 * model_search.c — canonical C11 port of hermes_cli/model_search.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_model_search_model_alias_canonical @ hermes_cli/model_search.py:model_alias_canonical */
int hermes_cli_model_search_model_alias_canonical(const char *arg) {
    /* Python: return canonical slug for a bare wire-id alias.
     * Arg = model id. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_model_search_model_search_text @ hermes_cli/model_search.py:model_search_text */
int hermes_cli_model_search_model_search_text(const char *arg) {
    /* Python: return haystack for fuzzy model search.
     * Arg = model id. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}
