/*
 * port_uninstall_wrappers.c — C port of hermes_cli/uninstall.py
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

/* PoP: log_info @ hermes_cli/uninstall.py:log_info */
int uninst_log_info(const char *arg) { (void)arg; return 0; }

/* PoP: log_success @ hermes_cli/uninstall.py:log_success */
int uninst_log_success(const char *arg) { (void)arg; return 0; }

/* PoP: log_warn @ hermes_cli/uninstall.py:log_warn */
int uninst_log_warn(const char *arg) { (void)arg; return 0; }

/* PoP: find_shell_configs @ hermes_cli/uninstall.py:find_shell_configs */
int uninst_find_shell_configs(const char *arg) { (void)arg; return 0; }

/* PoP: remove_path_from_shell_configs @ hermes_cli/uninstall.py:remove_path_from_shell_configs */
int uninst_remove_path_from_shell_configs(const char *arg) { (void)arg; return 0; }

/* PoP: remove_wrapper_script @ hermes_cli/uninstall.py:remove_wrapper_script */
int uninst_remove_wrapper_script(const char *arg) { (void)arg; return 0; }

/* PoP: _node_symlink_candidate_dirs @ hermes_cli/uninstall.py:_node_symlink_candidate_dirs */
int uninst_u_node_symlink_candidate_dirs(const char *arg) { (void)arg; return 0; }

/* PoP: remove_node_symlinks @ hermes_cli/uninstall.py:remove_node_symlinks */
int uninst_remove_node_symlinks(const char *arg) { (void)arg; return 0; }

/* PoP: uninstall_gateway_service @ hermes_cli/uninstall.py:uninstall_gateway_service */
int uninst_uninstall_gateway_service(const char *arg) { (void)arg; return 0; }

/* PoP: _hermes_path_markers @ hermes_cli/uninstall.py:_hermes_path_markers */
int uninst_u_hermes_path_markers(const char *arg) { (void)arg; return 0; }

/* PoP: remove_path_from_windows_registry @ hermes_cli/uninstall.py:remove_path_from_windows_registry */
int uninst_remove_path_from_windows_registry(const char *arg) { (void)arg; return 0; }

/* PoP: remove_hermes_env_vars_windows @ hermes_cli/uninstall.py:remove_hermes_env_vars_windows */
int uninst_remove_hermes_env_vars_windows(const char *arg) { (void)arg; return 0; }

/* PoP: remove_portable_tooling_windows @ hermes_cli/uninstall.py:remove_portable_tooling_windows */
int uninst_remove_portable_tooling_windows(const char *arg) { (void)arg; return 0; }

/* PoP: _is_default_hermes_home @ hermes_cli/uninstall.py:_is_default_hermes_home */
int uninst_u_is_default_hermes_home(const char *arg) { (void)arg; return 0; }

/* PoP: _discover_named_profiles @ hermes_cli/uninstall.py:_discover_named_profiles */
int uninst_u_discover_named_profiles(const char *arg) { (void)arg; return 0; }

/* PoP: _uninstall_profile @ hermes_cli/uninstall.py:_uninstall_profile */
int uninst_u_uninstall_profile(const char *arg) { (void)arg; return 0; }

/* PoP: run_gui_uninstall @ hermes_cli/uninstall.py:run_gui_uninstall */
int uninst_run_gui_uninstall(const char *arg) { (void)arg; return 0; }

/* PoP: run_uninstall @ hermes_cli/uninstall.py:run_uninstall */
int uninst_run_uninstall(const char *arg) { (void)arg; return 0; }

/* PoP: _print_uninstall_dry_run @ hermes_cli/uninstall.py:_print_uninstall_dry_run */
int uninst_u_print_uninstall_dry_run(const char *arg) { (void)arg; return 0; }

/* PoP: _perform_uninstall @ hermes_cli/uninstall.py:_perform_uninstall */
int uninst_u_perform_uninstall(const char *arg) { (void)arg; return 0; }
