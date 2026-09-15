#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/runtime_cwd.py: 2 missing functions ── */

/* PoP: _terminal_cwd_env @ agent/runtime_cwd.py:_terminal_cwd_env */
void _terminal_cwd_env(void) { }

/* PoP: scope_terminal_cwd @ agent/runtime_cwd.py:scope_terminal_cwd */
char *scope_terminal_cwd(void) { return strdup(""); }
