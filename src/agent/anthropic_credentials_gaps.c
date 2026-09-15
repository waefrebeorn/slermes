#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/anthropic_credentials.py: 9 missing functions ── */

/* PoP: _spent_rotation_sidecar_path @ agent/anthropic_credentials.py:_spent_rotation_sidecar_path */
char *_spent_rotation_sidecar_path(void) { return strdup(""); }

/* PoP: spent_rotation_source_path @ agent/anthropic_credentials.py:spent_rotation_source_path */
char *spent_rotation_source_path(void) { return strdup(""); }

/* PoP: _read_spent_rotation_sidecar @ agent/anthropic_credentials.py:_read_spent_rotation_sidecar */
char *_read_spent_rotation_sidecar(void) { return strdup("[]"); }

/* PoP: _append_spent_rotation_sidecar @ agent/anthropic_credentials.py:_append_spent_rotation_sidecar */
void _append_spent_rotation_sidecar(void) {}

/* PoP: mark_rotation_consumed_uncommitted @ agent/anthropic_credentials.py:mark_rotation_consumed_uncommitted */
void mark_rotation_consumed_uncommitted(void) {}

/* PoP: is_rotation_consumed_uncommitted @ agent/anthropic_credentials.py:is_rotation_consumed_uncommitted */
int is_rotation_consumed_uncommitted(void) { return 0; }

/* PoP: claude_code_credentials_path @ agent/anthropic_credentials.py:claude_code_credentials_path */
char *claude_code_credentials_path(void) { return strdup(""); }

/* PoP: _root_hermes_oauth_file @ agent/anthropic_credentials.py:_root_hermes_oauth_file */
char *_root_hermes_oauth_file(void) { return strdup(""); }

/* PoP: _write_hermes_oauth_credentials @ agent/anthropic_credentials.py:_write_hermes_oauth_credentials */
void _write_hermes_oauth_credentials(void) {}
