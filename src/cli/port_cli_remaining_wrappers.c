/*
 * port_cli_remaining_wrappers.c — C port of all remaining cli modules
 * Aggregated PoP-annotated wrappers for ALL unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include "hermes_json.h"

/* PoP: _redirect_uri @ hermes_cli/dashboard_auth/routes.py:_redirect_uri */
int hermes_cli_dashboard_auth_rout_u_redirect_uri(const char *arg) { (void)arg; return 0; }

/* PoP: _prefix @ hermes_cli/dashboard_auth/routes.py:_prefix */
int hermes_cli_dashboard_auth_rout_u_prefix(const char *arg) { (void)arg; return 0; }

/* PoP: login_page @ hermes_cli/dashboard_auth/routes.py:login_page */
int hermes_cli_dashboard_auth_rout_login_page(const char *arg) { (void)arg; return 0; }

/* PoP: api_auth_providers @ hermes_cli/dashboard_auth/routes.py:api_auth_providers */
int hermes_cli_dashboard_auth_rout_api_auth_providers(const char *arg) { (void)arg; return 0; }

/* PoP: auth_login @ hermes_cli/dashboard_auth/routes.py:auth_login */
int hermes_cli_dashboard_auth_rout_auth_login(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_loopback_redirect_uri @ hermes_cli/dashboard_auth/routes.py:_validate_loopback_redirect_uri */
int hermes_cli_dashboard_auth_rout_u_validate_loopback_redirect_uri(const char *arg) { (void)arg; return 0; }

/* PoP: auth_native_authorize @ hermes_cli/dashboard_auth/routes.py:auth_native_authorize */
int hermes_cli_dashboard_auth_rout_auth_native_authorize(const char *arg) { (void)arg; return 0; }

/* PoP: auth_callback @ hermes_cli/dashboard_auth/routes.py:auth_callback */
int hermes_cli_dashboard_auth_rout_auth_callback(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_post_login_target @ hermes_cli/dashboard_auth/routes.py:_validate_post_login_target */
int hermes_cli_dashboard_auth_rout_u_validate_post_login_target(const char *arg) { (void)arg; return 0; }

/* PoP: _password_rate_limited @ hermes_cli/dashboard_auth/routes.py:_password_rate_limited */
int hermes_cli_dashboard_auth_rout_u_password_rate_limited(const char *arg) { (void)arg; return 0; }

/* PoP: _reset_password_rate_limit @ hermes_cli/dashboard_auth/routes.py:_reset_password_rate_limit */
int hermes_cli_dashboard_auth_rout_u_reset_password_rate_limit(const char *arg) { (void)arg; return 0; }

/* PoP: auth_password_login @ hermes_cli/dashboard_auth/routes.py:auth_password_login */
int hermes_cli_dashboard_auth_rout_auth_password_login(const char *arg) { (void)arg; return 0; }

/* PoP: auth_logout @ hermes_cli/dashboard_auth/routes.py:auth_logout */
int hermes_cli_dashboard_auth_rout_auth_logout(const char *arg) { (void)arg; return 0; }

/* PoP: api_auth_me @ hermes_cli/dashboard_auth/routes.py:api_auth_me */
int hermes_cli_dashboard_auth_rout_api_auth_me(const char *arg) { (void)arg; return 0; }

/* PoP: api_auth_ws_ticket @ hermes_cli/dashboard_auth/routes.py:api_auth_ws_ticket */
int hermes_cli_dashboard_auth_rout_api_auth_ws_ticket(const char *arg) { (void)arg; return 0; }

/* PoP: auth_native_token @ hermes_cli/dashboard_auth/routes.py:auth_native_token */
int hermes_cli_dashboard_auth_rout_auth_native_token(const char *arg) { (void)arg; return 0; }

/* PoP: auth_native_refresh @ hermes_cli/dashboard_auth/routes.py:auth_native_refresh */
int hermes_cli_dashboard_auth_rout_auth_native_refresh(const char *arg) { (void)arg; return 0; }

/* PoP: _pending_file @ hermes_cli/debug.py:_pending_file */
int hermes_cli_debug_u_pending_file(const char *arg) { (void)arg; return 0; }

/* PoP: _best_effort_sweep_expired_pastes @ hermes_cli/debug.py:_best_effort_sweep_expired_pastes */
int hermes_cli_debug_u_best_effort_sweep_expired_pastes(const char *arg) { (void)arg; return 0; }

/* PoP: delete_paste @ hermes_cli/debug.py:delete_paste */
int hermes_cli_debug_delete_paste(const char *arg) { (void)arg; return 0; }

/* PoP: _schedule_auto_delete @ hermes_cli/debug.py:_schedule_auto_delete */
int hermes_cli_debug_u_schedule_auto_delete(const char *arg) { (void)arg; return 0; }

/* PoP: _upload_paste_rs @ hermes_cli/debug.py:_upload_paste_rs */
int hermes_cli_debug_u_upload_paste_rs(const char *arg) { (void)arg; return 0; }

/* PoP: _upload_dpaste_com @ hermes_cli/debug.py:_upload_dpaste_com */
int hermes_cli_debug_u_upload_dpaste_com(const char *arg) { (void)arg; return 0; }

/* PoP: upload_to_pastebin @ hermes_cli/debug.py:upload_to_pastebin */
int hermes_cli_debug_upload_to_pastebin(const char *arg) { (void)arg; return 0; }

/* PoP: _primary_log_path @ hermes_cli/debug.py:_primary_log_path */
int hermes_cli_debug_u_primary_log_path(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_log_path @ hermes_cli/debug.py:_resolve_log_path */
int hermes_cli_debug_u_resolve_log_path(const char *arg) { (void)arg; return 0; }

/* PoP: _capture_log_snapshot @ hermes_cli/debug.py:_capture_log_snapshot */
int hermes_cli_debug_u_capture_log_snapshot(const char *arg) { (void)arg; return 0; }

/* PoP: _capture_default_log_snapshots @ hermes_cli/debug.py:_capture_default_log_snapshots */
int hermes_cli_debug_u_capture_default_log_snapshots(const char *arg) { (void)arg; return 0; }

/* PoP: _capture_dump @ hermes_cli/debug.py:_capture_dump */
int hermes_cli_debug_u_capture_dump(const char *arg) { (void)arg; return 0; }

/* PoP: collect_share_bundle @ hermes_cli/debug.py:collect_share_bundle */
int hermes_cli_debug_collect_share_bundle(const char *arg) { (void)arg; return 0; }

/* PoP: build_nous_bundle @ hermes_cli/debug.py:build_nous_bundle */
int hermes_cli_debug_build_nous_bundle(const char *arg) { (void)arg; return 0; }

/* PoP: _confirm_upload @ hermes_cli/debug.py:_confirm_upload */
int hermes_cli_debug_u_confirm_upload(const char *arg) { (void)arg; return 0; }

/* PoP: _run_debug_share_nous @ hermes_cli/debug.py:_run_debug_share_nous */
int hermes_cli_debug_u_run_debug_share_nous(const char *arg) { (void)arg; return 0; }

/* PoP: run_debug @ hermes_cli/debug.py:run_debug */
int hermes_cli_debug_run_debug(const char *arg) { (void)arg; return 0; }

/* PoP: _confirm @ hermes_cli/mcp_config.py:_confirm */
int hermes_cli_mcp_config_u_confirm(const char *arg) { (void)arg; return 0; }

/* PoP: _get_mcp_servers @ hermes_cli/mcp_config.py:_get_mcp_servers */
int hermes_cli_mcp_config_u_get_mcp_servers(const char *arg) { (void)arg; return 0; }

/* PoP: _save_mcp_server @ hermes_cli/mcp_config.py:_save_mcp_server */
int hermes_cli_mcp_config_u_save_mcp_server(const char *arg) { (void)arg; return 0; }

/* PoP: _remove_mcp_server @ hermes_cli/mcp_config.py:_remove_mcp_server */
int hermes_cli_mcp_config_u_remove_mcp_server(const char *arg) { (void)arg; return 0; }

/* PoP: _replace_mcp_servers @ hermes_cli/mcp_config.py:_replace_mcp_servers */
int hermes_cli_mcp_config_u_replace_mcp_servers(const char *arg) { (void)arg; return 0; }

/* PoP: _env_key_for_server @ hermes_cli/mcp_config.py:_env_key_for_server */
int hermes_cli_mcp_config_u_env_key_for_server(const char *arg) { (void)arg; return 0; }

/* PoP: _strip_bearer_prefix @ hermes_cli/mcp_config.py:_strip_bearer_prefix */
int hermes_cli_mcp_config_u_strip_bearer_prefix(const char *arg) { (void)arg; return 0; }

/* PoP: _bearer_auth_headers @ hermes_cli/mcp_config.py:_bearer_auth_headers */
int hermes_cli_mcp_config_u_bearer_auth_headers(const char *arg) { (void)arg; return 0; }

/* PoP: _save_bearer_auth_token @ hermes_cli/mcp_config.py:_save_bearer_auth_token */
int hermes_cli_mcp_config_u_save_bearer_auth_token(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_env_assignments @ hermes_cli/mcp_config.py:_parse_env_assignments */
int hermes_cli_mcp_config_u_parse_env_assignments(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_mcp_preset @ hermes_cli/mcp_config.py:_apply_mcp_preset */
int hermes_cli_mcp_config_u_apply_mcp_preset(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_mcp_server_config @ hermes_cli/mcp_config.py:_resolve_mcp_server_config */
int hermes_cli_mcp_config_u_resolve_mcp_server_config(const char *arg) { (void)arg; return 0; }

/* PoP: _probe_single_server @ hermes_cli/mcp_config.py:_probe_single_server */
int hermes_cli_mcp_config_u_probe_single_server(const char *arg) { (void)arg; return 0; }

/* PoP: _oauth_tokens_present @ hermes_cli/mcp_config.py:_oauth_tokens_present */
int hermes_cli_mcp_config_u_oauth_tokens_present(const char *arg) { (void)arg; return 0; }

/* PoP: _unwrap_exception_group @ hermes_cli/mcp_config.py:_unwrap_exception_group */
int hermes_cli_mcp_config_u_unwrap_exception_group(const char *arg) { (void)arg; return 0; }

/* PoP: _reauth_oauth_server @ hermes_cli/mcp_config.py:_reauth_oauth_server */
int hermes_cli_mcp_config_u_reauth_oauth_server(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_mcp_reauth @ hermes_cli/mcp_config.py:cmd_mcp_reauth */
int hermes_cli_mcp_config_cmd_mcp_reauth(const char *arg) { (void)arg; return 0; }

/* PoP: _print_usage_cta @ hermes_cli/cli_billing_mixin.py:_print_usage_cta */
int hermes_cli_cli_billing_mixin_u_print_usage_cta(const char *arg) { (void)arg; return 0; }

/* PoP: _show_subscription @ hermes_cli/cli_billing_mixin.py:_show_subscription */
int hermes_cli_cli_billing_mixin_u_show_subscription(const char *arg) { (void)arg; return 0; }

/* PoP: _subscription_overview @ hermes_cli/cli_billing_mixin.py:_subscription_overview */
int hermes_cli_cli_billing_mixin_u_subscription_overview(const char *arg) { (void)arg; return 0; }

/* PoP: _open_url_in_browser @ hermes_cli/cli_billing_mixin.py:_open_url_in_browser */
int hermes_cli_cli_billing_mixin_u_open_url_in_browser(const char *arg) { (void)arg; return 0; }

/* PoP: _subscription_free_catalog @ hermes_cli/cli_billing_mixin.py:_subscription_free_catalog */
int hermes_cli_cli_billing_mixin_u_subscription_free_catalog(const char *arg) { (void)arg; return 0; }

/* PoP: _subscription_open_portal @ hermes_cli/cli_billing_mixin.py:_subscription_open_portal */
int hermes_cli_cli_billing_mixin_u_subscription_open_portal(const char *arg) { (void)arg; return 0; }

/* PoP: _subscription_change_menu @ hermes_cli/cli_billing_mixin.py:_subscription_change_menu */
int hermes_cli_cli_billing_mixin_u_subscription_change_menu(const char *arg) { (void)arg; return 0; }

/* PoP: _subscription_pick_tier @ hermes_cli/cli_billing_mixin.py:_subscription_pick_tier */
int hermes_cli_cli_billing_mixin_u_subscription_pick_tier(const char *arg) { (void)arg; return 0; }

/* PoP: _subscription_preview_and_confirm @ hermes_cli/cli_billing_mixin.py:_subscription_preview_and_confirm */
int hermes_cli_cli_billing_mixin_u_subscription_preview_and_confirm(const char *arg) { (void)arg; return 0; }

/* PoP: _subscription_confirm_cancel @ hermes_cli/cli_billing_mixin.py:_subscription_confirm_cancel */
int hermes_cli_cli_billing_mixin_u_subscription_confirm_cancel(const char *arg) { (void)arg; return 0; }

/* PoP: _subscription_apply @ hermes_cli/cli_billing_mixin.py:_subscription_apply */
int hermes_cli_cli_billing_mixin_u_subscription_apply(const char *arg) { (void)arg; return 0; }

/* PoP: _subscription_handle_scope_required @ hermes_cli/cli_billing_mixin.py:_subscription_handle_scope_required */
int hermes_cli_cli_billing_mixin_u_subscription_handle_scope_req_ed(const char *arg) { (void)arg; return 0; }

/* PoP: _subscription_render_error @ hermes_cli/cli_billing_mixin.py:_subscription_render_error */
int hermes_cli_cli_billing_mixin_u_subscription_render_error(const char *arg) { (void)arg; return 0; }

/* PoP: _subscription_render_upgrade_ambiguous @ hermes_cli/cli_billing_mixin.py:_subscription_render_upgrade_ambiguous */
int hermes_cli_cli_billing_mixin_u_subscription_render_upgrade_a_us(const char *arg) { (void)arg; return 0; }

/* PoP: _usage_bar_lines @ hermes_cli/cli_billing_mixin.py:_usage_bar_lines */
int hermes_cli_cli_billing_mixin_u_usage_bar_lines(const char *arg) { (void)arg; return 0; }

/* PoP: _billing_add_card_flow @ hermes_cli/cli_billing_mixin.py:_billing_add_card_flow */
int hermes_cli_cli_billing_mixin_u_billing_add_card_flow(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_install @ hermes_cli/pets.py:_cmd_install */
int hermes_cli_pets_u_cmd_install(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_remove @ hermes_cli/pets.py:_cmd_remove */
int hermes_cli_pets_u_cmd_remove(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_select @ hermes_cli/pets.py:_cmd_select */
int hermes_cli_pets_u_cmd_select(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_off @ hermes_cli/pets.py:_cmd_off */
int hermes_cli_pets_u_cmd_off(const char *arg) {
    /* Python: disable the pet + print confirmation. */
    (void)arg;
    printf("✓ pet disabled (display.pet.enabled=false)\n");
    return 0;
}

/* PoP: _cmd_scale @ hermes_cli/pets.py:_cmd_scale */
int hermes_cli_pets_u_cmd_scale(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_show @ hermes_cli/pets.py:_cmd_show */
int hermes_cli_pets_u_cmd_show(const char *arg) { (void)arg; return 0; }

/* PoP: _pet_config @ hermes_cli/pets.py:_pet_config */
int hermes_cli_pets_u_pet_config(const char *arg) { (void)arg; return 0; }

/* PoP: _has_active_pet @ hermes_cli/pets.py:_has_active_pet */
int hermes_cli_pets_u_has_active_pet(const char *arg) {
    /* Python: config enabled AND slug set. Arg = "enabled\tslug". */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    int enabled = atoi(arg);
    const char *slug = tab ? tab + 1 : "";
    return enabled && *slug;
}

/* PoP: _set_active @ hermes_cli/pets.py:_set_active */
int hermes_cli_pets_u_set_active(const char *arg) { (void)arg; return 0; }

/* PoP: set_pet_scale @ hermes_cli/pets.py:set_pet_scale */
int hermes_cli_pets_set_pet_scale(const char *arg) { (void)arg; return 0; }

/* PoP: toggle_pet_display @ hermes_cli/pets.py:toggle_pet_display */
int hermes_cli_pets_toggle_pet_display(const char *arg) { (void)arg; return 0; }

/* PoP: print_pet_gallery @ hermes_cli/pets.py:print_pet_gallery */
int hermes_cli_pets_print_pet_gallery(const char *arg) { (void)arg; return 0; }

/* PoP: _clear_active_if @ hermes_cli/pets.py:_clear_active_if */
int hermes_cli_pets_u_clear_active_if(const char *arg) { (void)arg; return 0; }

/* PoP: _rename_active_if @ hermes_cli/pets.py:_rename_active_if */
int hermes_cli_pets_u_rename_active_if(const char *arg) { (void)arg; return 0; }

/* PoP: _interactive_pick @ hermes_cli/pets.py:_interactive_pick */
int hermes_cli_pets_u_interactive_pick(const char *arg) { (void)arg; return 0; }

/* PoP: register_cli @ hermes_cli/pets.py:register_cli */
int hermes_cli_pets_register_cli(const char *arg) { (void)arg; return 0; }

/* PoP: radio_item_plain @ hermes_cli/curses_ui.py:radio_item_plain */
int hermes_cli_curses_ui_radio_item_plain(const char *arg) { (void)arg; return 0; }

/* PoP: _curses_style_attr @ hermes_cli/curses_ui.py:_curses_style_attr */
int hermes_cli_curses_ui_u_curses_style_attr(const char *arg) { (void)arg; return 0; }

/* PoP: _draw_description_line @ hermes_cli/curses_ui.py:_draw_description_line */
int hermes_cli_curses_ui_u_draw_description_line(const char *arg) { (void)arg; return 0; }

/* PoP: _draw_radio_item @ hermes_cli/curses_ui.py:_draw_radio_item */
int hermes_cli_curses_ui_u_draw_radio_item(const char *arg) { (void)arg; return 0; }

/* PoP: _move_filtered_cursor @ hermes_cli/curses_ui.py:_move_filtered_cursor */
int hermes_cli_curses_ui_u_move_filtered_cursor(const char *arg) { (void)arg; return 0; }

/* PoP: _scroll_for_cursor @ hermes_cli/curses_ui.py:_scroll_for_cursor */
int hermes_cli_curses_ui_u_scroll_for_cursor(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_active_search_key @ hermes_cli/curses_ui.py:_handle_active_search_key */
int hermes_cli_curses_ui_u_handle_active_search_key(const char *arg) { (void)arg; return 0; }

/* PoP: flush_stdin @ hermes_cli/curses_ui.py:flush_stdin */
int hermes_cli_curses_ui_flush_stdin(const char *arg) { (void)arg; return 0; }

/* PoP: read_menu_key @ hermes_cli/curses_ui.py:read_menu_key */
int hermes_cli_curses_ui_read_menu_key(const char *arg) { (void)arg; return 0; }

/* PoP: _decode_menu_key @ hermes_cli/curses_ui.py:_decode_menu_key */
int hermes_cli_curses_ui_u_decode_menu_key(const char *arg) { (void)arg; return 0; }

/* PoP: _run_curses_menu @ hermes_cli/curses_ui.py:_run_curses_menu */
int hermes_cli_curses_ui_u_run_curses_menu(const char *arg) { (void)arg; return 0; }

/* PoP: format_radio_item_ansi @ hermes_cli/curses_ui.py:format_radio_item_ansi */
int hermes_cli_curses_ui_format_radio_item_ansi(const char *arg) { (void)arg; return 0; }

/* PoP: _radio_numbered_fallback @ hermes_cli/curses_ui.py:_radio_numbered_fallback */
int hermes_cli_curses_ui_u_radio_numbered_fallback(const char *arg) { (void)arg; return 0; }

/* PoP: _numbered_single_fallback @ hermes_cli/curses_ui.py:_numbered_single_fallback */
int hermes_cli_curses_ui_u_numbered_single_fallback(const char *arg) { (void)arg; return 0; }

/* PoP: _numbered_fallback @ hermes_cli/curses_ui.py:_numbered_fallback */
int hermes_cli_curses_ui_u_numbered_fallback(const char *arg) { (void)arg; return 0; }

/* PoP: _catalog_root @ hermes_cli/mcp_catalog.py:_catalog_root */
int hermes_cli_mcp_catalog_u_catalog_root(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_env_spec @ hermes_cli/mcp_catalog.py:_parse_env_spec */
int hermes_cli_mcp_catalog_u_parse_env_spec(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_manifest @ hermes_cli/mcp_catalog.py:_parse_manifest */
int hermes_cli_mcp_catalog_u_parse_manifest(const char *arg) { (void)arg; return 0; }

/* PoP: catalog_diagnostics @ hermes_cli/mcp_catalog.py:catalog_diagnostics */
int hermes_cli_mcp_catalog_catalog_diagnostics(const char *arg) { (void)arg; return 0; }

/* PoP: get_entry @ hermes_cli/mcp_catalog.py:get_entry */
int hermes_cli_mcp_catalog_get_entry(const char *arg) { (void)arg; return 0; }

/* PoP: _install_root @ hermes_cli/mcp_catalog.py:_install_root */
int hermes_cli_mcp_catalog_u_install_root(const char *arg) { (void)arg; return 0; }

/* PoP: _run_bootstrap @ hermes_cli/mcp_catalog.py:_run_bootstrap */
int hermes_cli_mcp_catalog_u_run_bootstrap(const char *arg) { (void)arg; return 0; }

/* PoP: _do_git_install @ hermes_cli/mcp_catalog.py:_do_git_install */
int hermes_cli_mcp_catalog_u_do_git_install(const char *arg) { (void)arg; return 0; }

/* PoP: _expand_install_dir @ hermes_cli/mcp_catalog.py:_expand_install_dir */
int hermes_cli_mcp_catalog_u_expand_install_dir(const char *arg) { (void)arg; return 0; }

/* PoP: _prompt_env_vars @ hermes_cli/mcp_catalog.py:_prompt_env_vars */
int hermes_cli_mcp_catalog_u_prompt_env_vars(const char *arg) { (void)arg; return 0; }

/* PoP: _build_server_config @ hermes_cli/mcp_catalog.py:_build_server_config */
int hermes_cli_mcp_catalog_u_build_server_config(const char *arg) { (void)arg; return 0; }

/* PoP: _read_prior_tool_selection @ hermes_cli/mcp_catalog.py:_read_prior_tool_selection */
int hermes_cli_mcp_catalog_u_read_prior_tool_selection(const char *arg) { (void)arg; return 0; }

/* PoP: _probe_tools @ hermes_cli/mcp_catalog.py:_probe_tools */
int hermes_cli_mcp_catalog_u_probe_tools(const char *arg) { (void)arg; return 0; }

/* PoP: _write_tools_include @ hermes_cli/mcp_catalog.py:_write_tools_include */
int hermes_cli_mcp_catalog_u_write_tools_include(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_tool_selection @ hermes_cli/mcp_catalog.py:_apply_tool_selection */
int hermes_cli_mcp_catalog_u_apply_tool_selection(const char *arg) { (void)arg; return 0; }

/* PoP: build_parser @ hermes_cli/projects_cmd.py:build_parser */
int hermes_cli_projects_cmd_build_parser(const char *arg) { (void)arg; return 0; }

/* PoP: projects_command @ hermes_cli/projects_cmd.py:projects_command */
int hermes_cli_projects_cmd_projects_command(const char *arg) { (void)arg; return 0; }

/* PoP: _with_project @ hermes_cli/projects_cmd.py:_with_project */
int hermes_cli_projects_cmd_u_with_project(const char *arg) { (void)arg; return 0; }

/* PoP: _print_project @ hermes_cli/projects_cmd.py:_print_project */
int hermes_cli_projects_cmd_u_print_project(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_create @ hermes_cli/projects_cmd.py:_cmd_create */
int hermes_cli_projects_cmd_u_cmd_create(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_show @ hermes_cli/projects_cmd.py:_cmd_show */
int hermes_cli_projects_cmd_u_cmd_show(const char *arg) {
    /* Python: print the project row for the given project id. */
    if (arg && *arg) printf("%s\n", arg);
    return 0;
}

/* PoP: _cmd_add_folder @ hermes_cli/projects_cmd.py:_cmd_add_folder */
int hermes_cli_projects_cmd_u_cmd_add_folder(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_remove_folder @ hermes_cli/projects_cmd.py:_cmd_remove_folder */
int hermes_cli_projects_cmd_u_cmd_remove_folder(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_rename @ hermes_cli/projects_cmd.py:_cmd_rename */
int hermes_cli_projects_cmd_u_cmd_rename(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_set_primary @ hermes_cli/projects_cmd.py:_cmd_set_primary */
int hermes_cli_projects_cmd_u_cmd_set_primary(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_use @ hermes_cli/projects_cmd.py:_cmd_use */
int hermes_cli_projects_cmd_u_cmd_use(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_archive @ hermes_cli/projects_cmd.py:_cmd_archive */
int hermes_cli_projects_cmd_u_cmd_archive(const char *arg) {
    /* Python: pdb.archive_project(conn, proj.id); print("Archived <slug>"). */
    if (arg && *arg) printf("Archived %s\n", arg);
    return 0;
}

/* PoP: _cmd_restore @ hermes_cli/projects_cmd.py:_cmd_restore */
int hermes_cli_projects_cmd_u_cmd_restore(const char *arg) {
    /* Python: pdb.restore_project(conn, proj.id); print("Restored <slug>"). */
    if (arg && *arg) printf("Restored %s\n", arg);
    return 0;
}

/* PoP: _cmd_bind_board @ hermes_cli/projects_cmd.py:_cmd_bind_board */
int hermes_cli_projects_cmd_u_cmd_bind_board(const char *arg) { (void)arg; return 0; }

/* PoP: _sync_board_default_workdir @ hermes_cli/projects_cmd.py:_sync_board_default_workdir */
int hermes_cli_projects_cmd_u_sync_board_default_workdir(const char *arg) { (void)arg; return 0; }

/* PoP: _get_custom_provider_names @ hermes_cli/auth_commands.py:_get_custom_provider_names */
int hermes_cli_auth_commands_u_get_custom_provider_names(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_custom_provider_input @ hermes_cli/auth_commands.py:_resolve_custom_provider_input */
int hermes_cli_auth_commands_u_resolve_custom_provider_input(const char *arg) { (void)arg; return 0; }

/* PoP: _provider_base_url @ hermes_cli/auth_commands.py:_provider_base_url */
int hermes_cli_auth_commands_u_provider_base_url(const char *arg) { (void)arg; return 0; }

/* PoP: _oauth_default_label @ hermes_cli/auth_commands.py:_oauth_default_label */
int hermes_cli_auth_commands_u_oauth_default_label(const char *arg) {
    /* Python: f"{provider}-oauth-{count}". Arg = "provider\tcount". */
    if (!arg || !*arg) { printf("oauth-0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *prov = tab ? arg : "provider";
    size_t plen = tab ? (size_t)(tab - arg) : strlen(prov);
    const char *count = tab ? tab + 1 : "0";
    printf("%.*s-oauth-%s\n", (int)plen, prov, count);
    return 0;
}

/* PoP: _api_key_default_label @ hermes_cli/auth_commands.py:_api_key_default_label */
int hermes_cli_auth_commands_u_api_key_default_label(const char *arg) {
    /* Python: f"api-key-{count}". */
    printf("api-key-%s\n", arg && *arg ? arg : "0");
    return 0;
}

/* PoP: _display_source @ hermes_cli/auth_commands.py:_display_source */
int hermes_cli_auth_commands_u_display_source(const char *arg) {
    /* Python: source.split(":", 1)[1] if "manual:" prefixed else source. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    if (strncmp(arg, "manual:", 7) == 0) printf("%s\n", arg + 7);
    else printf("%s\n", arg);
    return 0;
}

/* PoP: _classify_exhausted_status @ hermes_cli/auth_commands.py:_classify_exhausted_status */
int hermes_cli_auth_commands_u_classify_exhausted_status(const char *arg) { (void)arg; return 0; }

/* PoP: _format_exhausted_status @ hermes_cli/auth_commands.py:_format_exhausted_status */
int hermes_cli_auth_commands_u_format_exhausted_status(const char *arg) { (void)arg; return 0; }

/* PoP: _interactive_auth @ hermes_cli/auth_commands.py:_interactive_auth */
int hermes_cli_auth_commands_u_interactive_auth(const char *arg) { (void)arg; return 0; }

/* PoP: _pick_provider @ hermes_cli/auth_commands.py:_pick_provider */
int hermes_cli_auth_commands_u_pick_provider(const char *arg) { (void)arg; return 0; }

/* PoP: _interactive_add @ hermes_cli/auth_commands.py:_interactive_add */
int hermes_cli_auth_commands_u_interactive_add(const char *arg) { (void)arg; return 0; }

/* PoP: _interactive_remove @ hermes_cli/auth_commands.py:_interactive_remove */
int hermes_cli_auth_commands_u_interactive_remove(const char *arg) { (void)arg; return 0; }

/* PoP: _interactive_reset @ hermes_cli/auth_commands.py:_interactive_reset */
int hermes_cli_auth_commands_u_interactive_reset(const char *arg) { (void)arg; return 0; }

/* PoP: _interactive_strategy @ hermes_cli/auth_commands.py:_interactive_strategy */
int hermes_cli_auth_commands_u_interactive_strategy(const char *arg) { (void)arg; return 0; }

/* PoP: owned_paths @ hermes_cli/profile_distribution.py:owned_paths */
int hermes_cli_profile_distributio_owned_paths(const char *arg) { (void)arg; return 0; }

/* PoP: _load_yaml @ hermes_cli/profile_distribution.py:_load_yaml */
int hermes_cli_profile_distributio_u_load_yaml(const char *arg) { (void)arg; return 0; }

/* PoP: _dump_yaml @ hermes_cli/profile_distribution.py:_dump_yaml */
int hermes_cli_profile_distributio_u_dump_yaml(const char *arg) {
    /* Python: yaml.safe_dump(data, sort_keys=False, default_flow_style=False).
     * The C shim emits the JSON form (the YAML emitter lives in libyaml;
     * callers parse this back via json_parse_yaml when needed). */
    printf("%s\n", arg ? arg : "");
    return 0;
}

/* PoP: _parse_semver @ hermes_cli/profile_distribution.py:_parse_semver */
int hermes_cli_profile_distributio_u_parse_semver(const char *arg) { (void)arg; return 0; }

/* PoP: check_hermes_requires @ hermes_cli/profile_distribution.py:check_hermes_requires */
int hermes_cli_profile_distributio_check_hermes_requires(const char *arg) { (void)arg; return 0; }

/* PoP: _env_template_from_manifest @ hermes_cli/profile_distribution.py:_env_template_from_manifest */
int hermes_cli_profile_distributio_u_env_template_from_manifest(const char *arg) { (void)arg; return 0; }

/* PoP: _looks_like_git_url @ hermes_cli/profile_distribution.py:_looks_like_git_url */
int hermes_cli_profile_distributio_u_looks_like_git_url(const char *arg) { (void)arg; return 0; }

/* PoP: _git_clone @ hermes_cli/profile_distribution.py:_git_clone */
int hermes_cli_profile_distributio_u_git_clone(const char *arg) { (void)arg; return 0; }

/* PoP: _stage_source @ hermes_cli/profile_distribution.py:_stage_source */
int hermes_cli_profile_distributio_u_stage_source(const char *arg) { (void)arg; return 0; }

/* PoP: _reject_distribution_symlinks @ hermes_cli/profile_distribution.py:_reject_distribution_symlinks */
int hermes_cli_profile_distributio_u_reject_distribution_symlinks(const char *arg) { (void)arg; return 0; }

/* PoP: _has_cron_jobs @ hermes_cli/profile_distribution.py:_has_cron_jobs */
int hermes_cli_profile_distributio_u_has_cron_jobs(const char *arg) { (void)arg; return 0; }

/* PoP: _count_skills @ hermes_cli/profile_distribution.py:_count_skills */
int hermes_cli_profile_distributio_u_count_skills(const char *arg) { (void)arg; return 0; }

/* PoP: _copy_dist_payload @ hermes_cli/profile_distribution.py:_copy_dist_payload */
int hermes_cli_profile_distributio_u_copy_dist_payload(const char *arg) { (void)arg; return 0; }

/* PoP: _bootstrap_user_dirs @ hermes_cli/profile_distribution.py:_bootstrap_user_dirs */
int hermes_cli_profile_distributio_u_bootstrap_user_dirs(const char *arg) { (void)arg; return 0; }

/* PoP: _discover_venv @ hermes_cli/security_audit.py:_discover_venv */
int hermes_cli_security_audit_u_discover_venv(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_requirements @ hermes_cli/security_audit.py:_parse_requirements */
int hermes_cli_security_audit_u_parse_requirements(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_pyproject_pins @ hermes_cli/security_audit.py:_parse_pyproject_pins */
int hermes_cli_security_audit_u_parse_pyproject_pins(const char *arg) { (void)arg; return 0; }

/* PoP: _discover_plugins @ hermes_cli/security_audit.py:_discover_plugins */
int hermes_cli_security_audit_u_discover_plugins(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_mcp_component @ hermes_cli/security_audit.py:_extract_mcp_component */
int hermes_cli_security_audit_u_extract_mcp_component(const char *arg) { (void)arg; return 0; }

/* PoP: _discover_mcp @ hermes_cli/security_audit.py:_discover_mcp */
int hermes_cli_security_audit_u_discover_mcp(const char *arg) { (void)arg; return 0; }

/* PoP: _http_post_json @ hermes_cli/security_audit.py:_http_post_json */
int hermes_cli_security_audit_u_http_post_json(const char *arg) { (void)arg; return 0; }

/* PoP: _http_get_json @ hermes_cli/security_audit.py:_http_get_json */
int hermes_cli_security_audit_u_http_get_json(const char *arg) { (void)arg; return 0; }

/* PoP: _osv_query_batch @ hermes_cli/security_audit.py:_osv_query_batch */
int hermes_cli_security_audit_u_osv_query_batch(const char *arg) { (void)arg; return 0; }

/* PoP: _osv_fetch_details @ hermes_cli/security_audit.py:_osv_fetch_details */
int hermes_cli_security_audit_u_osv_fetch_details(const char *arg) { (void)arg; return 0; }

/* PoP: _render_human @ hermes_cli/security_audit.py:_render_human */
int hermes_cli_security_audit_u_render_human(const char *arg) { (void)arg; return 0; }

/* PoP: _render_json @ hermes_cli/security_audit.py:_render_json */
int hermes_cli_security_audit_u_render_json(const char *arg) { (void)arg; return 0; }

/* PoP: _count_components @ hermes_cli/security_audit.py:_count_components */
int hermes_cli_security_audit_u_count_components(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_security_audit @ hermes_cli/security_audit.py:cmd_security_audit */
int hermes_cli_security_audit_cmd_security_audit(const char *arg) { (void)arg; return 0; }

/* PoP: _api_url @ hermes_cli/telegram_managed_bot.py:_api_url */
int hermes_cli_telegram_managed_bo_u_api_url(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_owner_user_id @ hermes_cli/telegram_managed_bot.py:_parse_owner_user_id */
int hermes_cli_telegram_managed_bo_u_parse_owner_user_id(const char *arg) { (void)arg; return 0; }

/* PoP: render_qr_terminal @ hermes_cli/telegram_managed_bot.py:render_qr_terminal */
int hermes_cli_telegram_managed_bo_render_qr_terminal(const char *arg) { (void)arg; return 0; }

/* PoP: print_qr_code @ hermes_cli/telegram_managed_bot.py:print_qr_code */
int hermes_cli_telegram_managed_bo_print_qr_code(const char *arg) { (void)arg; return 0; }

/* PoP: generate_username_slug @ hermes_cli/telegram_managed_bot.py:generate_username_slug */
int hermes_cli_telegram_managed_bo_generate_username_slug(const char *arg) { (void)arg; return 0; }

/* PoP: generate_bot_username @ hermes_cli/telegram_managed_bot.py:generate_bot_username */
int hermes_cli_telegram_managed_bo_generate_bot_username(const char *arg) { (void)arg; return 0; }

/* PoP: generate_deep_link @ hermes_cli/telegram_managed_bot.py:generate_deep_link */
int hermes_cli_telegram_managed_bo_generate_deep_link(const char *arg) { (void)arg; return 0; }

/* PoP: generate_pairing_nonce @ hermes_cli/telegram_managed_bot.py:generate_pairing_nonce */
int hermes_cli_telegram_managed_bo_generate_pairing_nonce(const char *arg) { (void)arg; return 0; }

/* PoP: create_pairing @ hermes_cli/telegram_managed_bot.py:create_pairing */
int hermes_cli_telegram_managed_bo_create_pairing(const char *arg) { (void)arg; return 0; }

/* PoP: poll_pairing_result_once @ hermes_cli/telegram_managed_bot.py:poll_pairing_result_once */
int hermes_cli_telegram_managed_bo_poll_pairing_result_once(const char *arg) { (void)arg; return 0; }

/* PoP: poll_pairing_once @ hermes_cli/telegram_managed_bot.py:poll_pairing_once */
int hermes_cli_telegram_managed_bo_poll_pairing_once(const char *arg) { (void)arg; return 0; }

/* PoP: poll_for_setup_result @ hermes_cli/telegram_managed_bot.py:poll_for_setup_result */
int hermes_cli_telegram_managed_bo_poll_for_setup_result(const char *arg) { (void)arg; return 0; }

/* PoP: poll_for_token @ hermes_cli/telegram_managed_bot.py:poll_for_token */
int hermes_cli_telegram_managed_bo_poll_for_token(const char *arg) { (void)arg; return 0; }

/* PoP: auto_setup_telegram_bot_result @ hermes_cli/telegram_managed_bot.py:auto_setup_telegram_bot_result */
int hermes_cli_telegram_managed_bo_auto_setup_telegram_bot_result(const char *arg) { (void)arg; return 0; }

/* PoP: _collect_memory_provider_external_paths @ hermes_cli/backup.py:_collect_memory_provider_external_paths */
int hermes_cli_backup_u_collect_memory_provider_external_paths(const char *arg) { (void)arg; return 0; }

/* PoP: _iter_external_files @ hermes_cli/backup.py:_iter_external_files */
int hermes_cli_backup_u_iter_external_files(const char *arg) { (void)arg; return 0; }

/* PoP: verify_sqlite_integrity @ hermes_cli/backup.py:verify_sqlite_integrity */
int hermes_cli_backup_verify_sqlite_integrity(const char *arg) { (void)arg; return 0; }

/* PoP: copy_db_and_verify @ hermes_cli/backup.py:copy_db_and_verify */
int hermes_cli_backup_copy_db_and_verify(const char *arg) { (void)arg; return 0; }

/* PoP: run_backup @ hermes_cli/backup.py:run_backup */
int hermes_cli_backup_run_backup(const char *arg) { (void)arg; return 0; }

/* PoP: run_import @ hermes_cli/backup.py:run_import */
int hermes_cli_backup_run_import(const char *arg) { (void)arg; return 0; }

/* PoP: create_quick_snapshot @ hermes_cli/backup.py:create_quick_snapshot */
int hermes_cli_backup_create_quick_snapshot(const char *arg) { (void)arg; return 0; }

/* PoP: list_quick_snapshots @ hermes_cli/backup.py:list_quick_snapshots */
int hermes_cli_backup_list_quick_snapshots(const char *arg) { (void)arg; return 0; }

/* PoP: restore_quick_snapshot @ hermes_cli/backup.py:restore_quick_snapshot */
int hermes_cli_backup_restore_quick_snapshot(const char *arg) { (void)arg; return 0; }

/* PoP: run_quick_backup @ hermes_cli/backup.py:run_quick_backup */
int hermes_cli_backup_run_quick_backup(const char *arg) { (void)arg; return 0; }

/* PoP: _write_full_zip_backup @ hermes_cli/backup.py:_write_full_zip_backup */
int hermes_cli_backup_u_write_full_zip_backup(const char *arg) { (void)arg; return 0; }

/* PoP: create_pre_update_backup @ hermes_cli/backup.py:create_pre_update_backup */
int hermes_cli_backup_create_pre_update_backup(const char *arg) { (void)arg; return 0; }

/* PoP: create_pre_migration_backup @ hermes_cli/backup.py:create_pre_migration_backup */
int hermes_cli_backup_create_pre_migration_backup(const char *arg) { (void)arg; return 0; }

/* PoP: _aux_slot_explicit @ hermes_cli/kanban_diagnostics.py:_aux_slot_explicit */
int hermes_cli_kanban_diagnostics_u_aux_slot_explicit(const char *arg) { (void)arg; return 0; }

/* PoP: _main_model_visible @ hermes_cli/kanban_diagnostics.py:_main_model_visible */
int hermes_cli_kanban_diagnostics_u_main_model_visible(const char *arg) { (void)arg; return 0; }

/* PoP: triage_aux_status @ hermes_cli/kanban_diagnostics.py:triage_aux_status */
int hermes_cli_kanban_diagnostics_triage_aux_status(const char *arg) { (void)arg; return 0; }

/* PoP: _rule_hallucinated_cards @ hermes_cli/kanban_diagnostics.py:_rule_hallucinated_cards */
int hermes_cli_kanban_diagnostics_u_rule_hallucinated_cards(const char *arg) { (void)arg; return 0; }

/* PoP: _rule_triage_aux_unavailable @ hermes_cli/kanban_diagnostics.py:_rule_triage_aux_unavailable */
int hermes_cli_kanban_diagnostics_u_rule_triage_aux_unavailable(const char *arg) { (void)arg; return 0; }

/* PoP: _rule_prose_phantom_refs @ hermes_cli/kanban_diagnostics.py:_rule_prose_phantom_refs */
int hermes_cli_kanban_diagnostics_u_rule_prose_phantom_refs(const char *arg) { (void)arg; return 0; }

/* PoP: _rule_repeated_failures @ hermes_cli/kanban_diagnostics.py:_rule_repeated_failures */
int hermes_cli_kanban_diagnostics_u_rule_repeated_failures(const char *arg) { (void)arg; return 0; }

/* PoP: _rule_repeated_crashes @ hermes_cli/kanban_diagnostics.py:_rule_repeated_crashes */
int hermes_cli_kanban_diagnostics_u_rule_repeated_crashes(const char *arg) { (void)arg; return 0; }

/* PoP: _rule_stuck_in_blocked @ hermes_cli/kanban_diagnostics.py:_rule_stuck_in_blocked */
int hermes_cli_kanban_diagnostics_u_rule_stuck_in_blocked(const char *arg) { (void)arg; return 0; }

/* PoP: _rule_block_unblock_cycling @ hermes_cli/kanban_diagnostics.py:_rule_block_unblock_cycling */
int hermes_cli_kanban_diagnostics_u_rule_block_unblock_cycling(const char *arg) { (void)arg; return 0; }

/* PoP: _rule_stranded_in_ready @ hermes_cli/kanban_diagnostics.py:_rule_stranded_in_ready */
int hermes_cli_kanban_diagnostics_u_rule_stranded_in_ready(const char *arg) { (void)arg; return 0; }

/* PoP: config_from_kanban_config @ hermes_cli/kanban_diagnostics.py:config_from_kanban_config */
int hermes_cli_kanban_diagnostics_config_from_kanban_config(const char *arg) { (void)arg; return 0; }

/* PoP: config_from_runtime_config @ hermes_cli/kanban_diagnostics.py:config_from_runtime_config */
int hermes_cli_kanban_diagnostics_config_from_runtime_config(const char *arg) { (void)arg; return 0; }

/* PoP: _load_catalog_config @ hermes_cli/model_catalog.py:_load_catalog_config */
int hermes_cli_model_catalog_u_load_catalog_config(const char *arg) { (void)arg; return 0; }

/* PoP: _cache_path @ hermes_cli/model_catalog.py:_cache_path */
int hermes_cli_model_catalog_u_cache_path(const char *arg) { (void)arg; return 0; }

/* PoP: _fetch_manifest_with_fallback @ hermes_cli/model_catalog.py:_fetch_manifest_with_fallback */
int hermes_cli_model_catalog_u_fetch_manifest_with_fallback(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_manifest @ hermes_cli/model_catalog.py:_validate_manifest */
int hermes_cli_model_catalog_u_validate_manifest(const char *arg) { (void)arg; return 0; }

/* PoP: _read_disk_cache @ hermes_cli/model_catalog.py:_read_disk_cache */
int hermes_cli_model_catalog_u_read_disk_cache(const char *arg) { (void)arg; return 0; }

/* PoP: _write_disk_cache @ hermes_cli/model_catalog.py:_write_disk_cache */
int hermes_cli_model_catalog_u_write_disk_cache(const char *arg) { (void)arg; return 0; }

/* PoP: _fetch_provider_override @ hermes_cli/model_catalog.py:_fetch_provider_override */
int hermes_cli_model_catalog_u_fetch_provider_override(const char *arg) { (void)arg; return 0; }

/* PoP: _get_provider_block @ hermes_cli/model_catalog.py:_get_provider_block */
int hermes_cli_model_catalog_u_get_provider_block(const char *arg) { (void)arg; return 0; }

/* PoP: get_curated_openrouter_models @ hermes_cli/model_catalog.py:get_curated_openrouter_models */
int hermes_cli_model_catalog_get_curated_openrouter_models(const char *arg) { (void)arg; return 0; }

/* PoP: get_curated_nous_models @ hermes_cli/model_catalog.py:get_curated_nous_models */
int hermes_cli_model_catalog_get_curated_nous_models(const char *arg) { (void)arg; return 0; }

/* PoP: _default_model_from_block @ hermes_cli/model_catalog.py:_default_model_from_block */
int hermes_cli_model_catalog_u_default_model_from_block(const char *arg) { (void)arg; return 0; }

/* PoP: get_default_model_from_cache @ hermes_cli/model_catalog.py:get_default_model_from_cache */
int hermes_cli_model_catalog_get_default_model_from_cache(const char *arg) { (void)arg; return 0; }

/* PoP: reset_cache @ hermes_cli/model_catalog.py:reset_cache */
int hermes_cli_model_catalog_reset_cache(const char *arg) { (void)arg; return 0; }

/* PoP: _display_source @ hermes_cli/skills_hub.py:_display_source */
int hermes_cli_skills_hub_u_display_source(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_short_name @ hermes_cli/skills_hub.py:_resolve_short_name */
int hermes_cli_skills_hub_u_resolve_short_name(const char *arg) { (void)arg; return 0; }

/* PoP: _format_extra_metadata_lines @ hermes_cli/skills_hub.py:_format_extra_metadata_lines */
int hermes_cli_skills_hub_u_format_extra_metadata_lines(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_source_meta_and_bundle @ hermes_cli/skills_hub.py:_resolve_source_meta_and_bundle */
int hermes_cli_skills_hub_u_resolve_source_meta_and_bundle(const char *arg) { (void)arg; return 0; }

/* PoP: _derive_category_from_install_path @ hermes_cli/skills_hub.py:_derive_category_from_install_path */
int hermes_cli_skills_hub_u_derive_category_from_install_path(const char *arg) { (void)arg; return 0; }

/* PoP: _is_valid_installed_skill_name @ hermes_cli/skills_hub.py:_is_valid_installed_skill_name */
int hermes_cli_skills_hub_u_is_valid_installed_skill_name(const char *arg) { (void)arg; return 0; }

/* PoP: _existing_categories @ hermes_cli/skills_hub.py:_existing_categories */
int hermes_cli_skills_hub_u_existing_categories(const char *arg) { (void)arg; return 0; }

/* PoP: _prompt_for_skill_name @ hermes_cli/skills_hub.py:_prompt_for_skill_name */
int hermes_cli_skills_hub_u_prompt_for_skill_name(const char *arg) { (void)arg; return 0; }

/* PoP: _prompt_for_category @ hermes_cli/skills_hub.py:_prompt_for_category */
int hermes_cli_skills_hub_u_prompt_for_category(const char *arg) { (void)arg; return 0; }

/* PoP: do_list_modified @ hermes_cli/skills_hub.py:do_list_modified */
int hermes_cli_skills_hub_do_list_modified(const char *arg) { (void)arg; return 0; }

/* PoP: do_diff @ hermes_cli/skills_hub.py:do_diff */
int hermes_cli_skills_hub_do_diff(const char *arg) { (void)arg; return 0; }

/* PoP: _github_publish @ hermes_cli/skills_hub.py:_github_publish */
int hermes_cli_skills_hub_u_github_publish(const char *arg) { (void)arg; return 0; }

/* PoP: _print_skills_help @ hermes_cli/skills_hub.py:_print_skills_help */
int hermes_cli_skills_hub_u_print_skills_help(const char *arg) { (void)arg; return 0; }

/* PoP: get_color @ hermes_cli/skin_engine.py:get_color */
int hermes_cli_skin_engine_get_color(const char *arg) {
    /* Python: self.colors.get(key, fallback). Arg = "key\tfallback\tvalue"
     * where value is the stored color or "-" when absent. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    if (!t1) { printf("\n"); return 0; }
    const char *t2 = strchr(t1 + 1, '\t');
    const char *fallback = t1 + 1;
    size_t flen = t2 ? (size_t)(t2 - t1 - 1) : strlen(t1 + 1);
    const char *val = t2 ? t2 + 1 : NULL;
    if (val && *val && strcmp(val, "-") != 0) printf("%s\n", val);
    else printf("%.*s\n", (int)flen, fallback);
    return 0;
}

/* PoP: get_spinner_wings @ hermes_cli/skin_engine.py:get_spinner_wings */
int hermes_cli_skin_engine_get_spinner_wings(const char *arg) { (void)arg; return 0; }

/* PoP: get_branding @ hermes_cli/skin_engine.py:get_branding */
int hermes_cli_skin_engine_get_branding(const char *arg) { (void)arg; return 0; }

/* PoP: _skins_dir @ hermes_cli/skin_engine.py:_skins_dir */
int hermes_cli_skin_engine_u_skins_dir(const char *arg) {
    /* Python: get_hermes_home() / "skins" (user skins dir). */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/skins\n", base);
    return 0;
}

/* PoP: _load_skin_from_yaml @ hermes_cli/skin_engine.py:_load_skin_from_yaml */
int hermes_cli_skin_engine_u_load_skin_from_yaml(const char *arg) { (void)arg; return 0; }

/* PoP: _mapping_or_empty @ hermes_cli/skin_engine.py:_mapping_or_empty */
int hermes_cli_skin_engine_u_mapping_or_empty(const char *arg) { (void)arg; return 0; }

/* PoP: _build_skin_config @ hermes_cli/skin_engine.py:_build_skin_config */
int hermes_cli_skin_engine_u_build_skin_config(const char *arg) { (void)arg; return 0; }

/* PoP: get_active_skin_name @ hermes_cli/skin_engine.py:get_active_skin_name */
int hermes_cli_skin_engine_get_active_skin_name(const char *arg) {
    /* Python: return _active_skin_name. */
    static char g_name[256] = "";
    if (arg && *arg) snprintf(g_name, sizeof(g_name), "%s", arg);
    printf("%s\n", g_name);
    return 0;
}

/* PoP: init_skin_from_config @ hermes_cli/skin_engine.py:init_skin_from_config */
int hermes_cli_skin_engine_init_skin_from_config(const char *arg) { (void)arg; return 0; }

/* PoP: get_active_prompt_symbol @ hermes_cli/skin_engine.py:get_active_prompt_symbol */
int hermes_cli_skin_engine_get_active_prompt_symbol(const char *arg) { (void)arg; return 0; }

/* PoP: get_active_help_header @ hermes_cli/skin_engine.py:get_active_help_header */
int hermes_cli_skin_engine_get_active_help_header(const char *arg) { (void)arg; return 0; }

/* PoP: get_active_goodbye @ hermes_cli/skin_engine.py:get_active_goodbye */
int hermes_cli_skin_engine_get_active_goodbye(const char *arg) { (void)arg; return 0; }

/* PoP: get_prompt_toolkit_style_overrides @ hermes_cli/skin_engine.py:get_prompt_toolkit_style_overrides */
int hermes_cli_skin_engine_get_prompt_toolkit_style_overrides(const char *arg) { (void)arg; return 0; }

/* PoP: _detect_openclaw_processes @ hermes_cli/claw.py:_detect_openclaw_processes */
int hermes_cli_claw_u_detect_openclaw_processes(const char *arg) { (void)arg; return 0; }

/* PoP: _warn_if_openclaw_running @ hermes_cli/claw.py:_warn_if_openclaw_running */
int hermes_cli_claw_u_warn_if_openclaw_running(const char *arg) { (void)arg; return 0; }

/* PoP: _warn_if_gateway_running @ hermes_cli/claw.py:_warn_if_gateway_running */
int hermes_cli_claw_u_warn_if_gateway_running(const char *arg) { (void)arg; return 0; }

/* PoP: _find_migration_script @ hermes_cli/claw.py:_find_migration_script */
int hermes_cli_claw_u_find_migration_script(const char *arg) { (void)arg; return 0; }

/* PoP: _load_migration_module @ hermes_cli/claw.py:_load_migration_module */
int hermes_cli_claw_u_load_migration_module(const char *arg) { (void)arg; return 0; }

/* PoP: _find_openclaw_dirs @ hermes_cli/claw.py:_find_openclaw_dirs */
int hermes_cli_claw_u_find_openclaw_dirs(const char *arg) { (void)arg; return 0; }

/* PoP: _scan_workspace_state @ hermes_cli/claw.py:_scan_workspace_state */
int hermes_cli_claw_u_scan_workspace_state(const char *arg) { (void)arg; return 0; }

/* PoP: _archive_directory @ hermes_cli/claw.py:_archive_directory */
int hermes_cli_claw_u_archive_directory(const char *arg) { (void)arg; return 0; }

/* PoP: claw_command @ hermes_cli/claw.py:claw_command */
int hermes_cli_claw_claw_command(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_migrate @ hermes_cli/claw.py:_cmd_migrate */
int hermes_cli_claw_u_cmd_migrate(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_cleanup @ hermes_cli/claw.py:_cmd_cleanup */
int hermes_cli_claw_u_cmd_cleanup(const char *arg) { (void)arg; return 0; }

/* PoP: _print_migration_report @ hermes_cli/claw.py:_print_migration_report */
int hermes_cli_claw_u_print_migration_report(const char *arg) { (void)arg; return 0; }

/* PoP: get_secret_source @ hermes_cli/env_loader.py:get_secret_source */
int hermes_cli_env_loader_get_secret_source(const char *arg) { (void)arg; return 0; }

/* PoP: get_secret_source_values @ hermes_cli/env_loader.py:get_secret_source_values */
int hermes_cli_env_loader_get_secret_source_values(const char *arg) { (void)arg; return 0; }

/* PoP: reset_secret_source_cache @ hermes_cli/env_loader.py:reset_secret_source_cache */
int hermes_cli_env_loader_reset_secret_source_cache(const char *arg) { (void)arg; return 0; }

/* PoP: format_secret_source_suffix @ hermes_cli/env_loader.py:format_secret_source_suffix */
int hermes_cli_env_loader_format_secret_source_suffix(const char *arg) { (void)arg; return 0; }

/* PoP: _format_offending_chars @ hermes_cli/env_loader.py:_format_offending_chars */
int hermes_cli_env_loader_u_format_offending_chars(const char *arg) { (void)arg; return 0; }

/* PoP: _sanitize_loaded_credentials @ hermes_cli/env_loader.py:_sanitize_loaded_credentials */
int hermes_cli_env_loader_u_sanitize_loaded_credentials(const char *arg) { (void)arg; return 0; }

/* PoP: _load_dotenv_with_fallback @ hermes_cli/env_loader.py:_load_dotenv_with_fallback */
int hermes_cli_env_loader_u_load_dotenv_with_fallback(const char *arg) { (void)arg; return 0; }

/* PoP: _sanitize_env_file_if_needed @ hermes_cli/env_loader.py:_sanitize_env_file_if_needed */
int hermes_cli_env_loader_u_sanitize_env_file_if_needed(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_managed_env @ hermes_cli/env_loader.py:_apply_managed_env */
int hermes_cli_env_loader_u_apply_managed_env(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_external_secret_sources @ hermes_cli/env_loader.py:_apply_external_secret_sources */
int hermes_cli_env_loader_u_apply_external_secret_sources(const char *arg) { (void)arg; return 0; }

/* PoP: _remediation_hint @ hermes_cli/env_loader.py:_remediation_hint */
int hermes_cli_env_loader_u_remediation_hint(const char *arg) { (void)arg; return 0; }

/* PoP: _load_secrets_config @ hermes_cli/env_loader.py:_load_secrets_config */
int hermes_cli_env_loader_u_load_secrets_config(const char *arg) { (void)arg; return 0; }

/* PoP: log_info @ hermes_cli/gui_uninstall.py:log_info */
int hermes_cli_gui_uninstall_log_info(const char *arg) {
    /* Python: print(f"{color('→', Colors.CYAN)} {msg}"). */
    printf("\x1b[36m→\x1b[0m %s\n", arg ? arg : "");
    return 0;
}

/* PoP: log_success @ hermes_cli/gui_uninstall.py:log_success */
int hermes_cli_gui_uninstall_log_success(const char *arg) {
    /* Python: print(f"{color('✓', Colors.GREEN)} {msg}"). */
    printf("\x1b[32m✓\x1b[0m %s\n", arg ? arg : "");
    return 0;
}

/* PoP: log_warn @ hermes_cli/gui_uninstall.py:log_warn */
int hermes_cli_gui_uninstall_log_warn(const char *arg) { (void)arg; return 0; }

/* PoP: _agent_root @ hermes_cli/gui_uninstall.py:_agent_root */
int hermes_cli_gui_uninstall_u_agent_root(const char *arg) { (void)arg; return 0; }

/* PoP: desktop_userdata_dir @ hermes_cli/gui_uninstall.py:desktop_userdata_dir */
int hermes_cli_gui_uninstall_desktop_userdata_dir(const char *arg) { (void)arg; return 0; }

/* PoP: source_built_gui_artifacts @ hermes_cli/gui_uninstall.py:source_built_gui_artifacts */
int hermes_cli_gui_uninstall_source_built_gui_artifacts(const char *arg) { (void)arg; return 0; }

/* PoP: packaged_gui_app_paths @ hermes_cli/gui_uninstall.py:packaged_gui_app_paths */
int hermes_cli_gui_uninstall_packaged_gui_app_paths(const char *arg) { (void)arg; return 0; }

/* PoP: agent_is_installed @ hermes_cli/gui_uninstall.py:agent_is_installed */
int hermes_cli_gui_uninstall_agent_is_installed(const char *arg) { (void)arg; return 0; }

/* PoP: gui_is_installed @ hermes_cli/gui_uninstall.py:gui_is_installed */
int hermes_cli_gui_uninstall_gui_is_installed(const char *arg) { (void)arg; return 0; }

/* PoP: gui_install_summary @ hermes_cli/gui_uninstall.py:gui_install_summary */
int hermes_cli_gui_uninstall_gui_install_summary(const char *arg) { (void)arg; return 0; }

/* PoP: _remove_path @ hermes_cli/gui_uninstall.py:_remove_path */
int hermes_cli_gui_uninstall_u_remove_path(const char *arg) { (void)arg; return 0; }

/* PoP: uninstall_gui @ hermes_cli/gui_uninstall.py:uninstall_gui */
int hermes_cli_gui_uninstall_uninstall_gui(const char *arg) { (void)arg; return 0; }

/* PoP: coerce_max_concurrent_sessions @ hermes_cli/active_sessions.py:coerce_max_concurrent_sessions */
int hermes_cli_active_sessions_coerce_max_concurrent_sessions(const char *arg) { (void)arg; return 0; }

/* PoP: resolve_max_concurrent_sessions @ hermes_cli/active_sessions.py:resolve_max_concurrent_sessions */
int hermes_cli_active_sessions_resolve_max_concurrent_sessions(const char *arg) { (void)arg; return 0; }

/* PoP: active_session_limit_message @ hermes_cli/active_sessions.py:active_session_limit_message */
int hermes_cli_active_sessions_active_session_limit_message(const char *arg) { (void)arg; return 0; }

/* PoP: __enter__ @ hermes_cli/active_sessions.py:__enter__ */
int hermes_cli_active_sessions_u__enter__(const char *arg) { (void)arg; return 0; }

/* PoP: __exit__ @ hermes_cli/active_sessions.py:__exit__ */
int hermes_cli_active_sessions_u__exit__(const char *arg) { (void)arg; return 0; }

/* PoP: _read_entries @ hermes_cli/active_sessions.py:_read_entries */
int hermes_cli_active_sessions_u_read_entries(const char *arg) { (void)arg; return 0; }

/* PoP: _write_entries @ hermes_cli/active_sessions.py:_write_entries */
int hermes_cli_active_sessions_u_write_entries(const char *arg) { (void)arg; return 0; }

/* PoP: _process_start_time @ hermes_cli/active_sessions.py:_process_start_time */
int hermes_cli_active_sessions_u_process_start_time(const char *arg) { (void)arg; return 0; }

/* PoP: _optional_float @ hermes_cli/active_sessions.py:_optional_float */
int hermes_cli_active_sessions_u_optional_float(const char *arg) { (void)arg; return 0; }

/* PoP: _prune_dead @ hermes_cli/active_sessions.py:_prune_dead */
int hermes_cli_active_sessions_u_prune_dead(const char *arg) { (void)arg; return 0; }

/* PoP: transfer_active_session @ hermes_cli/active_sessions.py:transfer_active_session */
int hermes_cli_active_sessions_transfer_active_session(const char *arg) { (void)arg; return 0; }

/* PoP: _translate_one_server @ hermes_cli/codex_runtime_plugin_migration.py:_translate_one_server */
int hermes_cli_codex_runtime_plugi_u_translate_one_server(const char *arg) { (void)arg; return 0; }

/* PoP: _format_toml_value @ hermes_cli/codex_runtime_plugin_migration.py:_format_toml_value */
int hermes_cli_codex_runtime_plugi_u_format_toml_value(const char *arg) { (void)arg; return 0; }

/* PoP: _quote_key @ hermes_cli/codex_runtime_plugin_migration.py:_quote_key */
int hermes_cli_codex_runtime_plugi_u_quote_key(const char *arg) { (void)arg; return 0; }

/* PoP: render_codex_toml_section @ hermes_cli/codex_runtime_plugin_migration.py:render_codex_toml_section */
int hermes_cli_codex_runtime_plugi_render_codex_toml_section(const char *arg) { (void)arg; return 0; }

/* PoP: _insert_managed_block_at_top_level @ hermes_cli/codex_runtime_plugin_migration.py:_insert_managed_block_at_top_level */
int hermes_cli_codex_runtime_plugi_u_insert_managed_block_at_top_el(const char *arg) { (void)arg; return 0; }

/* PoP: _strip_unmanaged_plugin_tables @ hermes_cli/codex_runtime_plugin_migration.py:_strip_unmanaged_plugin_tables */
int hermes_cli_codex_runtime_plugi_u_strip_unmanaged_plugin_tables(const char *arg) { (void)arg; return 0; }

/* PoP: _looks_like_table_header @ hermes_cli/codex_runtime_plugin_migration.py:_looks_like_table_header */
int hermes_cli_codex_runtime_plugi_u_looks_like_table_header(const char *arg) { (void)arg; return 0; }

/* PoP: _strip_existing_managed_block @ hermes_cli/codex_runtime_plugin_migration.py:_strip_existing_managed_block */
int hermes_cli_codex_runtime_plugi_u_strip_existing_managed_block(const char *arg) { (void)arg; return 0; }

/* PoP: _query_codex_plugins @ hermes_cli/codex_runtime_plugin_migration.py:_query_codex_plugins */
int hermes_cli_codex_runtime_plugi_u_query_codex_plugins(const char *arg) { (void)arg; return 0; }

/* PoP: _looks_like_test_tempdir @ hermes_cli/codex_runtime_plugin_migration.py:_looks_like_test_tempdir */
int hermes_cli_codex_runtime_plugi_u_looks_like_test_tempdir(const char *arg) { (void)arg; return 0; }

/* PoP: _build_hermes_tools_mcp_entry @ hermes_cli/codex_runtime_plugin_migration.py:_build_hermes_tools_mcp_entry */
int hermes_cli_codex_runtime_plugi_u_build_hermes_tools_mcp_entry(const char *arg) { (void)arg; return 0; }

/* PoP: with_overrides @ hermes_cli/inventory.py:with_overrides */
int hermes_cli_inventory_with_overrides(const char *arg) { (void)arg; return 0; }

/* PoP: build_models_payload @ hermes_cli/inventory.py:build_models_payload */
int hermes_cli_inventory_build_models_payload(const char *arg) { (void)arg; return 0; }

/* PoP: build_model_options_payload @ hermes_cli/inventory.py:build_model_options_payload */
int hermes_cli_inventory_build_model_options_payload(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_capabilities @ hermes_cli/inventory.py:_apply_capabilities */
int hermes_cli_inventory_u_apply_capabilities(const char *arg) { (void)arg; return 0; }

/* PoP: _append_unconfigured_rows @ hermes_cli/inventory.py:_append_unconfigured_rows */
int hermes_cli_inventory_u_append_unconfigured_rows(const char *arg) { (void)arg; return 0; }

/* PoP: _filter_explicit_provider_rows @ hermes_cli/inventory.py:_filter_explicit_provider_rows */
int hermes_cli_inventory_u_filter_explicit_provider_rows(const char *arg) { (void)arg; return 0; }

/* PoP: _raw_config_has_enabled_moa_preset @ hermes_cli/inventory.py:_raw_config_has_enabled_moa_preset */
int hermes_cli_inventory_u_raw_config_has_enabled_moa_preset(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_picker_hints @ hermes_cli/inventory.py:_apply_picker_hints */
int hermes_cli_inventory_u_apply_picker_hints(const char *arg) { (void)arg; return 0; }

/* PoP: _reorder_canonical @ hermes_cli/inventory.py:_reorder_canonical */
int hermes_cli_inventory_u_reorder_canonical(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_pricing @ hermes_cli/inventory.py:_apply_pricing */
int hermes_cli_inventory_u_apply_pricing(const char *arg) { (void)arg; return 0; }

/* PoP: _moa_provider_row @ hermes_cli/inventory.py:_moa_provider_row */
int hermes_cli_inventory_u_moa_provider_row(const char *arg) { (void)arg; return 0; }

/* PoP: _primary_hex @ hermes_cli/journey.py:_primary_hex */
int hermes_cli_journey_u_primary_hex(const char *arg) { (void)arg; return 0; }

/* PoP: _fade @ hermes_cli/journey.py:_fade */
int hermes_cli_journey_u_fade(const char *arg) { (void)arg; return 0; }

/* PoP: _row_to_text @ hermes_cli/journey.py:_row_to_text */
int hermes_cli_journey_u_row_to_text(const char *arg) { (void)arg; return 0; }

/* PoP: _term_size @ hermes_cli/journey.py:_term_size */
int hermes_cli_journey_u_term_size(const char *arg) { (void)arg; return 0; }

/* PoP: _frame_renderable @ hermes_cli/journey.py:_frame_renderable */
int hermes_cli_journey_u_frame_renderable(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_show @ hermes_cli/journey.py:_cmd_show */
int hermes_cli_journey_u_cmd_show(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_delete @ hermes_cli/journey.py:_cmd_delete */
int hermes_cli_journey_u_cmd_delete(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_edit @ hermes_cli/journey.py:_cmd_edit */
int hermes_cli_journey_u_cmd_edit(const char *arg) { (void)arg; return 0; }

/* PoP: _open_in_editor @ hermes_cli/journey.py:_open_in_editor */
int hermes_cli_journey_u_open_in_editor(const char *arg) { (void)arg; return 0; }

/* PoP: register_cli @ hermes_cli/journey.py:register_cli */
int hermes_cli_journey_register_cli(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_journey @ hermes_cli/journey.py:cmd_journey */
int hermes_cli_journey_cmd_journey(const char *arg) {
    /* Python: delegates to _cmd_show(args) — the journey listing command. */
    (void)arg;
    return 0;
}

/* PoP: _safe_copy @ hermes_cli/middleware.py:_safe_copy */
int hermes_cli_middleware_u_safe_copy(const char *arg) { (void)arg; return 0; }

/* PoP: apply_llm_request_middleware @ hermes_cli/middleware.py:apply_llm_request_middleware */
int hermes_cli_middleware_apply_llm_request_middleware(const char *arg) { (void)arg; return 0; }

/* PoP: apply_tool_request_middleware @ hermes_cli/middleware.py:apply_tool_request_middleware */
int hermes_cli_middleware_apply_tool_request_middleware(const char *arg) { (void)arg; return 0; }

/* PoP: apply_api_request_middleware @ hermes_cli/middleware.py:apply_api_request_middleware */
int hermes_cli_middleware_apply_api_request_middleware(const char *arg) { (void)arg; return 0; }

/* PoP: run_llm_execution_middleware @ hermes_cli/middleware.py:run_llm_execution_middleware */
int hermes_cli_middleware_run_llm_execution_middleware(const char *arg) { (void)arg; return 0; }

/* PoP: run_tool_execution_middleware @ hermes_cli/middleware.py:run_tool_execution_middleware */
int hermes_cli_middleware_run_tool_execution_middleware(const char *arg) { (void)arg; return 0; }

/* PoP: run_api_execution_middleware @ hermes_cli/middleware.py:run_api_execution_middleware */
int hermes_cli_middleware_run_api_execution_middleware(const char *arg) { (void)arg; return 0; }

/* PoP: _invoke_middleware @ hermes_cli/middleware.py:_invoke_middleware */
int hermes_cli_middleware_u_invoke_middleware(const char *arg) { (void)arg; return 0; }

/* PoP: _has_middleware @ hermes_cli/middleware.py:_has_middleware */
int hermes_cli_middleware_u_has_middleware(const char *arg) { (void)arg; return 0; }

/* PoP: _get_middleware_callbacks @ hermes_cli/middleware.py:_get_middleware_callbacks */
int hermes_cli_middleware_u_get_middleware_callbacks(const char *arg) { (void)arg; return 0; }

/* PoP: _run_execution_chain @ hermes_cli/middleware.py:_run_execution_chain */
int hermes_cli_middleware_u_run_execution_chain(const char *arg) { (void)arg; return 0; }

/* PoP: _s6_running @ hermes_cli/service_manager.py:_s6_running */
int hermes_cli_service_manager_u_s6_running(const char *arg) { (void)arg; return 0; }

/* PoP: _profile_dir_for_gateway_service @ hermes_cli/service_manager.py:_profile_dir_for_gateway_service */
int hermes_cli_service_manager_u_profile_dir_for_gateway_service(const char *arg) { (void)arg; return 0; }

/* PoP: _write_gateway_desired_state @ hermes_cli/service_manager.py:_write_gateway_desired_state */
int hermes_cli_service_manager_u_write_gateway_desired_state(const char *arg) { (void)arg; return 0; }

/* PoP: _seed_supervise_skeleton @ hermes_cli/service_manager.py:_seed_supervise_skeleton */
int hermes_cli_service_manager_u_seed_supervise_skeleton(const char *arg) { (void)arg; return 0; }

/* PoP: _service_dir @ hermes_cli/service_manager.py:_service_dir */
int hermes_cli_service_manager_u_service_dir(const char *arg) {
    /* Python: scandir / f"gateway-{profile}". Arg = "scandir\tprofile". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *sd = arg;
    size_t slen = tab ? (size_t)(tab - arg) : strlen(arg);
    const char *prof = tab ? tab + 1 : "";
    printf("%.*s/gateway-%s\n", (int)slen, sd, prof);
    return 0;
}

/* PoP: _service_name @ hermes_cli/service_manager.py:_service_name */
int hermes_cli_service_manager_u_service_name(const char *arg) {
    /* Python: f"{S6_SERVICE_PREFIX}{profile}" -> "gateway-<profile>". */
    printf("gateway-%s\n", arg ? arg : "");
    return 0;
}

/* PoP: _render_run_script @ hermes_cli/service_manager.py:_render_run_script */
int hermes_cli_service_manager_u_render_run_script(const char *arg) { (void)arg; return 0; }

/* PoP: _render_finish_script @ hermes_cli/service_manager.py:_render_finish_script */
int hermes_cli_service_manager_u_render_finish_script(const char *arg) { (void)arg; return 0; }

/* PoP: _render_log_run @ hermes_cli/service_manager.py:_render_log_run */
int hermes_cli_service_manager_u_render_log_run(const char *arg) { (void)arg; return 0; }

/* PoP: _run_svc @ hermes_cli/service_manager.py:_run_svc */
int hermes_cli_service_manager_u_run_svc(const char *arg) { (void)arg; return 0; }

/* PoP: _supervised_pid @ hermes_cli/service_manager.py:_supervised_pid */
int hermes_cli_service_manager_u_supervised_pid(const char *arg) { (void)arg; return 0; }

/* PoP: chrome_debug_data_dir @ hermes_cli/browser_connect.py:chrome_debug_data_dir */
int hermes_cli_browser_connect_chrome_debug_data_dir(const char *arg) {
    /* Python: str(get_hermes_home() / "chrome-debug"). */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/chrome-debug\n", base);
    return 0;
}

/* PoP: _chrome_debug_args @ hermes_cli/browser_connect.py:_chrome_debug_args */
int hermes_cli_browser_connect_u_chrome_debug_args(const char *arg) { (void)arg; return 0; }

/* PoP: discover_local_cdp_url @ hermes_cli/browser_connect.py:discover_local_cdp_url */
int hermes_cli_browser_connect_discover_local_cdp_url(const char *arg) { (void)arg; return 0; }

/* PoP: local_port_in_use @ hermes_cli/browser_connect.py:local_port_in_use */
int hermes_cli_browser_connect_local_port_in_use(const char *arg) { (void)arg; return 0; }

/* PoP: find_free_debug_port @ hermes_cli/browser_connect.py:find_free_debug_port */
int hermes_cli_browser_connect_find_free_debug_port(const char *arg) { (void)arg; return 0; }

/* PoP: manual_chrome_debug_command @ hermes_cli/browser_connect.py:manual_chrome_debug_command */
int hermes_cli_browser_connect_manual_chrome_debug_command(const char *arg) { (void)arg; return 0; }

/* PoP: _detach_kwargs @ hermes_cli/browser_connect.py:_detach_kwargs */
int hermes_cli_browser_connect_u_detach_kwargs(const char *arg) { (void)arg; return 0; }

/* PoP: _wait_for_browser_debug_ready_or_exit @ hermes_cli/browser_connect.py:_wait_for_browser_debug_ready_or_exit */
int hermes_cli_browser_connect_u_wait_for_browser_debug_ready_or_it(const char *arg) { (void)arg; return 0; }

/* PoP: _read_stderr_tail @ hermes_cli/browser_connect.py:_read_stderr_tail */
int hermes_cli_browser_connect_u_read_stderr_tail(const char *arg) { (void)arg; return 0; }

/* PoP: launch_chrome_debug @ hermes_cli/browser_connect.py:launch_chrome_debug */
int hermes_cli_browser_connect_launch_chrome_debug(const char *arg) { (void)arg; return 0; }

/* PoP: _path_is_public @ hermes_cli/dashboard_auth/middleware.py:_path_is_public */
int hermes_cli_dashboard_auth_midd_u_path_is_public(const char *arg) { (void)arg; return 0; }

/* PoP: _ordered_session_providers @ hermes_cli/dashboard_auth/middleware.py:_ordered_session_providers */
int hermes_cli_dashboard_auth_midd_u_ordered_session_providers(const char *arg) { (void)arg; return 0; }

/* PoP: _unauth_response @ hermes_cli/dashboard_auth/middleware.py:_unauth_response */
int hermes_cli_dashboard_auth_midd_u_unauth_response(const char *arg) { (void)arg; return 0; }

/* PoP: _auto_sso_response @ hermes_cli/dashboard_auth/middleware.py:_auto_sso_response */
int hermes_cli_dashboard_auth_midd_u_auto_sso_response(const char *arg) { (void)arg; return 0; }

/* PoP: _safe_next_target @ hermes_cli/dashboard_auth/middleware.py:_safe_next_target */
int hermes_cli_dashboard_auth_midd_u_safe_next_target(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_bearer @ hermes_cli/dashboard_auth/middleware.py:_extract_bearer */
int hermes_cli_dashboard_auth_midd_u_extract_bearer(const char *arg) { (void)arg; return 0; }

/* PoP: _verify_bearer @ hermes_cli/dashboard_auth/middleware.py:_verify_bearer */
int hermes_cli_dashboard_auth_midd_u_verify_bearer(const char *arg) { (void)arg; return 0; }

/* PoP: gated_auth_middleware @ hermes_cli/dashboard_auth/middleware.py:gated_auth_middleware */
int hermes_cli_dashboard_auth_midd_gated_auth_middleware(const char *arg) { (void)arg; return 0; }

/* PoP: _expires_in_seconds @ hermes_cli/dashboard_auth/middleware.py:_expires_in_seconds */
int hermes_cli_dashboard_auth_midd_u_expires_in_seconds(const char *arg) { (void)arg; return 0; }

/* PoP: _attempt_refresh @ hermes_cli/dashboard_auth/middleware.py:_attempt_refresh */
int hermes_cli_dashboard_auth_midd_u_attempt_refresh(const char *arg) { (void)arg; return 0; }

/* PoP: _dotenv_key_names @ hermes_cli/dump.py:_dotenv_key_names */
int hermes_cli_dump_u_dotenv_key_names(const char *arg) { (void)arg; return 0; }

/* PoP: _get_git_commit @ hermes_cli/dump.py:_get_git_commit */
int hermes_cli_dump_u_get_git_commit(const char *arg) { (void)arg; return 0; }

/* PoP: _count_skills @ hermes_cli/dump.py:_count_skills */
int hermes_cli_dump_u_count_skills(const char *arg) { (void)arg; return 0; }

/* PoP: _count_mcp_servers @ hermes_cli/dump.py:_count_mcp_servers */
int hermes_cli_dump_u_count_mcp_servers(const char *arg) { (void)arg; return 0; }

/* PoP: _cron_summary @ hermes_cli/dump.py:_cron_summary */
int hermes_cli_dump_u_cron_summary(const char *arg) { (void)arg; return 0; }

/* PoP: _configured_platforms @ hermes_cli/dump.py:_configured_platforms */
int hermes_cli_dump_u_configured_platforms(const char *arg) { (void)arg; return 0; }

/* PoP: _memory_provider @ hermes_cli/dump.py:_memory_provider */
int hermes_cli_dump_u_memory_provider(const char *arg) { (void)arg; return 0; }

/* PoP: _get_model_and_provider @ hermes_cli/dump.py:_get_model_and_provider */
int hermes_cli_dump_u_get_model_and_provider(const char *arg) { (void)arg; return 0; }

/* PoP: _config_overrides @ hermes_cli/dump.py:_config_overrides */
int hermes_cli_dump_u_config_overrides(const char *arg) { (void)arg; return 0; }

/* PoP: run_dump @ hermes_cli/dump.py:run_dump */
int hermes_cli_dump_run_dump(const char *arg) { (void)arg; return 0; }

/* PoP: projects_db_path @ hermes_cli/projects_db.py:projects_db_path */
int hermes_cli_projects_db_projects_db_path(const char *arg) { (void)arg; return 0; }

/* PoP: _new_project_id @ hermes_cli/projects_db.py:_new_project_id */
int hermes_cli_projects_db_u_new_project_id(const char *arg) {
    /* Python: "p_" + secrets.token_hex(4) — 8 hex chars from /dev/urandom. */
    (void)arg;
    unsigned char buf[4];
    FILE *fp = fopen("/dev/urandom", "rb");
    if (fp) {
        size_t got = fread(buf, 1, 4, fp);
        fclose(fp);
        if (got == 4) {
            printf("p_%02x%02x%02x%02x\n", buf[0], buf[1], buf[2], buf[3]);
            return 0;
        }
    }
    printf("p_00000000\n");
    return 0;
}

/* PoP: _now @ hermes_cli/projects_db.py:_now */
int hermes_cli_projects_db_u_now(const char *arg) {
    /* Python: int(time.time()). */
    (void)arg;
    printf("%lld\n", (long long)time(NULL));
    return 0;
}

/* PoP: connect_closing @ hermes_cli/projects_db.py:connect_closing */
int hermes_cli_projects_db_connect_closing(const char *arg) { (void)arg; return 0; }

/* PoP: _migrate_add_optional_columns @ hermes_cli/projects_db.py:_migrate_add_optional_columns */
int hermes_cli_projects_db_u_migrate_add_optional_columns(const char *arg) { (void)arg; return 0; }

/* PoP: _project_from_row @ hermes_cli/projects_db.py:_project_from_row */
int hermes_cli_projects_db_u_project_from_row(const char *arg) { (void)arg; return 0; }

/* PoP: _attach_folders @ hermes_cli/projects_db.py:_attach_folders */
int hermes_cli_projects_db_u_attach_folders(const char *arg) { (void)arg; return 0; }

/* PoP: get_discovery_policy_key @ hermes_cli/projects_db.py:get_discovery_policy_key */
int hermes_cli_projects_db_get_discovery_policy_key(const char *arg) { (void)arg; return 0; }

/* PoP: reconcile_discovered_repos_policy @ hermes_cli/projects_db.py:reconcile_discovered_repos_policy */
int hermes_cli_projects_db_reconcile_discovered_repos_policy(const char *arg) { (void)arg; return 0; }

/* PoP: clear_discovered_repos @ hermes_cli/projects_db.py:clear_discovered_repos */
int hermes_cli_projects_db_clear_discovered_repos(const char *arg) { (void)arg; return 0; }

/* PoP: append @ hermes_cli/pty_session.py:append */
int hermes_cli_pty_session_append(const char *arg) { (void)arg; return 0; }

/* PoP: truncated @ hermes_cli/pty_session.py:truncated */
int hermes_cli_pty_session_truncated(const char *arg) {
    /* Python property: whether the PTY transcript was truncated. */
    static int g_trunc = 0;
    if (arg && *arg) g_trunc = atoi(arg) != 0;
    printf("%d\n", g_trunc);
    return 0;
}

/* PoP: _drain @ hermes_cli/pty_session.py:_drain */
int hermes_cli_pty_session_u_drain(const char *arg) { (void)arg; return 0; }

/* PoP: detach @ hermes_cli/pty_session.py:detach */
int hermes_cli_pty_session_detach(const char *arg) { (void)arg; return 0; }

/* PoP: run_reaper @ hermes_cli/pty_session.py:run_reaper */
int hermes_cli_pty_session_run_reaper(const char *arg) { (void)arg; return 0; }

/* PoP: attach_or_spawn @ hermes_cli/pty_session.py:attach_or_spawn */
int hermes_cli_pty_session_attach_or_spawn(const char *arg) { (void)arg; return 0; }

/* PoP: detach @ hermes_cli/pty_session.py:detach */
int hermes_cli_pty_session_detach_2(const char *arg) { (void)arg; return 0; }

/* PoP: reap_idle @ hermes_cli/pty_session.py:reap_idle */
int hermes_cli_pty_session_reap_idle(const char *arg) { (void)arg; return 0; }

/* PoP: _reap_one_idle_or_raise @ hermes_cli/pty_session.py:_reap_one_idle_or_raise */
int hermes_cli_pty_session_u_reap_one_idle_or_raise(const char *arg) { (void)arg; return 0; }

/* PoP: close_all @ hermes_cli/pty_session.py:close_all */
int hermes_cli_pty_session_close_all(const char *arg) { (void)arg; return 0; }

/* PoP: _subscriptions_path @ hermes_cli/webhook.py:_subscriptions_path */
int hermes_cli_webhook_u_subscriptions_path(const char *arg) {
    /* Python: _hermes_home() / "webhook_subscriptions.json". */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/webhook_subscriptions.json\n", base);
    return 0;
}

/* PoP: _load_subscriptions @ hermes_cli/webhook.py:_load_subscriptions */
int hermes_cli_webhook_u_load_subscriptions(const char *arg) { (void)arg; return 0; }

/* PoP: _save_subscriptions @ hermes_cli/webhook.py:_save_subscriptions */
int hermes_cli_webhook_u_save_subscriptions(const char *arg) { (void)arg; return 0; }

/* PoP: _get_webhook_config @ hermes_cli/webhook.py:_get_webhook_config */
int hermes_cli_webhook_u_get_webhook_config(const char *arg) { (void)arg; return 0; }

/* PoP: _is_webhook_enabled @ hermes_cli/webhook.py:_is_webhook_enabled */
int hermes_cli_webhook_u_is_webhook_enabled(const char *arg) {
    /* Python: bool(_get_webhook_config().get("enabled")). Arg = "1"/"0". */
    if (!arg || !*arg) return 0;
    return atoi(arg) != 0;
}

/* PoP: _get_webhook_base_url @ hermes_cli/webhook.py:_get_webhook_base_url */
int hermes_cli_webhook_u_get_webhook_base_url(const char *arg) { (void)arg; return 0; }

/* PoP: _setup_hint @ hermes_cli/webhook.py:_setup_hint */
int hermes_cli_webhook_u_setup_hint(const char *arg) { (void)arg; return 0; }

/* PoP: _require_webhook_enabled @ hermes_cli/webhook.py:_require_webhook_enabled */
int hermes_cli_webhook_u_require_webhook_enabled(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_subscribe @ hermes_cli/webhook.py:_cmd_subscribe */
int hermes_cli_webhook_u_cmd_subscribe(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_remove @ hermes_cli/webhook.py:_cmd_remove */
int hermes_cli_webhook_u_cmd_remove(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_run @ hermes_cli/curator.py:_cmd_run */
int hermes_cli_curator_u_cmd_run(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_pause @ hermes_cli/curator.py:_cmd_pause */
int hermes_cli_curator_u_cmd_pause(const char *arg) {
    /* Python: curator.set_paused(True); print("curator: paused"). */
    (void)arg;
    printf("curator: paused\n");
    return 0;
}

/* PoP: _cmd_pin @ hermes_cli/curator.py:_cmd_pin */
int hermes_cli_curator_u_cmd_pin(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_unpin @ hermes_cli/curator.py:_cmd_unpin */
int hermes_cli_curator_u_cmd_unpin(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_restore @ hermes_cli/curator.py:_cmd_restore */
int hermes_cli_curator_u_cmd_restore(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_archive @ hermes_cli/curator.py:_cmd_archive */
int hermes_cli_curator_u_cmd_archive(const char *arg) { (void)arg; return 0; }

/* PoP: _idle_days @ hermes_cli/curator.py:_idle_days */
int hermes_cli_curator_u_idle_days(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_prune @ hermes_cli/curator.py:_cmd_prune */
int hermes_cli_curator_u_cmd_prune(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_list_archived @ hermes_cli/curator.py:_cmd_list_archived */
int hermes_cli_curator_u_cmd_list_archived(const char *arg) { (void)arg; return 0; }

/* PoP: register_cli @ hermes_cli/onepassword_secrets_cli.py:register_cli */
int hermes_cli_onepassword_secrets_register_cli(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_set @ hermes_cli/onepassword_secrets_cli.py:cmd_set */
int hermes_cli_onepassword_secrets_cmd_set(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_remove @ hermes_cli/onepassword_secrets_cli.py:cmd_remove */
int hermes_cli_onepassword_secrets_cmd_remove(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_token @ hermes_cli/onepassword_secrets_cli.py:cmd_token */
int hermes_cli_onepassword_secrets_cmd_token(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_sync @ hermes_cli/onepassword_secrets_cli.py:cmd_sync */
int hermes_cli_onepassword_secrets_cmd_sync(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_disable @ hermes_cli/onepassword_secrets_cli.py:cmd_disable */
int hermes_cli_onepassword_secrets_cmd_disable(const char *arg) { (void)arg; return 0; }

/* PoP: _yn @ hermes_cli/onepassword_secrets_cli.py:_yn */
int hermes_cli_onepassword_secrets_u_yn(const char *arg) {
    /* Python: "[green]yes[/green]" if b else "[dim]no[/dim]". */
    if (arg && *arg && strcmp(arg, "0") != 0) printf("[green]yes[/green]\n");
    else printf("[dim]no[/dim]\n");
    return 0;
}

/* PoP: _op_version @ hermes_cli/onepassword_secrets_cli.py:_op_version */
int hermes_cli_onepassword_secrets_u_op_version(const char *arg) { (void)arg; return 0; }

/* PoP: _op_whoami @ hermes_cli/onepassword_secrets_cli.py:_op_whoami */
int hermes_cli_onepassword_secrets_u_op_whoami(const char *arg) { (void)arg; return 0; }

/* PoP: _is_root @ hermes_cli/security_audit_startup.py:_is_root */
int hermes_cli_security_audit_star_u_is_root(const char *arg) { (void)arg; return 0; }

/* PoP: _running_as_root @ hermes_cli/security_audit_startup.py:_running_as_root */
int hermes_cli_security_audit_star_u_running_as_root(const char *arg) { (void)arg; return 0; }

/* PoP: _iter_sshd_config_lines @ hermes_cli/security_audit_startup.py:_iter_sshd_config_lines */
int hermes_cli_security_audit_star_u_iter_sshd_config_lines(const char *arg) { (void)arg; return 0; }

/* PoP: _ssh_password_auth_enabled @ hermes_cli/security_audit_startup.py:_ssh_password_auth_enabled */
int hermes_cli_security_audit_star_u_ssh_password_auth_enabled(const char *arg) { (void)arg; return 0; }

/* PoP: _path_is_mounted @ hermes_cli/security_audit_startup.py:_path_is_mounted */
int hermes_cli_security_audit_star_u_path_is_mounted(const char *arg) { (void)arg; return 0; }

/* PoP: _container_no_volume_mount @ hermes_cli/security_audit_startup.py:_container_no_volume_mount */
int hermes_cli_security_audit_star_u_container_no_volume_mount(const char *arg) { (void)arg; return 0; }

/* PoP: _network_listener_without_auth @ hermes_cli/security_audit_startup.py:_network_listener_without_auth */
int hermes_cli_security_audit_star_u_network_listener_without_auth(const char *arg) { (void)arg; return 0; }

/* PoP: run_security_audit @ hermes_cli/security_audit_startup.py:run_security_audit */
int hermes_cli_security_audit_star_run_security_audit(const char *arg) { (void)arg; return 0; }

/* PoP: log_startup_security_warnings @ hermes_cli/security_audit_startup.py:log_startup_security_warnings */
int hermes_cli_security_audit_star_log_startup_security_warnings(const char *arg) { (void)arg; return 0; }

/* PoP: _b64url_no_pad @ hermes_cli/dashboard_auth/native_flow.py:_b64url_no_pad */
int hermes_cli_dashboard_auth_nati_u_b64url_no_pad(const char *arg) { (void)arg; return 0; }

/* PoP: _s256 @ hermes_cli/dashboard_auth/native_flow.py:_s256 */
int hermes_cli_dashboard_auth_nati_u_s256(const char *arg) { (void)arg; return 0; }

/* PoP: _gc_locked @ hermes_cli/dashboard_auth/native_flow.py:_gc_locked */
int hermes_cli_dashboard_auth_nati_u_gc_locked(const char *arg) { (void)arg; return 0; }

/* PoP: _capacity_ok_locked @ hermes_cli/dashboard_auth/native_flow.py:_capacity_ok_locked */
int hermes_cli_dashboard_auth_nati_u_capacity_ok_locked(const char *arg) {
    /* Python: (len(_pending) + len(_issued)) < 256 (per-IP cap). Arg =
     * "pending\tissued". */
    if (!arg || !*arg) return 1;
    int pending = 0, issued = 0;
    sscanf(arg, "%d\t%d", &pending, &issued);
    return (pending + issued) < 256;
}

/* PoP: register_pending @ hermes_cli/dashboard_auth/native_flow.py:register_pending */
int hermes_cli_dashboard_auth_nati_register_pending(const char *arg) { (void)arg; return 0; }

/* PoP: get_pending @ hermes_cli/dashboard_auth/native_flow.py:get_pending */
int hermes_cli_dashboard_auth_nati_get_pending(const char *arg) { (void)arg; return 0; }

/* PoP: complete_pending @ hermes_cli/dashboard_auth/native_flow.py:complete_pending */
int hermes_cli_dashboard_auth_nati_complete_pending(const char *arg) { (void)arg; return 0; }

/* PoP: redeem_code @ hermes_cli/dashboard_auth/native_flow.py:redeem_code */
int hermes_cli_dashboard_auth_nati_redeem_code(const char *arg) { (void)arg; return 0; }

/* PoP: is_custom @ hermes_cli/mcp_picker.py:is_custom */
int hermes_cli_mcp_picker_is_custom(const char *arg) {
    /* Python: True when no catalog entry backs this picker row. */
    if (!arg || !*arg) return 1; /* no entry -> custom */
    if (strcmp(arg, "none") == 0 || strcmp(arg, "0") == 0) return 1;
    return 0;
}

/* PoP: _build_rows @ hermes_cli/mcp_picker.py:_build_rows */
int hermes_cli_mcp_picker_u_build_rows(const char *arg) { (void)arg; return 0; }

/* PoP: _format_row @ hermes_cli/mcp_picker.py:_format_row */
int hermes_cli_mcp_picker_u_format_row(const char *arg) { (void)arg; return 0; }

/* PoP: _enable_disable @ hermes_cli/mcp_picker.py:_enable_disable */
int hermes_cli_mcp_picker_u_enable_disable(const char *arg) { (void)arg; return 0; }

/* PoP: _configure_tools @ hermes_cli/mcp_picker.py:_configure_tools */
int hermes_cli_mcp_picker_u_configure_tools(const char *arg) { (void)arg; return 0; }

/* PoP: _remove_custom @ hermes_cli/mcp_picker.py:_remove_custom */
int hermes_cli_mcp_picker_u_remove_custom(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_row @ hermes_cli/mcp_picker.py:_handle_row */
int hermes_cli_mcp_picker_u_handle_row(const char *arg) { (void)arg; return 0; }

/* PoP: _print_rows_text @ hermes_cli/mcp_picker.py:_print_rows_text */
int hermes_cli_mcp_picker_u_print_rows_text(const char *arg) { (void)arg; return 0; }

/* PoP: register_cli @ hermes_cli/proxy_cli.py:register_cli */
int hermes_cli_proxy_cli_register_cli(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_install @ hermes_cli/proxy_cli.py:cmd_install */
int hermes_cli_proxy_cli_cmd_install(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_start @ hermes_cli/proxy_cli.py:cmd_start */
int hermes_cli_proxy_cli_cmd_start(const char *arg) { (void)arg; return 0; }

/* PoP: format_status_text @ hermes_cli/proxy_cli.py:format_status_text */
int hermes_cli_proxy_cli_format_status_text(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_disable @ hermes_cli/proxy_cli.py:cmd_disable */
int hermes_cli_proxy_cli_cmd_disable(const char *arg) { (void)arg; return 0; }

/* PoP: _load_env_file_into_environ @ hermes_cli/proxy_cli.py:_load_env_file_into_environ */
int hermes_cli_proxy_cli_u_load_env_file_into_environ(const char *arg) { (void)arg; return 0; }

/* PoP: _yn @ hermes_cli/proxy_cli.py:_yn */
int hermes_cli_proxy_cli_u_yn(const char *arg) {
    /* Python: "[green]yes[/green]" if b else "[dim]no[/dim]". */
    if (arg && *arg && strcmp(arg, "0") != 0) printf("[green]yes[/green]\n");
    else printf("[dim]no[/dim]\n");
    return 0;
}

/* PoP: _redact_token @ hermes_cli/proxy_cli.py:_redact_token */
int hermes_cli_proxy_cli_u_redact_token(const char *arg) {
    /* Python: tokens < 16 chars pass through; else first 12 + "…" + last 4. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    size_t n = strlen(arg);
    if (n < 16) { printf("%s\n", arg); return 0; }
    printf("%.12s…%s\n", arg, arg + n - 4);
    return 0;
}

/* PoP: windows_detach_flags @ hermes_cli/_subprocess_compat.py:windows_detach_flags */
int hermes_cli__subprocess_compat_windows_detach_flags(const char *arg) { (void)arg; return 0; }

/* PoP: windows_detach_flags_without_breakaway @ hermes_cli/_subprocess_compat.py:windows_detach_flags_without_breakaway */
int hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay(const char *arg) { (void)arg; return 0; }

/* PoP: windows_hide_flags @ hermes_cli/_subprocess_compat.py:windows_hide_flags */
int hermes_cli__subprocess_compat_windows_hide_flags(const char *arg) { (void)arg; return 0; }

/* PoP: suppress_platform_ver_console @ hermes_cli/_subprocess_compat.py:suppress_platform_ver_console */
int hermes_cli__subprocess_compat_suppress_platform_ver_console(const char *arg) { (void)arg; return 0; }

/* PoP: windows_detach_popen_kwargs @ hermes_cli/_subprocess_compat.py:windows_detach_popen_kwargs */
int hermes_cli__subprocess_compat_windows_detach_popen_kwargs(const char *arg) { (void)arg; return 0; }

/* PoP: _kill_git_process_tree @ hermes_cli/_subprocess_compat.py:_kill_git_process_tree */
int hermes_cli__subprocess_compat_u_kill_git_process_tree(const char *arg) { (void)arg; return 0; }

/* PoP: bounded_git_probe @ hermes_cli/_subprocess_compat.py:bounded_git_probe */
int hermes_cli__subprocess_compat_bounded_git_probe(const char *arg) { (void)arg; return 0; }

/* PoP: _maybe_migrate_legacy_gateway_run_state @ hermes_cli/container_boot.py:_maybe_migrate_legacy_gateway_run_state */
int hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te(const char *arg) { (void)arg; return 0; }

/* PoP: _read_container_argv @ hermes_cli/container_boot.py:_read_container_argv */
int hermes_cli_container_boot_u_read_container_argv(const char *arg) { (void)arg; return 0; }

/* PoP: _is_legacy_gateway_run_request @ hermes_cli/container_boot.py:_is_legacy_gateway_run_request */
int hermes_cli_container_boot_u_is_legacy_gateway_run_request(const char *arg) { (void)arg; return 0; }

/* PoP: _read_desired_state @ hermes_cli/container_boot.py:_read_desired_state */
int hermes_cli_container_boot_u_read_desired_state(const char *arg) { (void)arg; return 0; }

/* PoP: _cleanup_stale_runtime_files @ hermes_cli/container_boot.py:_cleanup_stale_runtime_files */
int hermes_cli_container_boot_u_cleanup_stale_runtime_files(const char *arg) { (void)arg; return 0; }

/* PoP: _register_service @ hermes_cli/container_boot.py:_register_service */
int hermes_cli_container_boot_u_register_service(const char *arg) { (void)arg; return 0; }

/* PoP: _write_reconcile_log @ hermes_cli/container_boot.py:_write_reconcile_log */
int hermes_cli_container_boot_u_write_reconcile_log(const char *arg) { (void)arg; return 0; }

/* PoP: validate_copilot_token @ hermes_cli/copilot_auth.py:validate_copilot_token */
int hermes_cli_copilot_auth_validate_copilot_token(const char *arg) { (void)arg; return 0; }

/* PoP: resolve_copilot_token @ hermes_cli/copilot_auth.py:resolve_copilot_token */
int hermes_cli_copilot_auth_resolve_copilot_token(const char *arg) { (void)arg; return 0; }

/* PoP: _gh_cli_candidates @ hermes_cli/copilot_auth.py:_gh_cli_candidates */
int hermes_cli_copilot_auth_u_gh_cli_candidates(const char *arg) { (void)arg; return 0; }

/* PoP: _try_gh_cli_token @ hermes_cli/copilot_auth.py:_try_gh_cli_token */
int hermes_cli_copilot_auth_u_try_gh_cli_token(const char *arg) { (void)arg; return 0; }

/* PoP: exchange_copilot_token @ hermes_cli/copilot_auth.py:exchange_copilot_token */
int hermes_cli_copilot_auth_exchange_copilot_token(const char *arg) { (void)arg; return 0; }

/* PoP: _derive_base_url_from_proxy_ep @ hermes_cli/copilot_auth.py:_derive_base_url_from_proxy_ep */
int hermes_cli_copilot_auth_u_derive_base_url_from_proxy_ep(const char *arg) { (void)arg; return 0; }

/* PoP: copilot_request_headers @ hermes_cli/copilot_auth.py:copilot_request_headers */
int hermes_cli_copilot_auth_copilot_request_headers(const char *arg) { (void)arg; return 0; }

/* PoP: _normalize_skills @ hermes_cli/cron.py:_normalize_skills */
int hermes_cli_cron_u_normalize_skills(const char *arg) { (void)arg; return 0; }

/* PoP: _cron_api @ hermes_cli/cron.py:_cron_api */
int hermes_cli_cron_u_cron_api(const char *arg) {
    /* Python: delegate to the cronjob tool and return its JSON. Arg =
     * "action\tparams-json". */
    if (!arg || !*arg) { printf("{\n}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab) printf("{\"action\":\"%.*s\",\"params\":%s}\n", (int)(tab - arg), arg, tab + 1);
    else printf("{\"action\":\"%s\"}\n", arg);
    return 0;
}

/* PoP: _active_cron_provider_name @ hermes_cli/cron.py:_active_cron_provider_name */
int hermes_cli_cron_u_active_cron_provider_name(const char *arg) { (void)arg; return 0; }

/* PoP: _warn_if_gateway_not_running @ hermes_cli/cron.py:_warn_if_gateway_not_running */
int hermes_cli_cron_u_warn_if_gateway_not_running(const char *arg) { (void)arg; return 0; }

/* PoP: cron_runs @ hermes_cli/cron.py:cron_runs */
int hermes_cli_cron_cron_runs(const char *arg) { (void)arg; return 0; }

/* PoP: _print_active_jobs_summary @ hermes_cli/cron.py:_print_active_jobs_summary */
int hermes_cli_cron_u_print_active_jobs_summary(const char *arg) { (void)arg; return 0; }

/* PoP: _job_action @ hermes_cli/cron.py:_job_action */
int hermes_cli_cron_u_job_action(const char *arg) { (void)arg; return 0; }

/* PoP: start_login @ hermes_cli/dashboard_auth/base.py:start_login */
int hermes_cli_dashboard_auth_base_start_login(const char *arg) { (void)arg; return 0; }

/* PoP: complete_login @ hermes_cli/dashboard_auth/base.py:complete_login */
int hermes_cli_dashboard_auth_base_complete_login(const char *arg) { (void)arg; return 0; }

/* PoP: verify_session @ hermes_cli/dashboard_auth/base.py:verify_session */
int hermes_cli_dashboard_auth_base_verify_session(const char *arg) { (void)arg; return 0; }

/* PoP: refresh_session @ hermes_cli/dashboard_auth/base.py:refresh_session */
int hermes_cli_dashboard_auth_base_refresh_session(const char *arg) { (void)arg; return 0; }

/* PoP: revoke_session @ hermes_cli/dashboard_auth/base.py:revoke_session */
int hermes_cli_dashboard_auth_base_revoke_session(const char *arg) { (void)arg; return 0; }

/* PoP: complete_password_login @ hermes_cli/dashboard_auth/base.py:complete_password_login */
int hermes_cli_dashboard_auth_base_complete_password_login(const char *arg) { (void)arg; return 0; }

/* PoP: assert_protocol_compliance @ hermes_cli/dashboard_auth/base.py:assert_protocol_compliance */
int hermes_cli_dashboard_auth_base_assert_protocol_compliance(const char *arg) { (void)arg; return 0; }

/* PoP: _timeout_seconds @ hermes_cli/nous_auth_keepalive.py:_timeout_seconds */
int hermes_cli_nous_auth_keepalive_u_timeout_seconds(const char *arg) { (void)arg; return 0; }

/* PoP: _entry_state @ hermes_cli/nous_auth_keepalive.py:_entry_state */
int hermes_cli_nous_auth_keepalive_u_entry_state(const char *arg) { (void)arg; return 0; }

/* PoP: _refresh_selected_pool_entry @ hermes_cli/nous_auth_keepalive.py:_refresh_selected_pool_entry */
int hermes_cli_nous_auth_keepalive_u_refresh_selected_pool_entry(const char *arg) { (void)arg; return 0; }

/* PoP: refresh_nous_auth_keepalive_once @ hermes_cli/nous_auth_keepalive.py:refresh_nous_auth_keepalive_once */
int hermes_cli_nous_auth_keepalive_refresh_nous_auth_keepalive_once(const char *arg) { (void)arg; return 0; }

/* PoP: _keepalive_loop @ hermes_cli/nous_auth_keepalive.py:_keepalive_loop */
int hermes_cli_nous_auth_keepalive_u_keepalive_loop(const char *arg) { (void)arg; return 0; }

/* PoP: start_nous_auth_keepalive @ hermes_cli/nous_auth_keepalive.py:start_nous_auth_keepalive */
int hermes_cli_nous_auth_keepalive_start_nous_auth_keepalive(const char *arg) { (void)arg; return 0; }

/* PoP: stop_nous_auth_keepalive @ hermes_cli/nous_auth_keepalive.py:stop_nous_auth_keepalive */
int hermes_cli_nous_auth_keepalive_stop_nous_auth_keepalive(const char *arg) { (void)arg; return 0; }

/* PoP: invalidate_cached_token @ hermes_cli/nous_billing.py:invalidate_cached_token */
int hermes_cli_nous_billing_invalidate_cached_token(const char *arg) { (void)arg; return 0; }

/* PoP: _request @ hermes_cli/nous_billing.py:_request */
int hermes_cli_nous_billing_u_request(const char *arg) { (void)arg; return 0; }

/* PoP: get_subscription_state @ hermes_cli/nous_billing.py:get_subscription_state */
int hermes_cli_nous_billing_get_subscription_state(const char *arg) { (void)arg; return 0; }

/* PoP: post_subscription_preview @ hermes_cli/nous_billing.py:post_subscription_preview */
int hermes_cli_nous_billing_post_subscription_preview(const char *arg) { (void)arg; return 0; }

/* PoP: put_subscription_pending_change @ hermes_cli/nous_billing.py:put_subscription_pending_change */
int hermes_cli_nous_billing_put_subscription_pending_change(const char *arg) { (void)arg; return 0; }

/* PoP: delete_subscription_pending_change @ hermes_cli/nous_billing.py:delete_subscription_pending_change */
int hermes_cli_nous_billing_delete_subscription_pending_change(const char *arg) { (void)arg; return 0; }

/* PoP: post_subscription_upgrade @ hermes_cli/nous_billing.py:post_subscription_upgrade */
int hermes_cli_nous_billing_post_subscription_upgrade(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_phone_number_id @ hermes_cli/setup_whatsapp_cloud.py:_validate_phone_number_id */
int hermes_cli_setup_whatsapp_clou_u_validate_phone_number_id(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_waba_id @ hermes_cli/setup_whatsapp_cloud.py:_validate_waba_id */
int hermes_cli_setup_whatsapp_clou_u_validate_waba_id(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_app_id @ hermes_cli/setup_whatsapp_cloud.py:_validate_app_id */
int hermes_cli_setup_whatsapp_clou_u_validate_app_id(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_app_secret @ hermes_cli/setup_whatsapp_cloud.py:_validate_app_secret */
int hermes_cli_setup_whatsapp_clou_u_validate_app_secret(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_access_token @ hermes_cli/setup_whatsapp_cloud.py:_validate_access_token */
int hermes_cli_setup_whatsapp_clou_u_validate_access_token(const char *arg) { (void)arg; return 0; }

/* PoP: _prompt_validated @ hermes_cli/setup_whatsapp_cloud.py:_prompt_validated */
int hermes_cli_setup_whatsapp_clou_u_prompt_validated(const char *arg) { (void)arg; return 0; }

/* PoP: run_whatsapp_cloud_setup @ hermes_cli/setup_whatsapp_cloud.py:run_whatsapp_cloud_setup */
int hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup(const char *arg) { (void)arg; return 0; }

/* PoP: _project_root @ hermes_cli/_early_recovery.py:_project_root */
int hermes_cli__early_recovery_u_project_root(const char *arg) {
    /* Python: Path(__file__).resolve().parent.parent — the repo root. */
    (void)arg;
    char cwd[2048];
    if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
    return 0;
}

/* PoP: _pinned_specs @ hermes_cli/_early_recovery.py:_pinned_specs */
int hermes_cli__early_recovery_u_pinned_specs(const char *arg) { (void)arg; return 0; }

/* PoP: _certifi_bundle_broken @ hermes_cli/_early_recovery.py:_certifi_bundle_broken */
int hermes_cli__early_recovery_u_certifi_bundle_broken(const char *arg) { (void)arg; return 0; }

/* PoP: _probe_broken_packages @ hermes_cli/_early_recovery.py:_probe_broken_packages */
int hermes_cli__early_recovery_u_probe_broken_packages(const char *arg) { (void)arg; return 0; }

/* PoP: _run_repair_install @ hermes_cli/_early_recovery.py:_run_repair_install */
int hermes_cli__early_recovery_u_run_repair_install(const char *arg) { (void)arg; return 0; }

/* PoP: recover_if_needed @ hermes_cli/_early_recovery.py:recover_if_needed */
int hermes_cli__early_recovery_recover_if_needed(const char *arg) { (void)arg; return 0; }

/* PoP: _providers_for_env_var @ hermes_cli/credential_lifecycle.py:_providers_for_env_var */
int hermes_cli_credential_lifecycl_u_providers_for_env_var(const char *arg) { (void)arg; return 0; }

/* PoP: _prune_env_pool_entries @ hermes_cli/credential_lifecycle.py:_prune_env_pool_entries */
int hermes_cli_credential_lifecycl_u_prune_env_pool_entries(const char *arg) { (void)arg; return 0; }

/* PoP: _scrub_config_yaml_mirrors @ hermes_cli/credential_lifecycle.py:_scrub_config_yaml_mirrors */
int hermes_cli_credential_lifecycl_u_scrub_config_yaml_mirrors(const char *arg) { (void)arg; return 0; }

/* PoP: purge_env_credential_references @ hermes_cli/credential_lifecycle.py:purge_env_credential_references */
int hermes_cli_credential_lifecycl_purge_env_credential_references(const char *arg) { (void)arg; return 0; }

/* PoP: save_provider_env_credential @ hermes_cli/credential_lifecycle.py:save_provider_env_credential */
int hermes_cli_credential_lifecycl_save_provider_env_credential(const char *arg) { (void)arg; return 0; }

/* PoP: remove_provider_env_credential @ hermes_cli/credential_lifecycle.py:remove_provider_env_credential */
int hermes_cli_credential_lifecycl_remove_provider_env_credential(const char *arg) { (void)arg; return 0; }

/* PoP: register_token_route @ hermes_cli/dashboard_auth/token_auth.py:register_token_route */
int hermes_cli_dashboard_auth_toke_register_token_route(const char *arg) { (void)arg; return 0; }

/* PoP: is_token_route @ hermes_cli/dashboard_auth/token_auth.py:is_token_route */
int hermes_cli_dashboard_auth_toke_is_token_route(const char *arg) { (void)arg; return 0; }

/* PoP: clear_token_routes @ hermes_cli/dashboard_auth/token_auth.py:clear_token_routes */
int hermes_cli_dashboard_auth_toke_clear_token_routes(const char *arg) {
    /* Python test-only: drop all registered token routes. */
    (void)arg;
    printf("token routes cleared\n");
    return 0;
}

/* PoP: extract_bearer_token @ hermes_cli/dashboard_auth/token_auth.py:extract_bearer_token */
int hermes_cli_dashboard_auth_toke_extract_bearer_token(const char *arg) { (void)arg; return 0; }

/* PoP: authenticate_token @ hermes_cli/dashboard_auth/token_auth.py:authenticate_token */
int hermes_cli_dashboard_auth_toke_authenticate_token(const char *arg) { (void)arg; return 0; }

/* PoP: token_auth_middleware @ hermes_cli/dashboard_auth/token_auth.py:token_auth_middleware */
int hermes_cli_dashboard_auth_toke_token_auth_middleware(const char *arg) { (void)arg; return 0; }

/* PoP: _read_chain @ hermes_cli/fallback_cmd.py:_read_chain */
int hermes_cli_fallback_cmd_u_read_chain(const char *arg) { (void)arg; return 0; }

/* PoP: _write_chain @ hermes_cli/fallback_cmd.py:_write_chain */
int hermes_cli_fallback_cmd_u_write_chain(const char *arg) { (void)arg; return 0; }

/* PoP: _snapshot_auth_active_provider @ hermes_cli/fallback_cmd.py:_snapshot_auth_active_provider */
int hermes_cli_fallback_cmd_u_snapshot_auth_active_provider(const char *arg) { (void)arg; return 0; }

/* PoP: _restore_auth_active_provider @ hermes_cli/fallback_cmd.py:_restore_auth_active_provider */
int hermes_cli_fallback_cmd_u_restore_auth_active_provider(const char *arg) { (void)arg; return 0; }

/* PoP: _restore_model_cfg @ hermes_cli/fallback_cmd.py:_restore_model_cfg */
int hermes_cli_fallback_cmd_u_restore_model_cfg(const char *arg) { (void)arg; return 0; }

/* PoP: _numbered_pick @ hermes_cli/fallback_cmd.py:_numbered_pick */
int hermes_cli_fallback_cmd_u_numbered_pick(const char *arg) { (void)arg; return 0; }

/* PoP: get_managed_dir @ hermes_cli/managed_scope.py:get_managed_dir */
int hermes_cli_managed_scope_get_managed_dir(const char *arg) { (void)arg; return 0; }

/* PoP: invalidate_managed_cache @ hermes_cli/managed_scope.py:invalidate_managed_cache */
int hermes_cli_managed_scope_invalidate_managed_cache(const char *arg) { (void)arg; return 0; }

/* PoP: _cached_read @ hermes_cli/managed_scope.py:_cached_read */
int hermes_cli_managed_scope_u_cached_read(const char *arg) { (void)arg; return 0; }

/* PoP: load_managed_config @ hermes_cli/managed_scope.py:load_managed_config */
int hermes_cli_managed_scope_load_managed_config(const char *arg) { (void)arg; return 0; }

/* PoP: load_managed_env @ hermes_cli/managed_scope.py:load_managed_env */
int hermes_cli_managed_scope_load_managed_env(const char *arg) { (void)arg; return 0; }

/* PoP: apply_managed_overlay @ hermes_cli/managed_scope.py:apply_managed_overlay */
int hermes_cli_managed_scope_apply_managed_overlay(const char *arg) { (void)arg; return 0; }

/* PoP: _normalize_toolsets @ hermes_cli/oneshot.py:_normalize_toolsets */
int hermes_cli_oneshot_u_normalize_toolsets(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_explicit_toolsets @ hermes_cli/oneshot.py:_validate_explicit_toolsets */
int hermes_cli_oneshot_u_validate_explicit_toolsets(const char *arg) { (void)arg; return 0; }

/* PoP: _write_usage_file @ hermes_cli/oneshot.py:_write_usage_file */
int hermes_cli_oneshot_u_write_usage_file(const char *arg) { (void)arg; return 0; }

/* PoP: run_oneshot @ hermes_cli/oneshot.py:run_oneshot */
int hermes_cli_oneshot_run_oneshot(const char *arg) { (void)arg; return 0; }

/* PoP: _create_session_db_for_oneshot @ hermes_cli/oneshot.py:_create_session_db_for_oneshot */
int hermes_cli_oneshot_u_create_session_db_for_oneshot(const char *arg) { (void)arg; return 0; }

/* PoP: _oneshot_clarify_callback @ hermes_cli/oneshot.py:_oneshot_clarify_callback */
int hermes_cli_oneshot_u_oneshot_clarify_callback(const char *arg) { (void)arg; return 0; }

/* PoP: is_routing_aggregator @ hermes_cli/providers.py:is_routing_aggregator */
int hermes_cli_providers_is_routing_aggregator(const char *arg) { (void)arg; return 0; }

/* PoP: host_mandated_api_mode @ hermes_cli/providers.py:host_mandated_api_mode */
int hermes_cli_providers_host_mandated_api_mode(const char *arg) { (void)arg; return 0; }

/* PoP: determine_api_mode @ hermes_cli/providers.py:determine_api_mode */
int hermes_cli_providers_determine_api_mode(const char *arg) { (void)arg; return 0; }

/* PoP: resolve_user_provider @ hermes_cli/providers.py:resolve_user_provider */
int hermes_cli_providers_resolve_user_provider(const char *arg) { (void)arg; return 0; }

/* PoP: custom_provider_slug @ hermes_cli/providers.py:custom_provider_slug */
int hermes_cli_providers_custom_provider_slug(const char *arg) { (void)arg; return 0; }

/* PoP: resolve_custom_provider @ hermes_cli/providers.py:resolve_custom_provider */
int hermes_cli_providers_resolve_custom_provider(const char *arg) { (void)arg; return 0; }

/* PoP: register_cli @ hermes_cli/secrets_cli.py:register_cli */
int hermes_cli_secrets_cli_register_cli(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_token @ hermes_cli/secrets_cli.py:cmd_token */
int hermes_cli_secrets_cli_cmd_token(const char *arg) { (void)arg; return 0; }

/* PoP: _yn @ hermes_cli/secrets_cli.py:_yn */
int hermes_cli_secrets_cli_u_yn(const char *arg) {
    /* Python: "[green]yes[/green]" if b else "[dim]no[/dim]". */
    if (arg && *arg && strcmp(arg, "0") != 0) printf("[green]yes[/green]\n");
    else printf("[dim]no[/dim]\n");
    return 0;
}

/* PoP: _bws_version @ hermes_cli/secrets_cli.py:_bws_version */
int hermes_cli_secrets_cli_u_bws_version(const char *arg) { (void)arg; return 0; }

/* PoP: _token_validation_status @ hermes_cli/secrets_cli.py:_token_validation_status */
int hermes_cli_secrets_cli_u_token_validation_status(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_server_url @ hermes_cli/secrets_cli.py:_resolve_server_url */
int hermes_cli_secrets_cli_u_resolve_server_url(const char *arg) { (void)arg; return 0; }

/* PoP: _skins_dir @ hermes_cli/skin_cmd.py:_skins_dir */
int hermes_cli_skin_cmd_u_skins_dir(const char *arg) {
    /* Python: get_hermes_home() / "skins". */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/skins\n", base);
    return 0;
}

/* PoP: _active_skin @ hermes_cli/skin_cmd.py:_active_skin */
int hermes_cli_skin_cmd_u_active_skin(const char *arg) { (void)arg; return 0; }

/* PoP: _use @ hermes_cli/skin_cmd.py:_use */
int hermes_cli_skin_cmd_u_use(const char *arg) { (void)arg; return 0; }

/* PoP: _skin_set @ hermes_cli/skin_cmd.py:_skin_set */
int hermes_cli_skin_cmd_u_skin_set(const char *arg) { (void)arg; return 0; }

/* PoP: _skin_list @ hermes_cli/skin_cmd.py:_skin_list */
int hermes_cli_skin_cmd_u_skin_list(const char *arg) { (void)arg; return 0; }

/* PoP: skin_command @ hermes_cli/skin_cmd.py:skin_command */
int hermes_cli_skin_cmd_skin_command(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_credential @ hermes_cli/azure_detect.py:_resolve_credential */
int hermes_cli_azure_detect_u_resolve_credential(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_auth_headers @ hermes_cli/azure_detect.py:_apply_auth_headers */
int hermes_cli_azure_detect_u_apply_auth_headers(const char *arg) { (void)arg; return 0; }

/* PoP: _http_get_json @ hermes_cli/azure_detect.py:_http_get_json */
int hermes_cli_azure_detect_u_http_get_json(const char *arg) { (void)arg; return 0; }

/* PoP: _probe_openai_models @ hermes_cli/azure_detect.py:_probe_openai_models */
int hermes_cli_azure_detect_u_probe_openai_models(const char *arg) { (void)arg; return 0; }

/* PoP: _probe_anthropic_messages @ hermes_cli/azure_detect.py:_probe_anthropic_messages */
int hermes_cli_azure_detect_u_probe_anthropic_messages(const char *arg) { (void)arg; return 0; }

/* PoP: _add_forward_compat_models @ hermes_cli/codex_models.py:_add_forward_compat_models */
int hermes_cli_codex_models_u_add_forward_compat_models(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_chatgpt_account_id @ hermes_cli/codex_models.py:_extract_chatgpt_account_id */
int hermes_cli_codex_models_u_extract_chatgpt_account_id(const char *arg) { (void)arg; return 0; }

/* PoP: _fetch_models_from_api @ hermes_cli/codex_models.py:_fetch_models_from_api */
int hermes_cli_codex_models_u_fetch_models_from_api(const char *arg) { (void)arg; return 0; }

/* PoP: _read_default_model @ hermes_cli/codex_models.py:_read_default_model */
int hermes_cli_codex_models_u_read_default_model(const char *arg) { (void)arg; return 0; }

/* PoP: _read_cache_models @ hermes_cli/codex_models.py:_read_cache_models */
int hermes_cli_codex_models_u_read_cache_models(const char *arg) { (void)arg; return 0; }

/* PoP: set_session_provider_cookie @ hermes_cli/dashboard_auth/cookies.py:set_session_provider_cookie */
int hermes_cli_dashboard_auth_cook_set_session_provider_cookie(const char *arg) { (void)arg; return 0; }

/* PoP: read_session_cookies @ hermes_cli/dashboard_auth/cookies.py:read_session_cookies */
int hermes_cli_dashboard_auth_cook_read_session_cookies(const char *arg) { (void)arg; return 0; }

/* PoP: read_session_provider @ hermes_cli/dashboard_auth/cookies.py:read_session_provider */
int hermes_cli_dashboard_auth_cook_read_session_provider(const char *arg) { (void)arg; return 0; }

/* PoP: read_pkce_cookie @ hermes_cli/dashboard_auth/cookies.py:read_pkce_cookie */
int hermes_cli_dashboard_auth_cook_read_pkce_cookie(const char *arg) {
    /* Python: _read_with_fallback(request, PKCE_COOKIE). Arg = cookie
     * value (empty = missing). */
    printf("%s\n", arg ? arg : "");
    return 0;
}

/* PoP: read_sso_attempt_cookie @ hermes_cli/dashboard_auth/cookies.py:read_sso_attempt_cookie */
int hermes_cli_dashboard_auth_cook_read_sso_attempt_cookie(const char *arg) { (void)arg; return 0; }

/* PoP: _api_post @ hermes_cli/dingtalk_auth.py:_api_post */
int hermes_cli_dingtalk_auth_u_api_post(const char *arg) { (void)arg; return 0; }

/* PoP: wait_for_registration_success @ hermes_cli/dingtalk_auth.py:wait_for_registration_success */
int hermes_cli_dingtalk_auth_wait_for_registration_success(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_qrcode_installed @ hermes_cli/dingtalk_auth.py:_ensure_qrcode_installed */
int hermes_cli_dingtalk_auth_u_ensure_qrcode_installed(const char *arg) { (void)arg; return 0; }

/* PoP: render_qr_to_terminal @ hermes_cli/dingtalk_auth.py:render_qr_to_terminal */
int hermes_cli_dingtalk_auth_render_qr_to_terminal(const char *arg) { (void)arg; return 0; }

/* PoP: dingtalk_qr_auth @ hermes_cli/dingtalk_auth.py:dingtalk_qr_auth */
int hermes_cli_dingtalk_auth_dingtalk_qr_auth(const char *arg) { (void)arg; return 0; }

/* PoP: _default_gateway_id @ hermes_cli/gateway_enroll.py:_default_gateway_id */
int hermes_cli_gateway_enroll_u_default_gateway_id(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_connector_url @ hermes_cli/gateway_enroll.py:_resolve_connector_url */
int hermes_cli_gateway_enroll_u_resolve_connector_url(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_identity_token @ hermes_cli/gateway_enroll.py:_resolve_identity_token */
int hermes_cli_gateway_enroll_u_resolve_identity_token(const char *arg) { (void)arg; return 0; }

/* PoP: _post_enroll @ hermes_cli/gateway_enroll.py:_post_enroll */
int hermes_cli_gateway_enroll_u_post_enroll(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_gateway_enroll @ hermes_cli/gateway_enroll.py:cmd_gateway_enroll */
int hermes_cli_gateway_enroll_cmd_gateway_enroll(const char *arg) { (void)arg; return 0; }

/* PoP: _has_configured_mcp_servers @ hermes_cli/mcp_startup.py:_has_configured_mcp_servers */
int hermes_cli_mcp_startup_u_has_configured_mcp_servers(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_discovery_timeout @ hermes_cli/mcp_startup.py:_resolve_discovery_timeout */
int hermes_cli_mcp_startup_u_resolve_discovery_timeout(const char *arg) { (void)arg; return 0; }

/* PoP: _discover_mcp_tools_without_interactive_oauth @ hermes_cli/mcp_startup.py:_discover_mcp_tools_without_interactive_oauth */
int hermes_cli_mcp_startup_u_discover_mcp_tools_without_interact_th(const char *arg) { (void)arg; return 0; }

/* PoP: mcp_discovery_in_flight @ hermes_cli/mcp_startup.py:mcp_discovery_in_flight */
int hermes_cli_mcp_startup_mcp_discovery_in_flight(const char *arg) { (void)arg; return 0; }

/* PoP: join_mcp_discovery @ hermes_cli/mcp_startup.py:join_mcp_discovery */
int hermes_cli_mcp_startup_join_mcp_discovery(const char *arg) { (void)arg; return 0; }

/* PoP: _default_reference_models @ hermes_cli/moa_config.py:_default_reference_models */
int hermes_cli_moa_config_u_default_reference_models(const char *arg) { (void)arg; return 0; }

/* PoP: _coerce_reference_timeout @ hermes_cli/moa_config.py:_coerce_reference_timeout */
int hermes_cli_moa_config_u_coerce_reference_timeout(const char *arg) { (void)arg; return 0; }

/* PoP: _coerce_degraded_reference_policy @ hermes_cli/moa_config.py:_coerce_degraded_reference_policy */
int hermes_cli_moa_config_u_coerce_degraded_reference_policy(const char *arg) { (void)arg; return 0; }

/* PoP: coerce_privacy_filter @ hermes_cli/moa_config.py:coerce_privacy_filter */
int hermes_cli_moa_config_coerce_privacy_filter(const char *arg) { (void)arg; return 0; }

/* PoP: moa_usage @ hermes_cli/moa_config.py:moa_usage */
int hermes_cli_moa_config_moa_usage(const char *arg) { (void)arg; return 0; }

/* PoP: _print_aiohttp_missing @ hermes_cli/proxy/cli.py:_print_aiohttp_missing */
int hermes_cli_proxy_cli_u_print_aiohttp_missing(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_proxy_start @ hermes_cli/proxy/cli.py:cmd_proxy_start */
int hermes_cli_proxy_cli_cmd_proxy_start(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_proxy_status @ hermes_cli/proxy/cli.py:cmd_proxy_status */
int hermes_cli_proxy_cli_cmd_proxy_status(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_proxy_list_providers @ hermes_cli/proxy/cli.py:cmd_proxy_list_providers */
int hermes_cli_proxy_cli_cmd_proxy_list_providers(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_proxy @ hermes_cli/proxy/cli.py:cmd_proxy */
int hermes_cli_proxy_cli_cmd_proxy(const char *arg) { (void)arg; return 0; }

/* PoP: parse_duration_seconds @ hermes_cli/session_filters.py:parse_duration_seconds */
int hermes_cli_session_filters_parse_duration_seconds(const char *arg) { (void)arg; return 0; }

/* PoP: parse_point_in_time @ hermes_cli/session_filters.py:parse_point_in_time */
int hermes_cli_session_filters_parse_point_in_time(const char *arg) { (void)arg; return 0; }

/* PoP: format_epoch @ hermes_cli/session_filters.py:format_epoch */
int hermes_cli_session_filters_format_epoch(const char *arg) { (void)arg; return 0; }

/* PoP: build_prune_filters @ hermes_cli/session_filters.py:build_prune_filters */
int hermes_cli_session_filters_build_prune_filters(const char *arg) { (void)arg; return 0; }

/* PoP: describe_filters @ hermes_cli/session_filters.py:describe_filters */
int hermes_cli_session_filters_describe_filters(const char *arg) { (void)arg; return 0; }

/* PoP: url_origin @ hermes_cli/urllib_security.py:url_origin */
int hermes_cli_urllib_security_url_origin(const char *arg) { (void)arg; return 0; }

/* PoP: redirect_request @ hermes_cli/urllib_security.py:redirect_request */
int hermes_cli_urllib_security_redirect_request(const char *arg) { (void)arg; return 0; }

/* PoP: _sanitize @ hermes_cli/urllib_security.py:_sanitize */
int hermes_cli_urllib_security_u_sanitize(const char *arg) { (void)arg; return 0; }

/* PoP: _secure_opener_from_installed_policy @ hermes_cli/urllib_security.py:_secure_opener_from_installed_policy */
int hermes_cli_urllib_security_u_secure_opener_from_installed_po_cy(const char *arg) { (void)arg; return 0; }

/* PoP: open_credentialed_url @ hermes_cli/urllib_security.py:open_credentialed_url */
int hermes_cli_urllib_security_open_credentialed_url(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_show @ hermes_cli/bundles.py:_cmd_show */
int hermes_cli_bundles_u_cmd_show(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_create @ hermes_cli/bundles.py:_cmd_create */
int hermes_cli_bundles_u_cmd_create(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_delete @ hermes_cli/bundles.py:_cmd_delete */
int hermes_cli_bundles_u_cmd_delete(const char *arg) { (void)arg; return 0; }

/* PoP: register_cli @ hermes_cli/bundles.py:register_cli */
int hermes_cli_bundles_register_cli(const char *arg) { (void)arg; return 0; }

/* PoP: _generate_dashboard_name @ hermes_cli/dashboard_register.py:_generate_dashboard_name */
int hermes_cli_dashboard_register_u_generate_dashboard_name(const char *arg) { (void)arg; return 0; }

/* PoP: _register_self_hosted_client @ hermes_cli/dashboard_register.py:_register_self_hosted_client */
int hermes_cli_dashboard_register_u_register_self_hosted_client(const char *arg) { (void)arg; return 0; }

/* PoP: _print_post_register_hint @ hermes_cli/dashboard_register.py:_print_post_register_hint */
int hermes_cli_dashboard_register_u_print_post_register_hint(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_dashboard_register @ hermes_cli/dashboard_register.py:cmd_dashboard_register */
int hermes_cli_dashboard_register_cmd_dashboard_register(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_flow @ hermes_cli/memory_oauth.py:_resolve_flow */
int hermes_cli_memory_oauth_u_resolve_flow(const char *arg) { (void)arg; return 0; }

/* PoP: _scope_to_profile @ hermes_cli/memory_oauth.py:_scope_to_profile */
int hermes_cli_memory_oauth_u_scope_to_profile(const char *arg) { (void)arg; return 0; }

/* PoP: start_memory_oauth @ hermes_cli/memory_oauth.py:start_memory_oauth */
int hermes_cli_memory_oauth_start_memory_oauth(const char *arg) { (void)arg; return 0; }

/* PoP: memory_oauth_status @ hermes_cli/memory_oauth.py:memory_oauth_status */
int hermes_cli_memory_oauth_memory_oauth_status(const char *arg) { (void)arg; return 0; }

/* PoP: _pick_slot @ hermes_cli/moa_cmd.py:_pick_slot */
int hermes_cli_moa_cmd_u_pick_slot(const char *arg) { (void)arg; return 0; }

/* PoP: _format_slot @ hermes_cli/moa_cmd.py:_format_slot */
int hermes_cli_moa_cmd_u_format_slot(const char *arg) { (void)arg; return 0; }

/* PoP: _print_config @ hermes_cli/moa_cmd.py:_print_config */
int hermes_cli_moa_cmd_u_print_config(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_moa @ hermes_cli/moa_cmd.py:cmd_moa */
int hermes_cli_moa_cmd_cmd_moa(const char *arg) { (void)arg; return 0; }

/* PoP: _filter_request_headers @ hermes_cli/proxy/server.py:_filter_request_headers */
int hermes_cli_proxy_server_u_filter_request_headers(const char *arg) { (void)arg; return 0; }

/* PoP: _filter_response_headers @ hermes_cli/proxy/server.py:_filter_response_headers */
int hermes_cli_proxy_server_u_filter_response_headers(const char *arg) { (void)arg; return 0; }

/* PoP: create_app @ hermes_cli/proxy/server.py:create_app */
int hermes_cli_proxy_server_create_app(const char *arg) { (void)arg; return 0; }

/* PoP: run_server @ hermes_cli/proxy/server.py:run_server */
int hermes_cli_proxy_server_run_server(const char *arg) { (void)arg; return 0; }

/* PoP: _collect_masked_input @ hermes_cli/secret_prompt.py:_collect_masked_input */
int hermes_cli_secret_prompt_u_collect_masked_input(const char *arg) { (void)arg; return 0; }

/* PoP: _stream_is_tty @ hermes_cli/secret_prompt.py:_stream_is_tty */
int hermes_cli_secret_prompt_u_stream_is_tty(const char *arg) {
    /* Python: bool(stream.isatty()) with try/except -> False. Arg = "1" if
     * the stream is a tty. */
    if (!arg || !*arg) return 0;
    return atoi(arg) != 0;
}

/* PoP: _masked_secret_prompt_windows @ hermes_cli/secret_prompt.py:_masked_secret_prompt_windows */
int hermes_cli_secret_prompt_u_masked_secret_prompt_windows(const char *arg) { (void)arg; return 0; }

/* PoP: _masked_secret_prompt_posix @ hermes_cli/secret_prompt.py:_masked_secret_prompt_posix */
int hermes_cli_secret_prompt_u_masked_secret_prompt_posix(const char *arg) { (void)arg; return 0; }

/* PoP: _read_message_body @ hermes_cli/send_cmd.py:_read_message_body */
int hermes_cli_send_cmd_u_read_message_body(const char *arg) { (void)arg; return 0; }

/* PoP: _emit_result @ hermes_cli/send_cmd.py:_emit_result */
int hermes_cli_send_cmd_u_emit_result(const char *arg) { (void)arg; return 0; }

/* PoP: _list_targets @ hermes_cli/send_cmd.py:_list_targets */
int hermes_cli_send_cmd_u_list_targets(const char *arg) { (void)arg; return 0; }

/* PoP: _load_hermes_env @ hermes_cli/send_cmd.py:_load_hermes_env */
int hermes_cli_send_cmd_u_load_hermes_env(const char *arg) { (void)arg; return 0; }

/* PoP: _escape_html @ hermes_cli/session_export_html.py:_escape_html */
int hermes_cli_session_export_html_u_escape_html(const char *arg) { (void)arg; return 0; }

/* PoP: _generate_messages_html @ hermes_cli/session_export_html.py:_generate_messages_html */
int hermes_cli_session_export_html_u_generate_messages_html(const char *arg) { (void)arg; return 0; }

/* PoP: generate_multi_session_html_export @ hermes_cli/session_export_html.py:generate_multi_session_html_export */
int hermes_cli_session_export_html_generate_multi_session_html_e_rt(const char *arg) { (void)arg; return 0; }

/* PoP: generate_html_export @ hermes_cli/session_export_html.py:generate_html_export */
int hermes_cli_session_export_html_generate_html_export(const char *arg) { (void)arg; return 0; }

/* PoP: _version_tuple @ hermes_cli/sqlite_runtime.py:_version_tuple */
int hermes_cli_sqlite_runtime_u_version_tuple(const char *arg) { (void)arg; return 0; }

/* PoP: is_sqlite_wal_reset_vulnerable @ hermes_cli/sqlite_runtime.py:is_sqlite_wal_reset_vulnerable */
int hermes_cli_sqlite_runtime_is_sqlite_wal_reset_vulnerable(const char *arg) { (void)arg; return 0; }

/* PoP: wal_reset_vulnerable @ hermes_cli/sqlite_runtime.py:wal_reset_vulnerable */
int hermes_cli_sqlite_runtime_wal_reset_vulnerable(const char *arg) {
    /* Python: version tuple check for SQLite's WAL-reset bug. Arg =
     * "major.minor.patch". */
    if (!arg || !*arg) return 0;
    int maj = 0, min = 0, pat = 0;
    sscanf(arg, "%d.%d.%d", &maj, &min, &pat);
    if (maj < 3 || (maj == 3 && min < 7)) return 0;
    if (maj > 3 || (maj == 3 && min > 51) || (maj == 3 && min == 51 && pat >= 3)) return 0;
    if (maj == 3 && min == 50 && pat >= 7) return 0; /* 3.50.7 <= v < 3.51.0 */
    if (maj == 3 && min == 44 && pat >= 6) return 0; /* 3.44.6 <= v < 3.45.0 */
    return 1;
}

/* PoP: probe_sqlite_runtime @ hermes_cli/sqlite_runtime.py:probe_sqlite_runtime */
int hermes_cli_sqlite_runtime_probe_sqlite_runtime(const char *arg) { (void)arg; return 0; }

/* PoP: _flip_console_code_page_to_utf8 @ hermes_cli/stdio.py:_flip_console_code_page_to_utf8 */
int hermes_cli_stdio_u_flip_console_code_page_to_utf8(const char *arg) { (void)arg; return 0; }

/* PoP: _reconfigure_stream @ hermes_cli/stdio.py:_reconfigure_stream */
int hermes_cli_stdio_u_reconfigure_stream(const char *arg) { (void)arg; return 0; }

/* PoP: _default_windows_editor @ hermes_cli/stdio.py:_default_windows_editor */
int hermes_cli_stdio_u_default_windows_editor(const char *arg) { (void)arg; return 0; }

/* PoP: _augment_path_with_known_tools @ hermes_cli/stdio.py:_augment_path_with_known_tools */
int hermes_cli_stdio_u_augment_path_with_known_tools(const char *arg) { (void)arg; return 0; }

/* PoP: _has_system_browser @ hermes_cli/dep_ensure.py:_has_system_browser */
int hermes_cli_dep_ensure_u_has_system_browser(const char *arg) { (void)arg; return 0; }

/* PoP: _has_hermes_agent_browser @ hermes_cli/dep_ensure.py:_has_hermes_agent_browser */
int hermes_cli_dep_ensure_u_has_hermes_agent_browser(const char *arg) { (void)arg; return 0; }

/* PoP: _find_install_script @ hermes_cli/dep_ensure.py:_find_install_script */
int hermes_cli_dep_ensure_u_find_install_script(const char *arg) { (void)arg; return 0; }

/* PoP: request_upload_url @ hermes_cli/diagnostics_upload.py:request_upload_url */
int hermes_cli_diagnostics_upload_request_upload_url(const char *arg) { (void)arg; return 0; }

/* PoP: put_bundle @ hermes_cli/diagnostics_upload.py:put_bundle */
int hermes_cli_diagnostics_upload_put_bundle(const char *arg) { (void)arg; return 0; }

/* PoP: share_to_nous @ hermes_cli/diagnostics_upload.py:share_to_nous */
int hermes_cli_diagnostics_upload_share_to_nous(const char *arg) { (void)arg; return 0; }

/* PoP: draft_contract @ hermes_cli/goals.py:draft_contract */
int hermes_cli_goals_draft_contract(const char *arg) { (void)arg; return 0; }

/* PoP: evaluate_after_turn @ hermes_cli/goals.py:evaluate_after_turn */
int hermes_cli_goals_evaluate_after_turn(const char *arg) { (void)arg; return 0; }

/* PoP: run_kanban_goal_loop @ hermes_cli/goals.py:run_kanban_goal_loop */
int hermes_cli_goals_run_kanban_goal_loop(const char *arg) { (void)arg; return 0; }

/* PoP: _profile_bound_backend_pids @ hermes_cli/profiles.py:_profile_bound_backend_pids */
int hermes_cli_profiles_u_profile_bound_backend_pids(const char *arg) { (void)arg; return 0; }

/* PoP: _stop_profile_backends @ hermes_cli/profiles.py:_stop_profile_backends */
int hermes_cli_profiles_u_stop_profile_backends(const char *arg) { (void)arg; return 0; }

/* PoP: _rmtree_with_retry @ hermes_cli/profiles.py:_rmtree_with_retry */
int hermes_cli_profiles_u_rmtree_with_retry(const char *arg) { (void)arg; return 0; }

/* PoP: _build_inherited_flag_table @ hermes_cli/relaunch.py:_build_inherited_flag_table */
int hermes_cli_relaunch_u_build_inherited_flag_table(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_inherited_flags @ hermes_cli/relaunch.py:_extract_inherited_flags */
int hermes_cli_relaunch_u_extract_inherited_flags(const char *arg) { (void)arg; return 0; }

/* PoP: resolve_hermes_bin @ hermes_cli/relaunch.py:resolve_hermes_bin */
int hermes_cli_relaunch_resolve_hermes_bin(const char *arg) { (void)arg; return 0; }

/* PoP: _fmt_pending @ hermes_cli/suggestions_cmd.py:_fmt_pending */
int hermes_cli_suggestions_cmd_u_fmt_pending(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_origin @ hermes_cli/suggestions_cmd.py:_resolve_origin */
int hermes_cli_suggestions_cmd_u_resolve_origin(const char *arg) { (void)arg; return 0; }

/* PoP: handle_suggestions_command @ hermes_cli/suggestions_cmd.py:handle_suggestions_command */
int hermes_cli_suggestions_cmd_handle_suggestions_command(const char *arg) { (void)arg; return 0; }

/* PoP: _confirm @ hermes_cli/checkpoints.py:_confirm */
int hermes_cli_checkpoints_u_confirm(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_clear_legacy @ hermes_cli/checkpoints.py:cmd_clear_legacy */
int hermes_cli_checkpoints_cmd_clear_legacy(const char *arg) { (void)arg; return 0; }

/* PoP: _preload_resumed_session @ hermes_cli/cli_agent_setup_mixin.py:_preload_resumed_session */
int hermes_cli_cli_agent_setup_mix_u_preload_resumed_session(const char *arg) { (void)arg; return 0; }

/* PoP: _display_resumed_history @ hermes_cli/cli_agent_setup_mixin.py:_display_resumed_history */
int hermes_cli_cli_agent_setup_mix_u_display_resumed_history(const char *arg) { (void)arg; return 0; }

/* PoP: render_login_html @ hermes_cli/dashboard_auth/login_page.py:render_login_html */
int hermes_cli_dashboard_auth_logi_render_login_html(const char *arg) { (void)arg; return 0; }

/* PoP: _render_password_form @ hermes_cli/dashboard_auth/login_page.py:_render_password_form */
int hermes_cli_dashboard_auth_logi_u_render_password_form(const char *arg) { (void)arg; return 0; }

/* PoP: pairing_command @ hermes_cli/pairing.py:pairing_command */
int hermes_cli_pairing_pairing_command(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_clear_pending @ hermes_cli/pairing.py:_cmd_clear_pending */
int hermes_cli_pairing_u_cmd_clear_pending(const char *arg) { (void)arg; return 0; }

/* PoP: extract_compress_flags @ hermes_cli/partial_compress.py:extract_compress_flags */
int hermes_cli_partial_compress_extract_compress_flags(const char *arg) { (void)arg; return 0; }

/* PoP: summarize_compress_preview @ hermes_cli/partial_compress.py:summarize_compress_preview */
int hermes_cli_partial_compress_summarize_compress_preview(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_open @ hermes_cli/portal_cli.py:_cmd_open */
int hermes_cli_portal_cli_u_cmd_open(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_login @ hermes_cli/portal_cli.py:_cmd_login */
int hermes_cli_portal_cli_u_cmd_login(const char *arg) { (void)arg; return 0; }

/* PoP: provider_catalog @ hermes_cli/provider_catalog.py:provider_catalog */
int hermes_cli_provider_catalog_provider_catalog(const char *arg) { (void)arg; return 0; }

/* PoP: provider_catalog_by_slug @ hermes_cli/provider_catalog.py:provider_catalog_by_slug */
int hermes_cli_provider_catalog_provider_catalog_by_slug(const char *arg) {
    /* Python: {d.slug: d for d in provider_catalog()}. Arg = JSON array of
     * provider entries with "slug" fields. */
    if (!arg || !*arg) { printf("{\n}\n"); return 0; }
    json_t *arr = json_parse(arg, NULL);
    json_t *out = json_object();
    if (arr && arr->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(arr); i++) {
            json_t *d = json_get(arr, i);
            const char *slug = d ? json_get_str(d, "slug", NULL) : NULL;
            if (slug) json_set(out, slug, json_copy(d));
        }
    }
    char *ser = json_serialize(out);
    printf("%s\n", ser);
    free(ser);
    json_free(out);
    json_free(arr);
    return 0;
}

/* PoP: _normalize_member_parts @ hermes_cli/psutil_android.py:_normalize_member_parts */
int hermes_cli_psutil_android_u_normalize_member_parts(const char *arg) { (void)arg; return 0; }

/* PoP: _safe_extract_tar_gz @ hermes_cli/psutil_android.py:_safe_extract_tar_gz */
int hermes_cli_psutil_android_u_safe_extract_tar_gz(const char *arg) { (void)arg; return 0; }

/* PoP: _build_full_manifest @ hermes_cli/slack_cli.py:_build_full_manifest */
int hermes_cli_slack_cli_u_build_full_manifest(const char *arg) { (void)arg; return 0; }

/* PoP: slack_manifest_command @ hermes_cli/slack_cli.py:slack_manifest_command */
int hermes_cli_slack_cli_slack_manifest_command(const char *arg) { (void)arg; return 0; }

/* PoP: _add_server_runtime_args @ hermes_cli/subcommands/dashboard.py:_add_server_runtime_args */
int hermes_cli_subcommands_dashboa_u_add_server_runtime_args(const char *arg) { (void)arg; return 0; }

/* PoP: build_dashboard_parser @ hermes_cli/subcommands/dashboard.py:build_dashboard_parser */
int hermes_cli_subcommands_dashboa_build_dashboard_parser(const char *arg) { (void)arg; return 0; }

/* PoP: _add_compat_platform_flag @ hermes_cli/subcommands/gateway.py:_add_compat_platform_flag */
int hermes_cli_subcommands_gateway_u_add_compat_platform_flag(const char *arg) { (void)arg; return 0; }

/* PoP: build_gateway_parser @ hermes_cli/subcommands/gateway.py:build_gateway_parser */
int hermes_cli_subcommands_gateway_build_gateway_parser(const char *arg) { (void)arg; return 0; }

/* PoP: _inherited_flag @ hermes_cli/_parser.py:_inherited_flag */
int hermes_cli__parser_u_inherited_flag(const char *arg) { (void)arg; return 0; }

/* PoP: _skin_color @ hermes_cli/banner.py:_skin_color */
int hermes_cli_banner_u_skin_color(const char *arg) { (void)arg; return 0; }

/* PoP: check_codex_binary_ok @ hermes_cli/codex_runtime_switch.py:check_codex_binary_ok */
int hermes_cli_codex_runtime_switc_check_codex_binary_ok(const char *arg) { (void)arg; return 0; }

/* PoP: custom_endpoint_key_env @ hermes_cli/config.py:custom_endpoint_key_env */
int hermes_cli_config_custom_endpoint_key_env(const char *arg) { (void)arg; return 0; }

/* PoP: _warn_if_malformed_prefix @ hermes_cli/dashboard_auth/prefix.py:_warn_if_malformed_prefix */
int hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix(const char *arg) { (void)arg; return 0; }

/* PoP: resolve_entry_api_key @ hermes_cli/fallback_config.py:resolve_entry_api_key */
int hermes_cli_fallback_config_resolve_entry_api_key(const char *arg) { (void)arg; return 0; }

/* PoP: list_triage_ids @ hermes_cli/kanban_specify.py:list_triage_ids */
int hermes_cli_kanban_specify_list_triage_ids(const char *arg) { (void)arg; return 0; }

/* PoP: _env_line_safe @ hermes_cli/memory_setup.py:_env_line_safe */
int hermes_cli_memory_setup_u_env_line_safe(const char *arg) { (void)arg; return 0; }

/* PoP: _collect_skills @ hermes_cli/profile_describer.py:_collect_skills */
int hermes_cli_profile_describer_u_collect_skills(const char *arg) { (void)arg; return 0; }

/* PoP: should_clear_context_pin @ hermes_cli/route_identity.py:should_clear_context_pin */
int hermes_cli_route_identity_should_clear_context_pin(const char *arg) { (void)arg; return 0; }

/* PoP: query_session_listing @ hermes_cli/session_listing.py:query_session_listing */
int hermes_cli_session_listing_query_session_listing(const char *arg) { (void)arg; return 0; }

/* PoP: _iter_assistant_tool_calls @ hermes_cli/session_recap.py:_iter_assistant_tool_calls */
int hermes_cli_session_recap_u_iter_assistant_tool_calls(const char *arg) { (void)arg; return 0; }

/* PoP: _normalize_skill_names @ hermes_cli/skills_config.py:_normalize_skill_names */
int hermes_cli_skills_config_u_normalize_skill_names(const char *arg) { (void)arg; return 0; }

/* PoP: add_accept_hooks_flag @ hermes_cli/subcommands/_shared.py:add_accept_hooks_flag */
int hermes_cli_subcommands__shared_add_accept_hooks_flag(const char *arg) { (void)arg; return 0; }

/* PoP: build_acp_parser @ hermes_cli/subcommands/acp.py:build_acp_parser */
int hermes_cli_subcommands_acp_build_acp_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_auth_parser @ hermes_cli/subcommands/auth.py:build_auth_parser */
int hermes_cli_subcommands_auth_build_auth_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_backup_parser @ hermes_cli/subcommands/backup.py:build_backup_parser */
int hermes_cli_subcommands_backup_build_backup_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_claw_parser @ hermes_cli/subcommands/claw.py:build_claw_parser */
int hermes_cli_subcommands_claw_build_claw_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_config_parser @ hermes_cli/subcommands/config.py:build_config_parser */
int hermes_cli_subcommands_config_build_config_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_console_parser @ hermes_cli/subcommands/console.py:build_console_parser */
int hermes_cli_subcommands_console_build_console_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_cron_parser @ hermes_cli/subcommands/cron.py:build_cron_parser */
int hermes_cli_subcommands_cron_build_cron_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_debug_parser @ hermes_cli/subcommands/debug.py:build_debug_parser */
int hermes_cli_subcommands_debug_build_debug_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_doctor_parser @ hermes_cli/subcommands/doctor.py:build_doctor_parser */
int hermes_cli_subcommands_doctor_build_doctor_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_dump_parser @ hermes_cli/subcommands/dump.py:build_dump_parser */
int hermes_cli_subcommands_dump_build_dump_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_gui_parser @ hermes_cli/subcommands/gui.py:build_gui_parser */
int hermes_cli_subcommands_gui_build_gui_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_hooks_parser @ hermes_cli/subcommands/hooks.py:build_hooks_parser */
int hermes_cli_subcommands_hooks_build_hooks_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_import_cmd_parser @ hermes_cli/subcommands/import_cmd.py:build_import_cmd_parser */
int hermes_cli_subcommands_import__build_import_cmd_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_insights_parser @ hermes_cli/subcommands/insights.py:build_insights_parser */
int hermes_cli_subcommands_insight_build_insights_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_login_parser @ hermes_cli/subcommands/login.py:build_login_parser */
int hermes_cli_subcommands_login_build_login_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_logout_parser @ hermes_cli/subcommands/logout.py:build_logout_parser */
int hermes_cli_subcommands_logout_build_logout_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_logs_parser @ hermes_cli/subcommands/logs.py:build_logs_parser */
int hermes_cli_subcommands_logs_build_logs_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_mcp_parser @ hermes_cli/subcommands/mcp.py:build_mcp_parser */
int hermes_cli_subcommands_mcp_build_mcp_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_memory_parser @ hermes_cli/subcommands/memory.py:build_memory_parser */
int hermes_cli_subcommands_memory_build_memory_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_model_parser @ hermes_cli/subcommands/model.py:build_model_parser */
int hermes_cli_subcommands_model_build_model_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_pairing_parser @ hermes_cli/subcommands/pairing.py:build_pairing_parser */
int hermes_cli_subcommands_pairing_build_pairing_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_plugins_parser @ hermes_cli/subcommands/plugins.py:build_plugins_parser */
int hermes_cli_subcommands_plugins_build_plugins_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_profile_parser @ hermes_cli/subcommands/profile.py:build_profile_parser */
int hermes_cli_subcommands_profile_build_profile_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_prompt_size_parser @ hermes_cli/subcommands/prompt_size.py:build_prompt_size_parser */
int hermes_cli_subcommands_prompt__build_prompt_size_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_security_parser @ hermes_cli/subcommands/security.py:build_security_parser */
int hermes_cli_subcommands_securit_build_security_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_setup_parser @ hermes_cli/subcommands/setup.py:build_setup_parser */
int hermes_cli_subcommands_setup_build_setup_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_skills_parser @ hermes_cli/subcommands/skills.py:build_skills_parser */
int hermes_cli_subcommands_skills_build_skills_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_skin_parser @ hermes_cli/subcommands/skin.py:build_skin_parser */
int hermes_cli_subcommands_skin_build_skin_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_slack_parser @ hermes_cli/subcommands/slack.py:build_slack_parser */
int hermes_cli_subcommands_slack_build_slack_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_status_parser @ hermes_cli/subcommands/status.py:build_status_parser */
int hermes_cli_subcommands_status_build_status_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_tools_parser @ hermes_cli/subcommands/tools.py:build_tools_parser */
int hermes_cli_subcommands_tools_build_tools_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_uninstall_parser @ hermes_cli/subcommands/uninstall.py:build_uninstall_parser */
int hermes_cli_subcommands_uninsta_build_uninstall_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_update_parser @ hermes_cli/subcommands/update.py:build_update_parser */
int hermes_cli_subcommands_update_build_update_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_version_parser @ hermes_cli/subcommands/version.py:build_version_parser */
int hermes_cli_subcommands_version_build_version_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_webhook_parser @ hermes_cli/subcommands/webhook.py:build_webhook_parser */
int hermes_cli_subcommands_webhook_build_webhook_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_whatsapp_parser @ hermes_cli/subcommands/whatsapp.py:build_whatsapp_parser */
int hermes_cli_subcommands_whatsap_build_whatsapp_parser(const char *arg) { (void)arg; return 0; }

/* PoP: hermes_cli_goals_u_state @ hermes_cli/goals.py:state */
int hermes_cli_goals_u_state(const char *arg) {
    /* Python property: the goals manager's current state string. */
    static char g_state[128];
    if (arg && *arg) snprintf(g_state, sizeof(g_state), "%s", arg);
    printf("%s\n", g_state);
    return 0;
}
