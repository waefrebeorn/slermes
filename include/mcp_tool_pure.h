/*
 * mcp_tool_pure.h — Pure-logic helpers ported from tools/mcp_tool.py.
 * Self-contained: libjson + libpath + slermes_home only.
 */

#ifndef HERMES_MCP_TOOL_PURE_H
#define HERMES_MCP_TOOL_PURE_H

#include <stddef.h>
#include <stdbool.h>
#include <json.h>

/* _LockCookie — opaque handle on a held cross-process file lock.
 * Returned by mcp_try_acquire_mcp_discovery_lock. release() drops the lock;
 * free() releases + frees the cookie. */
struct mcp_lock_cookie;

/* _acquire_lock_on_fh: try a non-blocking exclusive flock on fd.
 * Returns true (acquired), false (held by another).  If a non-recoverable
 * fcntl error occurs, *out_errno_reraise (if non-NULL) receives errno. */
bool mcp_acquire_lock_on_fh(int fd, int *out_errno_reraise);

/* _try_acquire_mcp_discovery_lock — cross-process lock on ~/.mcp-discovery.lock.
 * Returns MCP_LOCK_COOKIE (acquired, cookie in *out), MCP_LOCK_BUSY (held),
 * or MCP_LOCK_UNAVAILABLE (fs unavailable/broken). */
enum mcp_lock_result { MCP_LOCK_COOKIE, MCP_LOCK_BUSY, MCP_LOCK_UNAVAILABLE };
enum mcp_lock_result mcp_try_acquire_mcp_discovery_lock(struct mcp_lock_cookie **out);

void mcp_lock_cookie_release(struct mcp_lock_cookie *c);
void mcp_lock_cookie_free(struct mcp_lock_cookie *c);

/* _warn_hidden_whitespace: collect dotted key paths in `config` whose string
 * values have leading/trailing whitespace.  is_warned/mark_warned handle the
 * process-global de-dup (caller backs them with a set).  Returns a malloc'd
 * array of path strings, count in *out_count.  Free with
 * mcp_warn_hidden_whitespace_free. */
char **mcp_warn_hidden_whitespace(const char *server_name,
                                  const json_t *config,
                                  bool (*is_warned)(const char *srv, const char *path),
                                  void (*mark_warned)(const char *srv, const char *path),
                                  size_t *out_count);
void mcp_warn_hidden_whitespace_free(char **items, size_t n);

/* matches_name_filter — exact-match first (O(1)), then fnmatch-case globs.
 * patterns[n] must be non-NULL strings; NULL entries skipped. */
bool matches_name_filter(const char *tool_name, const char **patterns, size_t n);

#endif /* HERMES_MCP_TOOL_PURE_H */
