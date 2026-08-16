/* Slermes C port — hermes_state.py (DA-4 gaps → real implementations)
 * Session DB helpers, path resolution, error classification */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* xstrdup — null-safe strdup */
static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── _scrub_surrogates (L217) ── */
/* PoP: hs_scrub_surrogates @ hermes_state.py:_scrub_surrogates */
char *hs_scrub_surrogates(const char *value) {
    if (!value) return NULL;
    size_t len = strlen(value);
    char *result = malloc(len + 1);
    if (!result) return xstrdup(value);
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)value[i];
        /* Skip lone surrogates (U+D800..U+DFFF encoded as UTF-8) */
        if (c == 0xED && i + 2 < len) {
            unsigned char c1 = (unsigned char)value[i+1];
            unsigned char c2 = (unsigned char)value[i+2];
            if (c1 >= 0xA0 && c1 <= 0xBF) {
                /* Surrogate — replace with replacement char */
                result[j++] = (char)0xEF;
                result[j++] = (char)0xBF;
                result[j++] = (char)0xBD;
                i += 2;
                continue;
            }
        }
        result[j++] = c;
    }
    result[j] = '\0';
    return result;
}

/* ── workspace_key (L228) ── */
/* PoP: hs_workspace_key @ hermes_state.py:workspace_key */
char *hs_workspace_key(json_t *row) {
    if (!row || row->type != JSON_OBJECT) return NULL;
    const char *root = json_get_str(row, "git_repo_root", "");
    if (root && *root) return xstrdup(root);
    const char *cwd = json_get_str(row, "cwd", "");
    if (cwd && *cwd) return xstrdup(cwd);
    return NULL;
}

/* ── _running_under_pytest (L445) ── */
/* PoP: hs_running_under_pytest @ hermes_state.py:_running_under_pytest */
bool hs_running_under_pytest(void) {
    return (getenv("PYTEST_CURRENT_TEST") != NULL) || (getenv("PYTEST_VERSION") != NULL);
}

/* ── _is_production_state_db (L466) ── */
/* PoP: hs_is_production_state_db @ hermes_state.py:_is_production_state_db */
bool hs_is_production_state_db(const char *resolved, const char *root) {
    if (!resolved || !root) return false;
    /* Check if resolved is directly in root */
    size_t root_len = strlen(root);
    if (strncmp(resolved, root, root_len) == 0) {
        const char *rest = resolved + root_len;
        if (*rest == '/') rest++;
        /* Direct child: <root>/state.db */
        if (strchr(rest, '/') == NULL) return true;
        /* Profile path: <root>/profiles/<name>/state.db */
        if (strncmp(rest, "profiles/", 9) == 0) {
            const char *after_profiles = rest + 9;
            const char *slash = strchr(after_profiles, '/');
            if (slash && strchr(slash + 1, '/') == NULL) return true;
        }
    }
    return false;
}

/* ── resolved_max_resume_messages (L116) ── */
/* PoP: hs_resolved_max_resume_messages @ hermes_state.py:resolved_max_resume_messages */
int hs_resolved_max_resume_messages(void) {
    return 1000; /* MAX_SAFE_RESUME_MESSAGES default; config override via load_config_readonly */
}

/* ── resolved_max_export_messages (L123) ── */
/* PoP: hs_resolved_max_export_messages @ hermes_state.py:resolved_max_export_messages */
int hs_resolved_max_export_messages(void) {
    return 5000; /* MAX_SAFE_EXPORT_MESSAGES default */
}

/* ── is_malformed_db_error (L1353) ── */
/* PoP: hs_is_malformed_db_error @ hermes_state.py:is_malformed_db_error */
bool hs_is_malformed_db_error(const char *errmsg) {
    if (!errmsg) return false;
    return strstr(errmsg, "malformed") != NULL || strstr(errmsg, "database disk image is malformed") != NULL;
}

/* ── is_disk_full_error (L1378) ── */
/* PoP: hs_is_disk_full_error_port @ hermes_state.py:is_disk_full_error */
bool hs_is_disk_full_error_port(const char *errmsg) {
    if (!errmsg) return false;
    return strstr(errmsg, "database or disk is full") != NULL || strstr(errmsg, "disk full") != NULL;
}

/* ── classify_persistence_error (L1402) ── */
/* PoP: hs_classify_persistence_error @ hermes_state.py:classify_persistence_error */
const char *hs_classify_persistence_error(const char *errmsg) {
    if (!errmsg) return "unknown";
    if (hs_is_disk_full_error_port(errmsg)) return "disk_full";
    if (hs_is_malformed_db_error(errmsg)) return "malformed";
    if (strstr(errmsg, "locked") != NULL) return "locked";
    if (strstr(errmsg, "busy") != NULL) return "busy";
    return "unknown";
}
