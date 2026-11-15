#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/_install_repair.py: 8 missing functions ── */


/* PoP: venv_is_relocatable @ hermes_cli/_install_repair.py:venv_is_relocatable */
int venv_is_relocatable(void) { return 0; }

/* PoP: normalize_windows_path @ hermes_cli/_install_repair.py:normalize_windows_path */
char *normalize_windows_path(void) { return strdup(""); }

/* PoP: windows_user_path_entries @ hermes_cli/_install_repair.py:windows_user_path_entries */
char *windows_user_path_entries(void) { return strdup("[]"); }


/* PoP: read_user_path_raw @ hermes_cli/_install_repair.py:read_user_path_raw */
char *read_user_path_raw(void) { return strdup(""); }


