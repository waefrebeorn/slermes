#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/tool_executor.py: 6 missing functions ── */


/* PoP: authorization_gate_lock_timeout @ agent/tool_executor.py:authorization_gate_lock_timeout */
int authorization_gate_lock_timeout(void) { return 30; }

/* PoP: _human_wait_seconds @ agent/tool_executor.py:_human_wait_seconds */
int _human_wait_seconds(void) { return 0; }


/* PoP: resolve_sequential_tool_timeout @ agent/tool_executor.py:resolve_sequential_tool_timeout */
int resolve_sequential_tool_timeout(void) { return 30; }

/* PoP: run_sequential_tool_execution_middleware @ agent/tool_executor.py:run_sequential_tool_execution_middleware */
char *run_sequential_tool_execution_middleware(void) { return strdup("{}"); }
