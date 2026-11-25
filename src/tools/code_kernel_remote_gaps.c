#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/code_kernel_remote.py: 7 missing functions ── */

int _is_alive(void) { return 0; }

/* Was `void _kill(void)` -- that name is MinGW's kill(2) shim (hermes_win32.h:385),
 * so the stub collided with the platform header. The Python name is kept in a comment and
 * the C symbol is namespaced to the module. */
void code_kernel_remote_kill(void) {}



/* PoP: spawn_remote_kernel @ tools/code_kernel_remote.py:_spawn_remote_kernel */
char *spawn_remote_kernel(void) { return strdup("{}"); }

/* PoP: execute_in_remote_kernel @ tools/code_kernel_remote.py:execute_in_remote_kernel */
char *execute_in_remote_kernel(void) { return strdup("{}"); }

/* PoP: run_remote_cell @ tools/code_kernel_remote.py:_run_remote_cell */
char *run_remote_cell(void) { return strdup("{}"); }
