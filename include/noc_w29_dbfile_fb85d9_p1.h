/*
 * noc_w29_dbfile_fb85d9_p1.h - C surface for upstream
 * persistence/sqlite/_state/dbfile.py (gate w29_dbfile_fb85d9_p1).
 *
 * The file-identity half of the state.db health helpers: the SQLite header
 * reader (magic, page size coercion, the five big-endian u32 fields, the
 * application_id probe), the WAL/SHM sidecar path derivation (literal,
 * canonical and realpath-resolved spellings), the "truly unlinked" identity
 * verdicts, and the holder-PID classification.
 *
 * Every filesystem answer is a seam and arrives in the request: os.path is
 * POSIX here, `realpath` is an explicit map, `os.stat`, `os.geteuid`,
 * `own_descriptors()` and `_pread_db_header` are scripted by the fixture.
 * Nothing in this file opens a file or a database.
 */
#ifndef NOC_W29_DBFILE_FB85D9_P1_H
#define NOC_W29_DBFILE_FB85D9_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Byte offsets / sizes the header reader depends on. */
#define W29_DBFILE_FB85D9_P1_APPLICATION_ID_OFFSET 68
#define W29_DBFILE_FB85D9_P1_HEADER_BYTES 100
#define W29_DBFILE_FB85D9_P1_MAGIC "SQLite format 3"

/* PoP: w29_dbfile_fb85d9_p1_header @ persistence/sqlite/_state/dbfile.py:_parse_sqlite_header
 * in:  hex string of the file's first bytes ("" for a 0-byte read).
 * out: {"valid": false} when shorter than 100 bytes or not "SQLite format 3\0",
 *      else {"valid": true, "page_size": <65536 when the raw u16 is 1, else the
 *      raw u16>, "change_counter", "page_count", "user_version",
 *      "application_id", "version_valid_for"} read as big-endian u32 at
 *      offsets 24, 28, 60, 68, 92. */
json_t *w29_dbfile_fb85d9_p1_header(const char *hex);

/* PoP: w29_dbfile_fb85d9_p1_appid @ persistence/sqlite/_state/dbfile.py:_read_sqlite_application_id
 * in:  {"header": <hex>|null}  (the lock-safe pread's bytes, null when it failed)
 * out: the big-endian u32 at offset 68, or null when the read was None, was
 *      shorter than 72 bytes, or did not start with "SQLite format 3\0". */
json_t *w29_dbfile_fb85d9_p1_appid(const json_t *req);

/* PoP: w29_dbfile_fb85d9_p1_sidecars @ persistence/sqlite/_state/dbfile.py:_stat_sqlite_sidecar_identity
 * in:  {"stat": {"-wal": [dev,ino]|null, "-shm": [dev,ino]|null}}
 * out: the same object with the null identities dropped (absent sidecars never
 *      appear). */
json_t *w29_dbfile_fb85d9_p1_sidecars(const json_t *req);

/* PoP: w29_dbfile_fb85d9_p1_watched @ persistence/sqlite/_state/dbfile.py:_watched_sqlite_sidecar_paths
 * in:  {"path": <abs posix path>, "resolve": {<literal>: <resolved>, ...}}
 * out: {<canonical>: <literal still-named path>} for -wal and -shm: the two
 *      literal spellings first, then the realpath'd parent + literal basename,
 *      then the fully realpath'd path; a canonical already present keeps its
 *      first literal (dict setdefault).  `resolve` is the realpath seam: it is
 *      an exact-match map, an unlisted path resolves to itself. */
json_t *w29_dbfile_fb85d9_p1_watched(const json_t *req);

/* PoP: w29_dbfile_fb85d9_p1_unlinked_identity @ persistence/sqlite/_state/dbfile.py:_identity_is_truly_unlinked
 * in:  {"identity": [dev,ino], "stat": [dev,ino]|null}  (null = os.stat raised)
 * out: true when the watched path cannot be stat'd, or names a different
 *      (st_dev, st_ino). */
json_t *w29_dbfile_fb85d9_p1_unlinked_identity(const json_t *req);

/* PoP: w29_dbfile_fb85d9_p1_unlinked_fd @ persistence/sqlite/_state/dbfile.py:_fd_is_truly_unlinked
 * in:  {"fd_stat": [dev,ino] | {"errno": <n>}, "watched_stat": [dev,ino]|null}
 * out: false when the fd stat failed with ENOENT(2)/ESRCH(3) (a closed
 *      descriptor cannot keep a generation alive), true on any other stat
 *      error, else the identity verdict. */
json_t *w29_dbfile_fb85d9_p1_unlinked_fd(const json_t *req);

/* PoP: w29_dbfile_fb85d9_p1_own_fd @ persistence/sqlite/_state/dbfile.py:_own_descriptor_for_identity
 * in:  {"identity": [dev,ino]|null, "fds": [[fd,[dev,ino]], ...]|null}
 * out: this process's descriptor for that exact inode, or null.  Pathnames are
 *      never consulted; `fds` is own_descriptors() (null = unreadable). */
json_t *w29_dbfile_fb85d9_p1_own_fd(const json_t *req);

/* PoP: w29_dbfile_fb85d9_p1_oplink @ persistence/sqlite/_state/dbfile.py:_is_operator_store_link
 * in:  {"link_uid": <uid>, "euid": <uid>, "target_exists": bool,
 *       "target_uid": <uid>|null}
 * out: true only when the link resolves to an existing file AND both the link
 *      and its target belong to the effective user. */
json_t *w29_dbfile_fb85d9_p1_oplink(const json_t *req);

/* PoP: w29_dbfile_fb85d9_p1_holder_pids @ persistence/sqlite/_state/dbfile.py:_concrete_state_db_holder_pids
 * in:  {"path": <abs posix path>, "holders": [[pid, <path>], ...]}
 * out: unique PIDs (first-appearance order) proven to hold the DB, -wal or -shm:
 *      pid > 0 and canonical_sqlite_path(path) in the three canonical spellings. */
json_t *w29_dbfile_fb85d9_p1_holder_pids(const json_t *req);

/* PoP: w29_dbfile_fb85d9_p1_canon @ persistence/sqlite/_state/holders.py:canonical_sqlite_path
 * in:  <path string, possibly with a " (deleted)" suffix>
 * out: normcase(abspath(path without the suffix)) - POSIX, so a lexical
 *      normalization of the absolute spelling. */
json_t *w29_dbfile_fb85d9_p1_canon(const char *path);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W29_DBFILE_FB85D9_P1_H */
