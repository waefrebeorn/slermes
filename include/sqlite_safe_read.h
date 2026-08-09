/*
 * sqlite_safe_read.h — C11 port of hermes_cli/sqlite_safe_read.py
 * (NS-609). Lock-safe inspection of SQLite database files.
 *
 * POSIX advisory locks are cancelled process-wide by close() on any FD for
 * a file. This module guards against byte-probing a database whose
 * connections are still live in this process, which is the documented route
 * to "database disk image is malformed".
 *
 * Pure logic: thread-safe connection registry + safe file byte-reads.
 * Reuses libsqlite3 (sqlite3.h) for connection introspection.
 */

#ifndef HERMES_SQLITE_SAFE_READ_H
#define HERMES_SQLITE_SAFE_READ_H

#include <stddef.h>
#include <stdbool.h>
#include <sqlite3.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Registry ─────────────────────────────────────────────── */

/* PoP: track_connection @ hermes_cli/sqlite_safe_read.py:track_connection */
void sqlite_safe_track_connection(const char *path);

/* PoP: untrack_connection @ hermes_cli/sqlite_safe_read.py:untrack_connection */
void sqlite_safe_untrack_connection(const char *path);

/* PoP: has_live_connection @ hermes_cli/sqlite_safe_read.py:has_live_connection */
bool sqlite_safe_has_live_connection(const char *path);

/* ── Connection introspection (via sqlite3 PRAGMA) ────────── */

/* PoP: _canonical_db_path @ hermes_cli/sqlite_safe_read.py:_canonical_db_path */
char *sqlite_safe_canonical_db_path(sqlite3 *conn);

/* PoP: _key @ hermes_cli/sqlite_safe_read.py:_key */
char *sqlite_safe_key(const char *path);

/* ── Safe reads ───────────────────────────────────────────── */

/* PoP: page_count_bytes @ hermes_cli/sqlite_safe_read.py:page_count_bytes */
long long sqlite_safe_page_count_bytes(sqlite3 *conn);

/* PoP: file_length_matches_header @ hermes_cli/sqlite_safe_read.py:file_length_matches_header */
bool sqlite_safe_file_length_matches_header(sqlite3 *conn);

/* PoP: read_header_bytes_preopen @ hermes_cli/sqlite_safe_read.py:read_header_bytes_preopen */
unsigned char *sqlite_safe_read_header_bytes_preopen(const char *path, size_t length, bool force, size_t *out_len);

/* ── Offline file access guard ───────────────────────────── */

/* PoP: LiveConnectionError @ hermes_cli/sqlite_safe_read.py:LiveConnectionError */
typedef int sqlite_safe_error_t;
#define SQLITE_SAFE_ERR_LIVE_CONN 1
#define SQLITE_SAFE_ERR_OK 0

/* PoP: offline_file_access @ hermes_cli/sqlite_safe_read.py:offline_file_access */
sqlite_safe_error_t sqlite_safe_offline_file_access_enter(const char *path);
void sqlite_safe_offline_file_access_exit(const char *path);

/* ── Connected, tracked connection ───────────────────────── */

/* PoP: connect_tracked @ hermes_cli/sqlite_safe_read.py:connect_tracked */
typedef int (*sqlite_safe_open_fn)(const char *path, sqlite3 **, unsigned int, const char *);
sqlite3 *sqlite_safe_connect_tracked(const char *path,
                                     const char *tracking_path,
                                     const sqlite_safe_open_fn *open_fn,
                                     const char *uri);

/* ── Tracked connection wrapper ──────────────────────────── */
/*
 * Python mixes a tracking 'close()' into any Connection subclass via type()
 * (dynamic subclass creation) and __class__ reassignment (_retrofit_tracking).
 * sqlite3* is opaque in C11 and cannot be subclassed, so the faithful
 * translation is an opaque wrapper holding the db* + tracked path, with an
 * explicit close() that untracks — same correctness contract, no metaclass.
 */
/* PoP: _TrackingMixin @ hermes_cli/sqlite_safe_read.py:_TrackingMixin */
/* PoP: _tracking_factory @ hermes_cli/sqlite_safe_read.py:_tracking_factory */
/* PoP: _retrofit_tracking @ hermes_cli/sqlite_safe_read.py:_retrofit_tracking */
typedef struct sqlite_safe_tracked_conn sqlite_safe_tracked_conn_t;
struct sqlite_safe_tracked_conn {
    sqlite3 *db;
    char    *tracked_path;   /* canonical path we registered for this conn */
};

/* PoP: TrackedConnection @ hermes_cli/sqlite_safe_read.py:TrackedConnection */
/* PoP: close @ hermes_cli/sqlite_safe_read.py:_TrackingMixin.close */
sqlite_safe_tracked_conn_t *sqlite_safe_tracked_conn_open(const char *path,
                                                          const char *tracking_path,
                                                          const sqlite_safe_open_fn *open_fn,
                                                          const char *uri);
/* Wraps sqlite3_close + untrack; mirrors Python's TrackedConnection.close
 * contract: untrack only after close() succeeds. */
int sqlite_safe_tracked_conn_close(sqlite_safe_tracked_conn_t *tc);



/* PoP: SQLITE_HEADER_MAGIC @ hermes_cli/sqlite_safe_read.py:SQLITE_HEADER_MAGIC */
extern const unsigned char sqlite_safe_header_magic[16];

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SQLITE_SAFE_READ_H */
