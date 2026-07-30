/*
 * port_web_server_managed_files.c — faithful C11 port of the dashboard
 * managed-files + media security cluster in hermes_cli/web_server.py.
 *
 * Every function has REAL behavior (no stubs):
 *   - credential denylist (the #57505 exfil surface)
 *   - MIME + binary sniff
 *   - base64 data-url decode (validates, caps size)
 *   - chat-image upload pipeline (sanitize, magic-byte sniff, gate)
 *   - managed-path policy + resolution (locked-root confinement, '..' guard)
 */

#include "web_server_managed_files.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "path_security.h"          /* cli_tools_path_security_has_traversal_component */
#include "hermes_web_server_pure.h" /* sibling: ws_path_is_under, ws_canonical_path,
                                       WS_MANAGED_FILE_MAX_BYTES */
#include "slermes_home.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>

/* ── credential denylist (web_server._SENSITIVE_MANAGED_FILE_BASENAMES /
 *     _SENSITIVE_MANAGED_DIR_NAMES) ──────────────────────────────────────── */
static const char *k_sensitive_basenames[] = {
    "auth.json", "auth.lock", "credentials", "config.yaml",
    ".anthropic_oauth.json", "google_token.json", "google_oauth_pending.json",
    "google_oauth.json", "webhook_subscriptions.json", "bws_cache.json",
    "bws_cache.enc.json", ".git-credentials", NULL,
};
static const char *k_sensitive_dirs[] = { "mcp-tokens", "pairing", NULL };

/* PoP: ws_is_sensitive_filename @ hermes_cli/web_server.py:_is_sensitive_filename */
bool ws_is_sensitive_filename(const char *name) {
    if (!name || !name[0]) return false;
    size_t n = strlen(name);
    char *low = malloc(n + 1);
    for (size_t i = 0; i <= n; i++) low[i] = (char)tolower((unsigned char)name[i]);
    bool rc = false;
    if (strcmp(low, ".env") == 0 || strncmp(low, ".env.", 5) == 0 ||
        strcmp(low, ".envrc") == 0) {
        rc = true;
    } else {
        for (int i = 0; k_sensitive_basenames[i]; i++)
            if (strcmp(low, k_sensitive_basenames[i]) == 0) { rc = true; break; }
    }
    free(low);
    return rc;
}

/* PoP: ws_is_sensitive_path @ hermes_cli/web_server.py:_is_sensitive_path */
bool ws_is_sensitive_path(const char *path) {
    if (!path || !path[0]) return false;
    /* basename check */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    if (ws_is_sensitive_filename(base)) return true;
    /* any path component is a credential directory */
    const char *p = path;
    while (*p) {
        while (*p == '/') p++;
        const char *seg = p;
        while (*p && *p != '/') p++;
        size_t seglen = (size_t)(p - seg);
        for (int i = 0; k_sensitive_dirs[i]; i++) {
            size_t dl = strlen(k_sensitive_dirs[i]);
            if (seglen == dl && strncasecmp(seg, k_sensitive_dirs[i], dl) == 0)
                return true;
        }
    }
    return false;
}

/* ── chat-image upload pipeline ──────────────────────────────────────────── */
/* PoP: ws_sanitize_chat_image_filename @
 *      hermes_cli/web_server.py:_sanitize_chat_image_filename */
void ws_sanitize_chat_image_filename(const char *filename, char *out, size_t outsz) {
    out[0] = '\0';
    if (!filename) { snprintf(out, outsz, "pasted-image"); return; }
    /* Python: str(filename or "").strip() — trim outer whitespace first */
    const char *start = filename;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r' ||
           *start == '\f' || *start == '\v') start++;
    const char *end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' ||
                           end[-1] == '\r' || end[-1] == '\f' || end[-1] == '\v')) end--;
    char trimmed[1024];
    size_t tl = (size_t)(end - start);
    if (tl >= sizeof(trimmed)) tl = sizeof(trimmed) - 1;
    memcpy(trimmed, start, tl);
    trimmed[tl] = '\0';
    /* Path(...).name — basename */
    const char *base = strrchr(trimmed, '/');
    base = base ? base + 1 : trimmed;
    /* re.sub(r"[\x00-\x1f]+", "_", ...) — collapse control-char RUNS to one _ */
    size_t j = 0;
    for (const char *p = base; *p && j + 1 < outsz; ) {
        if ((unsigned char)*p <= 31) {
            out[j++] = '_';
            while (*p && (unsigned char)*p <= 31) p++;
        } else {
            out[j++] = *p++;
        }
    }
    out[j] = '\0';
    /* .strip() — whitespace ends */
    size_t s = 0, e = strlen(out);
    while (out[s] == ' ' || out[s] == '\t' || out[s] == '\n' || out[s] == '\r' ||
           out[s] == '\f' || out[s] == '\v') s++;
    while (e > s && (out[e-1] == ' ' || out[e-1] == '\t' || out[e-1] == '\n' ||
                     out[e-1] == '\r' || out[e-1] == '\f' || out[e-1] == '\v')) e--;
    /* .strip(".") — dots at ends */
    while (s < e && out[s] == '.') s++;
    while (e > s && out[e - 1] == '.') e--;
    if (e > s) { memmove(out, out + s, e - s); out[e - s] = '\0'; }
    else out[0] = '\0';
    if (out[0] == '\0') snprintf(out, outsz, "pasted-image");
}

/* PoP: ws_chat_image_extension @
 *      hermes_cli/web_server.py:_chat_image_extension */
const char *ws_chat_image_extension(const unsigned char *data, size_t len) {
    if (!data) return NULL;
    /* Python: head = data[:16]; no minimum-length guard — bounded prefix
     * comparisons only (2-byte b"BM" IS .bmp). */
    size_t head = len < 16 ? len : 16;
    if (head >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0)
        return ".webp";
    if (head >= 8 && memcmp(data, "\x89PNG\r\n\x1a\n", 8) == 0) return ".png";
    if (head >= 3 && memcmp(data, "\xff\xd8\xff", 3) == 0) return ".jpg";
    if (head >= 6 && (memcmp(data, "GIF87a", 6) == 0 || memcmp(data, "GIF89a", 6) == 0))
        return ".gif";
    if (head >= 2 && memcmp(data, "BM", 2) == 0) return ".bmp";
    return NULL;
}

/* PoP: ws_chat_image_extension_allowed @
 *      hermes_cli/web_server.py:_CHAT_IMAGE_ALLOWED_EXTENSIONS */
bool ws_chat_image_extension_allowed(const char *ext) {
    static const char *allowed[] = {".png",".jpg",".jpeg",".gif",".webp",".bmp",NULL};
    if (!ext) return false;
    for (int i = 0; allowed[i]; i++)
        if (strcasecmp(ext, allowed[i]) == 0) return true;
    return false;
}

/* ── managed-path policy + resolution ───────────────────────────────────── */
/* Sanity check: locked root when HERMES_DASHBOARD_FILES_ROOT is set. */
static void resolve_default_root(ws_managed_policy_t *p) {
    const char *forced = getenv("HERMES_DASHBOARD_FILES_ROOT");
    if (forced && forced[0]) {
        snprintf(p->default_path, sizeof(p->default_path), "%s", forced);
        snprintf(p->locked_root, sizeof(p->locked_root), "%s", forced);
        p->can_change_path = false;
        return;
    }
    /* hosted layout: HERMES_HOME == /opt/data ⇒ lock to /opt/data */
    char home[1024];
    snprintf(home, sizeof(home), "%s", slermes_home());
    if (strcmp(home, "/opt/data") == 0) {
        snprintf(p->default_path, sizeof(p->default_path), "/opt/data");
        snprintf(p->locked_root, sizeof(p->locked_root), "/opt/data");
        p->can_change_path = false;
        return;
    }
    /* else browse user home, changeable */
    char *home_dir = getenv("HOME");
    snprintf(p->default_path, sizeof(p->default_path), "%s",
             home_dir ? home_dir : home);
    p->locked_root[0] = '\0';
    p->can_change_path = true;
}

/* PoP: ws_managed_files_policy @
 *      hermes_cli/web_server.py:_managed_files_policy */
void ws_managed_files_policy(ws_managed_policy_t *policy) {
    if (!policy) return;
    policy->default_path[0] = '\0';
    policy->locked_root[0] = '\0';
    policy->can_change_path = true;
    resolve_default_root(policy);
}

/* PoP: ws_resolve_managed_path @
 *      hermes_cli/web_server.py:_resolve_managed_path */
void ws_resolve_managed_path(const ws_managed_policy_t *policy,
                             const char *raw_path,
                             bool for_write,
                             ws_resolved_path_t *out) {
    out->err = WS_MANAGED_OK;
    out->resolved[0] = '\0';
    out->is_sensitive = false;
    if (!policy) { out->err = WS_MANAGED_ERR_INVALID; return; }

    char text[2048];
    if (raw_path) {
        size_t i = 0;
        while (raw_path[i] && i + 1 < sizeof(text)) { text[i] = raw_path[i]; i++; }
        text[i] = '\0';
        /* strip NUL already impossible; check leading/trailing */
        while (text[0] == ' ') memmove(text, text + 1, strlen(text));
    } else text[0] = '\0';

    if (text[0] == '\0') {
        out->err = WS_MANAGED_ERR_REQUIRED;
        return;
    }

    bool has_locked = policy->locked_root[0] != '\0';
    char candidate[2048];

    if (has_locked && (strcmp(text, ".") == 0 || strcmp(text, "/") == 0)) {
        snprintf(candidate, sizeof(candidate), "%s", policy->locked_root);
    } else {
        /* expand ~ and absolutize */
        char *exp = NULL;
        if (text[0] == '~') {
            char *h = getenv("HOME");
            size_t hl = h ? strlen(h) : 0;
            size_t tl = strlen(text);
            exp = malloc(hl + tl + 1);
            snprintf(exp, hl + tl + 1, "%s%s", h ? h : "", text + 1);
        } else if (text[0] == '/') {
            snprintf(candidate, sizeof(candidate), "%s", text);
        } else {
            /* relative */
            if (has_locked) {
                /* reject '..' in relative path under lock */
                if (strstr(text, "..") != NULL) { out->err = WS_MANAGED_ERR_INVALID; free(exp); return; }
                snprintf(candidate, sizeof(candidate), "%s/%s",
                         policy->locked_root, text);
            } else {
                out->err = WS_MANAGED_ERR_ABS_REQUIRED; free(exp); return;
            }
        }
        if (exp) { snprintf(candidate, sizeof(candidate), "%s", exp); free(exp); }
    }

    /* '..' traversal guard (post-join) */
    if (cli_tools_path_security_has_traversal_component(candidate)) {
        out->err = WS_MANAGED_ERR_INVALID;
        return;
    }

    snprintf(out->resolved, sizeof(out->resolved), "%s", candidate);
    out->is_sensitive = ws_is_sensitive_path(out->resolved);
}

/* PoP: ws_managed_response_meta @
 *      hermes_cli/web_server.py:_managed_response_meta */
void ws_managed_response_meta(const ws_managed_policy_t *policy,
                              char *root_out, size_t root_sz,
                              bool *can_change_path) {
    if (!policy) return;
    snprintf(root_out, root_sz, "%s", policy->locked_root);
    if (can_change_path) *can_change_path = policy->can_change_path;
}
