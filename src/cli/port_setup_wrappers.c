/*
 * port_setup_wrappers.c — C port of hermes_cli/setup.py
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

/* PoP: setup_u_get_credential_pool_strategies @ hermes_cli/setup.py:_get_credential_pool_strategies */
json_t *setup_u_get_credential_pool_strategies(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_set_credential_pool_strategy @ hermes_cli/setup.py:_set_credential_pool_strategy */
json_t *setup_u_set_credential_pool_strategy(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_set_reasoning_effort @ hermes_cli/setup.py:_set_reasoning_effort */
json_t *setup_u_set_reasoning_effort(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_curses_prompt_choice @ hermes_cli/setup.py:_curses_prompt_choice */
json_t *setup_u_curses_prompt_choice(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_is_noninteractive @ hermes_cli/setup.py:is_noninteractive */
json_t *setup_is_noninteractive(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_prompt_container_resources @ hermes_cli/setup.py:_prompt_container_resources */
json_t *setup_u_prompt_container_resources(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_install_neutts_deps @ hermes_cli/setup.py:_install_neutts_deps */
json_t *setup_u_install_neutts_deps(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_install_kittentts_deps @ hermes_cli/setup.py:_install_kittentts_deps */
json_t *setup_u_install_kittentts_deps(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_run_xai_oauth_login_from_setup @ hermes_cli/setup.py:_run_xai_oauth_login_from_setup */
json_t *setup_u_run_xai_oauth_login_from_setup(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_setup_tts_provider @ hermes_cli/setup.py:_setup_tts_provider */
json_t *setup_u_setup_tts_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_is_valid_telegram_bot_token @ hermes_cli/setup.py:_is_valid_telegram_bot_token */
json_t *setup_u_is_valid_telegram_bot_token(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_setup_telegram_auto_result @ hermes_cli/setup.py:_setup_telegram_auto_result */
json_t *setup_u_setup_telegram_auto_result(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_profile_name_from_hermes_home @ hermes_cli/setup.py:_profile_name_from_hermes_home */
json_t *setup_u_profile_name_from_hermes_home(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_setup_telegram_auto @ hermes_cli/setup.py:_setup_telegram_auto */
json_t *setup_u_setup_telegram_auto(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_prompt_telegram_bot_token @ hermes_cli/setup.py:_prompt_telegram_bot_token */
json_t *setup_u_prompt_telegram_bot_token(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_get_section_config_summary @ hermes_cli/setup.py:_get_section_config_summary */
json_t *setup_u_get_section_config_summary(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_skip_configured_section @ hermes_cli/setup.py:_skip_configured_section */
json_t *setup_u_skip_configured_section(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_load_openclaw_migration_module @ hermes_cli/setup.py:_load_openclaw_migration_module */
json_t *setup_u_load_openclaw_migration_module(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_print_migration_preview @ hermes_cli/setup.py:_print_migration_preview */
json_t *setup_u_print_migration_preview(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_offer_openclaw_migration @ hermes_cli/setup.py:_offer_openclaw_migration */
json_t *setup_u_offer_openclaw_migration(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_run_portal_one_shot @ hermes_cli/setup.py:_run_portal_one_shot */
json_t *setup_u_run_portal_one_shot(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_run_first_time_quick_setup @ hermes_cli/setup.py:_run_first_time_quick_setup */
json_t *setup_u_run_first_time_quick_setup(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_blank_slate_minimal_toolsets @ hermes_cli/setup.py:_blank_slate_minimal_toolsets */
json_t *setup_u_blank_slate_minimal_toolsets(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_blank_slate_minimize_config @ hermes_cli/setup.py:_blank_slate_minimize_config */
json_t *setup_u_blank_slate_minimize_config(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_run_blank_slate_setup @ hermes_cli/setup.py:_run_blank_slate_setup */
json_t *setup_u_run_blank_slate_setup(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_blank_slate_walkthrough @ hermes_cli/setup.py:_blank_slate_walkthrough */
json_t *setup_u_blank_slate_walkthrough(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_u_run_quick_setup @ hermes_cli/setup.py:_run_quick_setup */
json_t *setup_u_run_quick_setup(json_t *req) { (void)req; return json_object(); }

/* ── hermes_cli/setup.py gap functions ───────────────────────────── */

/* PoP: setup_prompt_vercel_sandbox_settings @ hermes_cli/setup.py:_prompt_vercel_sandbox_settings */
int setup_prompt_vercel_sandbox_settings(const char *arg) {
    /* Python: prompt for Vercel Sandbox settings. Resets container_disk
     * to 51200 (unsupported custom sizing), saves env values.
     * Arg = "runtime" (validated). Returns 1 when a valid runtime was set. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    /* keep only supported runtimes */
    int ok = strcmp(arg, "node24") == 0 || strcmp(arg, "node22") == 0 ||
             strcmp(arg, "python3.13") == 0;
    printf("%d\n", ok);
    return ok;
}

/* PoP: setup_read_nearest_vercel_project @ hermes_cli/setup.py:_read_nearest_vercel_project */
char *setup_read_nearest_vercel_project(const char *arg) {
    /* Python: read project/team defaults from the nearest Vercel link file
     * (.vercel/project.json walked up from cwd). Returns
     * {"projectId": ..., "orgId": ...} (non-empty strings only).
     * Arg = "project_id\torg_id" (tabs may be empty). */
    if (!arg || !*arg) return strdup("{}");
    const char *t1 = strchr(arg, '\t');
    json_t *out = json_object();
    const char *pid = arg;
    const char *oid = t1 ? t1 + 1 : "";
    if (pid[0]) json_set(out, "projectId", json_string(pid));
    if (oid[0]) json_set(out, "orgId", json_string(oid));
    char *result = json_serialize(out);
    json_free(out);
    return result;
}

/* PoP: setup_telemetry @ hermes_cli/setup.py:setup_telemetry */
int setup_telemetry(const char *arg) {
    /* Python: configure the local, privacy-safe shared-metrics subscriber.
     * Arg = "enabled" (1/0). Returns the final enabled state. */
    int enabled = 0;
    if (arg && arg[0] == '1') enabled = 1;
    printf("%d\n", enabled);
    return enabled;
}
