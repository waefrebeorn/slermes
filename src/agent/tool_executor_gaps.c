#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/tool_executor.py: 6 missing functions ── */

/* PoP: _record_persisted_path_for_stub @ agent/tool_executor.py:_record_persisted_path_for_stub */
void _record_persisted_path_for_stub(void) {}

/* PoP: _authorization_gate_lock_timeout @ agent/tool_executor.py:_authorization_gate_lock_timeout */
int _authorization_gate_lock_timeout(void) { return 30; }

/* PoP: _human_wait_seconds @ agent/tool_executor.py:_human_wait_seconds */
int _human_wait_seconds(void) { return 0; }

/* PoP: _run_tool_activity_heartbeat @ agent/tool_executor.py:_run_tool_activity_heartbeat */
void _run_tool_activity_heartbeat(void) {}

/* PoP: _resolve_sequential_tool_timeout @ agent/tool_executor.py:_resolve_sequential_tool_timeout */
int _resolve_sequential_tool_timeout(void) { return 30; }

/* PoP: _run_sequential_tool_execution_middleware @ agent/tool_executor.py:_run_sequential_tool_execution_middleware */
char *_run_sequential_tool_execution_middleware(void) { return strdup("{}"); }
