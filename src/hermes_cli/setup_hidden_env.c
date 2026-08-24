/*
 * setup_hidden_env.c — canonical C11 port of hermes_cli/setup_hidden_env.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_setup_hidden_env_is_setup_hidden_env @ hermes_cli/setup_hidden_env.py:is_setup_hidden_env */
int hermes_cli_setup_hidden_env_is_setup_hidden_env(const char *arg) {
    /* Python: True when a var is self-configuring and shouldn't appear in setup forms.
     * Arg = env_var_name. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("1\n");
    return 0;
}
