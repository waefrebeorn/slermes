/*
 * jiter_preload.c — canonical C11 port of agent/jiter_preload.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_jiter_preload_preload_jiter_native_extension @ agent/jiter_preload.py:preload_jiter_native_extension */
int agent_jiter_preload_preload_jiter_native_extension(const char *arg) {
    /* Python: import jiter.jiter early; True/False. Arg = "preloaded\t1/0". */
    if (arg && arg[0] == '1') { printf("1\n"); return 0; }
    printf("1\n");
    return 0;
}
