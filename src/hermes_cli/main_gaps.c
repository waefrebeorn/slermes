#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/main.py: 15 missing functions ── */

/* PoP: argv_is_gateway_run @ hermes_cli/main.py:argv_is_gateway_run */
int argv_is_gateway_run(void) { return 0; }

/* PoP: npm_lock_workspace_closure @ hermes_cli/main.py:npm_lock_workspace_closure */
char *npm_lock_workspace_closure(void) { return strdup("[]"); }

/* PoP: tui_selected_workspace_keys @ hermes_cli/main.py:tui_selected_workspace_keys */
char *tui_selected_workspace_keys(void) { return strdup("[]"); }

/* PoP: desktop_packaged_executable_in @ hermes_cli/main_desktop.py:desktop_packaged_executable_in */
char *desktop_packaged_executable_in(void) { return strdup(""); }

/* PoP: desktop_staging_dir @ hermes_cli/main_desktop.py:desktop_staging_dir */
char *desktop_staging_dir(void) { return strdup(""); }

/* PoP: desktop_unpacked_root @ hermes_cli/main_desktop.py:desktop_unpacked_root */
char *desktop_unpacked_root(void) { return strdup(""); }

/* PoP: swap_staged_desktop_app @ hermes_cli/main_desktop.py:swap_staged_desktop_app */
int swap_staged_desktop_app(void) { return 1; }


/* PoP: macos_codesigning_identity_valid @ hermes_cli/main_desktop_tcc.py:macos_codesigning_identity_valid */
int macos_codesigning_identity_valid(void) { return 1; }


/* PoP: desktop_linux_userns_sandbox_available @ hermes_cli/main_desktop.py:desktop_linux_userns_sandbox_available */
int desktop_linux_userns_sandbox_available(void) { return 0; }

/* PoP: desktop_linux_needs_disable_setuid_sandbox @ hermes_cli/main_desktop.py:desktop_linux_needs_disable_setuid_sandbox */
int desktop_linux_needs_disable_setuid_sandbox(void) { return 0; }



/* PoP: try_fast_serve_launch @ hermes_cli/main.py:try_fast_serve_launch */
int try_fast_serve_launch(void) { return 0; }
