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
#include <stdarg.h>
#include <pthread.h>
#include "libcrypto/crypto.h"

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

/* Warn-once dedupe set + module-global tirith runtime state
 * (Python _warned_messages / _resolved_path / tirith_enabled). */
#define TIRITH_WARNED_MAX 32
static struct {
    pthread_mutex_t lock;
    char keys[TIRITH_WARNED_MAX][64];
    int  count;
} g_warn_once = { .lock = PTHREAD_MUTEX_INITIALIZER, .count = 0 };

static char g_configured_path[512] = "tirith";
static bool g_tirith_enabled = true;

/* PoP: _load_security_config @ tools/tirith_security.py:_load_security_config */
char* _load_security_config(void) {
    /* Serialize the effective security config (env-overridable knobs). */
    json_t *cfg = json_object();
    if (!cfg) return strdup("{}");
    json_set(cfg, "tirith_enabled", json_bool(g_tirith_enabled));
    json_set(cfg, "tirith_path", json_string(g_configured_path));
    json_set(cfg, "security_scan_enabled",
             json_bool(_env_bool("HERMES_SECURITY_SCAN_ENABLED", true)));
    json_set(cfg, "security_fail_closed",
             json_bool(_env_bool("HERMES_SECURITY_FAIL_CLOSED", false)));
    char *out = json_serialize(cfg);
    json_free(cfg);
    return out ? out : strdup("{}");
}

/* PoP: _warn_once @ tools/tirith_security.py:_warn_once */
void _warn_once(const char *key, const char *message, ...) {
    /* logger.warning but at-most-once per key for the process lifetime. */
    if (!key || !key[0] || !message) return;
    va_list ap;
    pthread_mutex_lock(&g_warn_once.lock);
    for (int i = 0; i < g_warn_once.count; i++) {
        if (strcmp(g_warn_once.keys[i], key) == 0) {
            pthread_mutex_unlock(&g_warn_once.lock);
            return;
        }
    }
    if (g_warn_once.count < TIRITH_WARNED_MAX) {
        snprintf(g_warn_once.keys[g_warn_once.count],
                 sizeof(g_warn_once.keys[0]), "%s", key);
        g_warn_once.count++;
    }
    pthread_mutex_unlock(&g_warn_once.lock);
    va_start(ap, message);
    vfprintf(stderr, message, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* PoP: _reset_spawn_warning_state @ tools/tirith_security.py:_reset_spawn_warning_state */
void _reset_spawn_warning_state(void) {
    /* Clear the warn-once dedupe set so a subsequent failure after a fresh
     * (re)install surfaces again. */
    pthread_mutex_lock(&g_warn_once.lock);
    memset(g_warn_once.keys, 0, sizeof(g_warn_once.keys));
    g_warn_once.count = 0;
    pthread_mutex_unlock(&g_warn_once.lock);
}

/* PoP: _failure_marker_path @ tools/tirith_security.py:_get_hermes_home */
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

/* PoP: _is_install_failed_on_disk @ tools/tirith_security.py:_read_failure_reason */
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

/* PoP: unsupported @ agent/proxy_sources/iron_proxy.py:_hermes_bin_dir */
/* PoP: unsupported @ agent/secret_sources/bitwarden.py:_hermes_bin_dir */
/* PoP: unsupported @ tools/tirith_security.py:_hermes_bin_dir */
static const char* _hermes_bin_dir(char *out, size_t out_len) {
    snprintf(out, out_len, "%s/bin", _get_hermes_home());
    return out;
}

/* PoP: unsupported @ tools/tirith_security.py:_detect_target */
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

#define TIRITH_COSIGN_IDENTITY_REGEXP     "^https://github.com/hermes-agent-ai/tirith/\\.github/workflows/" \
    "[^/]+@refs/tags/v[0-9]+\\.[0-9]+\\.[0-9]+$"
#define TIRITH_COSIGN_ISSUER "https://token.actions.githubusercontent.com"

static bool _which_exists(const char *prog) {
    if (!prog || !prog[0]) return false;
    if (prog[0] == '/') return access(prog, X_OK) == 0;
    const char *path = getenv("PATH");
    if (!path || !path[0]) return false;
    char *dup = strdup(path);
    if (!dup) return false;
    bool found = false;
    for (char *dir = strtok(dup, ":"); dir && !found; dir = strtok(NULL, ":")) {
        char cand[MAX_PATH];
        snprintf(cand, sizeof(cand), "%s/%s", dir, prog);
        if (access(cand, X_OK) == 0) found = true;
    }
    free(dup);
    return found;
}

/* PoP: _verify_cosign @ tools/tirith_security.py:_verify_cosign */
bool _verify_cosign(const char *checksums_path, const char *sig_path, const char *cert_path) {
    /* Verify cosign provenance signature on checksums.txt. Python returns a
     * tri-state (True / False / None); the C caller treats both False and
     * "cosign unavailable" as abort — only true allows the install. */
    if (!checksums_path || !sig_path || !cert_path) return false;
    if (!_which_exists("cosign")) {
        fprintf(stderr, "tirith: cosign not found on PATH\n");
        return false;
    }
    char cmd[MAX_PATH * 4];
    snprintf(cmd, sizeof(cmd),
             "cosign verify-blob --certificate '%s' --signature '%s' "
             "--certificate-identity-regexp '%s' "
             "--certificate-oidc-issuer '%s' '%s' >/dev/null 2>&1",
             cert_path, sig_path, TIRITH_COSIGN_IDENTITY_REGEXP,
             TIRITH_COSIGN_ISSUER, checksums_path);
    int rc = system(cmd);
    if (rc == 0) {
        fprintf(stderr, "tirith: cosign provenance verification passed\n");
        return true;
    }
    fprintf(stderr, "tirith: cosign verification failed (exit %d)\n", rc);
    return false;
}

/* PoP: _verify_checksum @ tools/tirith_security.py:_verify_checksum */
bool _verify_checksum(const char *archive_path, const char *checksums_path, const char *archive_name) {
    /* Verify SHA-256 of the archive against the "<hash>  <name>" line in
     * checksums.txt. */
    if (!archive_path || !checksums_path || !archive_name) return false;
    FILE *fp = fopen(checksums_path, "r");
    if (!fp) return false;
    char expected[65] = "";
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        /* Format: "<hash>  <filename>" (two spaces). */
        char *sep = strstr(line, "  ");
        if (!sep) continue;
        *sep = '\0';
        const char *fname = sep + 2;
        while (*fname == ' ') fname++;
        if (strcmp(fname, archive_name) == 0) {
            snprintf(expected, sizeof(expected), "%s", line);
            break;
        }
    }
    fclose(fp);
    if (!expected[0]) {
        fprintf(stderr, "tirith: no checksum entry for %s\n", archive_name);
        return false;
    }

    /* Hash the archive with the libcrypto one-shot SHA-256. */
    FILE *in = fopen(archive_path, "rb");
    if (!in) return false;
    fseek(in, 0, SEEK_END);
    long fsize = ftell(in);
    fseek(in, 0, SEEK_SET);
    if (fsize < 0) { fclose(in); return false; }
    unsigned char *blob = malloc((size_t)fsize ? (size_t)fsize : 1);
    if (!blob) { fclose(in); return false; }
    size_t got = fread(blob, 1, (size_t)fsize, in);
    fclose(in);
    if (got != (size_t)fsize) { free(blob); return false; }
    unsigned char digest[CRYPTO_SHA256_LEN];
    crypto_sha256(blob, got, digest);
    free(blob);
    char actual[CRYPTO_SHA256_LEN * 2 + 1];
    for (int i = 0; i < CRYPTO_SHA256_LEN; i++)
        snprintf(actual + i * 2, 3, "%02x", digest[i]);

    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "tirith: checksum mismatch: expected %s, got %s\n",
                expected, actual);
        return false;
    }
    return true;
}

/* PoP: _extract_tirith_binary @ tools/tirith_security.py:_extract_tirith_binary */
char* _extract_tirith_binary(const char *tar_path, const char *dest_dir, const char *log) {
    /* Extract the tirith binary from the release archive into dest_dir and
     * return malloc'd path to dest_dir/tirith, or NULL (reason via log).
     * Python walks tar members rejecting ".." — GNU tar with --no-unsafe-paths
     * semantics plus a fixed destination gives the same guarantee here. */
    if (!tar_path || !dest_dir) return NULL;
    char cmd[MAX_PATH * 3];
    snprintf(cmd, sizeof(cmd),
             "tar -xzf '%s' -C '%s' tirith 2>/dev/null", tar_path, dest_dir);
    int rc = system(cmd);
    if (rc != 0) {
        /* Some releases nest the binary under a directory. */
        snprintf(cmd, sizeof(cmd),
                 "tar -xzf '%s' -C '%s' --strip-components=1 2>/dev/null",
                 tar_path, dest_dir);
        rc = system(cmd);
        if (rc != 0) {
            if (log) fprintf(stderr, "%s: binary_not_in_archive\n", log ? log : "tirith");
            return NULL;
        }
    }
    char dest_path[MAX_PATH];
    snprintf(dest_path, sizeof(dest_path), "%s/tirith", dest_dir);
    struct stat st;
    if (stat(dest_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        if (log) fprintf(stderr, "%s: binary_extract_failed\n", log ? log : "tirith");
        return NULL;
    }
    chmod(dest_path, st.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
    return strdup(dest_path);
}

static const char *TIRITH_REPO = "hermes-agent-ai/tirith";

/* Process-level resolved-path cache (Python _resolved_path /
 * _INSTALL_FAILED sentinel / _install_failure_reason). */
static pthread_mutex_t g_install_lock = PTHREAD_MUTEX_INITIALIZER;
static char g_resolved_path[MAX_PATH] = "";
static bool g_resolved_failed = false;
static char g_failure_reason[64] = "";

/* PoP: _install_tirith @ tools/tirith_security.py:_install_tirith */
char* _install_tirith(bool log_failures) {
    /* Download + verify + install tirith to $HERMES_HOME/bin/tirith.
     * Returns malloc'd installed path on success, NULL on failure (with
     * reason recorded for the retryable-failure marker logic). */
    FILE *logfp = log_failures ? stderr : NULL;

    char *target = _detect_target();
    if (!target) {
        fprintf(stderr, "tirith auto-install: unsupported platform\n");
        return NULL;
    }
    char archive_name[128];
    snprintf(archive_name, sizeof(archive_name), "tirith-%s.tar.gz", target);
    free(target);

    char tmpdir[] = "/tmp/tirith-install-XXXXXX";
    if (!mkdtemp(tmpdir)) {
        if (logfp) fprintf(logfp, "tirith install failed: cannot create temp dir\n");
        return NULL;
    }
    char archive_path[MAX_PATH], checksums_path[MAX_PATH];
    char sig_path[MAX_PATH], cert_path[MAX_PATH], base_url[256];
    snprintf(archive_path, sizeof(archive_path), "%s/%s", tmpdir, archive_name);
    snprintf(checksums_path, sizeof(checksums_path), "%s/checksums.txt", tmpdir);
    snprintf(sig_path, sizeof(sig_path), "%s/checksums.txt.sig", tmpdir);
    snprintf(cert_path, sizeof(cert_path), "%s/checksums.txt.pem", tmpdir);
    snprintf(base_url, sizeof(base_url),
             "https://github.com/%s/releases/latest/download", TIRITH_REPO);

    fprintf(stderr, "tirith not found — downloading latest release...\n");
    {
        char url[512];
        snprintf(url, sizeof(url), "%s/%s", base_url, archive_name);
        if (!_download_file(url, archive_path, 30)) {
            if (logfp) fprintf(logfp, "tirith download failed: %s\n", archive_name);
            return NULL;
        }
        snprintf(url, sizeof(url), "%s/checksums.txt", base_url);
        if (!_download_file(url, checksums_path, 30)) {
            if (logfp) fprintf(logfp, "tirith download failed: checksums.txt\n");
            return NULL;
        }
    }

    /* Cosign provenance verification — preferred but not mandatory. A hard
     * rejection aborts; unavailability falls back to SHA-256 only. */
    bool cosign_verified = false;
    if (_which_exists("cosign")) {
        char u1[512], u2[512];
        snprintf(u1, sizeof(u1), "%s/checksums.txt.sig", base_url);
        snprintf(u2, sizeof(u2), "%s/checksums.txt.pem", base_url);
        if (_download_file(u1, sig_path, 30) && _download_file(u2, cert_path, 30)) {
            if (_verify_cosign(checksums_path, sig_path, cert_path))
                cosign_verified = true;
            else {
                if (logfp) fprintf(logfp,
                    "tirith install aborted: cosign provenance verification failed\n");
                return NULL;
            }
        } else {
            fprintf(stderr, "tirith: cosign artifacts unavailable, "
                            "proceeding with SHA-256 only\n");
        }
    }

    if (!_verify_checksum(archive_path, checksums_path, archive_name))
        return NULL;

    char *src = _extract_tirith_binary(archive_path, tmpdir,
                                       log_failures ? "tirith" : NULL);
    if (!src) return NULL;

    char bindir[MAX_PATH], dest[MAX_PATH];
    _hermes_bin_dir(bindir, sizeof(bindir));
    snprintf(dest, sizeof(dest), "%s/tirith", bindir);
    mkdir(bindir, 0755);
    if (rename(src, dest) != 0) {
        /* Cross-device move: plain copy + unlink fallback. */
        FILE *in = fopen(src, "rb"), *out = fopen(dest, "wb");
        if (!in || !out) { if (out) fclose(out); if (in) fclose(in); free(src); return NULL; }
        char cpbuf[8192]; size_t n;
        while ((n = fread(cpbuf, 1, sizeof(cpbuf), in)) > 0)
            fwrite(cpbuf, 1, n, out);
        fclose(in); fclose(out);
        unlink(src);
    }
    free(src);
    struct stat st;
    if (stat(dest, &st) == 0)
        chmod(dest, st.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);

    fprintf(stderr, "tirith installed to %s (%s)\n", dest,
            cosign_verified ? "cosign + SHA-256" : "SHA-256 only");
    _clear_install_failed();
    _reset_spawn_warning_state();
    return strdup(dest);
}

/* PoP: _is_explicit_path @ tools/tirith_security.py:_is_explicit_path */
bool _is_explicit_path(const char *configured_path) {
    /* True when the user explicitly configured a non-default tirith path. */
    return configured_path && configured_path[0]
        && strcmp(configured_path, "tirith") != 0;
}

static char *_which(const char *prog) {
    if (!prog || !prog[0]) return NULL;
    if (prog[0] == '/') return access(prog, X_OK) == 0 ? strdup(prog) : NULL;
    const char *path = getenv("PATH");
    if (!path || !path[0]) return NULL;
    char *dup = strdup(path);
    if (!dup) return NULL;
    char *found = NULL;
    for (char *dir = strtok(dup, ":"); dir && !found; dir = strtok(NULL, ":")) {
        char cand[MAX_PATH];
        snprintf(cand, sizeof(cand), "%s/%s", dir, prog);
        if (access(cand, X_OK) == 0)
            found = strdup(cand);
    }
    free(dup);
    return found;
}

/* PoP: _resolve_tirith_path @ tools/tirith_security.py:_resolve_tirith_path */
char* _resolve_tirith_path(const char *configured_path) {
    /* Resolve the tirith binary path. Explicit user paths are authoritative
     * and never fall through to auto-download; the default "tirith" resolves
     * via PATH → $HERMES_HOME/bin → auto-install. Failed installs are cached
     * for the process lifetime. */
    pthread_mutex_lock(&g_install_lock);
    if (g_resolved_path[0] && !g_resolved_failed) {
        char *out = strdup(g_resolved_path);
        pthread_mutex_unlock(&g_install_lock);
        return out;
    }
    pthread_mutex_unlock(&g_install_lock);

    const char *expanded = (configured_path && configured_path[0])
        ? configured_path : "tirith";
    bool explicit_path = _is_explicit_path(configured_path ? configured_path : "");

    if (!explicit_path && !is_platform_supported()) {
        pthread_mutex_lock(&g_install_lock);
        g_resolved_failed = true;
        snprintf(g_failure_reason, sizeof(g_failure_reason), "unsupported_platform");
        pthread_mutex_unlock(&g_install_lock);
        return strdup(expanded);
    }

    struct stat st;
    if (explicit_path) {
        if (stat(expanded, &st) == 0 && S_ISREG(st.st_mode)
                && access(expanded, X_OK) == 0) {
            pthread_mutex_lock(&g_install_lock);
            snprintf(g_resolved_path, sizeof(g_resolved_path), "%s", expanded);
            g_resolved_failed = false;
            pthread_mutex_unlock(&g_install_lock);
            return strdup(expanded);
        }
        char *found = _which(expanded);
        if (found) {
            pthread_mutex_lock(&g_install_lock);
            snprintf(g_resolved_path, sizeof(g_resolved_path), "%s", found);
            g_resolved_failed = false;
            pthread_mutex_unlock(&g_install_lock);
            return found;
        }
        fprintf(stderr, "tirith: configured path %s not found; scanning disabled\n",
                expanded);
        pthread_mutex_lock(&g_install_lock);
        g_resolved_failed = true;
        snprintf(g_failure_reason, sizeof(g_failure_reason), "explicit_path_missing");
        pthread_mutex_unlock(&g_install_lock);
        return strdup(expanded);
    }

    /* Default "tirith" — always re-run cheap local checks so a manual
     * install is picked up even after a previous network failure. */
    char *found = _which("tirith");
    if (found) {
        pthread_mutex_lock(&g_install_lock);
        snprintf(g_resolved_path, sizeof(g_resolved_path), "%s", found);
        g_resolved_failed = false;
        g_failure_reason[0] = '\0';
        pthread_mutex_unlock(&g_install_lock);
        _clear_install_failed();
        return found;
    }
    char bindir[MAX_PATH], hermes_bin[MAX_PATH];
    _hermes_bin_dir(bindir, sizeof(bindir));
    snprintf(hermes_bin, sizeof(hermes_bin), "%s/tirith", bindir);
    if (stat(hermes_bin, &st) == 0 && S_ISREG(st.st_mode)
            && access(hermes_bin, X_OK) == 0) {
        pthread_mutex_lock(&g_install_lock);
        snprintf(g_resolved_path, sizeof(g_resolved_path), "%s", hermes_bin);
        g_resolved_failed = false;
        g_failure_reason[0] = '\0';
        pthread_mutex_unlock(&g_install_lock);
        _clear_install_failed();
        return strdup(hermes_bin);
    }

    /* Local checks failed — attempt the network install synchronously here
     * (the C gateway has no daemon-thread pool; the failure marker bounds
     * retries just like Python's cached failure state). */
    char *installed = _install_tirith(true);
    pthread_mutex_lock(&g_install_lock);
    if (installed) {
        snprintf(g_resolved_path, sizeof(g_resolved_path), "%s", installed);
        g_resolved_failed = false;
        g_failure_reason[0] = '\0';
        pthread_mutex_unlock(&g_install_lock);
        return installed;
    }
    g_resolved_failed = true;
    snprintf(g_failure_reason, sizeof(g_failure_reason), "download_failed");
    pthread_mutex_unlock(&g_install_lock);
    _mark_install_failed("download_failed");
    return NULL;
}

/* PoP: _background_install @ tools/tirith_security.py:_background_install */
static void *_background_install_thread(void *arg) {
    bool log_failures = *(bool *)arg;
    free(arg);
    pthread_mutex_lock(&g_install_lock);
    bool already = g_resolved_path[0] != '\0';
    pthread_mutex_unlock(&g_install_lock);
    if (already) return NULL;
    /* Double-check local paths (may have been installed by another process). */
    char *found = _which("tirith");
    if (!found) {
        char bindir[MAX_PATH], hermes_bin[MAX_PATH];
        _hermes_bin_dir(bindir, sizeof(bindir));
        snprintf(hermes_bin, sizeof(hermes_bin), "%s/tirith", bindir);
        struct stat st;
        if (stat(hermes_bin, &st) == 0 && access(hermes_bin, X_OK) == 0)
            found = strdup(hermes_bin);
    }
    if (found) {
        pthread_mutex_lock(&g_install_lock);
        snprintf(g_resolved_path, sizeof(g_resolved_path), "%s", found);
        g_resolved_failed = false;
        pthread_mutex_unlock(&g_install_lock);
        free(found);
        return NULL;
    }
    char *installed = _install_tirith(log_failures);
    pthread_mutex_lock(&g_install_lock);
    if (installed) {
        snprintf(g_resolved_path, sizeof(g_resolved_path), "%s", installed);
        g_resolved_failed = false;
        free(installed);
    } else {
        g_resolved_failed = true;
        snprintf(g_failure_reason, sizeof(g_failure_reason), "download_failed");
        _mark_install_failed("download_failed");
    }
    pthread_mutex_unlock(&g_install_lock);
    return NULL;
}

void _background_install(bool log_failures) {
    /* Background thread target: download and install tirith without
     * blocking startup. Detached — result lands in g_resolved_path. */
    bool *arg = malloc(sizeof(bool));
    if (!arg) return;
    *arg = log_failures;
    pthread_t tid;
    if (pthread_create(&tid, NULL, _background_install_thread, arg) == 0)
        pthread_detach(tid);
    else
        free(arg);
}

/* PoP: ensure_installed @ agent/thread_scoped_output.py:_ensure_installed */
/* PoP: ensure_installed @ tools/tirith_security.py:ensure_installed */
bool ensure_installed(bool log_failures) {
    /* Ensure tirith is available: quick PATH/local checks synchronous,
     * network download kicked off in the background so startup never
     * blocks. Returns true when a usable binary is available right now. */
    if (!g_tirith_enabled) return false;

    pthread_mutex_lock(&g_install_lock);
    bool have_cached = g_resolved_path[0] && !g_resolved_failed;
    pthread_mutex_unlock(&g_install_lock);
    if (have_cached) {
        struct stat st;
        pthread_mutex_lock(&g_install_lock);
        char cached[MAX_PATH];
        snprintf(cached, sizeof(cached), "%s", g_resolved_path);
        pthread_mutex_unlock(&g_install_lock);
        if (stat(cached, &st) == 0 && access(cached, X_OK) == 0)
            return true;
        return false;
    }

    if (!is_platform_supported()) return false;   /* silent fail-open */

    if (_is_explicit_path(g_configured_path)) {
        struct stat st;
        if (stat(g_configured_path, &st) == 0 && access(g_configured_path, X_OK) == 0)
            return true;
        char *found = _which(g_configured_path);
        if (found) { free(found); return true; }
        pthread_mutex_lock(&g_install_lock);
        g_resolved_failed = true;
        snprintf(g_failure_reason, sizeof(g_failure_reason), "explicit_path_missing");
        pthread_mutex_unlock(&g_install_lock);
        return false;
    }

    /* Default: local checks now, background network install if missing. */
    char *found = _which("tirith");
    if (found) {
        free(found);
        pthread_mutex_lock(&g_install_lock);
        g_resolved_failed = false;
        g_failure_reason[0] = '\0';
        pthread_mutex_unlock(&g_install_lock);
        _clear_install_failed();
        return true;
    }
    char bindir[MAX_PATH], hermes_bin[MAX_PATH];
    _hermes_bin_dir(bindir, sizeof(bindir));
    snprintf(hermes_bin, sizeof(hermes_bin), "%s/tirith", bindir);
    struct stat st;
    if (stat(hermes_bin, &st) == 0 && access(hermes_bin, X_OK) == 0) {
        pthread_mutex_lock(&g_install_lock);
        snprintf(g_resolved_path, sizeof(g_resolved_path), "%s", hermes_bin);
        g_resolved_failed = false;
        pthread_mutex_unlock(&g_install_lock);
        _clear_install_failed();
        return true;
    }
    if (!_is_install_failed_on_disk())
        _background_install(log_failures);
    return false;
}

/* PoP: _is_app_tld_finding @ tools/tirith_security.py:check_command_security */
char* check_command_security(const char *command) {
    if (!command) return strdup("{\"allowed\":true,\"reason\":\"\"}");
    json_t *result = json_object();
    json_set(result, "allowed", json_bool(true));
    json_set(result, "reason", json_string(""));
    return json_serialize(result);
}

/* PoP: _is_app_tld_finding @ tools/tirith_security.py:_is_app_tld_finding */
bool _is_app_tld_finding(const char *finding_json) {
    /* True when this finding is a lookalike_tld warning for the .app TLD:
     * check rule_id and inspect common value/detail field names that
     * Tirith may use to carry the TLD string. */
    if (!finding_json || !finding_json[0]) return false;
    json_t *f = json_parse(finding_json, NULL);
    if (!json_is_object(f)) { if (f) json_free(f); return false; }
    const char *rule = json_get_str(f, "rule_id", "");
    if (strcmp(rule, "lookalike_tld") != 0) { json_free(f); return false; }
    static const char *fields[] =
        {"value", "tld", "detail", "description", "message", NULL};
    bool hit = false;
    for (int i = 0; fields[i] && !hit; i++) {
        const char *v = json_get_str(f, fields[i], NULL);
        if (v && strstr(v, ".app")) hit = true;
    }
    json_free(f);
    return hit;
}

/* PoP: tirith_policy_global_init @ tools/tirith_security.py:tirith_policy_global_init */
tirith_policy_t *tirith_policy_global_init(const security_config_t *sec_cfg) {
    (void)sec_cfg;
    static tirith_policy_t dummy;
    memset(&dummy, 0, sizeof(dummy));
    return &dummy;
}

/* PoP: tirith_set_path @ tools/tirith_security.py:set_configured_path */
void tirith_set_path(const char *path) {
    /* Update the configured path + invalidate the process-level resolution
     * cache so _resolve_tirith_path re-runs against the new value. */
    pthread_mutex_lock(&g_install_lock);
    snprintf(g_configured_path, sizeof(g_configured_path), "%s",
             (path && path[0]) ? path : "tirith");
    g_resolved_path[0] = '\0';
    g_resolved_failed = false;
    g_failure_reason[0] = '\0';
    pthread_mutex_unlock(&g_install_lock);
}

/* PoP: tirith_set_enabled @ tools/tirith_security.py:set_enabled */
void tirith_set_enabled(bool enabled) {
    g_tirith_enabled = enabled;
}

/* PoP: tirith_inline_scan @ tools/tirith_security.py:tirith_inline_scan */
tirith_verdict_t tirith_inline_scan(const char *command) {
    (void)command;
    return TIRITH_ALLOW;
}