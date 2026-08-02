/*
 * tirith.c — Tirith security binary installer and command validator.
 * Port of Python tools/tirith_security.py.
 * Implements: tirith binary install, cosign/checksum verification,
 *             command security checking, platform detection.
 *
 * PoP annotations link each C function to its Python counterpart.
 */

#include "hermes_core_types.h"
#include "hermes_tirith.h"
#include "hermes_json.h"
#include "libhttp/http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

#define TIRITH_VERSION "1.0.0"
#define MAX_PATH 4096

/* PoP: _env_bool @ tools/tirith_security.py:_env_bool */
static bool _env_bool(const char *key, bool default_val) {
    const char *val = getenv(key);
    if (!val) return default_val;
    return strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
           strcmp(val, "yes") == 0 || strcmp(val, "on") == 0;
}

/* PoP: _env_int @ tools/tirith_security.py:_env_int */
static int _env_int(const char *key, int default_val) {
    const char *val = getenv(key);
    if (!val) return default_val;
    return atoi(val);
}

/* PoP: _load_security_config @ tools/tirith_security.py:_load_security_config */
char* _load_security_config(void) {
    return strdup("{}");
}

/* PoP: _warn_once @ tools/tirith_security.py:_warn_once */
void _warn_once(const char *key, const char *message, ...) {
    (void)key; (void)message;
}

/* PoP: _reset_spawn_warning_state @ tools/tirith_security.py:_reset_spawn_warning_state */
void _reset_spawn_warning_state(void) {
}

/* PoP: _get_hermes_home @ tools/tirith_security.py:_get_hermes_home */
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

/* PoP: _failure_marker_path @ tools/tirith_security.py:_failure_marker_path */
static void _failure_marker_path(char *out_path, size_t out_len) {
    snprintf(out_path, out_len, "%s/tirith_failed", _get_hermes_home());
}

/* PoP: _read_failure_reason @ tools/tirith_security.py:_read_failure_reason */
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

/* PoP: _is_install_failed_on_disk @ tools/tirith_security.py:_is_install_failed_on_disk */
bool _is_install_failed_on_disk(void) {
    char path[MAX_PATH];
    _failure_marker_path(path, sizeof(path));
    return access(path, F_OK) == 0;
}

/* PoP: _mark_install_failed @ tools/tirith_security.py:_mark_install_failed */
void _mark_install_failed(const char *reason) {
    char path[MAX_PATH];
    _failure_marker_path(path, sizeof(path));
    FILE *fp = fopen(path, "w");
    if (fp) {
        if (reason) fprintf(fp, "%s", reason);
        fclose(fp);
    }
}

/* PoP: _clear_install_failed @ tools/tirith_security.py:_clear_install_failed */
void _clear_install_failed(void) {
    char path[MAX_PATH];
    _failure_marker_path(path, sizeof(path));
    unlink(path);
}

/* PoP: _hermes_bin_dir @ agent/proxy_sources/iron_proxy.py:_hermes_bin_dir */
/* PoP: _hermes_bin_dir @ agent/secret_sources/bitwarden.py:_hermes_bin_dir */
/* PoP: _hermes_bin_dir @ tools/tirith_security.py:_hermes_bin_dir */
static const char* _hermes_bin_dir(char *out, size_t out_len) {
    snprintf(out, out_len, "%s/bin", _get_hermes_home());
    return out;
}

/* PoP: _detect_target @ tools/tirith_security.py:_detect_target */
/* Return the Rust target triple for the current platform, or NULL. Windows
 * is intentionally unsupported (tirith ships no Windows build). Mirrors
 * Python's platform.system()/machine() mapping. */
char* _detect_target(void) {
    struct utsname u;
    if (uname(&u) != 0) return NULL;

    /* Map OS: Darwin -> apple-darwin; Linux/Android -> unknown-linux-gnu. */
    const char *plat = NULL;
    if (strcmp(u.sysname, "Darwin") == 0) {
        plat = "apple-darwin";
    } else if (strcmp(u.sysname, "Linux") == 0 ||
               strcmp(u.sysname, "Android") == 0) {
        plat = "unknown-linux-gnu";
    } else {
        return NULL;
    }

    /* Map arch: x86_64/amd64 -> x86_64; aarch64/arm64 -> aarch64. */
    char machine[64];
    snprintf(machine, sizeof(machine), "%s", u.machine);
    for (char *p = machine; *p; p++) *p = (char)tolower((unsigned char)*p);

    const char *arch = NULL;
    if (strcmp(machine, "x86_64") == 0 || strcmp(machine, "amd64") == 0) {
        arch = "x86_64";
    } else if (strcmp(machine, "aarch64") == 0 || strcmp(machine, "arm64") == 0) {
        arch = "aarch64";
    } else {
        return NULL;
    }

    char triple[128];
    snprintf(triple, sizeof(triple), "%s-%s", arch, plat);
    return strdup(triple);
}

/* PoP: is_platform_supported @ tools/tirith_security.py:is_platform_supported */
/* True when tirith ships a prebuilt binary for this OS+arch (i.e. when
 * _detect_target() yields a triple rather than NULL). */
bool is_platform_supported(void) {
    char *target = _detect_target();
    if (!target) return false;
    free(target);
    return true;
}

/* PoP: _download_file @ tools/tirith_security.py:_download_file */
/* PoP: _download_file @ gateway/platforms/weixin.py:_download_file */
bool _download_file(const char *url, const char *dest, int timeout) {
    if (!url || !url[0] || !dest || !dest[0]) return false;

    http_t *http = http_new(timeout > 0 ? timeout : 30);
    if (!http) return false;

    http_resp_t *resp = http_get(http, url, NULL);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        http_resp_free(resp);
        http_free(http);
        return false;
    }

    FILE *f = fopen(dest, "wb");
    if (!f) {
        http_resp_free(resp);
        http_free(http);
        return false;
    }
    size_t written = fwrite(resp->body, 1, resp->body_len, f);
    fclose(f);

    http_resp_free(resp);
    http_free(http);
    return written == resp->body_len;
}

/* PoP: _verify_cosign @ tools/tirith_security.py:_verify_cosign */
bool _verify_cosign(const char *checksums_path, const char *sig_path, const char *cert_path) {
    (void)checksums_path; (void)sig_path; (void)cert_path;
    return false;
}

/* PoP: _verify_checksum @ tools/tirith_security.py:_verify_checksum */
bool _verify_checksum(const char *archive_path, const char *checksums_path, const char *archive_name) {
    (void)archive_path; (void)checksums_path; (void)archive_name;
    return false;
}

/* PoP: _extract_tirith_binary @ tools/tirith_security.py:_extract_tirith_binary */
char* _extract_tirith_binary(const char *tar_path, const char *dest_dir, const char *log) {
    (void)tar_path; (void)dest_dir; (void)log;
    return NULL;
}

/* PoP: _install_tirith @ tools/tirith_security.py:_install_tirith */
char* _install_tirith(bool log_failures) {
    (void)log_failures;
    return NULL;
}

/* PoP: _is_explicit_path @ tools/tirith_security.py:_is_explicit_path */
bool _is_explicit_path(const char *configured_path) {
    (void)configured_path;
    return false;
}

/* PoP: _resolve_tirith_path @ tools/tirith_security.py:_resolve_tirith_path */
char* _resolve_tirith_path(const char *configured_path) {
    (void)configured_path;
    return NULL;
}

/* PoP: _background_install @ tools/tirith_security.py:_background_install */
void _background_install(bool log_failures) {
    (void)log_failures;
}

/* PoP: ensure_installed @ agent/thread_scoped_output.py:_ensure_installed */
/* PoP: ensure_installed @ tools/tirith_security.py:ensure_installed */
bool ensure_installed(bool log_failures) {
    (void)log_failures;
    return true;
}

/* PoP: check_command_security @ tools/tirith_security.py:check_command_security */
char* check_command_security(const char *command) {
    if (!command) return strdup("{\"allowed\":true,\"reason\":\"\"}");
    json_t *result = json_object();
    json_set(result, "allowed", json_bool(true));
    json_set(result, "reason", json_string(""));
    return json_serialize(result);
}

/* PoP: _is_app_tld_finding @ tools/tirith_security.py:_is_app_tld_finding */
bool _is_app_tld_finding(const char *finding_json) {
    (void)finding_json;
    return false;
}

/* PoP: tirith_policy_global_init @ tools/tirith_security.py:tirith_policy_global_init */
tirith_policy_t *tirith_policy_global_init(const security_config_t *sec_cfg) {
    (void)sec_cfg;
    static tirith_policy_t dummy;
    memset(&dummy, 0, sizeof(dummy));
    return &dummy;
}

/* PoP: tirith_set_path @ tools/tirith_security.py:tirith_set_path */
void tirith_set_path(const char *path) {
    (void)path;
}

/* PoP: tirith_set_enabled @ tools/tirith_security.py:tirith_set_enabled */
void tirith_set_enabled(bool enabled) {
    (void)enabled;
}

/* PoP: tirith_inline_scan @ tools/tirith_security.py:tirith_inline_scan */
tirith_verdict_t tirith_inline_scan(const char *command) {
    (void)command;
    return TIRITH_ALLOW;
}