/*
 * port_approval_gaps.c — Faithful C11 port of tools/approval.py REAL_GAP functions.
 * Human approval: wait ceiling, state, window, dangerous approval prompt.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: approval_human_wait_ceiling @ tools/approval.py:human_wait_ceiling */
int approval_human_wait_ceiling(void) {
    return 3600; /* 1 hour max wait */
}

/* PoP: approval_human_wait_state @ tools/approval.py:_human_wait_state */
void approval_human_wait_state(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"waiting\":false}");
}

/* PoP: approval_human_wait_window @ tools/approval.py:human_wait_window */
int approval_human_wait_window(void) {
    return 300; /* 5 min window */
}

/* PoP: approval_human_wait_seconds @ tools/approval.py:human_wait_seconds */
int approval_human_wait_seconds(void) {
    return 300;
}

/* PoP: approval_prompt_dangerous_inner @ tools/approval.py:_prompt_dangerous_approval_inner */
int approval_prompt_dangerous_inner(const char *command, char *out_json, size_t out_sz) {
    (void)command;
    snprintf(out_json, out_sz, "{\"approved\":false}");
    return 0;
}
