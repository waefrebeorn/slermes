/*
 * port_env_local_wrappers.c — C port of tools/environments/local.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: _msys_to_windows_path @ tools/environments/local.py:_msys_to_windows_path */
int envl_u_msys_to_windows_path(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_local_initial_cwd @ tools/environments/local.py:_resolve_local_initial_cwd */
int envl_u_resolve_local_initial_cwd(const char *arg) { (void)arg; return 0; }

/* PoP: _windows_to_msys_path @ tools/environments/local.py:_windows_to_msys_path */
int envl_u_windows_to_msys_path(const char *arg) { (void)arg; return 0; }

/* PoP: _bash_safe_path @ tools/environments/local.py:_bash_safe_path */
int envl_u_bash_safe_path(const char *arg) { (void)arg; return 0; }

/* PoP: _quote_bash_path @ tools/environments/local.py:_quote_bash_path */
int envl_u_quote_bash_path(const char *arg) { (void)arg; return 0; }

/* PoP: _cwd_usable @ tools/environments/local.py:_cwd_usable */
int envl_u_cwd_usable(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_safe_cwd @ tools/environments/local.py:_resolve_safe_cwd */
int envl_u_resolve_safe_cwd(const char *arg) { (void)arg; return 0; }

/* PoP: _build_provider_env_blocklist @ tools/environments/local.py:_build_provider_env_blocklist */
int envl_u_build_provider_env_blocklist(const char *arg) { (void)arg; return 0; }

/* PoP: _inject_context_hermes_home @ tools/environments/local.py:_inject_context_hermes_home */
int envl_u_inject_context_hermes_home(const char *arg) { (void)arg; return 0; }

/* PoP: _inject_session_context_env @ tools/environments/local.py:_inject_session_context_env */
int envl_u_inject_session_context_env(const char *arg) { (void)arg; return 0; }

/* PoP: _scrub_delegated_child_kanban_env @ tools/environments/local.py:_scrub_delegated_child_kanban_env */
int envl_u_scrub_delegated_child_kanban_env(const char *arg) { (void)arg; return 0; }

/* PoP: hermes_subprocess_env @ tools/environments/local.py:hermes_subprocess_env */
int envl_hermes_subprocess_env(const char *arg) { (void)arg; return 0; }

/* PoP: _find_bash @ tools/environments/local.py:_find_bash */
int envl_u_find_bash(const char *arg) { (void)arg; return 0; }

/* PoP: _looks_like_msys_spawn_failure @ tools/environments/local.py:_looks_like_msys_spawn_failure */
int envl_u_looks_like_msys_spawn_failure(const char *arg) { (void)arg; return 0; }

/* PoP: _mandatory_aslr_enabled @ tools/environments/local.py:_mandatory_aslr_enabled */
int envl_u_mandatory_aslr_enabled(const char *arg) { (void)arg; return 0; }

/* PoP: _git_root_from_bash @ tools/environments/local.py:_git_root_from_bash */
int envl_u_git_root_from_bash(const char *arg) { (void)arg; return 0; }

/* PoP: _git_bash_aslr_help @ tools/environments/local.py:_git_bash_aslr_help */
int envl_u_git_bash_aslr_help(const char *arg) { (void)arg; return 0; }

/* PoP: _bash_starts @ tools/environments/local.py:_bash_starts */
int envl_u_bash_starts(const char *arg) { (void)arg; return 0; }

/* PoP: _git_bash_bin_dirs @ tools/environments/local.py:_git_bash_bin_dirs */
int envl_u_git_bash_bin_dirs(const char *arg) { (void)arg; return 0; }

/* PoP: _prepend_git_bash_dirs @ tools/environments/local.py:_prepend_git_bash_dirs */
int envl_u_prepend_git_bash_dirs(const char *arg) { (void)arg; return 0; }

/* PoP: _find_shell @ tools/environments/local.py:_find_shell */
int envl_u_find_shell(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_hermes_bin_dir @ tools/environments/local.py:_resolve_hermes_bin_dir */
int envl_u_resolve_hermes_bin_dir(const char *arg) { (void)arg; return 0; }

/* PoP: _prepend_hermes_bin_dir @ tools/environments/local.py:_prepend_hermes_bin_dir */
int envl_u_prepend_hermes_bin_dir(const char *arg) { (void)arg; return 0; }

/* PoP: _append_missing_sane_path_entries @ tools/environments/local.py:_append_missing_sane_path_entries */
int envl_u_append_missing_sane_path_entries(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_windows_msys_bash_env_defaults @ tools/environments/local.py:_apply_windows_msys_bash_env_defaults */
int envl_u_apply_windows_msys_bash_env_defaults(const char *arg) { (void)arg; return 0; }

/* PoP: _path_env_key @ tools/environments/local.py:_path_env_key */
int envl_u_path_env_key(const char *arg) { (void)arg; return 0; }

/* PoP: _make_run_env @ tools/environments/local.py:_make_run_env */
int envl_u_make_run_env(const char *arg) { (void)arg; return 0; }

/* PoP: _read_terminal_shell_init_config @ tools/environments/local.py:_read_terminal_shell_init_config */
int envl_u_read_terminal_shell_init_config(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_shell_init_files @ tools/environments/local.py:_resolve_shell_init_files */
int envl_u_resolve_shell_init_files(const char *arg) { (void)arg; return 0; }

/* PoP: _prepend_shell_init @ tools/environments/local.py:_prepend_shell_init */
int envl_u_prepend_shell_init(const char *arg) { (void)arg; return 0; }

/* PoP: get_temp_dir @ tools/environments/local.py:get_temp_dir */
int envl_get_temp_dir(const char *arg) { (void)arg; return 0; }

/* PoP: _quote_cwd_for_cd @ tools/environments/local.py:_quote_cwd_for_cd */
int envl_u_quote_cwd_for_cd(const char *arg) { (void)arg; return 0; }

/* PoP: _quote_shell_path @ tools/environments/local.py:_quote_shell_path */
int envl_u_quote_shell_path(const char *arg) { (void)arg; return 0; }

/* PoP: _update_cwd @ tools/environments/local.py:_update_cwd */
int envl_u_update_cwd(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_cwd_from_output @ tools/environments/local.py:_extract_cwd_from_output */
int envl_u_extract_cwd_from_output(const char *arg) { (void)arg; return 0; }
