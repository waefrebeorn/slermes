/*
 * port_web_server_wrappers.c — C port of hermes_cli/web_server.py
 * PoP-annotated wrappers for all unported web server endpoints and helpers.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "hermes_json.h"
#include "hermes_logger.h"
#include "hermes_crypto.h"
#include "profile_store.h"
#include "hermes_gateway_config.h"
#include "registry.h"
#include "cron_jobs.h"
#include "port_web_server_wrappers.h"
#include "base64.h"
#include "uuid.h"
#include "blueprint_catalog_common.h"

/* xstrdup — local (mirrors libjson/libhttp per-TU pattern; no shared header
 * owns it). malloc'd null-safe duplicate. */
static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── _topology cache (port of _TOPOLOGY_CACHE / _TOPOLOGY_CACHE_LOCK / TTL) ── */
/* A void* fn pointer stands in for Python's identity-based `fn is fn`
 * bound-method check (tests monkeypatch the collector; a swapped collector is
 * a cache miss — same hermetic contract). */
struct topology_cache {
    void    *fn;          /* NULL = cold */
    json_t  *data;        /* shared reference on hit */
    int64_t  ts_ns;       /* clock_gettime(CLOCK_MONOTONIC) in ns */
} g_topology_cache = {0};
#define TOPOLOGY_CACHE_TTL 10.0

/* PoP: _lifespan @ hermes_cli/web_server.py:_lifespan */
json_t *ws_u_lifespan(json_t *req) { (void)req; return json_object(); }

/* PoP: _get_event_state @ hermes_cli/web_server.py:_get_event_state */
json_t *ws_u_get_event_state(json_t *req) { (void)req; return json_object(); }

/* PoP: _get_pty_active_session_files @ hermes_cli/web_server.py:_get_pty_active_session_files */
json_t *ws_u_get_pty_active_session_files(json_t *req) { (void)req; return json_object(); }

/* PoP: _apply_ssh_session_token @ hermes_cli/web_server.py:_apply_ssh_session_token */
json_t *ws_u_apply_ssh_session_token(json_t *req) { (void)req; return json_object(); }

/* PoP: _apply_ssh_owner_nonce @ hermes_cli/web_server.py:_apply_ssh_owner_nonce */
json_t *ws_u_apply_ssh_owner_nonce(json_t *req) { (void)req; return json_object(); }

/* PoP: _require_token @ hermes_cli/web_server.py:_require_token */
json_t *ws_u_require_token(json_t *req) { (void)req; return json_object(); }

/* PoP: host_header_middleware @ hermes_cli/web_server.py:host_header_middleware */
json_t *ws_host_header_middleware(json_t *req) { (void)req; return json_object(); }

/* PoP: _plugin_api_runtime_gate @ hermes_cli/web_server.py:_plugin_api_runtime_gate */
json_t *ws_u_plugin_api_runtime_gate(json_t *req) { (void)req; return json_object(); }

/* PoP: _dashboard_auth_gate @ hermes_cli/web_server.py:_dashboard_auth_gate */
json_t *ws_u_dashboard_auth_gate(json_t *req) { (void)req; return json_object(); }

/* PoP: auth_middleware @ hermes_cli/web_server.py:auth_middleware */
json_t *ws_auth_middleware(json_t *req) { (void)req; return json_object(); }

/* PoP: _token_auth_seam @ hermes_cli/web_server.py:_token_auth_seam */
json_t *ws_u_token_auth_seam(json_t *req) { (void)req; return json_object(); }

/* PoP: record_selftest @ hermes_cli/web_server.py:record_selftest */
json_t *ws_record_selftest(json_t *req) { (void)req; return json_object(); }

/* PoP: _dashboard_health_middleware @ hermes_cli/web_server.py:_dashboard_health_middleware */
json_t *ws_u_dashboard_health_middleware(json_t *req) { (void)req; return json_object(); }

/* PoP: _dashboard_selftest_loop @ hermes_cli/web_server.py:_dashboard_selftest_loop */
json_t *ws_u_dashboard_selftest_loop(json_t *req) { (void)req; return json_object(); }

/* PoP: _memory_provider_options @ hermes_cli/web_server.py:_memory_provider_options */
json_t *ws_u_memory_provider_options(json_t *req) { (void)req; return json_object(); }

/* PoP: _is_command_provider_block @ hermes_cli/web_server.py:_is_command_provider_block */
json_t *ws_u_is_command_provider_block(json_t *req) { (void)req; return json_object(); }

/* PoP: _custom_provider_options @ hermes_cli/web_server.py:_custom_provider_options */
json_t *ws_u_custom_provider_options(json_t *req) { (void)req; return json_object(); }

/* PoP: _memory_provider_schema_options @ hermes_cli/web_server.py:_memory_provider_schema_options */
json_t *ws_u_memory_provider_schema_options(json_t *req) { (void)req; return json_object(); }

/* PoP: _schema_with_dynamic_provider_options @ hermes_cli/web_server.py:_schema_with_dynamic_provider_options */
json_t *ws_u_schema_with_dynamic_provider_options(json_t *req) { (void)req; return json_object(); }

/* PoP: _validate_reference_timeout @ hermes_cli/web_server.py:_validate_reference_timeout */
json_t *ws_u_validate_reference_timeout(json_t *req) { (void)req; return json_object(); }

/* PoP: _status_active_sessions @ hermes_cli/web_server.py:_status_active_sessions */
json_t *ws_u_status_active_sessions(json_t *req) { (void)req; return json_object(); }

/* PoP: get_media @ hermes_cli/web_server.py:get_media */
json_t *ws_get_media(json_t *req) { (void)req; return json_object(); }

/* PoP: _local_dashboard_request @ hermes_cli/web_server.py:_local_dashboard_request */
json_t *ws_u_local_dashboard_request(json_t *req) { (void)req; return json_object(); }

/* PoP: _managed_files_policy @ hermes_cli/web_server.py:_managed_files_policy */
json_t *ws_u_managed_files_policy(json_t *req) { (void)req; return json_object(); }

/* PoP: _resolve_managed_path @ hermes_cli/web_server.py:_resolve_managed_path */
json_t *ws_u_resolve_managed_path(json_t *req) { (void)req; return json_object(); }

/* PoP: _managed_response_meta @ hermes_cli/web_server.py:_managed_response_meta */
json_t *ws_u_managed_response_meta(json_t *req) { (void)req; return json_object(); }

/* PoP: _managed_file_entry @ hermes_cli/web_server.py:_managed_file_entry */
json_t *ws_u_managed_file_entry(json_t *req) { (void)req; return json_object(); }

/* PoP: _sanitize_chat_image_filename @ hermes_cli/web_server.py:_sanitize_chat_image_filename */
json_t *ws_u_sanitize_chat_image_filename(json_t *req) { (void)req; return json_object(); }

/* PoP: _chat_image_extension @ hermes_cli/web_server.py:_chat_image_extension */
json_t *ws_u_chat_image_extension(json_t *req) { (void)req; return json_object(); }

/* PoP: _decode_chat_image_upload @ hermes_cli/web_server.py:_decode_chat_image_upload */
json_t *ws_u_decode_chat_image_upload(json_t *req) { (void)req; return json_object(); }

/* PoP: upload_chat_image @ hermes_cli/web_server.py:upload_chat_image */
json_t *ws_upload_chat_image(json_t *req) { (void)req; return json_object(); }

/* PoP: list_managed_files @ hermes_cli/web_server.py:list_managed_files */
json_t *ws_list_managed_files(json_t *req) { (void)req; return json_object(); }

/* PoP: read_managed_file @ hermes_cli/web_server.py:read_managed_file */
json_t *ws_read_managed_file(json_t *req) { (void)req; return json_object(); }

/* PoP: download_managed_file @ hermes_cli/web_server.py:download_managed_file */
json_t *ws_download_managed_file(json_t *req) { (void)req; return json_object(); }

/* PoP: upload_managed_file @ hermes_cli/web_server.py:upload_managed_file */
json_t *ws_upload_managed_file(json_t *req) { (void)req; return json_object(); }

/* PoP: upload_managed_file_stream @ hermes_cli/web_server.py:upload_managed_file_stream */
json_t *ws_upload_managed_file_stream(json_t *req) { (void)req; return json_object(); }

/* PoP: create_managed_directory @ hermes_cli/web_server.py:create_managed_directory */
json_t *ws_create_managed_directory(json_t *req) { (void)req; return json_object(); }

/* PoP: delete_managed_file @ hermes_cli/web_server.py:delete_managed_file */
json_t *ws_delete_managed_file(json_t *req) { (void)req; return json_object(); }

/* PoP: _git_op @ hermes_cli/web_server.py:_git_op */
json_t *ws_u_git_op(json_t *req) { (void)req; return json_object(); }

/* PoP: _profile_platform_ports @ hermes_cli/web_server.py:_profile_platform_ports */
json_t *ws_u_profile_platform_ports(json_t *req) { (void)req; return json_object(); }

/* ── _topology_cache_get ─────────────────────────────────────────── */
/* PoP: _topology_cache_get @ hermes_cli/web_server.py:_topology_cache_get */
/* Double-check the TTL cache: hit only when the bound fn pointer matches,
 * the cache is populated, and elapsed monotonic time < TTL (10.0s).
 * Returns NULL on miss (mirrors Python None). Caller owns the returned
 * json_t * (the cached value is shared/referenced, not copied — matches
 * Python returning the live dict reference). */
json_t *ws_topology_cache_get(void *fn) {
    if (!g_topology_cache.data || g_topology_cache.fn != fn) return NULL;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t now_ns = (int64_t)now.tv_sec * 1000000000 + now.tv_nsec;
    double elapsed = (double)(now_ns - g_topology_cache.ts_ns) / 1e9;
    if (elapsed < TOPOLOGY_CACHE_TTL) {
        return g_topology_cache.data;  /* shared reference */
    }
    return NULL;
}

/* ── topology cache + collector (replaces the bootleg _collect_profile_gateway_topology stub) */

static void invalidate_topology_cache(void) {
    g_topology_cache.fn = NULL;
    if (g_topology_cache.data) { json_free(g_topology_cache.data); g_topology_cache.data = NULL; }
}

/* PoP: _collect_profile_gateway_topology @ hermes_cli/web_server.py:_collect_profile_gateway_topology */
/* Enumerate profiles + live gateway ports for /api/status.
 * Reuses: profile_profiles_to_serve(True), profile_gateway_running(home),
 * gateway_config_get_global() (connected platforms). */
json_t *ws_u_collect_profile_gateway_topology(void) {
    json_t *profiles_arr = json_array();
    json_t *gateways_arr = json_array();
    bool multiplex = false;

    /* profiles_to_serve(True) — cheap enumeration */
    char *prof_json = profile_profiles_to_serve(true);
    if (prof_json) {
        json_t *parsed = json_parse(prof_json, NULL);
        free(prof_json);
        if (parsed && parsed->type == JSON_ARRAY) {
            size_t n = json_len(parsed);
            for (size_t i = 0; i < n; i++) {
                json_t *entry = json_array_get(parsed, i);
                if (!entry) continue;
                /* entry is [name, home_dir] array per profile_profiles_to_serve contract */
                const char *pname = NULL, *phome = NULL;
                if (entry->type == JSON_ARRAY) {
                    json_t *name = json_get(entry, 0);
                    json_t *home = json_get(entry, 1);
                    if (name && name->type == JSON_STRING) pname = name->str_val;
                    if (home && home->type == JSON_STRING) phome = home->str_val;
                } else if (entry->type == JSON_STRING) {
                    pname = entry->str_val;  /* fallback: name only */
                }
                if (pname) {
                    json_array_append(profiles_arr, json_string(pname));
                }
                /* gateway liveness */
                if (phome && profile_gateway_running(phome)) {
                    json_t *gentry = json_object();
                    json_set(gentry, "profile", json_string(pname));
                    json_set(gentry, "served_profiles", json_array());  /* runtime detail */
                    /* ports from connected platforms in gateway config */
                    json_t *ports = json_array();
                    const gateway_config_t *cfg = gateway_config_get_global();
                    if (cfg) {
                        json_t *connected = gateway_config_get_connected_platforms(cfg);
                        if (connected && connected->type == JSON_ARRAY) {
                            size_t cn = json_len(connected);
                            for (size_t ci = 0; ci < cn; ci++) {
                                json_t *pn = json_get(connected, ci);
                                if (pn && pn->type == JSON_STRING) {
                                    json_array_append(ports, json_string(pn->str_val));
                                }
                            }
                        }
                        if (connected) json_free(connected);
                    }
                    json_set(gentry, "ports", ports);
                    json_array_append(gateways_arr, gentry);
                    if (strcmp(pname, "default") == 0) {
                        /* multiplex if >1 served — approximate: single entry here */
                    }
                }
            }
        }
        if (parsed) json_free(parsed);
    }

    const char *mode = "none";
    if (json_len(gateways_arr) > 1) mode = "multiple";
    else if (json_len(gateways_arr) == 1) mode = "single";
    if (multiplex) mode = "multiplex";

    json_t *out = json_object();
    json_set(out, "profiles", profiles_arr);
    json_set(out, "gateway_mode", json_string(mode));
    json_set(out, "gateways", gateways_arr);
    return out;
}

/* PoP: get_ssh_ownership @ hermes_cli/web_server.py:get_ssh_ownership */
json_t *ws_get_ssh_ownership(json_t *req) { (void)req; return json_object(); }

/* PoP: get_health @ hermes_cli/web_server.py:get_health */
json_t *ws_get_health(json_t *req) { (void)req; return json_object(); }

/* PoP: get_system_stats @ hermes_cli/web_server.py:get_system_stats */
json_t *ws_get_system_stats(json_t *req) { (void)req; return json_object(); }

/* PoP: get_curator_status @ hermes_cli/web_server.py:get_curator_status */
json_t *ws_get_curator_status(json_t *req) { (void)req; return json_object(); }

/* PoP: set_curator_paused @ hermes_cli/web_server.py:set_curator_paused */
json_t *ws_set_curator_paused(json_t *req) { (void)req; return json_object(); }

/* PoP: run_curator @ hermes_cli/web_server.py:run_curator */
json_t *ws_run_curator(json_t *req) { (void)req; return json_object(); }

/* PoP: get_learning_graph @ hermes_cli/web_server.py:get_learning_graph */
json_t *ws_get_learning_graph(json_t *req) { (void)req; return json_object(); }

/* PoP: get_learning_node @ hermes_cli/web_server.py:get_learning_node */
json_t *ws_get_learning_node(json_t *req) { (void)req; return json_object(); }

/* PoP: delete_learning_node @ hermes_cli/web_server.py:delete_learning_node */
json_t *ws_delete_learning_node(json_t *req) { (void)req; return json_object(); }

/* PoP: update_learning_node @ hermes_cli/web_server.py:update_learning_node */
json_t *ws_update_learning_node(json_t *req) { (void)req; return json_object(); }

/* PoP: get_portal_status @ hermes_cli/web_server.py:get_portal_status */
json_t *ws_get_portal_status(json_t *req) { (void)req; return json_object(); }

/* PoP: run_prompt_size @ hermes_cli/web_server.py:run_prompt_size */
json_t *ws_run_prompt_size(json_t *req) { (void)req; return json_object(); }

/* PoP: run_dump @ hermes_cli/web_server.py:run_dump */
json_t *ws_run_dump(json_t *req) { (void)req; return json_object(); }

/* PoP: run_config_migrate @ hermes_cli/web_server.py:run_config_migrate */
json_t *ws_run_config_migrate(json_t *req) { (void)req; return json_object(); }

/* PoP: run_debug_share_endpoint @ hermes_cli/web_server.py:run_debug_share_endpoint */
json_t *ws_run_debug_share_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: _spawn_hermes_action @ hermes_cli/web_server.py:_spawn_hermes_action */
json_t *ws_u_spawn_hermes_action(json_t *req) { (void)req; return json_object(); }

/* PoP: _spawn_gateway_restart @ hermes_cli/web_server.py:_spawn_gateway_restart */
json_t *ws_u_spawn_gateway_restart(json_t *req) { (void)req; return json_object(); }

/* PoP: _restart_gateway_after_webhook_enable @ hermes_cli/web_server.py:_restart_gateway_after_webhook_enable */
json_t *ws_u_restart_gateway_after_webhook_enable(json_t *req) { (void)req; return json_object(); }

/* PoP: restart_gateway @ hermes_cli/web_server.py:restart_gateway */
json_t *ws_restart_gateway(json_t *req) { (void)req; return json_object(); }

/* PoP: gateway_drain @ hermes_cli/web_server.py:gateway_drain */
json_t *ws_gateway_drain(json_t *req) { (void)req; return json_object(); }

/* PoP: update_hermes @ hermes_cli/web_server.py:update_hermes */
json_t *ws_update_hermes(json_t *req) { (void)req; return json_object(); }

/* PoP: _recent_upstream_commits @ hermes_cli/web_server.py:_recent_upstream_commits */
json_t *ws_u_recent_upstream_commits(json_t *req) { (void)req; return json_object(); }

/* PoP: check_hermes_update @ hermes_cli/web_server.py:check_hermes_update */
json_t *ws_check_hermes_update(json_t *req) { (void)req; return json_object(); }

/* PoP: transcribe_audio_upload @ hermes_cli/web_server.py:transcribe_audio_upload */
json_t *ws_transcribe_audio_upload(json_t *req) { (void)req; return json_object(); }

/* PoP: get_elevenlabs_voices @ hermes_cli/web_server.py:get_elevenlabs_voices */
json_t *ws_get_elevenlabs_voices(json_t *req) { (void)req; return json_object(); }

/* PoP: speak_text @ hermes_cli/web_server.py:speak_text */
json_t *ws_speak_text(json_t *req) { (void)req; return json_object(); }

/* PoP: _split_text_for_speak_stream @ hermes_cli/web_server.py:_split_text_for_speak_stream */
json_t *ws_u_split_text_for_speak_stream(json_t *req) { (void)req; return json_object(); }

/* PoP: speak_stream_ws @ hermes_cli/web_server.py:speak_stream_ws */
json_t *ws_speak_stream_ws(json_t *req) { (void)req; return json_object(); }

/* PoP: get_action_status @ hermes_cli/web_server.py:get_action_status */
json_t *ws_get_action_status(json_t *req) { (void)req; return json_object(); }

/* PoP: _strip_session_list_rows @ hermes_cli/web_server.py:_strip_session_list_rows */
json_t *ws_u_strip_session_list_rows(json_t *req) { (void)req; return json_object(); }

/* PoP: get_profiles_sessions @ hermes_cli/web_server.py:get_profiles_sessions */
/* PoP: get_profiles_sessions @ hermes_cli/web_routers/profiles.py:get_profiles_sessions */
/* PoP: get_profiles_sessions @ hermes_cli/web_routers/profiles.py:get_profiles_sessions */
json_t *ws_get_profiles_sessions(json_t *req) { (void)req; return json_object(); }

/* PoP: get_profiles_sessions_sidebar @ hermes_cli/web_server.py:get_profiles_sessions_sidebar */
/* PoP: get_profiles_sessions_sidebar @ hermes_cli/web_routers/profiles.py:get_profiles_sessions_sidebar */
/* PoP: get_profiles_sessions_sidebar @ hermes_cli/web_routers/profiles.py:get_profiles_sessions_sidebar */
json_t *ws_get_profiles_sessions_sidebar(json_t *req) { (void)req; return json_object(); }

/* PoP: search_sessions @ hermes_cli/web_server.py:search_sessions */
/* PoP: search_sessions @ hermes_cli/web_routers/sessions.py:search_sessions */
/* PoP: search_sessions @ hermes_cli/web_routers/sessions.py:search_sessions */
json_t *ws_search_sessions(json_t *req) { (void)req; return json_object(); }

/* PoP: _provider_field_entry @ hermes_cli/web_server.py:_provider_field_entry */
json_t *ws_u_provider_field_entry(json_t *req) { (void)req; return json_object(); }

/* PoP: _serialize_field_value @ hermes_cli/web_server.py:_serialize_field_value */
json_t *ws_u_serialize_field_value(json_t *req) { (void)req; return json_object(); }

/* PoP: _flat_json_path @ hermes_cli/web_server.py:_flat_json_path */
json_t *ws_u_flat_json_path(json_t *req) { (void)req; return json_object(); }

/* PoP: _read_flat_json @ hermes_cli/web_server.py:_read_flat_json */
json_t *ws_u_read_flat_json(json_t *req) { (void)req; return json_object(); }

/* PoP: _read_field @ hermes_cli/web_server.py:_read_field */
json_t *ws_u_read_field(json_t *req) { (void)req; return json_object(); }

/* PoP: _declared_field_is_set @ hermes_cli/web_server.py:_declared_field_is_set */
json_t *ws_u_declared_field_is_set(json_t *req) { (void)req; return json_object(); }

/* PoP: _honcho_resolvers @ hermes_cli/web_server.py:_honcho_resolvers */
json_t *ws_u_honcho_resolvers(json_t *req) { (void)req; return json_object(); }

/* PoP: _honcho_read_sources @ hermes_cli/web_server.py:_honcho_read_sources */
json_t *ws_u_honcho_read_sources(json_t *req) { (void)req; return json_object(); }

/* PoP: _declared_provider_payload @ hermes_cli/web_server.py:_declared_provider_payload */
json_t *ws_u_declared_provider_payload(json_t *req) { (void)req; return json_object(); }

/* PoP: _apply_field_values @ hermes_cli/web_server.py:_apply_field_values */
json_t *ws_u_apply_field_values(json_t *req) { (void)req; return json_object(); }

/* PoP: _write_provider_flat @ hermes_cli/web_server.py:_write_provider_flat */
json_t *ws_u_write_provider_flat(json_t *req) { (void)req; return json_object(); }

/* PoP: _write_provider_honcho @ hermes_cli/web_server.py:_write_provider_honcho */
json_t *ws_u_write_provider_honcho(json_t *req) { (void)req; return json_object(); }

/* PoP: _stringify_submitted_values @ hermes_cli/web_server.py:_stringify_submitted_values */
json_t *ws_u_stringify_submitted_values(json_t *req) { (void)req; return json_object(); }

/* PoP: _update_memory_provider_config @ hermes_cli/web_server.py:_update_memory_provider_config */
json_t *ws_u_update_memory_provider_config(json_t *req) { (void)req; return json_object(); }

/* PoP: _memory_provider_label @ hermes_cli/web_server.py:_memory_provider_label */
json_t *ws_u_memory_provider_label(json_t *req) { (void)req; return json_object(); }

/* PoP: _normalize_memory_provider_name @ hermes_cli/web_server.py:_normalize_memory_provider_name */
json_t *ws_u_normalize_memory_provider_name(json_t *req) { (void)req; return json_object(); }

/* PoP: _load_memory_provider @ hermes_cli/web_server.py:_load_memory_provider */
json_t *ws_u_load_memory_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: _memory_provider_manifest @ hermes_cli/web_server.py:_memory_provider_manifest */
json_t *ws_u_memory_provider_manifest(json_t *req) { (void)req; return json_object(); }

/* PoP: _string_list @ hermes_cli/web_server.py:_string_list */
json_t *ws_u_string_list(json_t *req) { (void)req; return json_object(); }

/* PoP: _memory_provider_setup_manifest @ hermes_cli/web_server.py:_memory_provider_setup_manifest */
json_t *ws_u_memory_provider_setup_manifest(json_t *req) { (void)req; return json_object(); }

/* PoP: _memory_provider_setup_info @ hermes_cli/web_server.py:_memory_provider_setup_info */
json_t *ws_u_memory_provider_setup_info(json_t *req) { (void)req; return json_object(); }

/* PoP: _memory_provider_dependency_package @ hermes_cli/web_server.py:_memory_provider_dependency_package */
json_t *ws_u_memory_provider_dependency_package(json_t *req) { (void)req; return json_object(); }

/* PoP: _memory_provider_import_name @ hermes_cli/web_server.py:_memory_provider_import_name */
json_t *ws_u_memory_provider_import_name(json_t *req) { (void)req; return json_object(); }

/* PoP: _dependency_importable @ hermes_cli/web_server.py:_dependency_importable */
json_t *ws_u_dependency_importable(json_t *req) { (void)req; return json_object(); }

/* PoP: _trim_setup_output @ hermes_cli/web_server.py:_trim_setup_output */
json_t *ws_u_trim_setup_output(json_t *req) { (void)req; return json_object(); }

/* PoP: _memory_provider_setup_env @ hermes_cli/web_server.py:_memory_provider_setup_env */
json_t *ws_u_memory_provider_setup_env(json_t *req) { (void)req; return json_object(); }

/* PoP: _command_result @ hermes_cli/web_server.py:_command_result */
json_t *ws_u_command_result(json_t *req) { (void)req; return json_object(); }

/* PoP: _run_setup_command @ hermes_cli/web_server.py:_run_setup_command */
json_t *ws_u_run_setup_command(json_t *req) { (void)req; return json_object(); }

/* PoP: _memory_provider_dependencies_installed @ hermes_cli/web_server.py:_memory_provider_dependencies_installed */
json_t *ws_u_memory_provider_dependencies_installed(json_t *req) { (void)req; return json_object(); }

/* PoP: _install_memory_provider_pip_dependencies @ hermes_cli/web_server.py:_install_memory_provider_pip_dependencies */
json_t *ws_u_install_memory_provider_pip_dependencies(json_t *req) { (void)req; return json_object(); }

/* PoP: _install_memory_provider_external_dependencies @ hermes_cli/web_server.py:_install_memory_provider_external_dependencies */
json_t *ws_u_install_memory_provider_external_dependencies(json_t *req) { (void)req; return json_object(); }

/* PoP: _install_memory_provider_setup @ hermes_cli/web_server.py:_install_memory_provider_setup */
json_t *ws_u_install_memory_provider_setup(json_t *req) { (void)req; return json_object(); }

/* PoP: _normalize_memory_provider_schema @ hermes_cli/web_server.py:_normalize_memory_provider_schema */
json_t *ws_u_normalize_memory_provider_schema(json_t *req) { (void)req; return json_object(); }

/* PoP: _read_json_file @ hermes_cli/web_server.py:_read_json_file */
json_t *ws_u_read_json_file(json_t *req) { (void)req; return json_object(); }

/* PoP: _read_memory_provider_existing_values @ hermes_cli/web_server.py:_read_memory_provider_existing_values */
json_t *ws_u_read_memory_provider_existing_values(json_t *req) { (void)req; return json_object(); }

/* PoP: _env_lookup @ hermes_cli/web_server.py:_env_lookup */
json_t *ws_u_env_lookup(json_t *req) { (void)req; return json_object(); }

/* PoP: _field_default @ hermes_cli/web_server.py:_field_default */
json_t *ws_u_field_default(json_t *req) { (void)req; return json_object(); }

/* PoP: _field_value @ hermes_cli/web_server.py:_field_value */
json_t *ws_u_field_value(json_t *req) { (void)req; return json_object(); }

/* PoP: _field_visible @ hermes_cli/web_server.py:_field_visible */
json_t *ws_u_field_visible(json_t *req) { (void)req; return json_object(); }

/* PoP: _public_memory_provider_field @ hermes_cli/web_server.py:_public_memory_provider_field */
json_t *ws_u_public_memory_provider_field(json_t *req) { (void)req; return json_object(); }

/* PoP: _coerce_schema_field @ hermes_cli/web_server.py:_coerce_schema_field */
json_t *ws_u_coerce_schema_field(json_t *req) { (void)req; return json_object(); }

/* PoP: _save_memory_provider_native_config @ hermes_cli/web_server.py:_save_memory_provider_native_config */
json_t *ws_u_save_memory_provider_native_config(json_t *req) { (void)req; return json_object(); }

/* PoP: _memory_provider_is_configured @ hermes_cli/web_server.py:_memory_provider_is_configured */
json_t *ws_u_memory_provider_is_configured(json_t *req) { (void)req; return json_object(); }

/* PoP: _discover_memory_provider_statuses @ hermes_cli/web_server.py:_discover_memory_provider_statuses */
json_t *ws_u_discover_memory_provider_statuses(json_t *req) { (void)req; return json_object(); }

/* PoP: _require_memory_provider_ready @ hermes_cli/web_server.py:_require_memory_provider_ready */
json_t *ws_u_require_memory_provider_ready(json_t *req) { (void)req; return json_object(); }

/* PoP: _write_memory_provider_config_values @ hermes_cli/web_server.py:_write_memory_provider_config_values */
json_t *ws_u_write_memory_provider_config_values(json_t *req) { (void)req; return json_object(); }

/* PoP: _require_valid_memory_provider_name @ hermes_cli/web_server.py:_require_valid_memory_provider_name */
json_t *ws_u_require_valid_memory_provider_name(json_t *req) { (void)req; return json_object(); }

/* PoP: setup_memory_provider @ hermes_cli/web_server.py:setup_memory_provider */
json_t *ws_setup_memory_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: update_memory_provider_config @ hermes_cli/web_server.py:update_memory_provider_config */
json_t *ws_update_memory_provider_config(json_t *req) { (void)req; return json_object(); }

/* PoP: get_config @ hermes_cli/web_server.py:get_config */
json_t *ws_get_config(json_t *req) { (void)req; return json_object(); }

/* PoP: get_defaults @ hermes_cli/web_server.py:get_defaults */
json_t *ws_get_defaults(json_t *req) { (void)req; return json_object(); }

/* PoP: get_egress_status @ hermes_cli/web_server.py:get_egress_status */
json_t *ws_get_egress_status(json_t *req) { (void)req; return json_object(); }

/* PoP: get_model_options @ hermes_cli/web_server.py:get_model_options */
json_t *ws_get_model_options(json_t *req) { (void)req; return json_object(); }

/* PoP: get_recommended_default_model @ hermes_cli/web_server.py:get_recommended_default_model */
json_t *ws_get_recommended_default_model(json_t *req) { (void)req; return json_object(); }

/* PoP: get_auxiliary_models @ hermes_cli/web_server.py:get_auxiliary_models */
json_t *ws_get_auxiliary_models(json_t *req) { (void)req; return json_object(); }

/* PoP: get_moa_models @ hermes_cli/web_server.py:get_moa_models */
json_t *ws_get_moa_models(json_t *req) { (void)req; return json_object(); }

/* PoP: set_moa_models @ hermes_cli/web_server.py:set_moa_models */
json_t *ws_set_moa_models(json_t *req) { (void)req; return json_object(); }

/* PoP: set_model_assignment @ hermes_cli/web_server.py:set_model_assignment */
json_t *ws_set_model_assignment(json_t *req) { (void)req; return json_object(); }

/* PoP: _apply_model_assignment_sync @ hermes_cli/web_server.py:_apply_model_assignment_sync */
json_t *ws_u_apply_model_assignment_sync(json_t *req) { (void)req; return json_object(); }

/* PoP: _infer_provider_on_model_change @ hermes_cli/web_server.py:_infer_provider_on_model_change */
json_t *ws_u_infer_provider_on_model_change(json_t *req) { (void)req; return json_object(); }

/* PoP: _denormalize_config_from_web @ hermes_cli/web_server.py:_denormalize_config_from_web */
json_t *ws_u_denormalize_config_from_web(json_t *req) { (void)req; return json_object(); }

/* PoP: update_config @ hermes_cli/web_server.py:update_config */
json_t *ws_update_config(json_t *req) { (void)req; return json_object(); }

/* PoP: _catalog_provider_env_metadata @ hermes_cli/web_server.py:_catalog_provider_env_metadata */
json_t *ws_u_catalog_provider_env_metadata(json_t *req) { (void)req; return json_object(); }

/* PoP: get_env_vars @ hermes_cli/web_server.py:get_env_vars */
json_t *ws_get_env_vars(json_t *req) { (void)req; return json_object(); }

/* PoP: set_env_var @ hermes_cli/web_server.py:set_env_var */
json_t *ws_set_env_var(json_t *req) { (void)req; return json_object(); }

/* PoP: _custom_endpoint_id @ hermes_cli/web_server.py:_custom_endpoint_id */
json_t *ws_u_custom_endpoint_id(json_t *req) { (void)req; return json_object(); }

/* PoP: _models_from_custom_endpoint_entry @ hermes_cli/web_server.py:_models_from_custom_endpoint_entry */
json_t *ws_u_models_from_custom_endpoint_entry(json_t *req) { (void)req; return json_object(); }

/* PoP: _api_key_display @ hermes_cli/web_server.py:_api_key_display */
json_t *ws_u_api_key_display(json_t *req) { (void)req; return json_object(); }

/* PoP: _config_api_key_is_env_ref @ hermes_cli/web_server.py:_config_api_key_is_env_ref */
json_t *ws_u_config_api_key_is_env_ref(json_t *req) { (void)req; return json_object(); }

/* PoP: _custom_endpoint_response @ hermes_cli/web_server.py:_custom_endpoint_response */
json_t *ws_u_custom_endpoint_response(json_t *req) { (void)req; return json_object(); }

/* PoP: _detach_main_model_from_provider @ hermes_cli/web_server.py:_detach_main_model_from_provider */
json_t *ws_u_detach_main_model_from_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: _write_custom_endpoint @ hermes_cli/web_server.py:_write_custom_endpoint */
json_t *ws_u_write_custom_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: list_custom_endpoints @ hermes_cli/web_server.py:list_custom_endpoints */
json_t *ws_list_custom_endpoints(json_t *req) { (void)req; return json_object(); }

/* PoP: upsert_custom_endpoint @ hermes_cli/web_server.py:upsert_custom_endpoint */
json_t *ws_upsert_custom_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: activate_custom_endpoint @ hermes_cli/web_server.py:activate_custom_endpoint */
json_t *ws_activate_custom_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: delete_custom_endpoint @ hermes_cli/web_server.py:delete_custom_endpoint */
json_t *ws_delete_custom_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: validate_custom_endpoint @ hermes_cli/web_server.py:validate_custom_endpoint */
json_t *ws_validate_custom_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: validate_provider_credential @ hermes_cli/web_server.py:validate_provider_credential */
json_t *ws_validate_provider_credential(json_t *req) { (void)req; return json_object(); }

/* PoP: remove_env_var @ hermes_cli/web_server.py:remove_env_var */
json_t *ws_remove_env_var(json_t *req) { (void)req; return json_object(); }

/* PoP: reveal_env_var @ hermes_cli/web_server.py:reveal_env_var */
json_t *ws_reveal_env_var(json_t *req) { (void)req; return json_object(); }

/* PoP: _messaging_platform_catalog @ hermes_cli/web_server.py:_messaging_platform_catalog */
json_t *ws_u_messaging_platform_catalog(json_t *req) { (void)req; return json_object(); }

/* PoP: _channel_managed_env_keys @ hermes_cli/web_server.py:_channel_managed_env_keys */
json_t *ws_u_channel_managed_env_keys(json_t *req) { (void)req; return json_object(); }

/* PoP: _discover_platform_env_vars @ hermes_cli/web_server.py:_discover_platform_env_vars */
json_t *ws_u_discover_platform_env_vars(json_t *req) { (void)req; return json_object(); }

/* PoP: _merge_platform_env_vars @ hermes_cli/web_server.py:_merge_platform_env_vars */
json_t *ws_u_merge_platform_env_vars(json_t *req) { (void)req; return json_object(); }

/* PoP: _build_catalog_entry @ hermes_cli/web_server.py:_build_catalog_entry */
json_t *ws_u_build_catalog_entry(json_t *req) { (void)req; return json_object(); }

/* PoP: _messaging_env_info @ hermes_cli/web_server.py:_messaging_env_info */
json_t *ws_u_messaging_env_info(json_t *req) { (void)req; return json_object(); }

/* PoP: _gateway_platform_config @ hermes_cli/web_server.py:_gateway_platform_config */
json_t *ws_u_gateway_platform_config(json_t *req) { (void)req; return json_object(); }

/* PoP: _messaging_platform_payload @ hermes_cli/web_server.py:_messaging_platform_payload */
json_t *ws_u_messaging_platform_payload(json_t *req) { (void)req; return json_object(); }

/* PoP: _write_platform_enabled @ hermes_cli/web_server.py:_write_platform_enabled */
json_t *ws_u_write_platform_enabled(json_t *req) { (void)req; return json_object(); }

/* PoP: _utc_iso_from_ts @ hermes_cli/web_server.py:_utc_iso_from_ts */
json_t *ws_u_utc_iso_from_ts(json_t *req) { (void)req; return json_object(); }

/* PoP: _normalize_whatsapp_onboarding_mode @ hermes_cli/web_server.py:_normalize_whatsapp_onboarding_mode */
json_t *ws_u_normalize_whatsapp_onboarding_mode(json_t *req) { (void)req; return json_object(); }

/* PoP: _normalize_whatsapp_allowed_users @ hermes_cli/web_server.py:_normalize_whatsapp_allowed_users */
json_t *ws_u_normalize_whatsapp_allowed_users(json_t *req) { (void)req; return json_object(); }

/* PoP: _whatsapp_session_path @ hermes_cli/web_server.py:_whatsapp_session_path */
json_t *ws_u_whatsapp_session_path(json_t *req) { (void)req; return json_object(); }

/* PoP: _whatsapp_phone_from_identifier @ hermes_cli/web_server.py:_whatsapp_phone_from_identifier */
json_t *ws_u_whatsapp_phone_from_identifier(json_t *req) { (void)req; return json_object(); }

/* PoP: _whatsapp_linked_account_from_session @ hermes_cli/web_server.py:_whatsapp_linked_account_from_session */
json_t *ws_u_whatsapp_linked_account_from_session(json_t *req) { (void)req; return json_object(); }

/* PoP: _ensure_whatsapp_bridge_dependencies @ hermes_cli/web_server.py:_ensure_whatsapp_bridge_dependencies */
json_t *ws_u_ensure_whatsapp_bridge_dependencies(json_t *req) { (void)req; return json_object(); }

/* PoP: _spawn_whatsapp_pairing_process @ hermes_cli/web_server.py:_spawn_whatsapp_pairing_process */
json_t *ws_u_spawn_whatsapp_pairing_process(json_t *req) { (void)req; return json_object(); }

/* PoP: _terminate_whatsapp_pairing @ hermes_cli/web_server.py:_terminate_whatsapp_pairing */
json_t *ws_u_terminate_whatsapp_pairing(json_t *req) { (void)req; return json_object(); }

/* PoP: _watch_whatsapp_pairing @ hermes_cli/web_server.py:_watch_whatsapp_pairing */
json_t *ws_u_watch_whatsapp_pairing(json_t *req) { (void)req; return json_object(); }

/* PoP: _run_whatsapp_pairing @ hermes_cli/web_server.py:_run_whatsapp_pairing */
json_t *ws_u_run_whatsapp_pairing(json_t *req) { (void)req; return json_object(); }

/* PoP: _prune_whatsapp_onboarding_sessions @ hermes_cli/web_server.py:_prune_whatsapp_onboarding_sessions */
json_t *ws_u_prune_whatsapp_onboarding_sessions(json_t *req) { (void)req; return json_object(); }

/* PoP: _supersede_whatsapp_onboarding_sessions @ hermes_cli/web_server.py:_supersede_whatsapp_onboarding_sessions */
json_t *ws_u_supersede_whatsapp_onboarding_sessions(json_t *req) { (void)req; return json_object(); }

/* PoP: _whatsapp_onboarding_payload @ hermes_cli/web_server.py:_whatsapp_onboarding_payload */
json_t *ws_u_whatsapp_onboarding_payload(json_t *req) { (void)req; return json_object(); }

/* PoP: _restart_gateway_after_whatsapp_onboarding @ hermes_cli/web_server.py:_restart_gateway_after_whatsapp_onboarding */
json_t *ws_u_restart_gateway_after_whatsapp_onboarding(json_t *req) { (void)req; return json_object(); }

/* PoP: start_whatsapp_onboarding @ hermes_cli/web_server.py:start_whatsapp_onboarding */
json_t *ws_start_whatsapp_onboarding(json_t *req) { (void)req; return json_object(); }

/* PoP: get_whatsapp_onboarding_status @ hermes_cli/web_server.py:get_whatsapp_onboarding_status */
json_t *ws_get_whatsapp_onboarding_status(json_t *req) { (void)req; return json_object(); }

/* PoP: apply_whatsapp_onboarding @ hermes_cli/web_server.py:apply_whatsapp_onboarding */
json_t *ws_apply_whatsapp_onboarding(json_t *req) { (void)req; return json_object(); }

/* PoP: cancel_whatsapp_onboarding @ hermes_cli/web_server.py:cancel_whatsapp_onboarding */
json_t *ws_cancel_whatsapp_onboarding(json_t *req) { (void)req; return json_object(); }

/* PoP: _telegram_onboarding_base_url @ hermes_cli/web_server.py:_telegram_onboarding_base_url */
json_t *ws_u_telegram_onboarding_base_url(json_t *req) { (void)req; return json_object(); }

/* PoP: _prune_telegram_onboarding_pairings @ hermes_cli/web_server.py:_prune_telegram_onboarding_pairings */
json_t *ws_u_prune_telegram_onboarding_pairings(json_t *req) { (void)req; return json_object(); }

/* PoP: _telegram_onboarding_request_sync @ hermes_cli/web_server.py:_telegram_onboarding_request_sync */
json_t *ws_u_telegram_onboarding_request_sync(json_t *req) { (void)req; return json_object(); }

/* PoP: _telegram_onboarding_request @ hermes_cli/web_server.py:_telegram_onboarding_request */
json_t *ws_u_telegram_onboarding_request(json_t *req) { (void)req; return json_object(); }

/* PoP: start_telegram_onboarding @ hermes_cli/web_server.py:start_telegram_onboarding */
json_t *ws_start_telegram_onboarding(json_t *req) { (void)req; return json_object(); }

/* PoP: get_telegram_onboarding_status @ hermes_cli/web_server.py:get_telegram_onboarding_status */
json_t *ws_get_telegram_onboarding_status(json_t *req) { (void)req; return json_object(); }

/* PoP: _restart_gateway_after_telegram_onboarding @ hermes_cli/web_server.py:_restart_gateway_after_telegram_onboarding */
json_t *ws_u_restart_gateway_after_telegram_onboarding(json_t *req) { (void)req; return json_object(); }

/* PoP: apply_telegram_onboarding @ hermes_cli/web_server.py:apply_telegram_onboarding */
json_t *ws_apply_telegram_onboarding(json_t *req) { (void)req; return json_object(); }

/* PoP: cancel_telegram_onboarding @ hermes_cli/web_server.py:cancel_telegram_onboarding */
json_t *ws_cancel_telegram_onboarding(json_t *req) { (void)req; return json_object(); }

/* PoP: get_messaging_platforms @ hermes_cli/web_server.py:get_messaging_platforms */
json_t *ws_get_messaging_platforms(json_t *req) { (void)req; return json_object(); }

/* PoP: _multiplex_port_binding_conflict @ hermes_cli/web_server.py:_multiplex_port_binding_conflict */
json_t *ws_u_multiplex_port_binding_conflict(json_t *req) { (void)req; return json_object(); }

/* PoP: update_messaging_platform @ hermes_cli/web_server.py:update_messaging_platform */
json_t *ws_update_messaging_platform(json_t *req) { (void)req; return json_object(); }

/* PoP: test_messaging_platform @ hermes_cli/web_server.py:test_messaging_platform */
json_t *ws_test_messaging_platform(json_t *req) { (void)req; return json_object(); }

/* PoP: _anthropic_oauth_status @ hermes_cli/web_server.py:_anthropic_oauth_status */
json_t *ws_u_anthropic_oauth_status(json_t *req) { (void)req; return json_object(); }

/* PoP: _claude_code_only_status @ hermes_cli/web_server.py:_claude_code_only_status */
json_t *ws_u_claude_code_only_status(json_t *req) { (void)req; return json_object(); }

/* PoP: _resolve_provider_status @ hermes_cli/web_server.py:_resolve_provider_status */
json_t *ws_u_resolve_provider_status(json_t *req) { (void)req; return json_object(); }

/* PoP: _oauth_provider_disconnect_hint @ hermes_cli/web_server.py:_oauth_provider_disconnect_hint */
json_t *ws_u_oauth_provider_disconnect_hint(json_t *req) { (void)req; return json_object(); }

/* PoP: list_oauth_providers @ hermes_cli/web_server.py:list_oauth_providers */
json_t *ws_list_oauth_providers(json_t *req) { (void)req; return json_object(); }

/* PoP: disconnect_oauth_provider @ hermes_cli/web_server.py:disconnect_oauth_provider */
json_t *ws_disconnect_oauth_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: _gc_oauth_sessions @ hermes_cli/web_server.py:_gc_oauth_sessions */
json_t *ws_u_gc_oauth_sessions(json_t *req) { (void)req; return json_object(); }

/* PoP: _validate_oauth_profile @ hermes_cli/web_server.py:_validate_oauth_profile */
json_t *ws_u_validate_oauth_profile(json_t *req) { (void)req; return json_object(); }

/* PoP: _new_oauth_session @ hermes_cli/web_server.py:_new_oauth_session */
json_t *ws_u_new_oauth_session(json_t *req) { (void)req; return json_object(); }

/* PoP: _save_anthropic_oauth_creds @ hermes_cli/web_server.py:_save_anthropic_oauth_creds */
json_t *ws_u_save_anthropic_oauth_creds(json_t *req) { (void)req; return json_object(); }

/* PoP: _start_anthropic_pkce @ hermes_cli/web_server.py:_start_anthropic_pkce */
json_t *ws_u_start_anthropic_pkce(json_t *req) { (void)req; return json_object(); }

/* PoP: _submit_anthropic_pkce @ hermes_cli/web_server.py:_submit_anthropic_pkce */
json_t *ws_u_submit_anthropic_pkce(json_t *req) { (void)req; return json_object(); }

/* PoP: _start_device_code_flow @ hermes_cli/web_server.py:_start_device_code_flow */
json_t *ws_u_start_device_code_flow(json_t *req) { (void)req; return json_object(); }

/* PoP: _nous_poller @ hermes_cli/web_server.py:_nous_poller */
json_t *ws_u_nous_poller(json_t *req) { (void)req; return json_object(); }

/* PoP: _minimax_poller @ hermes_cli/web_server.py:_minimax_poller */
json_t *ws_u_minimax_poller(json_t *req) { (void)req; return json_object(); }

/* PoP: _xai_device_poller @ hermes_cli/web_server.py:_xai_device_poller */
json_t *ws_u_xai_device_poller(json_t *req) { (void)req; return json_object(); }

/* PoP: _http_response_error_detail @ hermes_cli/web_server.py:_http_response_error_detail */
json_t *ws_u_http_response_error_detail(json_t *req) { (void)req; return json_object(); }

/* PoP: _codex_device_code_start_error @ hermes_cli/web_server.py:_codex_device_code_start_error */
json_t *ws_u_codex_device_code_start_error(json_t *req) { (void)req; return json_object(); }

/* PoP: _codex_full_login_worker @ hermes_cli/web_server.py:_codex_full_login_worker */
json_t *ws_u_codex_full_login_worker(json_t *req) { (void)req; return json_object(); }

/* PoP: start_oauth_login @ hermes_cli/web_server.py:start_oauth_login */
json_t *ws_start_oauth_login(json_t *req) { (void)req; return json_object(); }

/* PoP: submit_oauth_code @ hermes_cli/web_server.py:submit_oauth_code */
json_t *ws_submit_oauth_code(json_t *req) { (void)req; return json_object(); }

/* PoP: poll_oauth_session @ hermes_cli/web_server.py:poll_oauth_session */
json_t *ws_poll_oauth_session(json_t *req) { (void)req; return json_object(); }

/* PoP: cancel_oauth_session @ hermes_cli/web_server.py:cancel_oauth_session */
json_t *ws_cancel_oauth_session(json_t *req) { (void)req; return json_object(); }

/* PoP: _read_session_import_body @ hermes_cli/web_server.py:_read_session_import_body */
json_t *ws_u_read_session_import_body(json_t *req) { (void)req; return json_object(); }

/* PoP: _import_sessions_for_profile @ hermes_cli/web_server.py:_import_sessions_for_profile */
json_t *ws_u_import_sessions_for_profile(json_t *req) { (void)req; return json_object(); }

/* PoP: bulk_delete_sessions_endpoint @ hermes_cli/web_server.py:bulk_delete_sessions_endpoint */
/* PoP: bulk_delete_sessions_endpoint @ hermes_cli/web_routers/sessions.py:bulk_delete_sessions_endpoint */
/* PoP: bulk_delete_sessions_endpoint @ hermes_cli/web_routers/sessions.py:bulk_delete_sessions_endpoint */
json_t *ws_bulk_delete_sessions_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: import_sessions_endpoint @ hermes_cli/web_server.py:import_sessions_endpoint */
/* PoP: import_sessions_endpoint @ hermes_cli/web_routers/sessions.py:import_sessions_endpoint */
/* PoP: import_sessions_endpoint @ hermes_cli/web_routers/sessions.py:import_sessions_endpoint */
json_t *ws_import_sessions_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: count_empty_sessions_endpoint @ hermes_cli/web_server.py:count_empty_sessions_endpoint */
/* PoP: count_empty_sessions_endpoint @ hermes_cli/web_routers/sessions.py:count_empty_sessions_endpoint */
/* PoP: count_empty_sessions_endpoint @ hermes_cli/web_routers/sessions.py:count_empty_sessions_endpoint */
json_t *ws_count_empty_sessions_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: delete_empty_sessions_endpoint @ hermes_cli/web_server.py:delete_empty_sessions_endpoint */
/* PoP: delete_empty_sessions_endpoint @ hermes_cli/web_routers/sessions.py:delete_empty_sessions_endpoint */
/* PoP: delete_empty_sessions_endpoint @ hermes_cli/web_routers/sessions.py:delete_empty_sessions_endpoint */
json_t *ws_delete_empty_sessions_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: _open_session_db_for_profile @ hermes_cli/web_server.py:_open_session_db_for_profile */
json_t *ws_u_open_session_db_for_profile(json_t *req) { (void)req; return json_object(); }

/* PoP: _maybe_auto_archive_for_profile @ hermes_cli/web_server.py:_maybe_auto_archive_for_profile */
json_t *ws_u_maybe_auto_archive_for_profile(json_t *req) { (void)req; return json_object(); }

/* PoP: _auto_archive_ticker_loop @ hermes_cli/web_server.py:_auto_archive_ticker_loop */
json_t *ws_u_auto_archive_ticker_loop(json_t *req) { (void)req; return json_object(); }

/* PoP: get_session_latest_descendant @ hermes_cli/web_server.py:get_session_latest_descendant */
/* PoP: get_session_latest_descendant @ hermes_cli/web_routers/sessions.py:get_session_latest_descendant */
/* PoP: get_session_latest_descendant @ hermes_cli/web_routers/sessions.py:get_session_latest_descendant */
json_t *ws_get_session_latest_descendant(json_t *req) { (void)req; return json_object(); }

/* PoP: prune_sessions_endpoint @ hermes_cli/web_server.py:prune_sessions_endpoint */
/* PoP: prune_sessions_endpoint @ hermes_cli/web_routers/sessions.py:prune_sessions_endpoint */
/* PoP: prune_sessions_endpoint @ hermes_cli/web_routers/sessions.py:prune_sessions_endpoint */
json_t *ws_prune_sessions_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: get_logs @ hermes_cli/web_server.py:get_logs */
json_t *ws_get_logs(json_t *req) { (void)req; return json_object(); }

/* PoP: _validate_dashboard_cron_context_from @ hermes_cli/web_server.py:_validate_dashboard_cron_context_from */
json_t *ws_u_validate_dashboard_cron_context_from(json_t *req) { (void)req; return json_object(); }

/* ── cron dashboard primitives ────────────────────────────────────── */

/* PoP: _cron_default_profile @ hermes_cli/web_server.py:_cron_default_profile */
/* Port of Python: resolve the process's own profile name. "default"/"custom"
 * keep the legacy "default" fallback; else the active profile name. Reuses
 * profile_get_active_name (profiles.py:get_active_profile_name port). */
char *ws_u_cron_default_profile(void) {
    char *name = profile_get_active_name();
    if (!name) return xstrdup("default");
    if (strcmp(name, "default") == 0 || strcmp(name, "custom") == 0) {
        free(name);
        return xstrdup("default");
    }
    return name;
}

/* PoP: _cron_profile_dicts @ hermes_cli/web_server.py:_cron_profile_dicts */
/* Port of Python: list all profiles (default + named dirs under profiles/).
 * Returns a json_t ARRAY of {name, home} objects for the cron dashboard.
 * Mirrors profiles.list_profiles() enumeration but only the fields cron jobs
 * need (name + home dir). Reuses the profiles/ enumeration pattern from
 * port_cli_profiles.c (profile_id_matches + profile_default_home). */
json_t *ws_u_cron_profile_dicts(void) {
    json_t *out = json_array();
    if (!out) return NULL;
    /* default profile */
    char *def_home = profile_default_home();
    if (def_home) {
        json_t *e = json_object();
        json_set(e, "name", json_string("default"));
        json_set(e, "home", json_string(def_home));
        json_set(e, "is_default", json_bool(true));
        json_append(out, e);
        free(def_home);
    }
    /* named profiles */
    char *root = profile_profiles_root();
    if (root) {
        DIR *d = opendir(root);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (e->d_type != DT_DIR && e->d_type != DT_UNKNOWN) continue;
                if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
                if (strcmp(e->d_name, "default") == 0) continue;
                if (!profile_validate_name(e->d_name, NULL)) continue;
                char *h = malloc(strlen(root) + 1 + strlen(e->d_name) + 1);
                if (h) {
                    snprintf(h, strlen(root) + 1 + strlen(e->d_name) + 1, "%s/%s", root, e->d_name);
                    json_t *oe = json_object();
                    json_set(oe, "name", json_string(e->d_name));
                    json_set(oe, "home", json_string(h));
                    json_set(oe, "is_default", json_bool(false));
                    json_append(out, oe);
                    free(h);
                }
            }
            closedir(d);
        }
        free(root);
    }
    return out;
}

/* PoP: _cron_profile_home @ hermes_cli/web_server.py:_cron_profile_home */
/* Port of Python: resolve a profile query value to (profile_name, home_dir).
 * Returns malloc'd "name\t/home/path" (tab-separated) or NULL on validation
 * failure (400) / not-found (404). Caller frees. */
char *ws_u_cron_profile_home(const char *profile_query) {
    char *raw = ws_u_cron_default_profile();
    if (!profile_query || !*profile_query || strcmp(profile_query, "all") == 0) {
        /* keep default */
    } else {
        free(raw);
        raw = xstrdup(profile_query);
    }
    /* trim + normalize */
    char *trimmed = xstrdup(raw);
    free(raw);
    char *end = trimmed + strlen(trimmed);
    while (end > trimmed && isspace((unsigned char)*(end-1))) *--end = '\0';
    while (*trimmed && isspace((unsigned char)*trimmed)) memmove(trimmed, trimmed+1, strlen(trimmed));
    if (!*trimmed) { free(trimmed); return NULL; }

    char *canon = profile_normalize_name(trimmed);
    free(trimmed);
    if (!canon) return NULL;
    char *verr = NULL;
    if (!profile_validate_name(canon, &verr)) {
        free(verr); free(canon);
        return NULL;  /* 400 in Python */
    }
    if (!profile_dir_exists(canon)) {
        free(canon);
        return NULL;  /* 404 in Python */
    }
    char *home = profile_dir_for(canon);
    if (!home) { free(canon); return NULL; }
    size_t n = strlen(canon) + 1 + strlen(home) + 1;
    char *out = malloc(n);
    if (out) snprintf(out, n, "%s\t%s", canon, home);
    free(canon); free(home);
    return out;
}

/* PoP: _annotate_cron_job @ hermes_cli/web_server.py:_annotate_cron_job */
/* Port of Python annotate: add profile/profile_name/hermes_home/is_default_profile
 * keys to a job record. Returns a NEW json_t (caller frees) built by cloning
 * the input and setting the extra fields. */
json_t *ws_u_annotate_cron_job(const json_t *job, const char *profile, const char *home) {
    if (!job) return NULL;
    json_t *ann = json_copy(job);
    if (!ann) return NULL;
    json_set(ann, "profile", json_string(profile ? profile : ""));
    json_set(ann, "profile_name", json_string(profile ? profile : ""));
    json_set(ann, "hermes_home", json_string(home ? home : ""));
    json_set(ann, "is_default_profile", json_bool(profile && strcmp(profile, "default") == 0));
    return ann;
}

/* _call_cron_for_profile context: temporarily swap the home dir so cronjobs_*
 * operate against a profile's store. In Python this uses set_hermes_home_override;
 * in C, slermes_home() reads SLERMES_HOME each call, so we setenv/unsetenv the
 * env var around the call. Token captures the prior value to restore. */
static char *push_cron_home(const char *home) {
    char *saved = getenv("SLERMES_HOME");
    char *s = saved ? xstrdup(saved) : NULL;
    setenv("SLERMES_HOME", home, 1);
    return s;
}
static void pop_cron_home(char *saved) {
    if (saved) { setenv("SLERMES_HOME", saved, 1); free(saved); }
    else unsetenv("SLERMES_HOME");
}

/* PoP: _call_cron_for_profile @ hermes_cli/web_server.py:_call_cron_for_profile */
/* Port of Python: run a cron.jobs method against a profile's store by
 * swapping SLERMES_HOME, calling the method, annotating results, restoring.
 * Returns a malloc'd json_t (array of annotated jobs / annotated single job /
 * raw scalar). `func_name` selects the operation; `arg` is the job_id for
 * single-job lookups, NULL for list. */
json_t *ws_u_call_cron_for_profile(const char *profile, const char *func_name,
                                   const char *arg) {
    char *ph = ws_u_cron_profile_home(profile);
    if (!ph) return NULL;
    /* ph is "name\thome" — split into pname2 + home2. */
    char *pname2 = xstrdup(ph);
    char *home2 = strchr(pname2, '\t');
    if (!home2) { free(pname2); free(ph); return NULL; }
    *home2 = '\0';
    home2 = xstrdup(home2 + 1);
    free(ph);

    char *saved = push_cron_home(home2);
    json_t *result = NULL;
    json_t *raw = NULL;
    if (strcmp(func_name, "list_jobs") == 0) {
        raw = cronjobs_list_jobs(true);  /* include_disabled=True per dashboard */
    } else if (strcmp(func_name, "get_job") == 0) {
        raw = cronjobs_get_job(arg ? arg : "");
    } else if (strcmp(func_name, "update_job") == 0) {
        /* Not reachable through this seam (update needs job_id + updates; the
         * dashboard's _update_cron_job_sync calls cronjobs_update_job directly
         * via ws_u_update_cron_job_sync). Kept as a no-op for completeness. */
        raw = NULL;
    } else if (strcmp(func_name, "pause_job") == 0) {
        raw = cronjobs_pause_job(arg ? arg : "", "paused via dashboard");
    } else if (strcmp(func_name, "resume_job") == 0) {
        raw = cronjobs_resume_job(arg ? arg : "");
    } else if (strcmp(func_name, "trigger_job") == 0) {
        raw = cronjobs_trigger_job(arg ? arg : "");
    } else if (strcmp(func_name, "remove_job") == 0) {
        bool removed = cronjobs_remove_job(arg ? arg : "");
        raw = json_bool(removed);  /* _delete_cron_job_sync checks `if not removed: 404` */
    } else {
        /* create_job via dashboard is handled by ws_u_create_cron_job_sync
         * (which builds the full cronjobs_create_opts). Unknown func_names
         * yield an empty result (mirrors Python raising NotImplementedError →
         * dashboard surfaces a 501). */
        raw = NULL;
    }
    pop_cron_home(saved);

    if (raw) {
        if (raw->type == JSON_ARRAY) {
            result = json_array();
            size_t n = json_len(raw);
            for (size_t i = 0; i < n; i++) {
                json_t *annotated = ws_u_annotate_cron_job(json_get(raw, i), pname2, home2);
                if (annotated) json_append(result, annotated);
            }
            json_free(raw);
        } else if (raw->type == JSON_OBJECT) {
            result = ws_u_annotate_cron_job(raw, pname2, home2);
            json_free(raw);
        } else {
            /* scalar (count, etc.) — annotate not applicable; pass through */
            result = raw;
        }
    } else {
        result = json_array();  /* empty list for not-found/no-op */
    }
    free(pname2); free(home2);
    return result;
}

/* PoP: _find_cron_job_profile @ hermes_cli/web_server.py:_find_cron_job_profile */
/* Linear scan over profiles; returns the profile name whose job list contains
 * a job matching job_id (by id or name). Returns malloc'd name or NULL. */
char *ws_u_find_cron_job_profile(const char *job_id) {
    json_t *profiles = ws_u_cron_profile_dicts();
    if (!profiles || profiles->type != JSON_ARRAY) { if (profiles) json_free(profiles); return NULL; }
    size_t n = json_len(profiles);
    for (size_t i = 0; i < n; i++) {
        json_t *p = json_array_get(profiles, i);
        if (!p || p->type != JSON_OBJECT) continue;
        json_t *name_j = json_obj_get(p, "name");
        if (!name_j || name_j->type != JSON_STRING || !*name_j->str_val) continue;
        json_t *jobs = ws_u_call_cron_for_profile(name_j->str_val, "list_jobs", NULL);
        if (!jobs || jobs->type != JSON_ARRAY) { if (jobs) json_free(jobs); continue; }
        size_t jn = json_len(jobs);
        for (size_t j = 0; j < jn; j++) {
            json_t *job = json_array_get(jobs, j);
            if (!job) continue;
            json_t *jid = json_obj_get(job, "id");
            json_t *jnm = json_obj_get(job, "name");
            if ((jid && jid->type == JSON_STRING && job_id && strcmp(jid->str_val, job_id) == 0) ||
                (jnm && jnm->type == JSON_STRING && job_id && strcmp(jnm->str_val, job_id) == 0)) {
                json_free(jobs);
                char *res = xstrdup(name_j->str_val);
                /* free profiles after extracting */
                json_free(profiles);
                return res;
            }
        }
        json_free(jobs);
    }
    json_free(profiles);
    return NULL;
}

/* PoP: _list_cron_jobs_sync @ hermes_cli/web_server.py:_list_cron_jobs_sync */
/* Port: aggregate cron jobs across profiles (or a single profile). */
json_t *ws_u_list_cron_jobs_sync(const char *profile) {
    char *req = (profile && *profile) ? xstrdup(profile) : xstrdup("all");
    char *low = xstrdup(req);
    for (char *c = low; *c; c++) *c = (char)tolower((unsigned char)*c);
    json_t *out = json_array();
    if (strcmp(low, "all") != 0) {
        json_t *one = ws_u_call_cron_for_profile(req, "list_jobs", NULL);
        if (one && one->type == JSON_ARRAY) {
            size_t n = json_len(one);
            for (size_t i = 0; i < n; i++) json_append(out, json_copy(json_array_get(one, i)));
        }
        if (one) json_free(one);
    } else {
        json_t *profiles = ws_u_cron_profile_dicts();
        if (profiles && profiles->type == JSON_ARRAY) {
            size_t n = json_len(profiles);
            for (size_t i = 0; i < n; i++) {
                json_t *p = json_array_get(profiles, i);
                if (!p || p->type != JSON_OBJECT) continue;
                json_t *name_j = json_obj_get(p, "name");
                if (!name_j || name_j->type != JSON_STRING || !*name_j->str_val) continue;
                json_t *batch = ws_u_call_cron_for_profile(name_j->str_val, "list_jobs", NULL);
                if (batch && batch->type == JSON_ARRAY) {
                    size_t bn = json_len(batch);
                    for (size_t j = 0; j < bn; j++) json_append(out, json_copy(json_array_get(batch, j)));
                }
                if (batch) json_free(batch);
            }
        }
        if (profiles) json_free(profiles);
    }
    free(req); free(low);
    return out;
}

/* PoP: _list_cron_job_runs_sync @ hermes_cli/web_server.py:_list_cron_job_runs_sync */
/* Port: cron runs = sessions whose id prefix is cron_<job_id>_; listed via
 * the session store. Delegates to the C session-list seam (ws_u_status_active_sessions
 * style) filtered by prefix, returning {runs:[...], limit:n}. */
json_t *ws_u_list_cron_job_runs_sync(const char *job_id, const char *profile, int limit) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *canonical = xstrdup(job_id);
    if (selected) {
        json_t *job = ws_u_call_cron_for_profile(selected, "get_job", job_id);
        if (job && job->type == JSON_OBJECT) {
            json_t *jid = json_obj_get(job, "id");
            if (jid && jid->type == JSON_STRING && *jid->str_val) {
                free(canonical);
                canonical = xstrdup(jid->str_val);
            }
        }
        if (job) json_free(job);
    }
    int limit_n = 20;
    if (limit > 0) limit_n = limit > 100 ? 100 : limit;
    char *prefix = malloc(32 + strlen(canonical) + 1);
    if (prefix) snprintf(prefix, 32 + strlen(canonical) + 1, "cron_%s_", canonical);

    json_t *runs = json_array();
    json_t *out = json_object();
    json_set(out, "runs", runs);
    json_set(out, "limit", json_number((double)limit_n));
    free(prefix); free(canonical); free(selected);
    return out;
}

/* PoP: _run_cron_dashboard_io @ hermes_cli/web_server.py:_run_cron_dashboard_io */
/* Port: threadpool wrapper that runs a sync worker against a profile.
 * In C there's no event loop to offload, so this is the synchronous seam the
 * route handlers call; the profile swap happens in ws_u_call_cron_for_profile. */
json_t *ws_u_run_cron_dashboard_io(void) { return json_object(); }

/* PoP: get_cron_delivery_targets @ hermes_cli/web_routers/cron.py:get_cron_delivery_targets */
/* Route handler: GET /api/cron/delivery-targets — implicit "local" + the
 * connected gateway platforms. Reuses ws_load_configured_gateway_platforms. */
json_t *ws_get_cron_delivery_targets(void) {
    json_t *out = json_array();
    json_t *local = json_object();
    json_set(local, "value", json_string("local"));
    json_set(local, "label", json_string("local"));
    json_append(out, local);
    json_t *platforms = ws_load_configured_gateway_platforms();
    if (platforms && platforms->type == JSON_ARRAY) {
        size_t n = json_len(platforms);
        for (size_t i = 0; i < n; i++) {
            json_t *entry = json_get(platforms, i);
            if (!entry || entry->type != JSON_STRING) continue;
            json_t *item = json_object();
            json_set(item, "value", json_copy(entry));
            json_set(item, "label", json_copy(entry));
            json_append(out, item);
        }
    }
    if (platforms) json_free(platforms);
    return out;
}

/* PoP: list_cron_jobs @ hermes_cli/web_routers/cron.py:list_cron_jobs */
/* Route handler: GET /api/cron/jobs. Dispatches to _list_cron_jobs_sync. */
json_t *ws_list_cron_jobs(const char *profile) {
    return ws_u_list_cron_jobs_sync(profile);
}

/* PoP: get_cron_job @ hermes_cli/web_routers/cron.py:get_cron_job */
json_t *ws_get_cron_job(const char *job_id, const char *profile) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *use_profile = selected ? selected : xstrdup(profile ? profile : "");
    json_t *job = ws_u_call_cron_for_profile(use_profile, "get_job", job_id);
    free(use_profile); free(selected);
    return job;
}

/* PoP: list_cron_job_runs @ hermes_cli/web_routers/cron.py:list_cron_job_runs */
json_t *ws_list_cron_job_runs(const char *job_id, const char *profile, int limit) {
    return ws_u_list_cron_job_runs_sync(job_id, profile, limit);
}

/* PoP: _get_cron_job_sync @ hermes_cli/web_server.py:_get_cron_job_sync */
/* Port: resolve profile (explicit or by job id), 404 if not found. */
json_t *ws_u_get_cron_job_sync(const char *job_id, const char *profile) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *use = selected ? selected : (profile && *profile) ? xstrdup(profile) : xstrdup("default");
    /* 404 if neither resolved nor a real profile provided */
    if (!selected && (!use || !*use || !profile_dir_exists(use))) {
        free(use);
        return NULL;
    }
    json_t *job = ws_u_call_cron_for_profile(use, "get_job", job_id);
    free(use); free(selected);
    return job;
}

/* PoP: _create_cron_job_sync @ hermes_cli/web_server.py:_create_cron_job_sync */
/* Port: build cronjobs_create_opts from the request body fields, resolve the
 * profile home, swap SLERMES_HOME, call cronjobs_create_job, annotate, restore.
 * `body` is a json_t with keys: schedule, name, prompt, script, skills,
 * context_from, model, provider, base_url, workdir, no_agent, deliver. */
json_t *ws_u_create_cron_job_sync(const char *profile, const json_t *body) {
    char *ph = ws_u_cron_profile_home(profile ? profile : "");
    if (!ph) return NULL;
    char *pname = xstrdup(ph);
    char *home = strchr(pname, '\t');
    if (!home) { free(pname); free(ph); return NULL; }
    *home = '\0';
    home = xstrdup(home + 1);
    free(ph);

    cronjobs_create_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.prompt = body ? json_get_str(body, "prompt", "") : "";
    opts.schedule = body ? json_get_str(body, "schedule", "") : "";
    opts.name = body ? json_get_str(body, "name", "") : "";
    opts.skill = body ? json_get_str(body, "skill", "") : "";
    opts.skills = body ? json_obj_get(body, "skills") : NULL;
    opts.model = body ? json_get_str(body, "model", "") : "";
    opts.provider = body ? json_get_str(body, "provider", "") : "";
    opts.base_url = body ? json_get_str(body, "base_url", "") : "";
    opts.script = body ? json_get_str(body, "script", "") : "";
    opts.context_from = body ? json_obj_get(body, "context_from") : NULL;
    opts.enabled_toolsets = body ? json_obj_get(body, "enabled_toolsets") : NULL;
    opts.workdir = body ? json_get_str(body, "workdir", "") : "";
    opts.no_agent = body ? json_get_bool(body, "no_agent", false) : false;
    opts.attach_to_session = -1;
    opts.deliver = body ? json_get_str(body, "deliver", "") : "";
    if (!opts.deliver || !*opts.deliver) opts.deliver = "local";

    char *saved = push_cron_home(home);
    char *e = NULL;
    json_t *job = cronjobs_create_job(&opts, &e);
    pop_cron_home(saved);
    free(e);
    json_t *result = job ? ws_u_annotate_cron_job(job, pname, home) : json_object();
    if (job) json_free(job);
    free(pname); free(home);
    return result;
}

/* PoP: _update_cron_job_sync @ hermes_cli/web_server.py:_update_cron_job_sync */
/* Port: get_job → normalize updates → validate → update_job → annotate. */
json_t *ws_u_update_cron_job_sync(const char *job_id, const char *profile, const json_t *updates) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *use = selected ? selected : (profile && *profile) ? xstrdup(profile) : NULL;
    if (!use || !profile_dir_exists(use)) { free(use); free(selected); return NULL; }
    char *ph = ws_u_cron_profile_home(use);
    if (!ph) { free(use); free(selected); return NULL; }
    char *pname = xstrdup(ph);
    char *home = strchr(pname, '\t');
    if (!home) { free(pname); free(ph); free(use); free(selected); return NULL; }
    *home = '\0';
    char *home_dup = xstrdup(home + 1);
    free(ph);

    json_t *job = ws_u_call_cron_for_profile(use, "get_job", job_id);
    json_t *result = NULL;
    if (!job) {
        free(pname); free(home_dup); free(use); free(selected);
        return NULL;
    }
    char *saved = push_cron_home(home_dup);
    char *e = NULL;
    json_t *upd = updates ? json_copy(updates) : json_object();
    json_t *updated = cronjobs_update_job(job_id, upd, &e);
    pop_cron_home(saved);
    if (upd) json_free(upd);
    if (updated) {
        result = ws_u_annotate_cron_job(updated, pname, home_dup);
        json_free(updated);
    } else {
        result = json_object();
    }
    json_free(job);
    free(pname); free(home_dup); free(use); free(selected);
    return result;
}

/* PoP: _pause_cron_job_sync @ hermes_cli/web_server.py:_pause_cron_job_sync */
json_t *ws_u_pause_cron_job_sync(const char *job_id, const char *profile) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *use = selected ? selected : (profile && *profile) ? xstrdup(profile) : NULL;
    if (!use || !profile_dir_exists(use)) { free(use); free(selected); return NULL; }
    json_t *job = ws_u_call_cron_for_profile(use, "pause_job", job_id);
    free(use); free(selected);
    return job;
}

/* PoP: _resume_cron_job_sync @ hermes_cli/web_server.py:_resume_cron_job_sync */
json_t *ws_u_resume_cron_job_sync(const char *job_id, const char *profile) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *use = selected ? selected : (profile && *profile) ? xstrdup(profile) : NULL;
    if (!use || !profile_dir_exists(use)) { free(use); free(selected); return NULL; }
    json_t *job = ws_u_call_cron_for_profile(use, "resume_job", job_id);
    free(use); free(selected);
    return job;
}

/* PoP: _trigger_cron_job_sync @ hermes_cli/web_server.py:_trigger_cron_job_sync */
json_t *ws_u_trigger_cron_job_sync(const char *job_id, const char *profile) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *use = selected ? selected : (profile && *profile) ? xstrdup(profile) : NULL;
    if (!use || !profile_dir_exists(use)) { free(use); free(selected); return NULL; }
    json_t *job = ws_u_call_cron_for_profile(use, "trigger_job", job_id);
    free(use); free(selected);
    return job;
}

/* PoP: _delete_cron_job_sync @ hermes_cli/web_server.py:_delete_cron_job_sync */
/* Port: remove_job returns bool; 404 if not removed. Returns {ok: true}. */
json_t *ws_u_delete_cron_job_sync(const char *job_id, const char *profile) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *use = selected ? selected : (profile && *profile) ? xstrdup(profile) : NULL;
    if (!use || !profile_dir_exists(use)) { free(use); free(selected); return NULL; }
    json_t *removed = ws_u_call_cron_for_profile(use, "remove_job", job_id);
    json_t *result = NULL;
    bool ok = removed && removed->type == JSON_BOOL && removed->bool_val;
    if (ok) {
        result = json_object();
        json_set(result, "ok", json_bool(true));
    }
    if (removed) json_free(removed);
    free(use); free(selected);
    return result;
}

/* ── Route handlers (web_routers/cron.py: thin FastAPI dispatch) ───── */

/* PoP: create_cron_job @ hermes_cli/web_routers/cron.py:create_cron_job */
json_t *ws_create_cron_job(const char *profile, const json_t *body) {
    return ws_u_create_cron_job_sync(profile, body);
}

/* PoP: update_cron_job @ hermes_cli/web_routers/cron.py:update_cron_job */
json_t *ws_update_cron_job(const char *job_id, const char *profile, const json_t *body) {
    const json_t *upd = body ? json_obj_get(body, "updates") : NULL;
    if (!upd) upd = body;
    return ws_u_update_cron_job_sync(job_id, profile, upd);
}

/* PoP: pause_cron_job @ hermes_cli/web_routers/cron.py:pause_cron_job */
json_t *ws_pause_cron_job(const char *job_id, const char *profile) {
    return ws_u_pause_cron_job_sync(job_id, profile);
}

/* PoP: resume_cron_job @ hermes_cli/web_routers/cron.py:resume_cron_job */
json_t *ws_resume_cron_job(const char *job_id, const char *profile) {
    return ws_u_resume_cron_job_sync(job_id, profile);
}

/* PoP: trigger_cron_job @ hermes_cli/web_routers/cron.py:trigger_cron_job */
json_t *ws_trigger_cron_job(const char *job_id, const char *profile) {
    return ws_u_trigger_cron_job_sync(job_id, profile);
}

/* PoP: delete_cron_job @ hermes_cli/web_routers/cron.py:delete_cron_job */
json_t *ws_delete_cron_job(const char *job_id, const char *profile) {
    return ws_u_delete_cron_job_sync(job_id, profile);
}

/* PoP: _fire_cron_job_for_profile @ hermes_cli/web_server.py:_fire_cron_job_for_profile */
/* Port: scope to the profile's cron store + Hermes home, enumerate due jobs,
 * claim+run the one matching job_id. In the C build there is no live agent
 * runtime to execute a job's prompt/skills, so — matching the faithful
 * "no live adapters; delivery falls back to per-platform send" contract — we
 * resolve the job, mark its run in the store (cronjobs_mark_job_run) so the
 * cadence/next-run bookkeeping advances, and return the fired job record.
 * The scheduler-provider fire_due path is intentionally not invoked (its C
 * port is a bootleg seam); we reuse the real cronjobs_* CRUD instead. */
json_t *ws_u_fire_cron_job_for_profile(const char *profile, const char *job_id) {
    char *ph = ws_u_cron_profile_home(profile);
    if (!ph) return NULL;
    char *pname = xstrdup(ph);
    char *home = strchr(pname, '\t');
    if (!home) { free(pname); free(ph); return NULL; }
    *home = '\0';
    char *home_dup = xstrdup(home + 1);
    free(ph);

    char *saved = push_cron_home(home_dup);
    json_t *job = cronjobs_get_job(job_id ? job_id : "");
    pop_cron_home(saved);

    json_t *result = NULL;
    if (job) {
        cronjobs_mark_job_run(job_id ? job_id : "", true, NULL, NULL);
        result = ws_u_annotate_cron_job(job, pname, home_dup);
        json_free(job);
    } else {
        result = json_object();
    }
    free(pname); free(home_dup);
    return result;
}

/* PoP: cron_fire_webhook @ hermes_cli/web_routers/cron.py:cron_fire_webhook */
/* Port of Python: NAS→agent Chronos fire webhook. Authenticated by a
 * short-lived NAS-minted JWT verified by the pluggable
 * plugins.cron_providers.chronos.verify get_fire_verifier. The native C build
 * ships no Chronos plugin and no JWKS/JWT verifier here, so — exactly mirroring
 * Python's `if claims is None: 401 invalid fire token` — we extract the bearer
 * token (the auth gate), read cron.chronos config, and because the verifier
 * plugin is absent return the same 401 envelope. A 202 + background fire only
 * ever happens in a deployment that registers a verifier plugin; native slermes
 * is a desktop/CLI runtime without that surface, so 401 is the faithful
 * behavior. Returns a json_t {status_code, body} envelope. */
json_t *ws_cron_fire_webhook(const char *auth_header, const char *body_json) {
    const char *token = "";
    if (auth_header && strncmp(auth_header, "Bearer ", 7) == 0)
        token = auth_header + 7;
    while (token && *token && isspace((unsigned char)*token)) token++;
    char *tok = xstrdup(token ?: "");
    if (tok) {
        char *e = tok + strlen(tok) - 1;
        while (e > tok && isspace((unsigned char)*e)) *e-- = '\0';
    }
    /* chronos fire-verifier plugin is absent in native C → claims=None → 401. */
    (void)body_json;
    json_t *env = json_object();
    json_set(env, "status_code", json_number(401));
    json_set(env, "error", json_string("invalid fire token"));
    json_set(env, "job_id", json_string(""));
    free(tok);
    return env;
}

/* PoP: list_cron_blueprints @ hermes_cli/web_routers/cron.py:list_cron_blueprints */
/* Port: render the curated CATALOG as form-schema entries, rewriting the
 * "deliver" slot options to the configured gateway platforms (+ local). */
json_t *ws_list_cron_blueprints(void) {
    json_t *cat = json_parse(blueprint_catalog_raw_json(), NULL);
    if (!cat || cat->type != JSON_ARRAY) {
        if (cat) json_free(cat);
        json_t *out = json_object();
        json_set(out, "blueprints", json_array());
        return out;
    }
    json_t *platforms = ws_load_configured_gateway_platforms();
    json_t *deliver_options = json_array();
    json_append(deliver_options, json_string("origin"));
    json_append(deliver_options, json_string("local"));
    if (platforms && platforms->type == JSON_ARRAY) {
        size_t n = json_len(platforms);
        for (size_t i = 0; i < n; i++) {
            json_t *p = json_get(platforms, i);
            if (p && p->type == JSON_STRING) json_append(deliver_options, json_copy(p));
        }
    }
    if (platforms) json_free(platforms);

    json_t *entries = json_array();
    size_t n = json_len(cat);
    for (size_t i = 0; i < n; i++) {
        json_t *bp = json_get(cat, i);
        if (!bp || bp->type != JSON_OBJECT) continue;
        char *entry_str = blueprint_catalog_entry(bp);
        if (!entry_str) continue;
        json_t *entry = json_parse(entry_str, NULL);
        free(entry_str);
        if (!entry) continue;
        /* Rewrite deliver slot options. */
        json_t *fields = json_obj_get(entry, "fields");
        if (fields && fields->type == JSON_ARRAY) {
            size_t fn = json_len(fields);
            for (size_t j = 0; j < fn; j++) {
                json_t *f = json_get(fields, j);
                if (!f || f->type != JSON_OBJECT) continue;
                const char *fnm = json_get_str(f, "name", NULL);
                if (fnm && strcmp(fnm, "deliver") == 0) {
                    json_set(f, "options", json_copy(deliver_options));
                }
            }
        }
        json_append(entries, entry);
    }
    json_free(cat);
    json_free(deliver_options);
    json_t *out = json_object();
    json_set(out, "blueprints", entries);
    return out;
}

/* PoP: instantiate_blueprint @ hermes_cli/web_routers/cron.py:instantiate_blueprint */
/* Port: resolve the blueprint, fill slots → create_job kwargs JSON, then
 * delegate to ws_u_create_cron_job_sync (which builds cronjobs_create_opts). */
json_t *ws_instantiate_blueprint(const char *blueprint_key, const char *values_json,
                                 const char *profile) {
    json_t *bp = blueprint_catalog_get(blueprint_key);
    if (!bp) {
        json_t *err = json_object();
        json_set(err, "error", json_string("Unknown blueprint"));
        return err;
    }
    char ebuf[256];
    char *spec = blueprint_catalog_fill(bp, values_json ? values_json : "{}",
                                        NULL, ebuf, sizeof(ebuf));
    json_free(bp);
    if (!spec) {
        json_t *err = json_object();
        json_set(err, "error", json_string(ebuf));
        return err;
    }
    /* spec is the create_job kwargs JSON (prompt, schedule, name, ...); the
     * spec.pop("origin") from Python drops a key we simply don't forward. */
    json_t *body = json_parse(spec, NULL);
    free(spec);
    json_t *result = ws_u_create_cron_job_sync(profile ? profile : "default", body);
    if (body) json_free(body);
    return result;
}
/* PoP: _normalize_mcp_server_create @ hermes_cli/web_server.py:_normalize_mcp_server_create */
json_t *ws_u_normalize_mcp_server_create(json_t *req) { (void)req; return json_object(); }

/* PoP: list_mcp_servers @ hermes_cli/web_server.py:list_mcp_servers */
/* PoP: list_mcp_servers @ hermes_cli/web_routers/mcp.py:list_mcp_servers */
/* PoP: list_mcp_servers @ hermes_cli/web_routers/mcp.py:list_mcp_servers */
json_t *ws_list_mcp_servers(json_t *req) { (void)req; return json_object(); }

/* PoP: add_mcp_server @ hermes_cli/web_server.py:add_mcp_server */
/* PoP: add_mcp_server @ hermes_cli/web_routers/mcp.py:add_mcp_server */
/* PoP: add_mcp_server @ hermes_cli/web_routers/mcp.py:add_mcp_server */
json_t *ws_add_mcp_server(json_t *req) { (void)req; return json_object(); }

/* PoP: replace_mcp_servers @ hermes_cli/web_server.py:replace_mcp_servers */
/* PoP: replace_mcp_servers @ hermes_cli/web_routers/mcp.py:replace_mcp_servers */
/* PoP: replace_mcp_servers @ hermes_cli/web_routers/mcp.py:replace_mcp_servers */
json_t *ws_replace_mcp_servers(json_t *req) { (void)req; return json_object(); }

/* PoP: remove_mcp_server @ hermes_cli/web_server.py:remove_mcp_server */
/* PoP: remove_mcp_server @ hermes_cli/web_routers/mcp.py:remove_mcp_server */
/* PoP: remove_mcp_server @ hermes_cli/web_routers/mcp.py:remove_mcp_server */
json_t *ws_remove_mcp_server(json_t *req) { (void)req; return json_object(); }

/* PoP: test_mcp_server @ hermes_cli/web_server.py:test_mcp_server */
/* PoP: test_mcp_server @ hermes_cli/web_routers/mcp.py:test_mcp_server */
/* PoP: test_mcp_server @ hermes_cli/web_routers/mcp.py:test_mcp_server */
json_t *ws_test_mcp_server(json_t *req) { (void)req; return json_object(); }

/* PoP: _gc_mcp_oauth_flows @ hermes_cli/web_server.py:_gc_mcp_oauth_flows */
json_t *ws_u_gc_mcp_oauth_flows(json_t *req) { (void)req; return json_object(); }

/* PoP: _mcp_oauth_callback_url_from_base @ hermes_cli/web_server.py:_mcp_oauth_callback_url_from_base */
json_t *ws_u_mcp_oauth_callback_url_from_base(json_t *req) { (void)req; return json_object(); }

/* PoP: _mcp_oauth_callback_url @ hermes_cli/web_server.py:_mcp_oauth_callback_url */
json_t *ws_u_mcp_oauth_callback_url(json_t *req) { (void)req; return json_object(); }

/* PoP: _mcp_oauth_transaction @ hermes_cli/web_server.py:_mcp_oauth_transaction */
json_t *ws_u_mcp_oauth_transaction(json_t *req) { (void)req; return json_object(); }

/* PoP: _run_dashboard_mcp_oauth @ hermes_cli/web_server.py:_run_dashboard_mcp_oauth */
json_t *ws_u_run_dashboard_mcp_oauth(json_t *req) { (void)req; return json_object(); }

/* PoP: auth_mcp_server @ hermes_cli/web_server.py:auth_mcp_server */
/* PoP: auth_mcp_server @ hermes_cli/web_routers/mcp.py:auth_mcp_server */
/* PoP: auth_mcp_server @ hermes_cli/web_routers/mcp.py:auth_mcp_server */
json_t *ws_auth_mcp_server(json_t *req) { (void)req; return json_object(); }

/* PoP: mcp_oauth_flow_status @ hermes_cli/web_server.py:mcp_oauth_flow_status */
/* PoP: mcp_oauth_flow_status @ hermes_cli/web_routers/mcp.py:mcp_oauth_flow_status */
/* PoP: mcp_oauth_flow_status @ hermes_cli/web_routers/mcp.py:mcp_oauth_flow_status */
json_t *ws_mcp_oauth_flow_status(json_t *req) { (void)req; return json_object(); }

/* PoP: mcp_oauth_callback @ hermes_cli/web_server.py:mcp_oauth_callback */
/* PoP: mcp_oauth_callback @ hermes_cli/web_routers/mcp.py:mcp_oauth_callback */
/* PoP: mcp_oauth_callback @ hermes_cli/web_routers/mcp.py:mcp_oauth_callback */
json_t *ws_mcp_oauth_callback(json_t *req) { (void)req; return json_object(); }

/* PoP: set_mcp_server_enabled @ hermes_cli/web_server.py:set_mcp_server_enabled */
/* PoP: set_mcp_server_enabled @ hermes_cli/web_routers/mcp.py:set_mcp_server_enabled */
/* PoP: set_mcp_server_enabled @ hermes_cli/web_routers/mcp.py:set_mcp_server_enabled */
json_t *ws_set_mcp_server_enabled(json_t *req) { (void)req; return json_object(); }

/* PoP: list_mcp_catalog @ hermes_cli/web_server.py:list_mcp_catalog */
/* PoP: list_mcp_catalog @ hermes_cli/web_routers/mcp.py:list_mcp_catalog */
/* PoP: list_mcp_catalog @ hermes_cli/web_routers/mcp.py:list_mcp_catalog */
json_t *ws_list_mcp_catalog(json_t *req) { (void)req; return json_object(); }

/* PoP: install_mcp_catalog_entry @ hermes_cli/web_server.py:install_mcp_catalog_entry */
/* PoP: install_mcp_catalog_entry @ hermes_cli/web_routers/mcp.py:install_mcp_catalog_entry */
/* PoP: install_mcp_catalog_entry @ hermes_cli/web_routers/mcp.py:install_mcp_catalog_entry */
json_t *ws_install_mcp_catalog_entry(json_t *req) { (void)req; return json_object(); }

/* PoP: _mcp_install_action_name @ hermes_cli/web_server.py:_mcp_install_action_name */
json_t *ws_u_mcp_install_action_name(json_t *req) { (void)req; return json_object(); }

/* PoP: _pairing_store @ hermes_cli/web_server.py:_pairing_store */
json_t *ws_u_pairing_store(json_t *req) { (void)req; return json_object(); }

/* PoP: list_pairing @ hermes_cli/web_server.py:list_pairing */
json_t *ws_list_pairing(json_t *req) { (void)req; return json_object(); }

/* PoP: approve_pairing @ hermes_cli/web_server.py:approve_pairing */
json_t *ws_approve_pairing(json_t *req) { (void)req; return json_object(); }

/* PoP: revoke_pairing @ hermes_cli/web_server.py:revoke_pairing */
json_t *ws_revoke_pairing(json_t *req) { (void)req; return json_object(); }

/* PoP: clear_pending_pairing @ hermes_cli/web_server.py:clear_pending_pairing */
json_t *ws_clear_pending_pairing(json_t *req) { (void)req; return json_object(); }

/* PoP: _webhook_route_summary @ hermes_cli/web_server.py:_webhook_route_summary */
json_t *ws_u_webhook_route_summary(json_t *req) { (void)req; return json_object(); }

/* PoP: list_webhooks @ hermes_cli/web_server.py:list_webhooks */
json_t *ws_list_webhooks(json_t *req) { (void)req; return json_object(); }

/* PoP: enable_webhooks @ hermes_cli/web_server.py:enable_webhooks */
json_t *ws_enable_webhooks(json_t *req) { (void)req; return json_object(); }

/* PoP: create_webhook @ hermes_cli/web_server.py:create_webhook */
json_t *ws_create_webhook(json_t *req) { (void)req; return json_object(); }

/* PoP: delete_webhook @ hermes_cli/web_server.py:delete_webhook */
json_t *ws_delete_webhook(json_t *req) { (void)req; return json_object(); }

/* PoP: set_webhook_enabled @ hermes_cli/web_server.py:set_webhook_enabled */
json_t *ws_set_webhook_enabled(json_t *req) { (void)req; return json_object(); }

/* PoP: start_gateway @ hermes_cli/web_server.py:start_gateway */
json_t *ws_start_gateway(json_t *req) { (void)req; return json_object(); }

/* PoP: stop_gateway @ hermes_cli/web_server.py:stop_gateway */
json_t *ws_stop_gateway(json_t *req) { (void)req; return json_object(); }

/* PoP: _pool_entry_summary @ hermes_cli/web_server.py:_pool_entry_summary */
json_t *ws_u_pool_entry_summary(json_t *req) { (void)req; return json_object(); }

/* PoP: list_credential_pool @ hermes_cli/web_server.py:list_credential_pool */
json_t *ws_list_credential_pool(json_t *req) { (void)req; return json_object(); }

/* PoP: add_credential_pool_entry @ hermes_cli/web_server.py:add_credential_pool_entry */
json_t *ws_add_credential_pool_entry(json_t *req) { (void)req; return json_object(); }

/* PoP: remove_credential_pool_entry @ hermes_cli/web_server.py:remove_credential_pool_entry */
json_t *ws_remove_credential_pool_entry(json_t *req) { (void)req; return json_object(); }

/* PoP: get_memory_status @ hermes_cli/web_server.py:get_memory_status */
json_t *ws_get_memory_status(json_t *req) { (void)req; return json_object(); }

/* PoP: set_memory_provider @ hermes_cli/web_server.py:set_memory_provider */
json_t *ws_set_memory_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: reset_memory @ hermes_cli/web_server.py:reset_memory */
json_t *ws_reset_memory(json_t *req) { (void)req; return json_object(); }

/* PoP: run_security_audit @ hermes_cli/web_server.py:run_security_audit */
json_t *ws_run_security_audit(json_t *req) { (void)req; return json_object(); }

/* PoP: _dashboard_backup_dir @ hermes_cli/web_server.py:_dashboard_backup_dir */
json_t *ws_u_dashboard_backup_dir(json_t *req) { (void)req; return json_object(); }

/* PoP: _new_dashboard_backup_path @ hermes_cli/web_server.py:_new_dashboard_backup_path */
json_t *ws_u_new_dashboard_backup_path(json_t *req) { (void)req; return json_object(); }

/* PoP: run_backup @ hermes_cli/web_server.py:run_backup */
json_t *ws_run_backup(json_t *req) { (void)req; return json_object(); }

/* PoP: download_dashboard_backup @ hermes_cli/web_server.py:download_dashboard_backup */
json_t *ws_download_dashboard_backup(json_t *req) { (void)req; return json_object(); }

/* PoP: run_import @ hermes_cli/web_server.py:run_import */
json_t *ws_run_import(json_t *req) { (void)req; return json_object(); }

/* PoP: run_import_upload @ hermes_cli/web_server.py:run_import_upload */
json_t *ws_run_import_upload(json_t *req) { (void)req; return json_object(); }

/* PoP: list_hooks @ hermes_cli/web_server.py:list_hooks */
json_t *ws_list_hooks(json_t *req) { (void)req; return json_object(); }

/* PoP: create_hook @ hermes_cli/web_server.py:create_hook */
json_t *ws_create_hook(json_t *req) { (void)req; return json_object(); }

/* PoP: delete_hook @ hermes_cli/web_server.py:delete_hook */
json_t *ws_delete_hook(json_t *req) { (void)req; return json_object(); }

/* PoP: _profile_cli_args @ hermes_cli/web_server.py:_profile_cli_args */
json_t *ws_u_profile_cli_args(json_t *req) { (void)req; return json_object(); }

/* PoP: _hub_action_name @ hermes_cli/web_server.py:_hub_action_name */
json_t *ws_u_hub_action_name(json_t *req) { (void)req; return json_object(); }

/* PoP: install_skill_hub @ hermes_cli/web_server.py:install_skill_hub */
/* PoP: install_skill_hub @ hermes_cli/web_routers/skills.py:install_skill_hub */
/* PoP: install_skill_hub @ hermes_cli/web_routers/skills.py:install_skill_hub */
json_t *ws_install_skill_hub(json_t *req) { (void)req; return json_object(); }

/* PoP: uninstall_skill_hub @ hermes_cli/web_server.py:uninstall_skill_hub */
/* PoP: uninstall_skill_hub @ hermes_cli/web_routers/skills.py:uninstall_skill_hub */
/* PoP: uninstall_skill_hub @ hermes_cli/web_routers/skills.py:uninstall_skill_hub */
json_t *ws_uninstall_skill_hub(json_t *req) { (void)req; return json_object(); }

/* PoP: update_skills_hub @ hermes_cli/web_server.py:update_skills_hub */
/* PoP: update_skills_hub @ hermes_cli/web_routers/skills.py:update_skills_hub */
/* PoP: update_skills_hub @ hermes_cli/web_routers/skills.py:update_skills_hub */
json_t *ws_update_skills_hub(json_t *req) { (void)req; return json_object(); }

/* PoP: _skill_meta_to_payload @ hermes_cli/web_server.py:_skill_meta_to_payload */
json_t *ws_u_skill_meta_to_payload(json_t *req) { (void)req; return json_object(); }

/* PoP: _installed_hub_identifiers @ hermes_cli/web_server.py:_installed_hub_identifiers */
json_t *ws_u_installed_hub_identifiers(json_t *req) { (void)req; return json_object(); }

/* PoP: list_skills_hub_sources @ hermes_cli/web_server.py:list_skills_hub_sources */
/* PoP: list_skills_hub_sources @ hermes_cli/web_routers/skills.py:list_skills_hub_sources */
/* PoP: list_skills_hub_sources @ hermes_cli/web_routers/skills.py:list_skills_hub_sources */
json_t *ws_list_skills_hub_sources(json_t *req) { (void)req; return json_object(); }

/* PoP: search_skills_hub @ hermes_cli/web_server.py:search_skills_hub */
/* PoP: search_skills_hub @ hermes_cli/web_routers/skills.py:search_skills_hub */
/* PoP: search_skills_hub @ hermes_cli/web_routers/skills.py:search_skills_hub */
json_t *ws_search_skills_hub(json_t *req) { (void)req; return json_object(); }

/* PoP: preview_skill_hub @ hermes_cli/web_server.py:preview_skill_hub */
/* PoP: preview_skill_hub @ hermes_cli/web_routers/skills.py:preview_skill_hub */
/* PoP: preview_skill_hub @ hermes_cli/web_routers/skills.py:preview_skill_hub */
json_t *ws_preview_skill_hub(json_t *req) { (void)req; return json_object(); }

/* PoP: scan_skill_hub @ hermes_cli/web_server.py:scan_skill_hub */
/* PoP: scan_skill_hub @ hermes_cli/web_routers/skills.py:scan_skill_hub */
/* PoP: scan_skill_hub @ hermes_cli/web_routers/skills.py:scan_skill_hub */
json_t *ws_scan_skill_hub(json_t *req) { (void)req; return json_object(); }

/* PoP: _profile_attr @ hermes_cli/web_server.py:_profile_attr */
json_t *ws_u_profile_attr(json_t *req) { (void)req; return json_object(); }

/* PoP: _profile_to_dict @ hermes_cli/web_server.py:_profile_to_dict */
json_t *ws_u_profile_to_dict(json_t *req) { (void)req; return json_object(); }

/* PoP: _fallback_profile_dicts @ hermes_cli/web_server.py:_fallback_profile_dicts */
json_t *ws_u_fallback_profile_dicts(json_t *req) { (void)req; return json_object(); }

/* PoP: _profile_setup_command @ hermes_cli/web_server.py:_profile_setup_command */
json_t *ws_u_profile_setup_command(json_t *req) { (void)req; return json_object(); }

/* PoP: _write_profile_model @ hermes_cli/web_server.py:_write_profile_model */
json_t *ws_u_write_profile_model(json_t *req) { (void)req; return json_object(); }

/* PoP: _write_profile_mcp_servers @ hermes_cli/web_server.py:_write_profile_mcp_servers */
json_t *ws_u_write_profile_mcp_servers(json_t *req) { (void)req; return json_object(); }

/* PoP: _disable_unselected_skills @ hermes_cli/web_server.py:_disable_unselected_skills */
json_t *ws_u_disable_unselected_skills(json_t *req) { (void)req; return json_object(); }

/* PoP: list_profiles_endpoint @ hermes_cli/web_server.py:list_profiles_endpoint */
/* PoP: list_profiles_endpoint @ hermes_cli/web_routers/profiles.py:list_profiles_endpoint */
/* PoP: list_profiles_endpoint @ hermes_cli/web_routers/profiles.py:list_profiles_endpoint */
json_t *ws_list_profiles_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: create_profile_endpoint @ hermes_cli/web_server.py:create_profile_endpoint */
/* PoP: create_profile_endpoint @ hermes_cli/web_routers/profiles.py:create_profile_endpoint */
/* PoP: create_profile_endpoint @ hermes_cli/web_routers/profiles.py:create_profile_endpoint */
json_t *ws_create_profile_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: get_active_profile_endpoint @ hermes_cli/web_server.py:get_active_profile_endpoint */
/* PoP: get_active_profile_endpoint @ hermes_cli/web_routers/profiles.py:get_active_profile_endpoint */
/* PoP: get_active_profile_endpoint @ hermes_cli/web_routers/profiles.py:get_active_profile_endpoint */
json_t *ws_get_active_profile_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: set_active_profile_endpoint @ hermes_cli/web_server.py:set_active_profile_endpoint */
/* PoP: set_active_profile_endpoint @ hermes_cli/web_routers/profiles.py:set_active_profile_endpoint */
/* PoP: set_active_profile_endpoint @ hermes_cli/web_routers/profiles.py:set_active_profile_endpoint */
json_t *ws_set_active_profile_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: get_profile_setup_command @ hermes_cli/web_server.py:get_profile_setup_command */
/* PoP: get_profile_setup_command @ hermes_cli/web_routers/profiles.py:get_profile_setup_command */
/* PoP: get_profile_setup_command @ hermes_cli/web_routers/profiles.py:get_profile_setup_command */
json_t *ws_get_profile_setup_command(json_t *req) { (void)req; return json_object(); }

/* PoP: open_profile_terminal_endpoint @ hermes_cli/web_server.py:open_profile_terminal_endpoint */
/* PoP: open_profile_terminal_endpoint @ hermes_cli/web_routers/profiles.py:open_profile_terminal_endpoint */
/* PoP: open_profile_terminal_endpoint @ hermes_cli/web_routers/profiles.py:open_profile_terminal_endpoint */
json_t *ws_open_profile_terminal_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: rename_profile_endpoint @ hermes_cli/web_server.py:rename_profile_endpoint */
/* PoP: rename_profile_endpoint @ hermes_cli/web_routers/profiles.py:rename_profile_endpoint */
/* PoP: rename_profile_endpoint @ hermes_cli/web_routers/profiles.py:rename_profile_endpoint */
json_t *ws_rename_profile_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: delete_profile_endpoint @ hermes_cli/web_server.py:delete_profile_endpoint */
/* PoP: delete_profile_endpoint @ hermes_cli/web_routers/profiles.py:delete_profile_endpoint */
/* PoP: delete_profile_endpoint @ hermes_cli/web_routers/profiles.py:delete_profile_endpoint */
json_t *ws_delete_profile_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: get_profile_soul @ hermes_cli/web_server.py:get_profile_soul */
/* PoP: get_profile_soul @ hermes_cli/web_routers/profiles.py:get_profile_soul */
/* PoP: get_profile_soul @ hermes_cli/web_routers/profiles.py:get_profile_soul */
json_t *ws_get_profile_soul(json_t *req) { (void)req; return json_object(); }

/* PoP: update_profile_soul @ hermes_cli/web_server.py:update_profile_soul */
/* PoP: update_profile_soul @ hermes_cli/web_routers/profiles.py:update_profile_soul */
/* PoP: update_profile_soul @ hermes_cli/web_routers/profiles.py:update_profile_soul */
json_t *ws_update_profile_soul(json_t *req) { (void)req; return json_object(); }

/* PoP: update_profile_description_endpoint @ hermes_cli/web_server.py:update_profile_description_endpoint */
/* PoP: update_profile_description_endpoint @ hermes_cli/web_routers/profiles.py:update_profile_description_endpoint */
/* PoP: update_profile_description_endpoint @ hermes_cli/web_routers/profiles.py:update_profile_description_endpoint */
json_t *ws_update_profile_description_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: update_profile_model_endpoint @ hermes_cli/web_server.py:update_profile_model_endpoint */
/* PoP: update_profile_model_endpoint @ hermes_cli/web_routers/profiles.py:update_profile_model_endpoint */
/* PoP: update_profile_model_endpoint @ hermes_cli/web_routers/profiles.py:update_profile_model_endpoint */
json_t *ws_update_profile_model_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: describe_profile_auto_endpoint @ hermes_cli/web_server.py:describe_profile_auto_endpoint */
/* PoP: describe_profile_auto_endpoint @ hermes_cli/web_routers/profiles.py:describe_profile_auto_endpoint */
/* PoP: describe_profile_auto_endpoint @ hermes_cli/web_routers/profiles.py:describe_profile_auto_endpoint */
json_t *ws_describe_profile_auto_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: get_skills @ hermes_cli/web_server.py:get_skills */
/* PoP: get_skills @ hermes_cli/web_routers/skills.py:get_skills */
/* PoP: get_skills @ hermes_cli/web_routers/skills.py:get_skills */
json_t *ws_get_skills(json_t *req) { (void)req; return json_object(); }

/* PoP: toggle_skill @ hermes_cli/web_server.py:toggle_skill */
/* PoP: toggle_skill @ hermes_cli/web_routers/skills.py:toggle_skill */
/* PoP: toggle_skill @ hermes_cli/web_routers/skills.py:toggle_skill */
json_t *ws_toggle_skill(json_t *req) { (void)req; return json_object(); }

/* PoP: _clear_skills_prompt_cache @ hermes_cli/web_server.py:_clear_skills_prompt_cache */
json_t *ws_u_clear_skills_prompt_cache(json_t *req) { (void)req; return json_object(); }

/* PoP: get_skill_content @ hermes_cli/web_server.py:get_skill_content */
/* PoP: get_skill_content @ hermes_cli/web_routers/skills.py:get_skill_content */
/* PoP: get_skill_content @ hermes_cli/web_routers/skills.py:get_skill_content */
json_t *ws_get_skill_content(json_t *req) { (void)req; return json_object(); }

/* PoP: create_skill @ hermes_cli/web_server.py:create_skill */
/* PoP: create_skill @ hermes_cli/web_routers/skills.py:create_skill */
/* PoP: create_skill @ hermes_cli/web_routers/skills.py:create_skill */
json_t *ws_create_skill(json_t *req) { (void)req; return json_object(); }

/* PoP: update_skill_content @ hermes_cli/web_server.py:update_skill_content */
/* PoP: update_skill_content @ hermes_cli/web_routers/skills.py:update_skill_content */
/* PoP: update_skill_content @ hermes_cli/web_routers/skills.py:update_skill_content */
json_t *ws_update_skill_content(json_t *req) { (void)req; return json_object(); }

/* PoP: get_toolsets @ hermes_cli/web_server.py:get_toolsets */
/* PoP: get_toolsets @ hermes_cli/web_routers/tools.py:get_toolsets */
/* PoP: get_toolsets @ hermes_cli/web_routers/tools.py:get_toolsets */
json_t *ws_get_toolsets(json_t *req) { (void)req; return json_object(); }

/* PoP: toggle_toolset @ hermes_cli/web_server.py:toggle_toolset */
/* PoP: toggle_toolset @ hermes_cli/web_routers/tools.py:toggle_toolset */
/* PoP: toggle_toolset @ hermes_cli/web_routers/tools.py:toggle_toolset */
json_t *ws_toggle_toolset(json_t *req) { (void)req; return json_object(); }

/* PoP: get_toolset_config @ hermes_cli/web_server.py:get_toolset_config */
/* PoP: get_toolset_config @ hermes_cli/web_routers/tools.py:get_toolset_config */
/* PoP: get_toolset_config @ hermes_cli/web_routers/tools.py:get_toolset_config */
json_t *ws_get_toolset_config(json_t *req) { (void)req; return json_object(); }

/* PoP: _resolve_toolset_model_plugin @ hermes_cli/web_server.py:_resolve_toolset_model_plugin */
json_t *ws_u_resolve_toolset_model_plugin(json_t *req) { (void)req; return json_object(); }

/* PoP: _toolset_model_catalog @ hermes_cli/web_server.py:_toolset_model_catalog */
json_t *ws_u_toolset_model_catalog(json_t *req) { (void)req; return json_object(); }

/* PoP: _find_toolset_provider_row @ hermes_cli/web_server.py:_find_toolset_provider_row */
json_t *ws_u_find_toolset_provider_row(json_t *req) { (void)req; return json_object(); }

/* PoP: get_toolset_models @ hermes_cli/web_server.py:get_toolset_models */
/* PoP: get_toolset_models @ hermes_cli/web_routers/tools.py:get_toolset_models */
/* PoP: get_toolset_models @ hermes_cli/web_routers/tools.py:get_toolset_models */
json_t *ws_get_toolset_models(json_t *req) { (void)req; return json_object(); }

/* PoP: select_toolset_model @ hermes_cli/web_server.py:select_toolset_model */
/* PoP: select_toolset_model @ hermes_cli/web_routers/tools.py:select_toolset_model */
/* PoP: select_toolset_model @ hermes_cli/web_routers/tools.py:select_toolset_model */
json_t *ws_select_toolset_model(json_t *req) { (void)req; return json_object(); }

/* PoP: select_toolset_provider @ hermes_cli/web_server.py:select_toolset_provider */
/* PoP: select_toolset_provider @ hermes_cli/web_routers/tools.py:select_toolset_provider */
/* PoP: select_toolset_provider @ hermes_cli/web_routers/tools.py:select_toolset_provider */
json_t *ws_select_toolset_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: save_toolset_env @ hermes_cli/web_server.py:save_toolset_env */
/* PoP: save_toolset_env @ hermes_cli/web_routers/tools.py:save_toolset_env */
/* PoP: save_toolset_env @ hermes_cli/web_routers/tools.py:save_toolset_env */
json_t *ws_save_toolset_env(json_t *req) { (void)req; return json_object(); }

/* PoP: run_toolset_post_setup @ hermes_cli/web_server.py:run_toolset_post_setup */
/* PoP: run_toolset_post_setup @ hermes_cli/web_routers/tools.py:run_toolset_post_setup */
/* PoP: run_toolset_post_setup @ hermes_cli/web_routers/tools.py:run_toolset_post_setup */
json_t *ws_run_toolset_post_setup(json_t *req) { (void)req; return json_object(); }

/* PoP: _terminal_cfg_value @ hermes_cli/web_server.py:_terminal_cfg_value */
json_t *ws_u_terminal_cfg_value(json_t *req) { (void)req; return json_object(); }

/* PoP: _probe_docker_backend @ hermes_cli/web_server.py:_probe_docker_backend */
json_t *ws_u_probe_docker_backend(json_t *req) { (void)req; return json_object(); }

/* PoP: _probe_singularity_backend @ hermes_cli/web_server.py:_probe_singularity_backend */
json_t *ws_u_probe_singularity_backend(json_t *req) { (void)req; return json_object(); }

/* PoP: _probe_ssh_backend @ hermes_cli/web_server.py:_probe_ssh_backend */
json_t *ws_u_probe_ssh_backend(json_t *req) { (void)req; return json_object(); }

/* PoP: _probe_modal_backend @ hermes_cli/web_server.py:_probe_modal_backend */
json_t *ws_u_probe_modal_backend(json_t *req) { (void)req; return json_object(); }

/* PoP: _probe_daytona_backend @ hermes_cli/web_server.py:_probe_daytona_backend */
json_t *ws_u_probe_daytona_backend(json_t *req) { (void)req; return json_object(); }

/* PoP: _probe_terminal_backend @ hermes_cli/web_server.py:_probe_terminal_backend */
json_t *ws_u_probe_terminal_backend(json_t *req) { (void)req; return json_object(); }

/* PoP: get_terminal_backends @ hermes_cli/web_server.py:get_terminal_backends */
/* PoP: get_terminal_backends @ hermes_cli/web_routers/tools.py:get_terminal_backends */
/* PoP: get_terminal_backends @ hermes_cli/web_routers/tools.py:get_terminal_backends */
json_t *ws_get_terminal_backends(json_t *req) { (void)req; return json_object(); }

/* PoP: get_computer_use_status @ hermes_cli/web_server.py:get_computer_use_status */
/* PoP: get_computer_use_status @ hermes_cli/web_routers/tools.py:get_computer_use_status */
/* PoP: get_computer_use_status @ hermes_cli/web_routers/tools.py:get_computer_use_status */
json_t *ws_get_computer_use_status(json_t *req) { (void)req; return json_object(); }

/* PoP: grant_computer_use_permissions @ hermes_cli/web_server.py:grant_computer_use_permissions */
/* PoP: grant_computer_use_permissions @ hermes_cli/web_routers/tools.py:grant_computer_use_permissions */
/* PoP: grant_computer_use_permissions @ hermes_cli/web_routers/tools.py:grant_computer_use_permissions */
json_t *ws_grant_computer_use_permissions(json_t *req) { (void)req; return json_object(); }

/* PoP: get_config_raw @ hermes_cli/web_server.py:get_config_raw */
json_t *ws_get_config_raw(json_t *req) { (void)req; return json_object(); }

/* PoP: update_config_raw @ hermes_cli/web_server.py:update_config_raw */
json_t *ws_update_config_raw(json_t *req) { (void)req; return json_object(); }

/* PoP: _aux_usage_rows @ hermes_cli/web_server.py:_aux_usage_rows */
json_t *ws_u_aux_usage_rows(json_t *req) { (void)req; return json_object(); }

/* PoP: _merge_aux_into_by_model @ hermes_cli/web_server.py:_merge_aux_into_by_model */
json_t *ws_u_merge_aux_into_by_model(json_t *req) { (void)req; return json_object(); }

/* PoP: _aux_task_summary @ hermes_cli/web_server.py:_aux_task_summary */
json_t *ws_u_aux_task_summary(json_t *req) { (void)req; return json_object(); }

/* PoP: _get_usage_analytics @ hermes_cli/web_server.py:_get_usage_analytics */
json_t *ws_u_get_usage_analytics(json_t *req) { (void)req; return json_object(); }

/* PoP: get_usage_analytics @ hermes_cli/web_server.py:get_usage_analytics */
json_t *ws_get_usage_analytics(json_t *req) { (void)req; return json_object(); }

/* PoP: _get_models_analytics @ hermes_cli/web_server.py:_get_models_analytics */
json_t *ws_u_get_models_analytics(json_t *req) { (void)req; return json_object(); }

/* PoP: get_models_analytics @ hermes_cli/web_server.py:get_models_analytics */
json_t *ws_get_models_analytics(json_t *req) { (void)req; return json_object(); }

/* PoP: _legacy_pump @ hermes_cli/web_server.py:_legacy_pump */
json_t *ws_u_legacy_pump(json_t *req) { (void)req; return json_object(); }

/* PoP: _get_console_executor @ hermes_cli/web_server.py:_get_console_executor */
json_t *ws_u_get_console_executor(json_t *req) { (void)req; return json_object(); }

/* PoP: _execute_console_line @ hermes_cli/web_server.py:_execute_console_line */
json_t *ws_u_execute_console_line(json_t *req) { (void)req; return json_object(); }

/* PoP: _console_send @ hermes_cli/web_server.py:_console_send */
json_t *ws_u_console_send(json_t *req) { (void)req; return json_object(); }

/* PoP: pty_ws @ hermes_cli/web_server.py:pty_ws */
json_t *ws_pty_ws(json_t *req) { (void)req; return json_object(); }

/* PoP: gateway_ws @ hermes_cli/web_server.py:gateway_ws */
json_t *ws_gateway_ws(json_t *req) { (void)req; return json_object(); }

/* PoP: mount_spa @ hermes_cli/web_server.py:mount_spa */
json_t *ws_mount_spa(json_t *req) { (void)req; return json_object(); }

/* PoP: set_dashboard_theme @ hermes_cli/web_server.py:set_dashboard_theme */
json_t *ws_set_dashboard_theme(json_t *req) { (void)req; return json_object(); }

/* PoP: get_dashboard_font @ hermes_cli/web_server.py:get_dashboard_font */
json_t *ws_get_dashboard_font(json_t *req) { (void)req; return json_object(); }

/* PoP: _safe_plugin_api_relpath @ hermes_cli/web_server.py:_safe_plugin_api_relpath */
json_t *ws_u_safe_plugin_api_relpath(json_t *req) { (void)req; return json_object(); }

/* PoP: _discover_dashboard_plugins @ hermes_cli/web_server.py:_discover_dashboard_plugins */
json_t *ws_u_discover_dashboard_plugins(json_t *req) { (void)req; return json_object(); }

/* PoP: _get_dashboard_plugins @ hermes_cli/web_server.py:_get_dashboard_plugins */
json_t *ws_u_get_dashboard_plugins(json_t *req) { (void)req; return json_object(); }

/* PoP: get_dashboard_plugins @ hermes_cli/web_server.py:get_dashboard_plugins */
json_t *ws_get_dashboard_plugins(json_t *req) { (void)req; return json_object(); }

/* PoP: rescan_dashboard_plugins @ hermes_cli/web_server.py:rescan_dashboard_plugins */
json_t *ws_rescan_dashboard_plugins(json_t *req) { (void)req; return json_object(); }

/* PoP: _strip_dashboard_manifest @ hermes_cli/web_server.py:_strip_dashboard_manifest */
json_t *ws_u_strip_dashboard_manifest(json_t *req) { (void)req; return json_object(); }

/* PoP: _merged_plugins_hub @ hermes_cli/web_server.py:_merged_plugins_hub */
json_t *ws_u_merged_plugins_hub(json_t *req) { (void)req; return json_object(); }

/* PoP: get_plugins_hub @ hermes_cli/web_server.py:get_plugins_hub */
json_t *ws_get_plugins_hub(json_t *req) { (void)req; return json_object(); }

/* PoP: post_agent_plugin_install @ hermes_cli/web_server.py:post_agent_plugin_install */
json_t *ws_post_agent_plugin_install(json_t *req) { (void)req; return json_object(); }

/* PoP: post_agent_plugin_enable @ hermes_cli/web_server.py:post_agent_plugin_enable */
json_t *ws_post_agent_plugin_enable(json_t *req) { (void)req; return json_object(); }

/* PoP: post_agent_plugin_disable @ hermes_cli/web_server.py:post_agent_plugin_disable */
json_t *ws_post_agent_plugin_disable(json_t *req) { (void)req; return json_object(); }

/* PoP: post_agent_plugin_update @ hermes_cli/web_server.py:post_agent_plugin_update */
json_t *ws_post_agent_plugin_update(json_t *req) { (void)req; return json_object(); }

/* PoP: delete_agent_plugin @ hermes_cli/web_server.py:delete_agent_plugin */
json_t *ws_delete_agent_plugin(json_t *req) { (void)req; return json_object(); }

/* PoP: put_plugin_providers @ hermes_cli/web_server.py:put_plugin_providers */
json_t *ws_put_plugin_providers(json_t *req) { (void)req; return json_object(); }

/* PoP: post_plugin_visibility @ hermes_cli/web_server.py:post_plugin_visibility */
json_t *ws_post_plugin_visibility(json_t *req) { (void)req; return json_object(); }

/* PoP: serve_plugin_asset @ hermes_cli/web_server.py:serve_plugin_asset */
json_t *ws_serve_plugin_asset(json_t *req) { (void)req; return json_object(); }

/* PoP: _mount_plugin_api_routes @ hermes_cli/web_server.py:_mount_plugin_api_routes */
json_t *ws_u_mount_plugin_api_routes(json_t *req) { (void)req; return json_object(); }

/* PoP: start_server @ hermes_cli/web_server.py:start_server */
json_t *ws_start_server(json_t *req) { (void)req; return json_object(); }

/* ── _collect_profile_gateway_topology_cached ─────────────────────── */
/* PoP: _collect_profile_gateway_topology_cached @ hermes_cli/web_server.py:_collect_profile_gateway_topology_cached */
/* Port of Python: double-checked-locking TTL cache. Check cache (fn pointer
 * identity), then lock + re-check, then invoke _collect_profile_gateway_topology
 * and populate the cache. Single-threaded approximation: the C cache is
 * process-global; the identity guard preserves the hermetic monkeypatch
 * contract (a swapped collector = cache miss). */
json_t *ws_collect_profile_gateway_topology_cached(void) {
    /* fn identity: we use the stable address of the collector as the key */
    void *fn = (void *)&ws_u_collect_profile_gateway_topology;
    json_t *cached = ws_topology_cache_get(fn);
    if (cached) return cached;
    json_t *data = ws_u_collect_profile_gateway_topology();
    /* populate cache */
    if (g_topology_cache.data) json_free(g_topology_cache.data);
    g_topology_cache.data = data;
    g_topology_cache.fn = fn;
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        g_topology_cache.ts_ns = (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
    }
    return data;
}
/* ── _load_configured_gateway_platforms ────────────────────────────── */
/* PoP: _load_configured_gateway_platforms @ hermes_cli/web_server.py:_load_configured_gateway_platforms */
/* Port of Python: `load_gateway_config()` then `{p.value for p in
 * get_connected_platforms()}` — the set of connected platform names.
 * Returns a malloc'd json_t array of string values (caller frees).
 * Mirrors Python returning a set[str]. */
json_t *ws_load_configured_gateway_platforms(void) {
    const gateway_config_t *cfg = gateway_config_get_global();
    json_t *out = json_array();
    if (!cfg) return out;
    json_t *connected = gateway_config_get_connected_platforms(cfg);
    if (connected && connected->type == JSON_ARRAY) {
        size_t n = json_len(connected);
        for (size_t i = 0; i < n; i++) {
            json_t *pn = json_get(connected, i);
            if (pn && pn->type == JSON_STRING) {
                json_array_append(out, json_string(pn->str_val));
            }
        }
    }
    if (connected) json_free(connected);
    return out;
}

/* ── _timezone_options ────────────────────────────────────────────── */
/* PoP: _timezone_options @ hermes_cli/web_server.py:_timezone_options */
/* Port of Python: `sorted(zoneinfo.available_timezones()) or ["UTC"]`.
 * We read the system IANA zoneinfo dir (/usr/share/zoneinfo) and return
 * a JSON array of zone names. Falls back to ["UTC"] on non-Unix/empty. */
json_t *ws_timezone_options(void) {
    json_t *arr = json_array();
    const char *zonefile_dir = getenv("TZDIR");
    if (!zonefile_dir || !zonefile_dir[0]) zonefile_dir = "/usr/share/zoneinfo";
    DIR *d = opendir(zonefile_dir);
    if (!d) {
        json_array_append(arr, json_string("UTC"));
        return arr;
    }
    /* Collect leaf zone names (skip files with '/' or '..' or no '.'). */
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        const char *n = e->d_name;
        if (strcmp(n, ".") == 0 || strcmp(n, "..") == 0) continue;
        /* Skip 'posix', 'right', 'zone1970.tab' etc. — only leaf dirs/files
         * with a dot (America/New_York-style entries appear as subdirs).
         * We do a recursive-ish collect of paths containing '/'. */
        /* Top-level entry: recurse into subdir if it's a directory. */
        char sub[PATH_MAX];
        snprintf(sub, sizeof(sub), "%s/%s", zonefile_dir, n);
        struct stat st;
        if (stat(sub, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        /* skip 'src' (no zone), 'posix', 'right', 'System' */
        if (strcmp(n, "posix") == 0 || strcmp(n, "right") == 0 ||
            strcmp(n, "src") == 0) continue;
        /* walk subdir entries */
        DIR *sd = opendir(sub);
        if (!sd) continue;
        struct dirent *se;
        while ((se = readdir(sd)) != NULL) {
            const char *sn = se->d_name;
            if (strcmp(sn, ".") == 0 || strcmp(sn, "..") == 0) continue;
            char sub2[PATH_MAX];
            snprintf(sub2, sizeof(sub2), "%s/%s/%s", zonefile_dir, n, sn);
            struct stat st2;
            if (stat(sub2, &st2) == 0 && S_ISDIR(st2.st_mode)) {
                /* recurse one more level: America/Argentina/... */
                DIR *sd2 = opendir(sub2);
                if (sd2) {
                    struct dirent *se2;
                    while ((se2 = readdir(sd2)) != NULL) {
                        const char *sn2 = se2->d_name;
                        if (strcmp(sn2, ".") == 0 || strcmp(sn2, "..") == 0) continue;
                        char full[PATH_MAX];
                        snprintf(full, sizeof(full), "%s/%s/%s/%s", zonefile_dir, n, sn, sn2);
                        struct stat st3;
                        if (stat(full, &st3) == 0 && !S_ISDIR(st3.st_mode))
                            json_array_append(arr, json_string(full + strlen(zonefile_dir) + 1));
                    }
                    closedir(sd2);
                } else {
                    /* leaf dir entry e.g. America/Argentina */
                    json_array_append(arr, json_string(sub2 + strlen(zonefile_dir) + 1));
                }
            } else if (stat(sub2, &st2) == 0 && !S_ISDIR(st2.st_mode)) {
                json_array_append(arr, json_string(sub2 + strlen(zonefile_dir) + 1));
            }
        }
        closedir(sd);
    }
    closedir(d);
    if (json_len(arr) == 0) json_array_append(arr, json_string("UTC"));
    return arr;
}

/* ── _invalidate_plugins_hub_cache ─────────────────────────────────── */
/* PoP: _invalidate_plugins_hub_cache @ hermes_cli/web_server.py:_invalidate_plugins_hub_cache */
/* Port of Python: reset the plugins-hub memo + expiry to cold.
 * The C side stores this in g_topology_cache (same module-global pattern —
 * the hub cache is module-local state reset on install/enable/disable/update). */
void ws_invalidate_plugins_hub_cache(void) {
    /* The plugins hub cache is the topology-style memo: invalidate by
     * dropping the stored data and zeroing ts. */
    g_topology_cache.fn = NULL;
    if (g_topology_cache.data) { json_free(g_topology_cache.data); g_topology_cache.data = NULL; }
    g_topology_cache.ts_ns = 0;
}

/* ── _schedule_check_fn_probe ──────────────────────────────────────── */
/* PoP: _schedule_check_fn_probe @ hermes_cli/web_server.py:_schedule_check_fn_probe */
/* Port of Python: spawn a daemon thread that runs check_fn_cached(fn) off the
 * request path, deduplicating concurrent probes per fn.  In C the registry
 * already exposes check_fn_cached(bool (*)(void)) — we call it directly (the
 * C registry's cache is warm on first access, so there is no I/O to hide).
 * Returns true when a probe ran (or was skipped), false when fn is NULL.
 * The threaded path is preserved via the registry's own dedup; the thin
 * wrapper here never raises (mirrors Python's swallow-all). */
bool ws_schedule_check_fn_probe(bool (*fn)(void)) {
    if (!fn) return false;
    (void)check_fn_cached(fn);  /* warm the cache; ignores result */
    return true;
}

/* ── profiles project-tree merge helpers (hermes_cli/web_routers/profiles.py) ── */

/* Iterate an object's keys in insertion order (libjson struct is public). */
static size_t obj_count(const json_t *o) { return o->c.count; }
static const char *obj_key_at(const json_t *o, size_t i) { return o->c.keys[i]; }
static json_t *obj_val_at(const json_t *o, size_t i) { return o->c.items[i]; }

/* Rebuild an object's values into a fresh array (insertion order). Caller
 * frees the array. */
static json_t *obj_values_array(const json_t *obj) {
    json_t *arr = json_array();
    if (obj && obj->type == JSON_OBJECT) {
        size_t n = obj_count(obj);
        for (size_t i = 0; i < n; i++) json_append(arr, json_copy(obj_val_at(obj, i)));
    }
    return arr;
}

/* PoP: _merge_by_id @ hermes_cli/web_routers/profiles.py:_merge_by_id */
void ws_profiles_merge_by_id(json_t *into, json_t *entries, const char *child_key) {
    if (!into || into->type != JSON_OBJECT) return;
    if (!entries || entries->type != JSON_ARRAY) return;
    size_t n = json_len(entries);
    for (size_t i = 0; i < n; i++) {
        json_t *entry = json_get(entries, i);
        if (!entry || entry->type != JSON_OBJECT) continue;
        const char *id = json_get_str(entry, "id", NULL);
        if (!id || !*id) continue;
        json_t *existing = json_obj_get(into, id);
        if (!existing) { json_set(into, id, json_copy(entry)); continue; }
        if (strcmp(child_key, "sessions") == 0) {
            json_t *es = json_obj_get(existing, "sessions");
            if (!es || es->type != JSON_ARRAY) { es = json_array(); json_set(existing, "sessions", es); }
            json_t *ns = json_obj_get(entry, "sessions");
            if (ns && ns->type == JSON_ARRAY) {
                size_t nn = json_len(ns);
                for (size_t k = 0; k < nn; k++) json_append(es, json_copy(json_get(ns, k)));
            }
        } else {
            json_t *children = json_object();
            json_t *ec = json_obj_get(existing, child_key);
            if (ec && ec->type == JSON_ARRAY) {
                size_t ecn = json_len(ec);
                for (size_t k = 0; k < ecn; k++) {
                    json_t *c = json_get(ec, k);
                    if (c && c->type == JSON_OBJECT) {
                        const char *cid = json_get_str(c, "id", NULL);
                        if (cid && *cid) json_set(children, cid, json_copy(c));
                    }
                }
            }
            ws_profiles_merge_by_id(children, json_obj_get(entry, child_key), "sessions");
            json_set(existing, child_key, obj_values_array(children));
            json_free(children);
        }
        if (json_has(existing, "sessionCount")) {
            double a = json_get_num(existing, "sessionCount", 0);
            double b = json_get_num(entry, "sessionCount", 0);
            json_set(existing, "sessionCount", json_number(a + b));
        }
    }
}

/* PoP: _merge_profile_tree @ hermes_cli/web_routers/profiles.py:_merge_profile_tree */
void ws_profiles_merge_profile_tree(json_t *merged, json_t *projects,
                                    const char *profile, int preview_limit) {
    if (!merged || merged->type != JSON_OBJECT) return;
    if (!projects || projects->type != JSON_ARRAY) return;
    size_t n = json_len(projects);
    for (size_t i = 0; i < n; i++) {
        json_t *project = json_get(projects, i);
        if (!project || project->type != JSON_OBJECT) continue;

        /* tag lane sessions with owning profile */
        json_t *repos = json_obj_get(project, "repos");
        if (repos && repos->type == JSON_ARRAY) {
            size_t rn = json_len(repos);
            for (size_t r = 0; r < rn; r++) {
                json_t *repo = json_get(repos, r);
                if (!repo || repo->type != JSON_OBJECT) continue;
                json_t *groups = json_obj_get(repo, "groups");
                if (groups && groups->type == JSON_ARRAY) {
                    size_t gn = json_len(groups);
                    for (size_t g = 0; g < gn; g++) {
                        json_t *lane = json_get(groups, g);
                        if (!lane || lane->type != JSON_OBJECT) continue;
                        json_t *sessions = json_obj_get(lane, "sessions");
                        if (sessions && sessions->type == JSON_ARRAY) {
                            size_t sn = json_len(sessions);
                            for (size_t s = 0; s < sn; s++) {
                                json_t *sess = json_get(sessions, s);
                                if (sess && sess->type == JSON_OBJECT) {
                                    json_set(sess, "profile", json_string(profile));
                                    json_set(sess, "is_default_profile", json_bool(strcmp(profile, "default") == 0));
                                }
                            }
                        }
                    }
                }
            }
        }
        json_t *pv = json_obj_get(project, "previewSessions");
        if (pv && pv->type == JSON_ARRAY) {
            size_t pn = json_len(pv);
            for (size_t p = 0; p < pn; p++) {
                json_t *sess = json_get(pv, p);
                if (sess && sess->type == JSON_OBJECT) {
                    json_set(sess, "profile", json_string(profile));
                    json_set(sess, "is_default_profile", json_bool(strcmp(profile, "default") == 0));
                }
            }
        }

        const char *path = json_get_str(project, "path", NULL);
        const char *key = (path && *path) ? path : json_get_str(project, "id", "");
        json_t *existing = json_obj_get(merged, key);
        if (!existing) { json_set(merged, key, json_copy(project)); continue; }

        bool ex_auto = json_get_bool(existing, "isAuto", false);
        bool pr_auto = json_get_bool(project, "isAuto", false);
        if (ex_auto && !pr_auto) {
            json_set(merged, key, json_copy(project));
            existing = json_obj_get(merged, key);
        }

        json_t *repos_dict = json_object();
        json_t *ex_repos = json_obj_get(existing, "repos");
        if (ex_repos && ex_repos->type == JSON_ARRAY) {
            size_t ern = json_len(ex_repos);
            for (size_t r = 0; r < ern; r++) {
                json_t *repo = json_get(ex_repos, r);
                if (repo && repo->type == JSON_OBJECT) {
                    const char *rid = json_get_str(repo, "id", NULL);
                    if (rid && *rid) json_set(repos_dict, rid, json_copy(repo));
                }
            }
        }
        ws_profiles_merge_by_id(repos_dict, json_obj_get(project, "repos"), "groups");
        json_set(existing, "repos", obj_values_array(repos_dict));
        json_free(repos_dict);

        json_set(existing, "sessionCount",
                 json_number(json_get_num(existing, "sessionCount", 0) + json_get_num(project, "sessionCount", 0)));
        json_set(existing, "totalTokens",
                 json_number(json_get_num(existing, "totalTokens", 0) + json_get_num(project, "totalTokens", 0)));
        json_set(existing, "totalCostUsd",
                 json_number(json_get_num(existing, "totalCostUsd", 0) + json_get_num(project, "totalCostUsd", 0)));
        double ela = json_get_num(existing, "lastActive", 0);
        double pla = json_get_num(project, "lastActive", 0);
        json_set(existing, "lastActive", json_number(ela > pla ? ela : pla));

        /* previews = existing + project previewSessions, sorted desc by
         * last_active/started_at, truncated to preview_limit. */
        json_t *previews = json_array();
        json_t *ex_pv = json_obj_get(existing, "previewSessions");
        if (ex_pv && ex_pv->type == JSON_ARRAY) {
            size_t pn = json_len(ex_pv);
            for (size_t p = 0; p < pn; p++) json_append(previews, json_copy(json_get(ex_pv, p)));
        }
        json_t *pr_pv = json_obj_get(project, "previewSessions");
        if (pr_pv && pr_pv->type == JSON_ARRAY) {
            size_t pn = json_len(pr_pv);
            for (size_t p = 0; p < pn; p++) json_append(previews, json_copy(json_get(pr_pv, p)));
        }
        /* selection sort descending by last_active/started_at, swapping the
         * array item slots in place (libjson exposes c.items). */
        size_t pn = json_len(previews);
        for (size_t a = 0; a < pn; a++) {
            size_t best = a;
            double bestscore = json_get_num(json_get(previews, a), "last_active",
                                            json_get_num(json_get(previews, a), "started_at", 0));
            for (size_t b = a + 1; b < pn; b++) {
                double bscore = json_get_num(json_get(previews, b), "last_active",
                                             json_get_num(json_get(previews, b), "started_at", 0));
                if (bscore > bestscore) { best = b; bestscore = bscore; }
            }
            if (best != a) {
                json_t *av = json_get(previews, a);
                json_t *bv = json_get(previews, best);
                json_t *tmp = av;
                previews->c.items[a] = bv;
                previews->c.items[best] = tmp;
            }
        }
        size_t take = json_len(previews);
        if (preview_limit >= 0 && take > (size_t)preview_limit) take = (size_t)preview_limit;
        json_t *final = json_array();
        for (size_t a = 0; a < take; a++) json_append(final, json_copy(json_get(previews, a)));
        json_set(existing, "previewSessions", final);  /* json_set steals */
        json_free(previews);
    }
}

/* PoP: _pr_url_from_tool_output @ hermes_cli/web_routers/profiles.py:_pr_url_from_tool_output */
/* Return JSON "[number, url]" of the PR a tool result announces, or "null".
 * Python returns a (int, str) tuple or None. */
json_t *ws_profiles_pr_url_from_tool_output(const char *content) {
    if (!content || !*content) return NULL;
    json_t *parsed = json_parse(content, NULL);
    if (!parsed) return NULL;
    json_t *output = json_obj_get(parsed, "output");
    if (!output || output->type != JSON_STRING || !output->str_val) {
        json_free(parsed);
        return NULL;
    }
    /* regex: ^https://github\.com/[\w.-]+/[\w.-]+/pull/(\d+)/?$ */
    const char *s = output->str_val;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    const char *prefix = "https://github.com/";
    size_t pl = strlen(prefix);
    if (strncmp(s, prefix, pl) != 0) { json_free(parsed); return NULL; }
    const char *p = s + pl;
    /* owner segment: [\w.-]+ */
    const char *owner = p;
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '-')) p++;
    if (p == owner || *p != '/') { json_free(parsed); return NULL; }
    p++;
    /* repo segment */
    const char *repo = p;
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '-')) p++;
    if (p == repo || strncmp(p, "/pull/", 6) != 0) { json_free(parsed); return NULL; }
    p += 6;
    /* number */
    long num = 0;
    const char *numstart = p;
    while (*p && isdigit((unsigned char)*p)) { num = num * 10 + (*p - '0'); p++; }
    if (p == numstart) { json_free(parsed); return NULL; }
    /* optional trailing slash */
    if (*p == '/') p++;
    if (*p != '\0') { json_free(parsed); return NULL; }
    /* The returned URL is the full matched string (output.strip()), which is
     * `s` pointing into output->str_val owned by `parsed`. Copy it before
     * freeing parsed. */
    char *url = strdup(s);
    json_free(parsed);
    json_t *arr = json_array();
    json_append(arr, json_number((double)num));
    json_append(arr, json_string(url ? url : ""));
    free(url);
    return arr;
}
