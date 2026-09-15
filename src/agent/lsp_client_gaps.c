#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/lsp/client.py: 3 missing functions ── */

/* PoP: _folder @ agent/lsp/client.py:_folder */
char *_folder(void) { return strdup(""); }

/* PoP: _connection_is_open @ agent/lsp/client.py:_connection_is_open */
int _connection_is_open(void) { return 0; }

/* PoP: add_workspace_folder @ agent/lsp/client.py:add_workspace_folder */
void add_workspace_folder(void) {}
