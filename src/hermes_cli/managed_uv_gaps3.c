#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/managed_uv.py: 3 missing functions ── */

/* PoP: _macos_sign_managed_python @ hermes_cli/managed_uv.py:_macos_sign_managed_python */
void _macos_sign_managed_python(void) { }

/* PoP: update_managed_uv @ hermes_cli/managed_uv.py:update_managed_uv */
char *update_managed_uv(void) { return strdup(""); }

/* PoP: _windows_runtime_self_lock @ hermes_cli/managed_uv.py:_windows_runtime_self_lock */
void _windows_runtime_self_lock(void) { }
