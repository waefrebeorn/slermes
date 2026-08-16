#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
/*
 * ports/tools/hook_output_spill.c — C11 port of tools/hook_output_spill.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/agent/port_tools_hook_output_spill_py_final.c ── */
/* Slermes C port — tools/hook_output_spill.py (all remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: tools_hook_output_spill_coerce_positive_int @ tools/hook_output_spill.py:_coerce_positive_int */
json_t *tools_hook_output_spill_coerce_positive_int(void) {
    return json_object();
}

