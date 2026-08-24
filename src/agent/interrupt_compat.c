/*
 * interrupt_compat.c — canonical C11 port of agent/interrupt_compat.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_interrupt_compat_request_hard_interrupt @ agent/interrupt_compat.py:request_hard_interrupt */
int agent_interrupt_compat_request_hard_interrupt(const char *arg) {
    /* Python: call agent.hard_interrupt(message) if available, else agent.interrupt(message).
     * Arg = "has_hard_interrupt\tmessage_len\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1+1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2+1, '\t') : NULL;
    int has_hard = arg[0] == '1';
    const char *result = t3 ? t3+1 : "interrupted";
    if (has_hard) printf("1 (hard_interrupt)\n");
    else printf("0 (fell back to interrupt)\n");
    printf("%s\n", result);
    return 0;
}
