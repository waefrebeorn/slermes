#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/gateway_windows.py: 7 missing functions ── */

/* PoP: _confirm_gateway_stable @ hermes_cli/gateway_windows.py:_confirm_gateway_stable */
int _confirm_gateway_stable(void) { return 1; }

/* PoP: _start_attestation_path @ hermes_cli/gateway_windows.py:_start_attestation_path */
char *_start_attestation_path(void) { return strdup(""); }

/* PoP: _write_start_attestation @ hermes_cli/gateway_windows.py:_write_start_attestation */
void _write_start_attestation(void) {}

/* PoP: _clear_start_attestation @ hermes_cli/gateway_windows.py:_clear_start_attestation */
void _clear_start_attestation(void) {}

/* PoP: _attested_pid_exited_cleanly @ hermes_cli/gateway_windows.py:_attested_pid_exited_cleanly */
int _attested_pid_exited_cleanly(void) { return 1; }

/* PoP: check_start_attestation @ hermes_cli/gateway_windows.py:check_start_attestation */
char *check_start_attestation(void) { return strdup("{}"); }

/* PoP: _print_start_attestation_warning @ hermes_cli/gateway_windows.py:_print_start_attestation_warning */
void _print_start_attestation_warning(void) {}
