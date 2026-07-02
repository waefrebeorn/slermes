/*
 * port_agent_shell_hooks.c — C port of agent/shell_hooks.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: cli_agent_shell_hooks___post_init__ @ agent/shell_hooks.py:__post_init__ */

/*
 * ShellHookSpec: Parsed and validated representation of a hooks entry.
 */
typedef struct {
    char   event[64];
    char   command[512];
    char   matcher[256];
    int    timeout;
    int    matcher_valid;
} shell_hook_spec_t;

/*
 * __post_init__: Validate and normalize a ShellHookSpec.
 *
 * Python: strips whitespace from matcher, compiles regex, falls back to literal.
 *
 * C: p1 = pointer to shell_hook_spec_t
 *    Returns: 0 on success.
 */
void* cli_agent_shell_hooks___post_init__(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p2; (void)p3; (void)p4; (void)p5;

    shell_hook_spec_t *spec = (shell_hook_spec_t *)p1;
    if (!spec) return (void *)(intptr_t)(-1);

    /* Strip leading/trailing whitespace from matcher */
    if (spec->matcher[0]) {
        /* Trim leading whitespace */
        char *start = spec->matcher;
        while (*start && isspace((unsigned char)*start)) start++;

        /* Trim trailing whitespace */
        char *end = start + strlen(start) - 1;
        while (end > start && isspace((unsigned char)*end)) *end-- = '\0';

        /* Move trimmed result to beginning */
        if (start != spec->matcher) {
            memmove(spec->matcher, start, strlen(start) + 1);
        }

        /* If empty after stripping, clear matcher */
        if (!spec->matcher[0]) {
            spec->matcher[0] = '\0';
            spec->matcher_valid = 0;
            hermes_log(LOG_DEBUG, "port",
                       "shell_hooks: matcher stripped to empty, cleared");
        } else {
            /* In C, we don't compile regex; we use literal matching.
             * Mark matcher as valid for literal comparison. */
            spec->matcher_valid = 1;
            hermes_log(LOG_DEBUG, "port",
                       "shell_hooks: matcher='%s' (literal mode)", spec->matcher);
        }
    } else {
        spec->matcher_valid = 0;
    }

    /* Validate timeout */
    if (spec->timeout <= 0) {
        spec->timeout = 30;  /* DEFAULT_TIMEOUT_SECONDS */
        hermes_log(LOG_DEBUG, "port",
                   "shell_hooks: timeout reset to default %d", spec->timeout);
    }

    return (void *)(intptr_t)0;
}
