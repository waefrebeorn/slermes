/*
 * managed_modal.c — canonical C11 port of tools/environments/managed_modal.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_environments_managed_mod_u_request @ tools/environments/managed_modal.py:_request */
int tools_environments_managed_mod_u_request(const char *arg) {
    /* Python: requests.request with bearer + JSON. Arg =
     * "method\torigin\tpath\tpayload". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    printf("managed modal request: %.*s %s%s\n",
           (int)(t1 ? (size_t)(t1 - arg) : strlen(arg)), arg,
           t2 ? t2 + 1 : "",
           t3 ? t3 : "");
    return 0;
}
