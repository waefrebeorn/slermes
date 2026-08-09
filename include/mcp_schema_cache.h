/*
 * mcp_schema_cache.h — C11 port of tools/mcp_schema_cache.py.
 * Pure I/O + hashing + JSON.  Self-contained.
 */

#ifndef HERMES_MCP_SCHEMA_CACHE_H
#define HERMES_MCP_SCHEMA_CACHE_H

#include <stddef.h>
#include <stdbool.h>
#include <json.h>

/* _cache_path → cache/mcp_schema_cache.json under get_hermes_home().
 * Caller frees the returned path. */
char *mcp_cache_path(void);

/* config_fingerprint — SHA-256 of connection-defining config parts,
 * hex-encoded, truncated to 16 chars.  Caller frees. */
char *mcp_config_fingerprint(const json_t *config);

/* _load_all — read the cache file; empty object if missing/unreadable. */
json_t *mcp_load_all(void);
/* _save_all — atomically write (mode 0600). */
void mcp_save_all(const json_t *data);

/* Cross-process file lock. */
struct mcp_lock_cookie;
enum mcp_lock_result { MCP_LOCK_COOKIE, MCP_LOCK_BUSY, MCP_LOCK_UNAVAILABLE };

/* get_cached_entry — entry dict with matching fingerprint, or NULL (caller
 * frees).  has_cached_entry wraps it. */
json_t *mcp_get_cached_entry(const char *server_name, const char *fingerprint);
bool mcp_has_cached_entry(const char *server_name, const char *fingerprint);

/* write_cache_entry — persist tools after a successful live connect. */
void mcp_write_cache_entry(const char *server_name, const char *fingerprint,
                           const json_t *tools, const json_t *utility_tools);
/* clear_cache_entry — remove a server's entry. */
void mcp_clear_cache_entry(const char *server_name);

/* tools_from_cache_entry / utility_tools_from_cache_entry — extract the
 * tool-schema arrays (returns a fresh clone). */
json_t *mcp_tools_from_cache_entry(const json_t *entry);
json_t *mcp_utility_tools_from_cache_entry(const json_t *entry);

#endif /* HERMES_MCP_SCHEMA_CACHE_H */
