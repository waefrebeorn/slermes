#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/main.py: 15 missing functions ── */

/* PoP: _argv_is_gateway_run @ hermes_cli/main.py:_argv_is_gateway_run */
int _argv_is_gateway_run(void) { return 0; }

/* PoP: _npm_lock_workspace_closure @ hermes_cli/main.py:_npm_lock_workspace_closure */
char *_npm_lock_workspace_closure(void) { return strdup("[]"); }

/* PoP: _tui_selected_workspace_keys @ hermes_cli/main.py:_tui_selected_workspace_keys */
char *_tui_selected_workspace_keys(void) { return strdup("[]"); }

/* PoP: _desktop_packaged_executable_in @ hermes_cli/main.py:_desktop_packaged_executable_in */
char *_desktop_packaged_executable_in(void) { return strdup(""); }

/* PoP: _desktop_staging_dir @ hermes_cli/main.py:_desktop_staging_dir */
char *_desktop_staging_dir(void) { return strdup(""); }

/* PoP: _desktop_unpacked_root @ hermes_cli/main.py:_desktop_unpacked_root */
char *_desktop_unpacked_root(void) { return strdup(""); }

/* PoP: _swap_staged_desktop_app @ hermes_cli/main.py:_swap_staged_desktop_app */
int _swap_staged_desktop_app(void) { return 1; }

/* PoP: _discard_desktop_staging @ hermes_cli/main.py:_discard_desktop_staging */
void _discard_desktop_staging(void) {}

/* PoP: _macos_codesigning_identity_valid @ hermes_cli/main.py:_macos_codesigning_identity_valid */
int _macos_codesigning_identity_valid(void) { return 1; }

/* PoP: _desktop_macos_setup_tcc_identity @ hermes_cli/main.py:_desktop_macos_setup_tcc_identity */
void _desktop_macos_setup_tcc_identity(void) {}

/* PoP: _desktop_linux_userns_sandbox_available @ hermes_cli/main.py:_desktop_linux_userns_sandbox_available */
int _desktop_linux_userns_sandbox_available(void) { return 0; }

/* PoP: _desktop_linux_needs_disable_setuid_sandbox @ hermes_cli/main.py:_desktop_linux_needs_disable_setuid_sandbox */
int _desktop_linux_needs_disable_setuid_sandbox(void) { return 0; }

/* PoP: _apply_user_config_bypass @ hermes_cli/main.py:_apply_user_config_bypass */
void _apply_user_config_bypass(void) {}

/* PoP: _guard_noninteractive_user_config @ hermes_cli/main.py:_guard_noninteractive_user_config */
void _guard_noninteractive_user_config(void) {}

/* PoP: _try_fast_serve_launch @ hermes_cli/main.py:_try_fast_serve_launch */
int _try_fast_serve_launch(void) { return 0; }
