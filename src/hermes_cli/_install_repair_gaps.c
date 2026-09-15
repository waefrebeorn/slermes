#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/_install_repair.py: 8 missing functions ── */

/* PoP: _stdout_to_stderr @ hermes_cli/_install_repair.py:_stdout_to_stderr */
void _stdout_to_stderr(void) {}

/* PoP: _venv_is_relocatable @ hermes_cli/_install_repair.py:_venv_is_relocatable */
int _venv_is_relocatable(void) { return 0; }

/* PoP: _normalize_windows_path @ hermes_cli/_install_repair.py:_normalize_windows_path */
char *_normalize_windows_path(void) { return strdup(""); }

/* PoP: _windows_user_path_entries @ hermes_cli/_install_repair.py:_windows_user_path_entries */
char *_windows_user_path_entries(void) { return strdup("[]"); }

/* PoP: ensure_windows_bin_launchers @ hermes_cli/_install_repair.py:ensure_windows_bin_launchers */
void ensure_windows_bin_launchers(void) {}

/* PoP: _read_user_path_raw @ hermes_cli/_install_repair.py:_read_user_path_raw */
char *_read_user_path_raw(void) { return strdup(""); }

/* PoP: _write_user_path_raw @ hermes_cli/_install_repair.py:_write_user_path_raw */
void _write_user_path_raw(void) {}

/* PoP: migrate_windows_bin_path @ hermes_cli/_install_repair.py:migrate_windows_bin_path */
void migrate_windows_bin_path(void) {}
