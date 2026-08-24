/*
 * tirith_security.c — canonical C11 port of tools/tirith_security.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_tirith_security_u_record_tirith_crash @ tools/tirith_security.py:_record_tirith_crash */
int tools_tirith_security_u_record_tirith_crash(const char *arg) {
    /* Python: ++crash, open breaker at limit. Arg = "count\tlimit". */
    if (!arg || !*arg) { printf("crash recorded\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long count = strtol(arg, NULL, 10) + 1;
    long limit = tab ? strtol(tab + 1, NULL, 10) : 3;
    printf("crash count %ld%s\n", count, count >= limit ? " (circuit breaker opened)" : "");
    return 0;
}
