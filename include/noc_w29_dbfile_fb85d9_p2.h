/*
 * noc_w29_dbfile_fb85d9_p2.h - C surface for upstream
 * persistence/sqlite/_state/dbfile.py (gate w29_dbfile_fb85d9_p2).
 *
 * The decision half of the state.db file helpers: the invalid/zeroed header
 * verdicts, the quarantine sidecar name (kind + collision counter), the
 * read-only stats snapshot (statement text, bound parameters and every derived
 * field), the holder count and the Desktop-holder reap predicate.
 *
 * Seams: the byte probe (`_preopen_header`) and `is_zeroed_sqlite_file` answer
 * from the request; the stats snapshot is answered by a scripted connection
 * whose every `execute` is recorded, so the port reproduces the exact SQL and
 * the exact derivations without opening a database.
 */
#ifndef NOC_W29_DBFILE_FB85D9_P2_H
#define NOC_W29_DBFILE_FB85D9_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The zeroed-file signature prefix the fallback probe compares against (13
 * bytes: the 16-byte magic starts with it). */
#define W29_DBFILE_FB85D9_P2_MAGIC13 "SQLite format 3"

/* PoP: w29_dbfile_fb85d9_p2_invalid_header @ persistence/sqlite/_state/dbfile.py:has_invalid_sqlite_header_preopen
 * in:  {"head": <hex>|null}  (null = the probe refused to run)
 * out: true when the head was read and does NOT start with "SQLite format 3"
 *      (a 0-byte file reads as "" and is invalid). */
json_t *w29_dbfile_fb85d9_p2_invalid_header(const json_t *req);

/* PoP: w29_dbfile_fb85d9_p2_zeroed @ persistence/sqlite/_state/dbfile.py:is_zeroed_state_db
 * in:  {"head": <hex>|null, "probe": {"raises": bool, "answer": bool}}
 * out: the probe's own answer while it works; when it raises, the fallback:
 *      head read, no "SQLite format 3" prefix, and every byte NUL (an empty
 *      head qualifies via all()). */
json_t *w29_dbfile_fb85d9_p2_zeroed(const json_t *req);

/* PoP: w29_dbfile_fb85d9_p2_quarantine @ persistence/sqlite/_state/dbfile.py:quarantine_invalid_state_db
 * in:  {"name": <db file name>, "exists": bool, "invalid": bool, "zeroed": bool,
 *       "ts": "%Y%m%d-%H%M%S", "pid": int, "taken": [<names already on disk>],
 *       "locked": bool}
 * out: the quarantine file name (`<name>.<zeroed|notadb>-<ts>-<pid>.bak`,
 *      suffixed `-<n>` until free), or null when the lock was not acquired, the
 *      file is gone, or it is no longer invalid. */
json_t *w29_dbfile_fb85d9_p2_quarantine(const json_t *req);

/* PoP: w29_dbfile_fb85d9_p2_stats @ persistence/sqlite/_state/dbfile.py:collect_state_db_stats
 * in:  {"wal_size": <bytes>|null, "connect": "ok"|"raise",
 *       "drift": bool|"raise",
 *       "answers": [{"sql": <text>, "params": [...], "one": <v>}
 *                   | {"sql": ..., "params": [...], "all": [[<v>], ...]}
 *                   | {"sql": ..., "params": [...], "none": true}, ...]}
 * out: {"stats": <the 16-field snapshot>, "sql": [{"sql":..., "params":[...]}, ...]}
 *      - the stats object in the module's key order, nulls where a statement
 *      failed or a value did not coerce, and the statement log in execution
 *      order (text and bound parameters byte-for-byte). */
json_t *w29_dbfile_fb85d9_p2_stats(const json_t *req);

/* PoP: w29_dbfile_fb85d9_p2_count_holders @ persistence/sqlite/_state/dbfile.py:count_db_holders
 * in:  {"platform": <sys.platform>, "path": <path>, "resolve": {<p>:<realpath>},
 *       "stat": [dev,ino]|null, "fds": [{"pid":.., "link":..} | {"pid":.., "identity":[..]}]}
 * out: distinct PIDs holding the DB, or null when the host cannot be inspected
 *      (win32/anything but linux and darwin) — the darwin leg matches the
 *      vnode identity, the linux leg the literal resolved path. */
json_t *w29_dbfile_fb85d9_p2_count_holders(const json_t *req);

/* PoP: w29_dbfile_fb85d9_p2_orphan @ persistence/sqlite/_state/dbfile.py:_is_inactive_orphan_desktop_holder
 * in:  {"ppid": int, "age_seconds": num, "min_age_seconds": num,
 *       "ephemeral_backend": bool, "connection_statuses": [str, ...]}
 * out: true when the parent is init/orphaned (ppid 0 or 1), the holder is at
 *      least min_age old, the backend is ephemeral, and no connection is
 *      ESTABLISHED. */
json_t *w29_dbfile_fb85d9_p2_orphan(const json_t *req);

/* PoP: w29_dbfile_fb85d9_p2_constants @ persistence/sqlite/_state/dbfile.py:<module>
 * The retired-generation capture tunables, as an object. */
json_t *w29_dbfile_fb85d9_p2_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W29_DBFILE_FB85D9_P2_H */
