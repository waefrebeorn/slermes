/*
 * ssl_guard.c — Preventive SSL CA certificate checks for Hermes C.
 * AG40: Port of Python agent/ssl_guard.py
 *
 * Catches broken CA bundle paths before OpenSSL/curl turns them into
 * opaque "No such file or directory" failures.
 */

#include "ssl_guard.h"
#include "hermes_core_types.h"
#include "hermes_error.h"
#include "hermes_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

/* Environment variables that may point to CA bundles */
static const char *const CA_BUNDLE_ENV_VARS[] = {
    "HERMES_CA_BUNDLE",
    "SSL_CERT_FILE",
    "REQUESTS_CA_BUNDLE",
    "CURL_CA_BUNDLE",
    NULL
};

/* Values that mean "skip SSL guard" */
static const char *const SKIP_VALUES[] = {
    "1", "true", "yes", "on",
    NULL
};

/* ================================================================
 *  Internal helpers
 * ================================================================ */

static bool str_in_array(const char *needle, const char *const *haystack)
{
    if (!needle) return false;
    for (int i = 0; haystack[i]; i++) {
        if (strcasecmp(needle, haystack[i]) == 0)
            return true;
    }
    return false;
}

/* PoP: _skip_ssl_guard_enabled @ agent/ssl_guard.py:_skip_ssl_guard_enabled */
/* Port of Python agent/ssl_guard.py:_skip_ssl_guard_enabled(). */
bool ssl_guard_skip_enabled(void)
{
    const char *val = getenv("HERMES_SKIP_SSL_GUARD");
    if (!val) return false;
    return str_in_array(val, SKIP_VALUES);
}

/* AG26: Port of Python agent/ssl_guard.py:_repair_hint() */
static const char *ssl_repair_hint(void)
{
    /* Return a human-readable hint for fixing SSL CA bundle issues.
     * Python's _repair_hint() returns a static string; C does the same. */
    return "Repair: ensure certifi is installed and CA bundle env vars point to valid files";
}

/* AG26: Port of Python agent/ssl_guard.py:_ssl_err() */
/* PoP: _err @ hermes_cli/pets.py:_err */
static hermes_error_t ssl_err(hermes_error_code_t code, const char *message)
{
    hermes_error_t e = {0};
    e.code = code;
    char full_msg[1024];
    snprintf(full_msg, sizeof(full_msg), "%s\n%s", message, ssl_repair_hint());
    size_t len = strlen(full_msg);
    if (len >= sizeof(e.message)) len = sizeof(e.message) - 1;
    memcpy(e.message, full_msg, len);
    e.message[len] = '\0';
    return e;
}

/* PoP: _validate_bundle_path @ agent/ssl_guard.py:_validate_bundle_path */
/* Port of Python agent/ssl_guard.py:_validate_bundle_path(). */
static hermes_error_t validate_bundle_path(const char *label, const char *value, bool require_substantial)
{
    struct stat st;

    if (!value || !*value)
        return ssl_err(HERMES_ERR_CONFIG, "Empty CA bundle path");

    if (stat(value, &st) != 0)
        return ssl_err(HERMES_ERR_FILE_NOT_FOUND, label);

    if (!S_ISREG(st.st_mode))
        return ssl_err(HERMES_ERR_INVALID_FORMAT, label);

    if (require_substantial && st.st_size < 1024)
        return ssl_err(HERMES_ERR_INVALID_FORMAT, label);

    /* Try to load with OpenSSL */
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ctx)
        return ssl_err(HERMES_ERR_RUNTIME, "Failed to create SSL context");

    if (SSL_CTX_load_verify_locations(ctx, value, NULL) != 1) {
        SSL_CTX_free(ctx);
        return ssl_err(HERMES_ERR_INVALID_FORMAT, label);
    }

    /* Check if any certs were loaded */
    STACK_OF(X509_NAME) *certs = SSL_CTX_get_client_CA_list(ctx);
    if (!certs || sk_X509_NAME_num(certs) == 0) {
        SSL_CTX_free(ctx);
        return ssl_err(HERMES_ERR_CONFIG, label);
    }

    SSL_CTX_free(ctx);
    return (hermes_error_t){HERMES_OK, {0}, {0}};
}

/* ================================================================
 *  Public API
 * ================================================================ */

/* PoP: verify_ca_bundle @ agent/ssl_guard.py:verify_ca_bundle */
/* Port of Python agent/ssl_guard.py:verify_ca_bundle(). */
hermes_error_t ssl_guard_verify_ca_bundle(void)
{
    if (ssl_guard_skip_enabled()) {
        hermes_log(LOG_DEBUG, "ssl_guard", "SSL CA bundle guard skipped via HERMES_SKIP_SSL_GUARD");
        return (hermes_error_t){HERMES_OK, {0}, {0}};
    }

    /* Check explicit CA bundle environment variables */
    for (int i = 0; CA_BUNDLE_ENV_VARS[i]; i++) {
        const char *value = getenv(CA_BUNDLE_ENV_VARS[i]);
        if (value) {
            hermes_error_t err = validate_bundle_path(CA_BUNDLE_ENV_VARS[i], value, false);
            if (err.code != HERMES_OK)
                return err;
        }
    }

    /* Check certifi's bundled cacert.pem */
    /* In C, we rely on OpenSSL's default paths, but we can try to find certifi equivalent */
    /* For now, use OpenSSL's default cert store */
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ctx)
        return ssl_err(HERMES_ERR_RUNTIME, "Failed to create SSL context for certifi check");

    /* OpenSSL uses SSL_CTX_set_default_verify_paths() for system certs */
    SSL_CTX_set_default_verify_paths(ctx);

    STACK_OF(X509_NAME) *certs = SSL_CTX_get_client_CA_list(ctx);
    if (!certs || sk_X509_NAME_num(certs) == 0) {
        SSL_CTX_free(ctx);
        return ssl_err(HERMES_ERR_CONFIG, "System CA bundle did not load any certificates");
    }

    SSL_CTX_free(ctx);
    return (hermes_error_t){HERMES_OK, {0}, {0}};
}

/* PoP: verify_ca_bundle_with_fallback @ agent/ssl_guard.py:verify_ca_bundle_with_fallback */
/* Port of Python agent/ssl_guard.py:verify_ca_bundle_with_fallback(). */
hermes_error_t ssl_guard_verify_ca_bundle_with_fallback(void)
{
    /* Python's verify_ca_bundle_with_fallback() tries the primary bundle first,
     * then falls back to a secondary source. In C, we enforce the check directly
     * without a separate fallback path — delegate to verify_ca_bundle(). */
    return ssl_guard_verify_ca_bundle();
}