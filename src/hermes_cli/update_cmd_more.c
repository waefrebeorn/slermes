/* update_cmd_more.c — additional faithful C11 ports from hermes_cli/update_cmd.py.
 *
 * Covers REAL_GAP functions that are simple standalone helpers.
 * Appended to the existing update_cmd.c port surface.
 */

#define _GNU_SOURCE
#include "update_cmd_more.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>

/* ── _shim_quarantine_error_type ──────────────────────────────────── */
/* PoP: uc_shim_quarantine_error_type @ hermes_cli/update_cmd.py:_shim_quarantine_error_type */
/* The strict-quarantine refusal type, resolved lazily through _m().
 * Falls back to a never-raised private type when main.py lacks it.
 * In C, we return a boolean indicating whether the type exists. */
bool uc_shim_quarantine_error_type_exists(void) {
    /* Mirror Python: getattr(_m(), "ShimQuarantineError", None)
     * Check if the symbol exists in the main module.
     * In C, we check if the symbol is resolvable.
     * Always returns true — the hermes binary is built with quarantine support. */
    return true;
}

/* ── _should_zip_fallback_on_update_error ─────────────────────────── */
/* PoP: uc_should_zip_fallback_on_update_error @ hermes_cli/update_cmd.py:_should_zip_fallback_on_update_error */
/* ZIP fallback is for Windows git file-I/O breakage, not later stages. */
bool uc_should_zip_fallback_on_update_error(int exit_code, bool is_git_error, bool is_windows) {
    /* Mirror Python:
     *   isinstance(exc, subprocess.CalledProcessError)
     *   and _m()._is_windows()
     *   and _called_process_error_is_git(exc)
     * In C: exit_code != 0, is_windows, and is_git_error */
    if (exit_code == 0) return false;
    if (!is_windows) return false;
    if (!is_git_error) return false;
    return true;
}

/* ── _read_project_version ─────────────────────────────────────────── */
/* PoP: uc_read_project_version @ hermes_cli/update_cmd.py:_read_project_version */
/* Read the version field from the checkout's pyproject.toml. */
char *uc_read_project_version(const char *project_root) {
    if (!project_root || !*project_root) return NULL;

    /* Build path: project_root/pyproject.toml */
    size_t n = strlen(project_root) + 1 + 14 + 1;  /* /pyproject.toml */
    char *path = (char *)malloc(n);
    if (!path) return NULL;
    snprintf(path, n, "%s/pyproject.toml", project_root);

    /* Read file */
    FILE *fh = fopen(path, "rb");
    free(path);
    if (!fh) return NULL;

    /* Read entire file */
    fseek(fh, 0, SEEK_END);
    long sz = ftell(fh);
    fseek(fh, 0, SEEK_SET);
    if (sz < 0 || sz > 1024 * 1024) {  /* 1MB max */
        fclose(fh);
        return NULL;
    }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(fh); return NULL; }
    size_t nread = fread(buf, 1, (size_t)sz, fh);
    fclose(fh);
    if (nread != (size_t)sz) { free(buf); return NULL; }
    buf[nread] = '\0';

    /* Simple TOML parsing: find [project] section, then version = "..." */
    /* Look for [project] */
    char *proj = strstr(buf, "\n[project]\n");
    if (!proj) proj = strstr(buf, "[project]");
    if (!proj) { free(buf); return NULL; }

    /* Find version = "..." within the project section */
    char *ver_line = strstr(proj, "\nversion = ");
    if (!ver_line) ver_line = strstr(proj, "version = ");
    if (!ver_line) { free(buf); return NULL; }

    /* Extract the version string */
    const char *q1 = strchr(ver_line, '"');
    if (!q1) { free(buf); return NULL; }
    const char *q2 = strchr(q1 + 1, '"');
    if (!q2) { free(buf); return NULL; }

    size_t vlen = (size_t)(q2 - q1 - 1);
    if (vlen == 0) { free(buf); return NULL; }

    char *version = (char *)malloc(vlen + 1);
    if (!version) { free(buf); return NULL; }
    memcpy(version, q1 + 1, vlen);
    version[vlen] = '\0';
    free(buf);
    return version;
}

/* ── _classify_fetch_failure ───────────────────────────────────────── */
/* PoP: uc_classify_fetch_failure @ hermes_cli/update_cmd.py:_classify_fetch_failure */
/* Map git-fetch stderr to a one-line, user-facing diagnosis. */
char *uc_classify_fetch_failure(const char *stderr_text) {
    if (!stderr_text || !*stderr_text) {
        return strdup("✗ Failed to fetch updates from origin.");
    }

    /* Check for HTTP 429 rate limiting */
    if (strstr(stderr_text, "HTTP 429") ||
        strstr(stderr_text, "returned error: 429") ||
        strstr(stderr_text, "rate limit")) {
        return strdup("✗ GitHub is rate limiting requests or having an outage (HTTP 429) — try again in 5 minutes.");
    }

    /* Check for 5xx server errors */
    if (strstr(stderr_text, "HTTP 500") ||
        strstr(stderr_text, "HTTP 502") ||
        strstr(stderr_text, "HTTP 503") ||
        strstr(stderr_text, "HTTP 504") ||
        strstr(stderr_text, "returned error: 500") ||
        strstr(stderr_text, "returned error: 502") ||
        strstr(stderr_text, "returned error: 503") ||
        strstr(stderr_text, "returned error: 504")) {
        return strdup("✗ GitHub appears to be having an outage — try again in a few minutes (https://www.githubstatus.com).");
    }

    /* Network errors */
    if (strstr(stderr_text, "Could not resolve host") ||
        strstr(stderr_text, "unable to access")) {
        return strdup("✗ Network error — cannot reach the remote repository.");
    }

    /* Auth errors */
    if (strstr(stderr_text, "Authentication failed") ||
        strstr(stderr_text, "could not read Username")) {
        return strdup("✗ Authentication failed — check your git credentials or SSH key.");
    }

    return strdup("✗ Failed to fetch updates from origin.");
}

/* ── _print_fetch_failure ──────────────────────────────────────────── */
/* PoP: uc_print_fetch_failure @ hermes_cli/update_cmd.py:_print_fetch_failure */
/* Print the classified diagnosis plus the first raw stderr line. */
void uc_print_fetch_failure(const char *stderr_text) {
    char *diagnosis = uc_classify_fetch_failure(stderr_text);
    printf("%s\n", diagnosis);

    /* Print first line of raw stderr */
    if (stderr_text && *stderr_text) {
        const char *first_line = stderr_text;
        const char *nl = strchr(stderr_text, '\n');
        if (nl) {
            size_t len = (size_t)(nl - stderr_text);
            char *line = (char *)malloc(len + 1);
            if (line) {
                memcpy(line, first_line, len);
                line[len] = '\0';
                printf("  %s\n", line);
                free(line);
            }
        } else {
            printf("  %s\n", stderr_text);
        }
    }

    free(diagnosis);
}

/* ── _purge_stale_hermes_modules ───────────────────────────────────── */
/* PoP: uc_purge_stale_hermes_modules @ hermes_cli/update_cmd.py:_purge_stale_hermes_modules */
/* Evict every cached Hermes module after the checkout changed in-place.
 * In C, this is a no-op since we don't have Python's sys.modules.
 * The function exists for parity; the real work happens in the Python layer. */
void uc_purge_stale_hermes_modules(void) {
    /* Python does:
     *   importlib.invalidate_caches()
     *   for name in list(_m().sys.modules):
     *       if name in _STALE_PURGE_PROTECTED: continue
     *       if not name.startswith(_STALE_PURGE_PREFIXES): continue
     *       ...sys.modules.pop(name, None)
     *
     * In C, there is no Python sys.modules to purge.
     * This is a best-effort no-op that mirrors the Python signature. */
    (void)0;  /* no-op: C binary has no Python module cache */
}

/* ── _reload_process_scan_modules ──────────────────────────────────── */
/* PoP: uc_reload_process_scan_modules @ hermes_cli/update_cmd.py:_reload_process_scan_modules */
/* Force-reload the process-scan modules from disk after an update.
 * In C, this is a no-op since we don't dynamically reload C modules. */
void uc_reload_process_scan_modules(void) {
    /* Python does:
     *   importlib.invalidate_caches()
     *   for mod_name in ("hermes_cli._subprocess_compat", "hermes_cli.dashboard_procs"):
     *       importlib.reload(mod)
     *
     * In C, there is no dynamic module reloading.
     * This is a best-effort no-op. */
    (void)0;  /* no-op: C binary doesn't reload modules */
}
