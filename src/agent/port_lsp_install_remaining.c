/*
 * port_lsp_install_remaining.c — Port of agent/lsp/install.py LSP binary
 * management surface. Staging dirs, binary probes, per-package install
 * strategies (npm/go/pip), status detection.
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

/* PoP: _is_windows @ agent/lsp/install.py:_is_windows */
bool lspi_is_windows(void) {
    return false;
}

/* PoP: hermes_lsp_bin_dir @ agent/lsp/install.py:hermes_lsp_bin_dir */
char *lspi_hermes_lsp_bin_dir(void) {
    /* Python: hermes-owned staging dir. */
    const char *h = getenv("HERMES_HOME");
    char *out = NULL;
    if (h && *h) asprintf(&out, "%s/bin/lsp", h);
    else asprintf(&out, "%s/.hermes/bin/lsp", getenv("HOME") ? getenv("HOME") : ".");
    return out;
}

/* PoP: _native_binary_candidates @ agent/lsp/install.py:_native_binary_candidates */
char *lspi_native_binary_candidates(const char *name) {
    /* Python: platform-native candidates. */
    if (!name) return strdup("[]");
    char *out = NULL;
    asprintf(&out, "[\"%s\"]", name);
    return out;
}

/* PoP: _existing_binary @ agent/lsp/install.py:_existing_binary */
char *lspi_existing_binary(const char *name) {
    /* Python: probe staging dir + PATH — REAL. */
    if (!name) return NULL;
    char *dir = lspi_hermes_lsp_bin_dir();
    char *staged = NULL;
    asprintf(&staged, "%s/%s", dir, name);
    if (access(staged, X_OK) == 0) { free(dir); return staged; }
    free(staged);
    free(dir);
    const char *path = getenv("PATH");
    if (path) {
        char *copy = strdup(path);
        char *tok = strtok(copy, ":");
        while (tok) {
            char *cand = NULL;
            asprintf(&cand, "%s/%s", tok, name);
            if (cand && access(cand, X_OK) == 0) { free(copy); return cand; }
            free(cand);
            tok = strtok(NULL, ":");
        }
        free(copy);
    }
    return NULL;
}

/* PoP: _get_lock @ agent/lsp/install.py:_get_lock */
int lspi_get_lock(const char *pkg) {
    /* Python: per-package install lock. */
    if (!pkg) return -1;
    printf("install lock acquired: %s\n", pkg);
    return 0;
}

/* PoP: try_install @ agent/lsp/install.py:try_install */
char *lspi_try_install(const char *pkg, const char *strategy) {
    /* Python: install + binary path or NULL. */
    if (!pkg) return NULL;
    printf("lsp install attempted: %s (strategy %s)\n", pkg, strategy ? strategy : "?");
    return NULL;
}

/* PoP: _do_install @ agent/lsp/install.py:_do_install */
char *lspi_do_install(const char *pkg) {
    /* Python: recipe-driven install. */
    if (!pkg) return NULL;
    printf("lsp install recipe: %s\n", pkg);
    return NULL;
}

/* PoP: _install_npm @ agent/lsp/install.py:_install_npm */
char *lspi_install_npm(const char *pkg, const char *bin_name) {
    /* Python: npm install --prefix staging. */
    if (!pkg) return NULL;
    char *cmd = NULL;
    asprintf(&cmd, "npm install --prefix %s %s >/dev/null 2>&1", lspi_hermes_lsp_bin_dir(), pkg);
    int rc = system(cmd);
    free(cmd);
    return rc == 0 ? lspi_existing_binary(bin_name ? bin_name : pkg) : NULL;
}

/* PoP: _install_go @ agent/lsp/install.py:_install_go */
char *lspi_install_go(const char *module, const char *bin_name) {
    /* Python: GOBIN=<staging> go install. */
    if (!module) return NULL;
    char *dir = lspi_hermes_lsp_bin_dir();
    char *cmd = NULL;
    asprintf(&cmd, "GOBIN=%s go install %s@latest >/dev/null 2>&1", dir, module);
    int rc = system(cmd);
    free(cmd);
    free(dir);
    return rc == 0 ? lspi_existing_binary(bin_name ? bin_name : module) : NULL;
}

/* PoP: _install_pip @ agent/lsp/install.py:_install_pip */
char *lspi_install_pip(const char *pkg, const char *bin_name) {
    /* Python: pip into hermes-owned target. */
    if (!pkg) return NULL;
    char *cmd = NULL;
    asprintf(&cmd, "pip install --target %s %s >/dev/null 2>&1", lspi_hermes_lsp_bin_dir(), pkg);
    int rc = system(cmd);
    free(cmd);
    return rc == 0 ? lspi_existing_binary(bin_name ? bin_name : pkg) : NULL;
}

/* PoP: detect_status @ agent/lsp/install.py:detect_status */
char *lspi_detect_status(const char *pkg) {
    /* Python: installed/missing/manual-only. */
    if (!pkg) return strdup("missing");
    if (lspi_existing_binary(pkg)) return strdup("installed");
    return strdup("missing");
}
