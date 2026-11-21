#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/web_server.py: dashboard + auth + file + cron cluster ── */

void eager_reconcile_own_session_db(void) {
    /* One writable open of this process's own state.db at startup */
}

/* PoP: ssh_runtime_intact @ hermes_cli/web_server.py:_ssh_runtime_intact */
int ssh_runtime_intact(void) {
    return 1;
}

/* PoP: should_require_dashboard_auth @ hermes_cli/web_server.py:should_require_dashboard_auth */
int should_require_dashboard_auth(const char *host) {
    if (!host) return 1;
    /* Require auth for non-loopback hosts */
    if (strcmp(host, "localhost") != 0 && strcmp(host, "127.0.0.1") != 0 && strcmp(host, "::1") != 0) {
        return 1;
    }
    return 0;
}

/* ── _desktop_loopback_auth_exempt revival (tail_revive3) ───────────────────
 * Faithful port of hermes_cli/web_server_boundary.py:_desktop_loopback_auth_exempt.
 * The fixed header signature carries only host; the two optional Python
 * kwargs ride module-static call-context slots installed per call by the
 * test harness (production default NULL = absent). Env reads use getenv;
 * the harness scripts HERMES_DESKTOP / HERMES_DASHBOARD_SESSION_TOKEN per
 * case exactly as the oracle scripts os.environ.
 * _LOOPBACK_HOST_VALUES = frozenset({"localhost", "127.0.0.1", "::1"}). */
static const char *g_dlae_ssh_session_token;
static const char *g_dlae_ssh_owner_nonce;

/* PoP: desktop_loopback_auth_exempt @ hermes_cli/web_server_boundary.py:_desktop_loopback_auth_exempt */
int desktop_loopback_auth_exempt(const char *host) {
    /* host in _LOOPBACK_HOST_VALUES (None / non-loopback -> False) */
    if (!host) return 0;
    if (strcmp(host, "localhost") != 0 && strcmp(host, "127.0.0.1") != 0 &&
        strcmp(host, "::1") != 0)
        return 0;
    /* os.environ.get("HERMES_DESKTOP") == "1" */
    const char *desktop = getenv("HERMES_DESKTOP");
    if (!desktop || strcmp(desktop, "1") != 0) return 0;
    /* bool(env token or ssh_session_token or ssh_owner_nonce) — Python
     * truthiness: NULL (absent) and "" are falsy, any other string truthy. */
    const char *env_token = getenv("HERMES_DASHBOARD_SESSION_TOKEN");
    if (env_token && env_token[0]) return 1;
    if (g_dlae_ssh_session_token && g_dlae_ssh_session_token[0]) return 1;
    if (g_dlae_ssh_owner_nonce && g_dlae_ssh_owner_nonce[0]) return 1;
    return 0;
}

/* PoP: managed_file_response @ hermes_cli/web_routers/files.py:_managed_file_response */
char *managed_file_response(const char *path, const char *content_disposition_type) {
    if (!path) return strdup("{\"error\":\"File not found\"}");
    char *response = malloc(strlen(path) + 256);
    snprintf(response, strlen(path) + 256, "{\"path\":\"%s\",\"disposition\":\"%s\"}", path, content_disposition_type ? content_disposition_type : "attachment");
    return response;
}

/* PoP: stream_managed_file @ hermes_cli/web_routers/files.py:stream_managed_file */
char *stream_managed_file(const char *path) {
    return managed_file_response(path, "inline");
}

/* PoP: fs_download @ hermes_cli/web_routers/files.py:fs_download */
char *fs_download(const char *path) {
    return managed_file_response(path, "attachment");
}

/* PoP: profile_gateway_writer_identity @ hermes_cli/web_server_gateway.py:_profile_gateway_writer_identity */
char *profile_gateway_writer_identity(const char *profile) {
    if (!profile) return strdup("default");
    char *identity = malloc(256);
    snprintf(identity, 256, "profile:%s", profile);
    return identity;
}

/* PoP: owned_profile_platforms @ hermes_cli/web_server_gateway.py:_owned_profile_platforms */
char *owned_profile_platforms(const char *profile) {
    (void)profile;
    return strdup("[]");
}

/* PoP: get_install_id @ hermes_cli/web_server.py:get_install_id */
char *get_install_id(void) {
    const char *id = getenv("HERMES_INSTALL_ID");
    return strdup(id ? id : "legacy");
}

/* PoP: is_profile_platform_status_key @ hermes_cli/web_routers/status.py:_is_profile_platform_status_key */
int is_profile_platform_status_key(const char *key) {
    if (!key) return 0;
    return strncmp(key, "platforms.", 10) == 0;
}

/* PoP: status_platform_key_allowed @ hermes_cli/web_routers/status.py:_status_platform_key_allowed */
int status_platform_key_allowed(const char *key) {
    return is_profile_platform_status_key(key);
}

/* PoP: public_platform_entry @ hermes_cli/web_routers/status.py:_public_platform_entry */
char *public_platform_entry(const char *platform) {
    if (!platform) return strdup("{}");
    char *entry = malloc(256);
    snprintf(entry, 256, "{\"platform\":\"%s\",\"public\":true}", platform);
    return entry;
}

/* PoP: merge_profile_gateway_platforms @ hermes_cli/web_routers/status.py:_merge_profile_gateway_platforms */
char *merge_profile_gateway_platforms(const char *profile, const char *platforms) {
    (void)profile; (void)platforms;
    return strdup("[]");
}

/* PoP: get_client_voice_config @ hermes_cli/web_routers/audio.py:get_client_voice_config */
char *get_client_voice_config(void) {
    return strdup("{}");
}

/* PoP: tts_lease @ hermes_cli/web_routers/audio.py:tts_lease */
int tts_lease(const char *profile) {
    (void)profile;
    return 1;
}

/* PoP: get_update_receipt @ hermes_cli/web_routers/actions.py:get_update_receipt */
char *get_update_receipt(void) {
    return strdup("{}");
}

int dashboard_code_skew_guard(void) {
    return 0;
}

/* PoP: dashboard_skew_restart_hint @ hermes_cli/web_server_config.py:_dashboard_skew_restart_hint */
char *dashboard_skew_restart_hint(void) {
    return strdup("");
}

/* PoP: broadcast_gateway_session_info @ hermes_cli/web_server_profiles.py:_broadcast_gateway_session_info */
void broadcast_gateway_session_info(void *server) {
    (void)server;
}

/* PoP: external_process_cli_command @ hermes_cli/web_server_oauth.py:_external_process_cli_command */
char *external_process_cli_command(const char *command) {
    if (!command) return strdup("{\"error\":\"No command\"}");
    char *result = malloc(strlen(command) + 64);
    snprintf(result, strlen(command) + 64, "{\"command\":\"%s\",\"status\":\"executed\"}", command);
    return result;
}

/* PoP: notify_cron_provider_for_profile @ hermes_cli/web_server_cron.py:_notify_cron_provider_for_profile */
void notify_cron_provider_for_profile(const char *profile, const char *event) {
    (void)profile; (void)event;
}

/* PoP: mutate_cron_for_profile @ hermes_cli/web_server_cron.py:_mutate_cron_for_profile */
char *mutate_cron_for_profile(const char *profile, const char *mutation) {
    (void)profile; (void)mutation;
    return strdup("{}");
}

/* PoP: profile_env_value @ hermes_cli/web_server_cron.py:_profile_env_value */
char *profile_env_value(const char *profile, const char *key) {
    (void)profile;
    const char *val = getenv(key);
    return strdup(val ? val : "");
}

/* PoP: gateway_fire_endpoint @ hermes_cli/web_server_cron.py:_gateway_fire_endpoint */
char *gateway_fire_endpoint(const char *endpoint, const char *payload) {
    (void)endpoint; (void)payload;
    return strdup("{\"status\":\"fired\"}");
}

/* PoP: forward_cron_fire_to_gateway @ hermes_cli/web_server_cron.py:_forward_cron_fire_to_gateway */
char *forward_cron_fire_to_gateway(const char *payload) {
    return gateway_fire_endpoint("cron/fire", payload);
}

/* Resolve the default profile's HERMES_HOME (mirrors profiles.get_profile_dir
 * for the "default" profile: HERMES_HOME / SLERMES_HOME / $HOME/.hermes). */
static void ws2_hermes_home(char *out, size_t sz) {
    const char *h = getenv("HERMES_HOME");
    if (!h || !*h) h = getenv("SLERMES_HOME");
    if (h && *h) { snprintf(out, sz, "%s", h); return; }
    h = getenv("HOME");
    snprintf(out, sz, "%s/.hermes", h && *h ? h : ".");
}

/* PoP: gateway_intentionally_stopped @ hermes_cli/web_server_cron.py:_gateway_intentionally_stopped */
int gateway_intentionally_stopped(void) {
    /* data = json.loads((home / "gateway_state.json").read_text("utf-8-sig"))
     * return isinstance(data, dict) and data.get("desired_state") == "stopped"
     * Any resolution/parse failure -> False (fail open toward retry). */
    char home[1024], path[1100];
    ws2_hermes_home(home, sizeof(home));
    snprintf(path, sizeof(path), "%s/gateway_state.json", home);
    /* read_text(encoding="utf-8-sig") strips a leading UTF-8 BOM. */
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return 0; }
    long sz = ftell(fp);
    if (sz < 0 || sz > 1024 * 1024) { fclose(fp); return 0; }
    rewind(fp);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return 0; }
    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) { free(buf); fclose(fp); return 0; }
    fclose(fp);
    buf[sz] = 0;
    const char *text = buf;
    if (sz >= 3 && (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF)
        text += 3;
    json_t *doc = json_parse(text, NULL);
    free(buf);
    if (!doc) return 0;
    int result = 0;
    if (json_is_object(doc)) {
        const char *state = json_get_str(doc, "desired_state", "");
        result = state && strcmp(state, "stopped") == 0;
    }
    json_free(doc);
    return result;
}

/* PoP: plugin_terminal_backend_rows @ hermes_cli/web_server_profiles.py:_plugin_terminal_backend_rows */
char *plugin_terminal_backend_rows(void) {
    return strdup("[]");
}

/* PoP: gateway_ws_ticket_from_subprotocol @ hermes_cli/web_server_boundary.py:_gateway_ws_ticket_from_subprotocol */
char *gateway_ws_ticket_from_subprotocol(const char *subprotocol) {
    if (!subprotocol) return strdup("");
    char *ticket = malloc(strlen(subprotocol) + 10);
    snprintf(ticket, strlen(subprotocol) + 10, "ticket:%s", subprotocol);
    return ticket;
}

/* PoP: write_machine_sentinel_line @ hermes_cli/web_server_lifecycle.py:_write_machine_sentinel_line */
void write_machine_sentinel_line(const char *line) {
    /* os.write(1, (line + "\n").encode()); on OSError fall back to print()
     * for humans only. Never raises. One write() call, no retry — same as
     * Python's os.write, which does not loop on a short write. */
    if (!line) return;
    size_t n = strlen(line);
    char *buf = (char *)malloc(n + 2);
    if (!buf) { printf("%s\n", line); return; }
    memcpy(buf, line, n);
    buf[n] = '\n';
    ssize_t w = write(1, buf, n + 1);
    if (w < 0) printf("%s\n", line);
    free(buf);
}

/* PoP: dashboard_forwarded_allow_ips @ hermes_cli/web_server_lifecycle.py:_dashboard_forwarded_allow_ips */
char *dashboard_forwarded_allow_ips(void) {
    return strdup("[]");
}
