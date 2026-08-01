/* AUTO-GENERATED integration oracle harness for port_auth_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_auth_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int auth_u_resolve_api_key_provider_secret(const char *);
extern int auth_detect_zai_endpoint(const char *);
extern int auth_u_resolve_zai_base_url(const char *);
extern int auth_u_format_nous_entitlement_auth_error(const char *);
extern int auth_u_auth_lock_holder_for(const char *);
extern int auth_u_get_config_hint_for_unknown_provider(const char *);
extern int auth_u_parse_iso_timestamp(const char *);
extern int auth_u_read_qwen_cli_tokens(const char *);
extern int auth_u_save_qwen_cli_tokens(const char *);
extern int auth_u_refresh_qwen_cli_tokens(const char *);
extern int auth_u_mark_qwen_oauth_active(const char *);
extern int auth_resolve_qwen_runtime_credentials(const char *);
extern int auth_get_qwen_auth_status(const char *);
extern int auth_u_make_spotify_callback_handler(const char *);
extern int auth_u_spotify_wait_for_callback(const char *);
extern int auth_u_spotify_token_payload_to_state(const char *);
extern int auth_u_spotify_exchange_code_for_tokens(const char *);
extern int auth_u_refresh_spotify_oauth_state(const char *);
extern int auth_resolve_spotify_runtime_credentials(const char *);
extern int auth_get_spotify_auth_status(const char *);
extern int auth_u_spotify_interactive_setup(const char *);
extern int auth_login_spotify_command(const char *);
extern int auth_u_is_remote_session(const char *);
extern int auth_u_can_open_graphical_browser(const char *);
extern int auth_u_print_loopback_ssh_hint(const char *);
extern int auth_u_read_codex_tokens(const char *);
extern int auth_u_sync_codex_pool_entries(const char *);
extern int auth_u_save_codex_tokens(const char *);
extern int auth_u_recover_codex_tokens_from_cli(const char *);
extern int auth_refresh_codex_oauth_pure(const char *);
extern int auth_u_refresh_codex_auth_tokens(const char *);
extern int auth_u_import_codex_cli_tokens(const char *);
extern int auth_resolve_codex_runtime_credentials(const char *);
extern int auth_u_is_codex_rate_limit_shaped(const char *);
extern int auth_u_codex_usage_probe_url(const char *);
extern int auth_u_probe_codex_quota_restored(const char *);
extern int auth_clear_codex_pool_quota_cooldowns(const char *);
extern int auth_u_pool_codex_access_token(const char *);
extern int auth_u_read_xai_oauth_tokens(const char *);
extern int auth_u_save_xai_oauth_tokens(const char *);
extern int auth_u_xai_access_token_is_expiring(const char *);
extern int auth_u_xai_proactive_refresh_skew_seconds(const char *);
extern int auth_u_xai_validate_oauth_endpoint(const char *);
extern int auth_u_xai_validate_inference_base_url(const char *);
extern int auth_u_xai_oauth_discovery(const char *);
extern int auth_refresh_xai_oauth_pure(const char *);
extern int auth_u_refresh_xai_oauth_tokens(const char *);
extern int auth_resolve_xai_oauth_runtime_credentials(const char *);
extern int auth_u_request_device_code(const char *);
extern int auth_u_poll_for_token(const char *);
extern int auth_u_try_import_shared_nous_state(const char *);
extern int auth_u_refresh_access_token(const char *);
extern int auth_fetch_nous_models(const char *);
extern int auth_resolve_nous_access_token(const char *);
extern int auth_refresh_nous_oauth_pure(const char *);
extern int auth_refresh_nous_oauth_from_state(const char *);
extern int auth_persist_nous_credentials(const char *);
extern int auth_u_sync_nous_pool_from_auth_store(const char *);
extern int auth_resolve_nous_runtime_credentials(const char *);
extern int auth_u_snapshot_nous_pool_status(const char *);
extern int auth_get_nous_auth_status(const char *);
extern int auth_u_compute_nous_auth_status(const char *);
extern int auth_get_nous_session_validity(const char *);
extern int auth_get_codex_auth_status(const char *);
extern int auth_get_xai_oauth_auth_status(const char *);
extern int auth_get_api_key_provider_status(const char *);
extern int auth_get_external_process_provider_status(const char *);
extern int auth_u_get_azure_foundry_auth_status(const char *);
extern int auth_resolve_api_key_provider_credentials(const char *);
extern int auth_resolve_external_process_provider_credentials(const char *);
extern int auth_u_update_config_for_provider(const char *);
extern int auth_u_confirm_expensive_model_selection(const char *);
extern int auth_u_prompt_model_selection(const char *);
extern int auth_u_login_openai_codex(const char *);
extern int auth_u_login_xai_oauth(const char *);
extern int auth_u_xai_oauth_request_device_code(const char *);
extern int auth_u_xai_oauth_poll_device_token(const char *);
extern int auth_u_xai_oauth_device_code_login(const char *);
extern int auth_u_codex_device_code_login(const char *);
extern int auth_u_minimax_pkce_pair(const char *);
extern int auth_u_minimax_request_user_code(const char *);
extern int auth_u_minimax_poll_token(const char *);
extern int auth_u_minimax_save_auth_state(const char *);
extern int auth_u_minimax_oauth_login(const char *);
extern int auth_u_refresh_minimax_oauth_state(const char *);
extern int auth_u_minimax_oauth_quarantine_on_terminal_refresh(const char *);
extern int auth_build_minimax_oauth_token_provider(const char *);
extern int auth_resolve_minimax_oauth_runtime_credentials(const char *);
extern int auth_get_minimax_oauth_auth_status(const char *);
extern int auth_u_login_minimax_oauth(const char *);
extern int auth_u_login_nous(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_auth_u_resolve_api_key_provider_secret(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_resolve_api_key_provider_secret(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_resolve_api_key_provider_secret"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_detect_zai_endpoint(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_detect_zai_endpoint(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_detect_zai_endpoint"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_resolve_zai_base_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_resolve_zai_base_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_resolve_zai_base_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_format_nous_entitlement_auth_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_format_nous_entitlement_auth_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_format_nous_entitlement_auth_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_auth_lock_holder_for(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_auth_lock_holder_for(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_auth_lock_holder_for"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_get_config_hint_for_unknown_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_get_config_hint_for_unknown_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_get_config_hint_for_unknown_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_parse_iso_timestamp(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_parse_iso_timestamp(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_parse_iso_timestamp"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_read_qwen_cli_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_read_qwen_cli_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_read_qwen_cli_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_save_qwen_cli_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_save_qwen_cli_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_save_qwen_cli_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_refresh_qwen_cli_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_refresh_qwen_cli_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_refresh_qwen_cli_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_mark_qwen_oauth_active(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_mark_qwen_oauth_active(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_mark_qwen_oauth_active"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_resolve_qwen_runtime_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_resolve_qwen_runtime_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_resolve_qwen_runtime_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_get_qwen_auth_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_get_qwen_auth_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_get_qwen_auth_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_make_spotify_callback_handler(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_make_spotify_callback_handler(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_make_spotify_callback_handler"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_spotify_wait_for_callback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_spotify_wait_for_callback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_spotify_wait_for_callback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_spotify_token_payload_to_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_spotify_token_payload_to_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_spotify_token_payload_to_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_spotify_exchange_code_for_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_spotify_exchange_code_for_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_spotify_exchange_code_for_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_refresh_spotify_oauth_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_refresh_spotify_oauth_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_refresh_spotify_oauth_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_resolve_spotify_runtime_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_resolve_spotify_runtime_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_resolve_spotify_runtime_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_get_spotify_auth_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_get_spotify_auth_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_get_spotify_auth_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_spotify_interactive_setup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_spotify_interactive_setup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_spotify_interactive_setup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_login_spotify_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_login_spotify_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_login_spotify_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_is_remote_session(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_is_remote_session(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_is_remote_session"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_can_open_graphical_browser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_can_open_graphical_browser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_can_open_graphical_browser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_print_loopback_ssh_hint(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_print_loopback_ssh_hint(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_print_loopback_ssh_hint"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_read_codex_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_read_codex_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_read_codex_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_sync_codex_pool_entries(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_sync_codex_pool_entries(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_sync_codex_pool_entries"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_save_codex_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_save_codex_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_save_codex_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_recover_codex_tokens_from_cli(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_recover_codex_tokens_from_cli(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_recover_codex_tokens_from_cli"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_refresh_codex_oauth_pure(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_refresh_codex_oauth_pure(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_refresh_codex_oauth_pure"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_refresh_codex_auth_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_refresh_codex_auth_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_refresh_codex_auth_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_import_codex_cli_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_import_codex_cli_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_import_codex_cli_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_resolve_codex_runtime_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_resolve_codex_runtime_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_resolve_codex_runtime_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_is_codex_rate_limit_shaped(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_is_codex_rate_limit_shaped(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_is_codex_rate_limit_shaped"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_codex_usage_probe_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_codex_usage_probe_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_codex_usage_probe_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_probe_codex_quota_restored(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_probe_codex_quota_restored(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_probe_codex_quota_restored"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_clear_codex_pool_quota_cooldowns(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_clear_codex_pool_quota_cooldowns(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_clear_codex_pool_quota_cooldowns"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_pool_codex_access_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_pool_codex_access_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_pool_codex_access_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_read_xai_oauth_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_read_xai_oauth_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_read_xai_oauth_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_save_xai_oauth_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_save_xai_oauth_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_save_xai_oauth_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_xai_access_token_is_expiring(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_xai_access_token_is_expiring(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_xai_access_token_is_expiring"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_xai_proactive_refresh_skew_seconds(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_xai_proactive_refresh_skew_seconds(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_xai_proactive_refresh_skew_seconds"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_xai_validate_oauth_endpoint(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_xai_validate_oauth_endpoint(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_xai_validate_oauth_endpoint"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_xai_validate_inference_base_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_xai_validate_inference_base_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_xai_validate_inference_base_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_xai_oauth_discovery(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_xai_oauth_discovery(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_xai_oauth_discovery"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_refresh_xai_oauth_pure(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_refresh_xai_oauth_pure(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_refresh_xai_oauth_pure"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_refresh_xai_oauth_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_refresh_xai_oauth_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_refresh_xai_oauth_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_resolve_xai_oauth_runtime_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_resolve_xai_oauth_runtime_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_resolve_xai_oauth_runtime_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_request_device_code(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_request_device_code(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_request_device_code"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_poll_for_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_poll_for_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_poll_for_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_try_import_shared_nous_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_try_import_shared_nous_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_try_import_shared_nous_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_refresh_access_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_refresh_access_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_refresh_access_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_fetch_nous_models(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_fetch_nous_models(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_fetch_nous_models"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_resolve_nous_access_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_resolve_nous_access_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_resolve_nous_access_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_refresh_nous_oauth_pure(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_refresh_nous_oauth_pure(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_refresh_nous_oauth_pure"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_refresh_nous_oauth_from_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_refresh_nous_oauth_from_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_refresh_nous_oauth_from_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_persist_nous_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_persist_nous_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_persist_nous_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_sync_nous_pool_from_auth_store(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_sync_nous_pool_from_auth_store(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_sync_nous_pool_from_auth_store"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_resolve_nous_runtime_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_resolve_nous_runtime_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_resolve_nous_runtime_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_snapshot_nous_pool_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_snapshot_nous_pool_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_snapshot_nous_pool_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_get_nous_auth_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_get_nous_auth_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_get_nous_auth_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_compute_nous_auth_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_compute_nous_auth_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_compute_nous_auth_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_get_nous_session_validity(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_get_nous_session_validity(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_get_nous_session_validity"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_get_codex_auth_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_get_codex_auth_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_get_codex_auth_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_get_xai_oauth_auth_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_get_xai_oauth_auth_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_get_xai_oauth_auth_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_get_api_key_provider_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_get_api_key_provider_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_get_api_key_provider_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_get_external_process_provider_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_get_external_process_provider_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_get_external_process_provider_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_get_azure_foundry_auth_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_get_azure_foundry_auth_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_get_azure_foundry_auth_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_resolve_api_key_provider_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_resolve_api_key_provider_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_resolve_api_key_provider_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_resolve_external_process_provider_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_resolve_external_process_provider_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_resolve_external_process_provider_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_update_config_for_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_update_config_for_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_update_config_for_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_confirm_expensive_model_selection(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_confirm_expensive_model_selection(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_confirm_expensive_model_selection"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_prompt_model_selection(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_prompt_model_selection(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_prompt_model_selection"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_login_openai_codex(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_login_openai_codex(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_login_openai_codex"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_login_xai_oauth(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_login_xai_oauth(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_login_xai_oauth"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_xai_oauth_request_device_code(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_xai_oauth_request_device_code(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_xai_oauth_request_device_code"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_xai_oauth_poll_device_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_xai_oauth_poll_device_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_xai_oauth_poll_device_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_xai_oauth_device_code_login(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_xai_oauth_device_code_login(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_xai_oauth_device_code_login"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_codex_device_code_login(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_codex_device_code_login(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_codex_device_code_login"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_minimax_pkce_pair(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_minimax_pkce_pair(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_minimax_pkce_pair"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_minimax_request_user_code(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_minimax_request_user_code(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_minimax_request_user_code"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_minimax_poll_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_minimax_poll_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_minimax_poll_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_minimax_save_auth_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_minimax_save_auth_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_minimax_save_auth_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_minimax_oauth_login(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_minimax_oauth_login(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_minimax_oauth_login"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_refresh_minimax_oauth_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_refresh_minimax_oauth_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_refresh_minimax_oauth_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_minimax_oauth_quarantine_on_terminal_refresh(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_minimax_oauth_quarantine_on_terminal_refresh(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_minimax_oauth_quarantine_on_terminal_refresh"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_build_minimax_oauth_token_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_build_minimax_oauth_token_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_build_minimax_oauth_token_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_resolve_minimax_oauth_runtime_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_resolve_minimax_oauth_runtime_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_resolve_minimax_oauth_runtime_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_get_minimax_oauth_auth_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_get_minimax_oauth_auth_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_get_minimax_oauth_auth_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_login_minimax_oauth(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_login_minimax_oauth(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_login_minimax_oauth"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_auth_u_login_nous(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)auth_u_login_nous(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("auth_u_login_nous"));
    json_set(o, "out", json_int(v)); return o;
}

int main(int argc, char **argv){
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char *err = NULL; json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); free(input); return 2; }
    int n = json_array_size(root);
    for (int i = 0; i < n; i++){
        json_t *c = json_get(root, i);
        const char *op = json_get_str(c, "op", "");
        json_t *o = NULL;
        if (strcmp(op, "auth_u_resolve_api_key_provider_secret") == 0) o = emit_auth_u_resolve_api_key_provider_secret(c);
        if (strcmp(op, "auth_detect_zai_endpoint") == 0) o = emit_auth_detect_zai_endpoint(c);
        if (strcmp(op, "auth_u_resolve_zai_base_url") == 0) o = emit_auth_u_resolve_zai_base_url(c);
        if (strcmp(op, "auth_u_format_nous_entitlement_auth_error") == 0) o = emit_auth_u_format_nous_entitlement_auth_error(c);
        if (strcmp(op, "auth_u_auth_lock_holder_for") == 0) o = emit_auth_u_auth_lock_holder_for(c);
        if (strcmp(op, "auth_u_get_config_hint_for_unknown_provider") == 0) o = emit_auth_u_get_config_hint_for_unknown_provider(c);
        if (strcmp(op, "auth_u_parse_iso_timestamp") == 0) o = emit_auth_u_parse_iso_timestamp(c);
        if (strcmp(op, "auth_u_read_qwen_cli_tokens") == 0) o = emit_auth_u_read_qwen_cli_tokens(c);
        if (strcmp(op, "auth_u_save_qwen_cli_tokens") == 0) o = emit_auth_u_save_qwen_cli_tokens(c);
        if (strcmp(op, "auth_u_refresh_qwen_cli_tokens") == 0) o = emit_auth_u_refresh_qwen_cli_tokens(c);
        if (strcmp(op, "auth_u_mark_qwen_oauth_active") == 0) o = emit_auth_u_mark_qwen_oauth_active(c);
        if (strcmp(op, "auth_resolve_qwen_runtime_credentials") == 0) o = emit_auth_resolve_qwen_runtime_credentials(c);
        if (strcmp(op, "auth_get_qwen_auth_status") == 0) o = emit_auth_get_qwen_auth_status(c);
        if (strcmp(op, "auth_u_make_spotify_callback_handler") == 0) o = emit_auth_u_make_spotify_callback_handler(c);
        if (strcmp(op, "auth_u_spotify_wait_for_callback") == 0) o = emit_auth_u_spotify_wait_for_callback(c);
        if (strcmp(op, "auth_u_spotify_token_payload_to_state") == 0) o = emit_auth_u_spotify_token_payload_to_state(c);
        if (strcmp(op, "auth_u_spotify_exchange_code_for_tokens") == 0) o = emit_auth_u_spotify_exchange_code_for_tokens(c);
        if (strcmp(op, "auth_u_refresh_spotify_oauth_state") == 0) o = emit_auth_u_refresh_spotify_oauth_state(c);
        if (strcmp(op, "auth_resolve_spotify_runtime_credentials") == 0) o = emit_auth_resolve_spotify_runtime_credentials(c);
        if (strcmp(op, "auth_get_spotify_auth_status") == 0) o = emit_auth_get_spotify_auth_status(c);
        if (strcmp(op, "auth_u_spotify_interactive_setup") == 0) o = emit_auth_u_spotify_interactive_setup(c);
        if (strcmp(op, "auth_login_spotify_command") == 0) o = emit_auth_login_spotify_command(c);
        if (strcmp(op, "auth_u_is_remote_session") == 0) o = emit_auth_u_is_remote_session(c);
        if (strcmp(op, "auth_u_can_open_graphical_browser") == 0) o = emit_auth_u_can_open_graphical_browser(c);
        if (strcmp(op, "auth_u_print_loopback_ssh_hint") == 0) o = emit_auth_u_print_loopback_ssh_hint(c);
        if (strcmp(op, "auth_u_read_codex_tokens") == 0) o = emit_auth_u_read_codex_tokens(c);
        if (strcmp(op, "auth_u_sync_codex_pool_entries") == 0) o = emit_auth_u_sync_codex_pool_entries(c);
        if (strcmp(op, "auth_u_save_codex_tokens") == 0) o = emit_auth_u_save_codex_tokens(c);
        if (strcmp(op, "auth_u_recover_codex_tokens_from_cli") == 0) o = emit_auth_u_recover_codex_tokens_from_cli(c);
        if (strcmp(op, "auth_refresh_codex_oauth_pure") == 0) o = emit_auth_refresh_codex_oauth_pure(c);
        if (strcmp(op, "auth_u_refresh_codex_auth_tokens") == 0) o = emit_auth_u_refresh_codex_auth_tokens(c);
        if (strcmp(op, "auth_u_import_codex_cli_tokens") == 0) o = emit_auth_u_import_codex_cli_tokens(c);
        if (strcmp(op, "auth_resolve_codex_runtime_credentials") == 0) o = emit_auth_resolve_codex_runtime_credentials(c);
        if (strcmp(op, "auth_u_is_codex_rate_limit_shaped") == 0) o = emit_auth_u_is_codex_rate_limit_shaped(c);
        if (strcmp(op, "auth_u_codex_usage_probe_url") == 0) o = emit_auth_u_codex_usage_probe_url(c);
        if (strcmp(op, "auth_u_probe_codex_quota_restored") == 0) o = emit_auth_u_probe_codex_quota_restored(c);
        if (strcmp(op, "auth_clear_codex_pool_quota_cooldowns") == 0) o = emit_auth_clear_codex_pool_quota_cooldowns(c);
        if (strcmp(op, "auth_u_pool_codex_access_token") == 0) o = emit_auth_u_pool_codex_access_token(c);
        if (strcmp(op, "auth_u_read_xai_oauth_tokens") == 0) o = emit_auth_u_read_xai_oauth_tokens(c);
        if (strcmp(op, "auth_u_save_xai_oauth_tokens") == 0) o = emit_auth_u_save_xai_oauth_tokens(c);
        if (strcmp(op, "auth_u_xai_access_token_is_expiring") == 0) o = emit_auth_u_xai_access_token_is_expiring(c);
        if (strcmp(op, "auth_u_xai_proactive_refresh_skew_seconds") == 0) o = emit_auth_u_xai_proactive_refresh_skew_seconds(c);
        if (strcmp(op, "auth_u_xai_validate_oauth_endpoint") == 0) o = emit_auth_u_xai_validate_oauth_endpoint(c);
        if (strcmp(op, "auth_u_xai_validate_inference_base_url") == 0) o = emit_auth_u_xai_validate_inference_base_url(c);
        if (strcmp(op, "auth_u_xai_oauth_discovery") == 0) o = emit_auth_u_xai_oauth_discovery(c);
        if (strcmp(op, "auth_refresh_xai_oauth_pure") == 0) o = emit_auth_refresh_xai_oauth_pure(c);
        if (strcmp(op, "auth_u_refresh_xai_oauth_tokens") == 0) o = emit_auth_u_refresh_xai_oauth_tokens(c);
        if (strcmp(op, "auth_resolve_xai_oauth_runtime_credentials") == 0) o = emit_auth_resolve_xai_oauth_runtime_credentials(c);
        if (strcmp(op, "auth_u_request_device_code") == 0) o = emit_auth_u_request_device_code(c);
        if (strcmp(op, "auth_u_poll_for_token") == 0) o = emit_auth_u_poll_for_token(c);
        if (strcmp(op, "auth_u_try_import_shared_nous_state") == 0) o = emit_auth_u_try_import_shared_nous_state(c);
        if (strcmp(op, "auth_u_refresh_access_token") == 0) o = emit_auth_u_refresh_access_token(c);
        if (strcmp(op, "auth_fetch_nous_models") == 0) o = emit_auth_fetch_nous_models(c);
        if (strcmp(op, "auth_resolve_nous_access_token") == 0) o = emit_auth_resolve_nous_access_token(c);
        if (strcmp(op, "auth_refresh_nous_oauth_pure") == 0) o = emit_auth_refresh_nous_oauth_pure(c);
        if (strcmp(op, "auth_refresh_nous_oauth_from_state") == 0) o = emit_auth_refresh_nous_oauth_from_state(c);
        if (strcmp(op, "auth_persist_nous_credentials") == 0) o = emit_auth_persist_nous_credentials(c);
        if (strcmp(op, "auth_u_sync_nous_pool_from_auth_store") == 0) o = emit_auth_u_sync_nous_pool_from_auth_store(c);
        if (strcmp(op, "auth_resolve_nous_runtime_credentials") == 0) o = emit_auth_resolve_nous_runtime_credentials(c);
        if (strcmp(op, "auth_u_snapshot_nous_pool_status") == 0) o = emit_auth_u_snapshot_nous_pool_status(c);
        if (strcmp(op, "auth_get_nous_auth_status") == 0) o = emit_auth_get_nous_auth_status(c);
        if (strcmp(op, "auth_u_compute_nous_auth_status") == 0) o = emit_auth_u_compute_nous_auth_status(c);
        if (strcmp(op, "auth_get_nous_session_validity") == 0) o = emit_auth_get_nous_session_validity(c);
        if (strcmp(op, "auth_get_codex_auth_status") == 0) o = emit_auth_get_codex_auth_status(c);
        if (strcmp(op, "auth_get_xai_oauth_auth_status") == 0) o = emit_auth_get_xai_oauth_auth_status(c);
        if (strcmp(op, "auth_get_api_key_provider_status") == 0) o = emit_auth_get_api_key_provider_status(c);
        if (strcmp(op, "auth_get_external_process_provider_status") == 0) o = emit_auth_get_external_process_provider_status(c);
        if (strcmp(op, "auth_u_get_azure_foundry_auth_status") == 0) o = emit_auth_u_get_azure_foundry_auth_status(c);
        if (strcmp(op, "auth_resolve_api_key_provider_credentials") == 0) o = emit_auth_resolve_api_key_provider_credentials(c);
        if (strcmp(op, "auth_resolve_external_process_provider_credentials") == 0) o = emit_auth_resolve_external_process_provider_credentials(c);
        if (strcmp(op, "auth_u_update_config_for_provider") == 0) o = emit_auth_u_update_config_for_provider(c);
        if (strcmp(op, "auth_u_confirm_expensive_model_selection") == 0) o = emit_auth_u_confirm_expensive_model_selection(c);
        if (strcmp(op, "auth_u_prompt_model_selection") == 0) o = emit_auth_u_prompt_model_selection(c);
        if (strcmp(op, "auth_u_login_openai_codex") == 0) o = emit_auth_u_login_openai_codex(c);
        if (strcmp(op, "auth_u_login_xai_oauth") == 0) o = emit_auth_u_login_xai_oauth(c);
        if (strcmp(op, "auth_u_xai_oauth_request_device_code") == 0) o = emit_auth_u_xai_oauth_request_device_code(c);
        if (strcmp(op, "auth_u_xai_oauth_poll_device_token") == 0) o = emit_auth_u_xai_oauth_poll_device_token(c);
        if (strcmp(op, "auth_u_xai_oauth_device_code_login") == 0) o = emit_auth_u_xai_oauth_device_code_login(c);
        if (strcmp(op, "auth_u_codex_device_code_login") == 0) o = emit_auth_u_codex_device_code_login(c);
        if (strcmp(op, "auth_u_minimax_pkce_pair") == 0) o = emit_auth_u_minimax_pkce_pair(c);
        if (strcmp(op, "auth_u_minimax_request_user_code") == 0) o = emit_auth_u_minimax_request_user_code(c);
        if (strcmp(op, "auth_u_minimax_poll_token") == 0) o = emit_auth_u_minimax_poll_token(c);
        if (strcmp(op, "auth_u_minimax_save_auth_state") == 0) o = emit_auth_u_minimax_save_auth_state(c);
        if (strcmp(op, "auth_u_minimax_oauth_login") == 0) o = emit_auth_u_minimax_oauth_login(c);
        if (strcmp(op, "auth_u_refresh_minimax_oauth_state") == 0) o = emit_auth_u_refresh_minimax_oauth_state(c);
        if (strcmp(op, "auth_u_minimax_oauth_quarantine_on_terminal_refresh") == 0) o = emit_auth_u_minimax_oauth_quarantine_on_terminal_refresh(c);
        if (strcmp(op, "auth_build_minimax_oauth_token_provider") == 0) o = emit_auth_build_minimax_oauth_token_provider(c);
        if (strcmp(op, "auth_resolve_minimax_oauth_runtime_credentials") == 0) o = emit_auth_resolve_minimax_oauth_runtime_credentials(c);
        if (strcmp(op, "auth_get_minimax_oauth_auth_status") == 0) o = emit_auth_get_minimax_oauth_auth_status(c);
        if (strcmp(op, "auth_u_login_minimax_oauth") == 0) o = emit_auth_u_login_minimax_oauth(c);
        if (strcmp(op, "auth_u_login_nous") == 0) o = emit_auth_u_login_nous(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
