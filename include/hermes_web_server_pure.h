/*
 * hermes_web_server_pure.h — Pure-logic helpers for hermes_cli/web_server.py.
 *
 * Faithful C11 ports of the deterministic Python helpers in web_server.py
 * that have no FastAPI / asyncio dependency. Everything in this header
 * is appropriate for `tests/oracle/` parity coverage.
 *
 * Conventions:
 *   - ws_schema_*  ↔ web_server._build_schema_from_config and friends
 *   - ws_path_*    ↔ web_server._canonical_path / _path_is_under / _path_text
 *   - ws_fs_*      ↔ web_server._fs_mime_type / _fs_looks_binary /
 *                    _fs_regular_file / _fs_find_git_root
 *   - ws_data_url_decode ↔ web_server._decode_data_url
 *
 * No "void *passthrough", no "not implemented", no third-party deps. If
 * one of these returns false / "" the caller is expected to look at the
 * integer error code (see ws_path_status_* below).
 */

#ifndef HERMES_WEB_SERVER_PURE_H
#define HERMES_WEB_SERVER_PURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Schema (config field classification) ────────────────────────────────── */

/* Mirrors Python _infer_type. The strings are exactly the Python set so the
 * oracle diff stays byte-equal. */
const char *ws_infer_type_bool (bool   v);
const char *ws_infer_type_int  (long   v);
const char *ws_infer_type_f64  (double v);
const char *ws_infer_type_str  (const char *v);

/* Walk a `key → scalar` map (top-level config), producing the dot-key →
 * schema-entry mapping that drives the dashboard Settings page.
 *
 * - `keys`, `vals` are length-`n` parallel arrays (JSON null sentinel
 *   must already have been filtered out by the caller).
 * - The result is a freshly-malloc'd array of `ws_schema_entry_t` plus
 *   a freshly-malloc'd `keys` array of dot-path strings owned by entries
 *   (single free(NULL) safe per `ws_schema_free`). `n_out` receives the
 *   count.
 *
 * Skip keys (e.g. `_config_version`) and category merges are honoured to
 * the same byte-equal semantics as the Python reference. */
typedef struct {
    char  *key;     /* dot-path key, owned */
    char  *type;     /* "boolean"|"number"|"string"|"list"|"object", owned */
    char  *description; /* owned */
    char  *category; /* owned, possibly identical to a table key */
} ws_schema_entry_t;

ws_schema_entry_t *ws_build_schema_from_pairs(
    const char *const *keys,
    const char *const *vals,
    size_t   n,
    size_t  *n_out);

/* Pair-getter variant used by the oracle; reads the i-th entry's scalars
 * into `*out_key / *out_type / *out_desc / *out_cat`. Caller pre-allocates
 * buffers of size `cap`; returns false on overflow. */
bool ws_schema_entry_get(const ws_schema_entry_t *arr, size_t i,
                         char *out_key, size_t keycap,
                         char *out_type, size_t typecap,
                         char *out_desc, size_t desccap,
                         char *out_cat,  size_t catcap);

void ws_schema_free(ws_schema_entry_t *arr, size_t n);

/* ── Path helpers ───────────────────────────────────────────────────────── */

/* Web-server specific HTTP-style status that maps to the Python
 * HTTPException pathways. The Python code raises HTTPException on
 * bad inputs; these helpers never throw, they return one of these codes
 * alongside a (possibly unused) `out_buf`. */
typedef enum {
    WS_PATH_OK              = 0,
    WS_PATH_EMPTY          = 1,
    WS_PATH_HAS_NUL        = 2,
    WS_PATH_PARSE_FAILED   = 3,
    WS_PATH_NOT_FOUND      = 4,
    WS_PATH_IS_DIR         = 5,
    WS_PATH_NOT_REGULAR    = 6,
    WS_PATH_NOT_READABLE   = 7
} ws_path_status_t;

const char *ws_path_status_str(ws_path_status_t s);

/* Python `_path_text`: strip + reject NUL. status_out is optional.
 *
 * Caveat: `ws_path_text` is appropriate for NUL-terminated C-string inputs
 * (the common case — a path from PyArg_ParseTuple / a config value).
 * If the caller holds a byte buffer that MAY contain an embedded NUL — the
 * Python `_path_text(\"abc\\x00def\")` path, which rejects — call
 * `ws_path_text_n` and pass the true byte length. `ws_path_text` is a
 * thin wrapper around `ws_path_text_n(raw, raw ? strlen(raw) : 0, out, cap)`
 * and therefore CANNOT see bytes past the first embedded NUL. */
ws_path_status_t ws_path_text(const char *raw,
                              char *out, size_t cap);

/* Byte-length variant: Python sees the entire input string (it accepts NUL
 * bytes), so callers that decode escape sequences (`\\0`, `\\n`, `\\t`) into
 * raw bytes MUST use this variant to faithfully port Python behavior. */
ws_path_status_t ws_path_text_n(const char *raw, size_t raw_len,
                                char *out, size_t cap);

/* Python `_canonical_path`: realpath; require_exists folds in. */
ws_path_status_t ws_canonical_path(const char *raw, bool require_exists,
                                   char *out, size_t cap);

/* Python `_path_is_under`: the simpler `target == root || root in
 * target.parents` semantic, comparing *resolved* (lexicographic-equivalent)
 * paths. Both inputs must be canonical (realpath-resolved). */
bool ws_path_is_under(const char *root, const char *target);

/* Python `_decode_data_url`: split "data:<mime>;base64,<b64>", validate,
 * and decode. Returns the bytes via `out_bytes` (malloc'd, caller frees)
 * and mime type via `out_mime` (caller-owned buf). Status indicates
 * wavefront outcome. */
ws_path_status_t ws_decode_data_url(const char *data_url,
                                    char *out_mime, size_t mime_cap,
                                    unsigned char **out_bytes,
                                    size_t *out_len);

#define WS_MANAGED_FILE_MAX_BYTES (100u * 1024u * 1024u)

/* ── Filesystem mime / binary helpers ──────────────────────────────────── */

/* Python `_fs_mime_type`: extension table lookup, fall-through to
 * libmagic-equivalent (extension → mime) best-effort. */
const char *ws_fs_mime_type(const char *path);

/* Python `_fs_looks_binary`: NUL byte → true, otherwise ratio of
 * control-bytes-not-9-10-13 over total length > 0.12. */
bool ws_fs_looks_binary(const unsigned char *data, size_t len);

/* Python `_fs_regular_file`: stat the file, validate it's a regular
 * file and readable. Fills the out_stat buf on success. */
ws_path_status_t ws_fs_regular_file(const char *path,
                                   struct stat *out_stat);

/* PoP: ws_fs_path @ hermes_cli/web_server.py:_fs_path
 * strip; reject NUL; file: URL unwrap; expanduser; absolutize; resolve.
 * Returns malloc'd absolute path on success, NULL on error. */
char *ws_fs_path(const char *raw_path);

/* Python `_fs_find_git_root`: walk parents up to 50 levels looking for
 * `.git` directory/file. Returns malloc'd absolute path, or NULL. */
char *ws_fs_find_git_root(const char *start);

/* Python `_audio_extension_for_mime`: extension mapping. Returns NULL
 * if unknown. The returned pointer is to a string literal (no free). */
const char *ws_audio_extension_for_mime(const char *mime);

/* PoP: ws_git_path @ hermes_cli/web_server.py:_git_path
 * str(path or "").strip(); if "\\0" in path: raise; resolve with realpath.
 * Returns malloc'd path or NULL on error. */
char *ws_git_path(const char *raw_path);

/* PoP: ws_media_serve_roots @ hermes_cli/web_server.py:_media_serve_roots
 * Returns NULL-terminated array of malloc'd strings, caller frees each. */
char **ws_media_serve_roots(void);

/* PoP: ws_ensure_managed_root @ hermes_cli/web_server.py:_ensure_managed_root
 * Returns 0 on success, -1 on error. */
int ws_ensure_managed_root(const char *path);

/* ── Dashboard pure helpers (web_server.py deterministic helpers) ─────────── */

/* PoP: web_parse_expiry_ts @ hermes_cli/web_server.py:_parse_expiry_ts
 * Parse an ISO-8601 timestamp (with optional trailing 'Z') into a Unix
 * epoch (double). Falls back to now+600s on any parse error. */
double web_parse_expiry_ts(const char *value);

/* PoP: web_normalize_telegram_user_id @ hermes_cli/web_server.py:_normalize_telegram_user_id
 * Return a malloc'd normalized Telegram user id (digits only) or NULL if the
 * value does not fullmatch ^[0-9]+$. Caller frees. */
char *web_normalize_telegram_user_id(const char *value);

/* PoP: web_telegram_onboarding_error_message @ hermes_cli/web_server.py:_telegram_onboarding_error_message
 * Map an onboarding error code to a human message; returns `fallback` when the
 * code is unknown. Returns a string literal (no free). */
const char *web_telegram_onboarding_error_message(const char *error, const char *fallback);

/* PoP: web_truncate_token @ hermes_cli/web_server.py:_truncate_token
 * Return a malloc'd display-safe token: "…XXXXXX" (last `visible` chars).
 * JWTs (>=2 dots) are reduced to their trailing signature segment first.
 * Empty/invalid input returns a malloc'd empty string. Caller frees. */
char *web_truncate_token(const char *value, int visible);

/* PoP: web_windows_build_number @ hermes_cli/web_server.py:_windows_build_number
 * Extract the Windows NT build number (10.0.NNNNN) from a version/platform
 * string, or -1 if not found. */
int web_windows_build_number(const char *version, const char *platform_label);

/* PoP: web_cron_optional_text @ hermes_cli/web_server.py:_cron_optional_text
 * Strip whitespace; optionally strip a trailing slash. Returns a malloc'd
 * string, or NULL for None/empty input. Caller frees. */
char *web_cron_optional_text(const char *value, bool strip_trailing_slash);

/* PoP: web_cron_string_list @ hermes_cli/web_server.py:_cron_string_list
 * Normalize a newline/comma-separated string or a list into a NULL-terminated
 * array of malloc'd strings (whitespace-trimmed, empties dropped). Returns
 * NULL for None/invalid input. Caller frees each element and the array. */
char **web_cron_string_list(const char *value);

/* PoP: web_channel_or_close_code @ hermes_cli/web_server.py:_channel_or_close_code
 * Return a malloc'd copy of `channel` if it matches ^[A-Za-z0-9._-]{1,128}$,
 * else NULL. Caller frees. */
char *web_channel_or_close_code(const char *channel);

/* PoP: web_read_active_session_file @ hermes_cli/web_server.py:_read_active_session_file
 * Read a JSON file and return a malloc'd session_id (str of "session_id"
 * field) or NULL on any error / missing value. Caller frees. */
char *web_read_active_session_file(const char *path);

/* PoP: _platform_env_prefixes @ hermes_cli/web_server.py:_platform_env_prefixes
 * Env-var prefixes owned by a messaging platform card. Returns a malloc'd
 * NULL-terminated array of malloc'd strings, or NULL on alloc failure.
 * Caller frees with web_free_strv(). */
char **web_platform_env_prefixes(const char *platform_id);

/* Free a NULL-terminated array of malloc'd strings produced by the helpers. */
void web_free_strv(char **v);

/* Reusable client-IP resolution (Port of dashboard_auth _client_ip, identical
 * across middleware.py / routes.py / token_auth.py). Takes the parsed
 * X-Forwarded-For header value and the direct client host; returns a malloc'd
 * string (first forwarded hop, trimmed, else client_host, else ""). Caller frees. */
/* PoP: web_client_ip @ hermes_cli/dashboard_auth/middleware.py:_client_ip */
/* PoP: web_client_ip @ hermes_cli/dashboard_auth/routes.py:_client_ip */
/* PoP: web_client_ip @ hermes_cli/dashboard_auth/token_auth.py:_client_ip */
char *web_client_ip(const char *x_forwarded_for, const char *client_host);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_WEB_SERVER_PURE_H */
