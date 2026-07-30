/*
 * web_server_managed_files.h — managed-files + media security surface of the
 * Hermes dashboard (faithful C11 port of hermes_cli/web_server.py's
 * files/media cluster).
 *
 * Heavy security-critical dependency layer the managed-files API and chat-image
 * routes sit on. REAL behavior, no stubs:
 *   - sensitive-path guard  → credential denylist (.env, auth.json, mcp-tokens/,
 *                             pairing/, …) — the #57505 exfil surface
 *   - MIME + binary sniff   → _FS_MIME_TYPES table + fallback
 *   - base64 data-url decode → _decode_data_url (validates base64, size cap)
 *   - chat-image pipeline    → sanitize filename, magic-byte extension sniff,
 *                             allowed-extension gate
 *   - managed-path policy    → locked-root vs home-root resolution, '..' guard,
 *                             under-root confinement
 *
 * Reuses: libbase64 (base64_encode/decode), cli_tools_path_security
 * (has_traversal_component), slermes_home().
 */

#ifndef WEB_SERVER_MANAGED_FILES_H
#define WEB_SERVER_MANAGED_FILES_H

#include <stdbool.h>
#include <stddef.h>

#include "hermes_web_server_pure.h"  /* sibling: ws_decode_data_url,
                                       ws_fs_mime_type, ws_fs_looks_binary,
                                       ws_path_is_under, ws_canonical_path,
                                       WS_MANAGED_FILE_MAX_BYTES */

#ifdef __cplusplus
extern "C" {
#endif

/* ── sensitive-path guard (Python _is_sensitive_filename/_is_sensitive_path) ── */

/* True for a basename the managed-files API must never expose (case-insensitive
 * .env / .env.<suffix> / .envrc variants plus the canonical Hermes credential
 * basenames). */
bool ws_is_sensitive_filename(const char *name);

/* True for any path component tree that is credential material
 * (mcp-tokens/, pairing/) or a sensitive basename. Read-side exfil guard. */
bool ws_is_sensitive_path(const char *path);

/* ── chat-image upload pipeline (Python _sanitize_chat_image_filename /
 *     _chat_image_extension / _decode_chat_image_upload) ──────────────────── */
/* Strips control chars, leading/trailing dots; falls back to "pasted-image". */
void ws_sanitize_chat_image_filename(const char *filename, char *out, size_t outsz);

/* Magic-byte sniff → extension (".png"/".jpg"/".gif"/".bmp"/".webp") or NULL. */
const char *ws_chat_image_extension(const unsigned char *data, size_t len);

/* Allowed chat-image extensions + caps (mirror web_server constants). */
#define WS_CHAT_IMAGE_UPLOAD_MAX_BYTES (25 * 1024 * 1024)
bool ws_chat_image_extension_allowed(const char *ext);

/* ── managed-path policy + resolution (Python _managed_files_policy /
 *     _resolve_managed_path / _managed_response_meta / _managed_file_entry) ── */

typedef struct {
    char default_path[1024];   /* resolved default browse root */
    char locked_root[1024];    /* non-empty ⇒ locked to this root */
    bool can_change_path;      /* false when locked / hosted */
} ws_managed_policy_t;

/* Resolve the managed-files policy. When HERMES_DASHBOARD_FILES_ROOT is set,
 * the root is locked to it. When HERMES_HOME resolves to /opt/data (hosted
 * layout), the root is locked to /opt/data. Otherwise browses the user home
 * and can_change_path=true. */
void ws_managed_files_policy(ws_managed_policy_t *policy);

/* Result of resolving a managed path. */
typedef enum {
    WS_MANAGED_OK = 0,
    WS_MANAGED_ERR_REQUIRED = 400,     /* empty path */
    WS_MANAGED_ERR_INVALID = 401,      /* '..' or NUL (HTTP 400 in Python) */
    WS_MANAGED_ERR_ABS_REQUIRED = 402, /* relative without locked root (HTTP 400) */
    WS_MANAGED_ERR_OUTSIDE = 403,      /* escaped the managed root */
} ws_path_resolve_err_t;

typedef struct {
    ws_path_resolve_err_t err;
    char resolved[2048];   /* absolute resolved path (NUL on error) */
    bool is_sensitive;     /* resolved path hits the sensitive guard */
} ws_resolved_path_t;

/* Resolve `raw_path` against the policy. for_write allows a not-yet-existing
 * target (parent must exist). Confines to the locked root when present and
 * rejects '..' traversal. Fills `out`. */
void ws_resolve_managed_path(const ws_managed_policy_t *policy,
                             const char *raw_path,
                             bool for_write,
                             ws_resolved_path_t *out);

/* Metadata for the /api/files response (root/locked_root/can_change_path). */
void ws_managed_response_meta(const ws_managed_policy_t *policy,
                              char *root_out, size_t root_sz,
                              bool *can_change_path);

#ifdef __cplusplus
}
#endif

#endif /* WEB_SERVER_MANAGED_FILES_H */
