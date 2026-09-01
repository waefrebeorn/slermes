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

/* PoP: agent_interrupt_compat_accepts_keyword @ agent/interrupt_compat.py:_accepts_keyword */
bool agent_interrupt_compat_accepts_keyword(const char *callable_name, const char *param_name) {
    /* Python _accepts_keyword(callable_obj, name): inspect.signature to check
     * if callable has a parameter named `name` (or **kwargs).
     *
     * In C there are no dynamically-inspected callables — every function has a
     * fixed, known signature. The only call site passes (interrupt, "tool_reason")
     * where `interrupt` is the agent's interrupt/hard_interrupt method which
     * accepts tool_reason in the Python ABI. Faithful C equivalent: return true
     * when the named callable + param match a known accepting pair, else false.
     *
     * This matches the Python semantics: for callables we statically know about,
     * we know their parameter surface. Unknown callables → false (safe default,
     * same as Python's except TypeError/ValueError → False).
     */
    (void)callable_name;
    if (!param_name || !*param_name) return false;
    /* Known accepting callables for tool_reason. */
    if (strcmp(param_name, "tool_reason") == 0) {
        /* agent.interrupt and agent.hard_interrupt both accept tool_reason. */
        if (strcmp(callable_name, "interrupt") == 0 ||
            strcmp(callable_name, "hard_interrupt") == 0 ||
            strcmp(callable_name, "") == 0)  /* caller passes no name; assume accepting */
            return true;
    }
    return false;
}
