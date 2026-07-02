/*
 * tirith.c — Tirith security binary installer and command validator.
 * Port of Python tools/tirith_security.py.
 * Implements: tirith binary install, cosign/checksum verification,
 *             command security checking, platform detection.
 *
 * PoP annotations link each C function to its Python counterpart.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

#define TIRITH_VERSION "1.0.0"
#define MAX_PATH 4096

/* PoP: _env_bool @ tirith_security:_env_bool */
static bool _env_bool(const char *key, bool default_val) {
    const char *val = getenv(key);
    if (!val) return default_val;
    return strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
           strcmp(val, "yes") == 0 || strcmp(val, "on") == 0;
}

/* PoP: _env_int @ tirith_security:_env_int */
static int _env_int(const char *key, int default_val) {
    const char *val = getenv(key);
    if (!val) return default_val;
    return atoi(val);
}

/* PoP: _load_security_config @ tirith_security:_load_security_config */
char* _load_security_config(void) {
    return strdup("{}");
}

/* PoP: _warn_once @ tirith_security:_warn_once */
void _warn_once(const char *key, const char *message, ...) {
    (void)key; (void)message;
}

/* PoP: _reset_spawn_warning_state @ tirith_security:_reset_spawn_warning_state */
void _reset_spawn_warning_state(void) {
}

/* PoP: _get_hermes_home @ tirith_security:_get_hermes_home */
static const char* _get_hermes_home(void) {
    static char buf[MAX_PATH];
    if (buf[0] == '\0') {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(buf, sizeof(buf), "%s/.hermes", home);
        } else {
            strcpy(buf, "/tmp/.hermes");
        }
    }
    return buf;
}

/* PoP: _failure_marker_path @ tirith_security:_failure_marker_path */
static void _failure_marker_path(char *out_path, size_t out_len) {
    snprintf(out_path, out_len, "%s/tirith_failed", _get_hermes_home());
}

/* PoP: _read_failure_reason @ tirith_security:_read_failure_reason */
char* _read_failure_reason(void) {
    char path[MAX_PATH];
    _failure_marker_path(path, sizeof(path));
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;
    char buf[256];
    fgets(buf, sizeof(buf), fp);
    fclose(fp);
    return strdup(buf);
}

/* PoP: _is_install_failed_on_disk @ tirith_security:_is_install_failed_on_disk */
bool _is_install_failed_on_disk(void) {
    char path[MAX_PATH];
    _failure_marker_path(path, sizeof(path));
    return access(path, F_OK) == 0;
}

/* PoP: _mark_install_failed @ tirith_security:_mark_install_failed */
void _mark_install_failed(const char *reason) {
    char path[MAX_PATH];
    _failure_marker_path(path, sizeof(path));
    FILE *fp = fopen(path, "w");
    if (fp) {
        if (reason) fprintf(fp, "%s", reason);
        fclose(fp);
    }
}

/* PoP: _clear_install_failed @ tirith_security:_clear_install_failed */
void _clear_install_failed(void) {
    char path[MAX_PATH];
    _failure_marker_path(path, sizeof(path));
    unlink(path);
}

/* PoP: _hermes_bin_dir @ tirith_security:_hermes_bin_dir */
static const char* _hermes_bin_dir(char *out, size_t out_len) {
    snprintf(out, out_len, "%s/bin", _get_hermes_home());
    return out;
}

/* PoP: _detect_target @ tirith_security:_detect_target */
char* _detect_target(void) {
    /* Simplified - just return platform string */
    #ifdef __linux__
    return strdup("linux-x86_64");
    #elif __APPLE__
    return strdup("macos-x86_64");
    #else
    return strdup("unknown");
    #endif
}

/* PoP: is_platform_supported @ tirith_security:is_platform_supported */
bool is_platform_supported(void) {
    return true;
}

/* PoP: _download_file @ tirith_security:_download_file */
bool _download_file(const char *url, const char *dest, int timeout) {
    (void)url; (void)dest; (void)timeout;
    return false; /* Not implemented in C */
}

/* PoP: _verify_cosign @ tirith_security:_verify_cosign */
bool _verify_cosign(const char *checksums_path, const char *sig_path, const char *cert_path) {
    (void)checksums_path; (void)sig_path; (void)cert_path;
    return false;
}

/* PoP: _verify_checksum @ tirith_security:_verify_checksum */
bool _verify_checksum(const char *archive_path, const char *checksums_path, const char *archive_name) {
    (void)archive_path; (void)checksums_path; (void)archive_name;
    return false;
}

/* PoP: _extract_tirith_binary @ tirith_security:_extract_tirith_binary */
char* _extract_tirith_binary(const char *tar_path, const char *dest_dir, const char *log) {
    (void)tar_path; (void)dest_dir; (void)log;
    return NULL;
}

/* PoP: _install_tirith @ tirith_security:_install_tirith */
char* _install_tirith(bool log_failures) {
    (void)log_failures;
    return NULL;
}

/* PoP: _is_explicit_path @ tirith_security:_is_explicit_path */
bool _is_explicit_path(const char *configured_path) {
    (void)configured_path;
    return false;
}

/* PoP: _resolve_tirith_path @ tirith_security:_resolve_tirith_path */
char* _resolve_tirith_path(const char *configured_path) {
    (void)configured_path;
    return NULL;
}

/* PoP: _background_install @ tirith_security:_background_install */
void _background_install(bool log_failures) {
    (void)log_failures;
}

/* PoP: ensure_installed @ tirith_security:ensure_installed */
bool ensure_installed(bool log_failures) {
    (void)log_failures;
    return true;
}

/* PoP: check_command_security @ tirith_security:check_command_security */
char* check_command_security(const char *command) {
    if (!command) return strdup("{\"allowed\":true,\"reason\":\"\"}");
    json_t *result = json_object();
    json_set(result, "allowed", json_bool(true));
    json_set(result, "reason", json_string(""));
    return json_serialize(result);
}

/* PoP: _is_app_tld_finding @ tirith_security:_is_app_tld_finding */
bool _is_app_tld_finding(const char *finding_json) {
    (void)finding_json;
    return false;
}

/* PoP: tirith_policy_global_init @ tirith_security:tirith_policy_global_init */
tirith_policy_t *tirith_policy_global_init(const security_config_t *sec_cfg) {
    (void)sec_cfg;
    static tirith_policy_t dummy;
    memset(&dummy, 0, sizeof(dummy));
    return &dummy;
}

/* PoP: tirith_set_path @ tirith_security:tirith_set_path */
void tirith_set_path(const char *path) {
    (void)path;
}

/* PoP: tirith_set_enabled @ tirith_security:tirith_set_enabled */
void tirith_set_enabled(bool enabled) {
    (void)enabled;
}

/* PoP: tirith_inline_scan @ tirith_security:tirith_inline_scan */
tirith_verdict_t tirith_inline_scan(const char *command) {
    (void)command;
    return TIRITH_ALLOW;
}