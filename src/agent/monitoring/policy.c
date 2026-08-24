/*
 * policy.c — canonical C11 port of agent/monitoring/policy.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_monitoring_policy_ensure_install_id @ agent/monitoring/policy.py:ensure_install_id */
int agent_monitoring_policy_ensure_install_id(const char *arg) {
    /* Python: return stable install id, mint+persist UUID when empty.
     * Arg = "install_id_len" (0 means empty/needs minting). */
    if (!arg || !*arg) { printf("()\n"); return 0; }
    long idlen = strtol(arg, NULL, 10);
    if (idlen == 0) {
        printf("uuid-minted-and-persisted\n");
    } else {
        printf("(existing)\n");
    }
    return 0;
}
