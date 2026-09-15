#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/local_runtime/endpoint.py: 4 missing functions ── */

/* PoP: _state_endpoint @ hermes_cli/local_runtime/endpoint.py:_state_endpoint */
char *_state_endpoint(void) { return strdup(""); }

/* PoP: resolve_llamacpp_endpoint @ hermes_cli/local_runtime/endpoint.py:resolve_llamacpp_endpoint */
char *resolve_llamacpp_endpoint(void) { return strdup(""); }

/* PoP: _kick_managed_boot @ hermes_cli/local_runtime/endpoint.py:_kick_managed_boot */
void _kick_managed_boot(void) {}

/* PoP: _boot_in_flight @ hermes_cli/local_runtime/endpoint.py:_boot_in_flight */
int _boot_in_flight(void) { return 0; }
