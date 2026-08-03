/*
 * port_ssl_guard_remaining.c — Port of agent/ssl_guard.py CA-bundle
 * surface. Skip guard, repair hints, bundle validation with real fs.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _skip_ssl_guard_enabled @ agent/ssl_guard.py:_skip_ssl_guard_enabled */
bool sslg_skip_ssl_guard_enabled(void) {
    /* Python: HERMES_SKIP_SSL_GUARD in skip values. */
    const char *v = getenv("HERMES_SKIP_SSL_GUARD");
    if (!v) return false;
    char *l = lowerdup(v);
    if (!l) return false;
    bool r = strcmp(l, "true") == 0 || strcmp(l, "1") == 0 || strcmp(l, "yes") == 0 ||
             strcmp(l, "skip") == 0;
    free(l);
    return r;
}

/* PoP: _repair_hint @ agent/ssl_guard.py:_repair_hint */
char *sslg_repair_hint(void) {
    return strdup("Repair: run `hermes doctor --fix` (auto-reinstalls certifi), or set SSL_CERT_FILE to a valid bundle");
}

/* PoP: _ssl_err @ agent/ssl_guard.py:_ssl_err */
char *sslg_ssl_err(const char *label, const char *detail) {
    char *out = NULL;
    asprintf(&out, "{\"kind\": \"ssl_config\", \"label\": \"%s\", \"detail\": \"%s\"}",
             label ? label : "", detail ? detail : "");
    return out;
}

/* PoP: _validate_bundle_path @ agent/ssl_guard.py:_validate_bundle_path */
int sslg_validate_bundle_path(const char *value) {
    /* Python: expanduser + exists check — REAL. */
    if (!value) return -1;
    const char *p = value;
    char *expanded = NULL;
    if (p[0] == '~' && (p[1] == '/' || p[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home) asprintf(&expanded, "%s%s", home, p + 1);
    }
    const char *path = expanded ? expanded : p;
    int rc = access(path, F_OK) == 0 ? 0 : -1;
    free(expanded);
    return rc;
}

/* PoP: verify_ca_bundle @ agent/ssl_guard.py:verify_ca_bundle */
int sslg_verify_ca_bundle(const char *bundle_path) {
    /* Python: present + loadable. */
    if (!bundle_path) return -1;
    if (sslg_validate_bundle_path(bundle_path) != 0) return -1;
    FILE *f = fopen(bundle_path, "r");
    if (!f) return -1;
    char buf[256];
    size_t r = fread(buf, 1, sizeof(buf) - 1, f);
    buf[r] = '\0';
    fclose(f);
    /* PEM markers present */
    if (strstr(buf, "-----BEGIN") == NULL) return -1;
    return 0;
}

/* PoP: verify_ca_bundle_with_fallback @ agent/ssl_guard.py:verify_ca_bundle_with_fallback */
int sslg_verify_ca_bundle_with_fallback(const char *bundle_path) {
    /* Python: backward-compatible wrapper. */
    int rc = sslg_verify_ca_bundle(bundle_path);
    if (rc == 0) return 0;
    /* fallback: default certifi path */
    const char *h = getenv("HERMES_HOME");
    char *fb = NULL;
    if (h && *h) asprintf(&fb, "%s/certifi/cacert.pem", h);
    if (fb) {
        if (sslg_verify_ca_bundle(fb) == 0) { free(fb); return 0; }
        free(fb);
    }
    return -1;
}
